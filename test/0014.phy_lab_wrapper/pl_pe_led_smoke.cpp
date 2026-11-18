#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

// LED macro expansion, verified against the Unity demos:
//   Light-Emitting Diode        : pin0 -> LED -> 300 ohm -> pin1 (green path only)
//   Dual Light-Emitting Diode   : green pin0 -> LED -> R -> pin1
//                                 red   pin1 -> LED -> R -> pin0 (anti-parallel)
//   Color Light-Emitting Diode  : pin0/pin1/pin2 -> LED -> R -> pin3 (common cathode)
//
// The series resistor is 工作电压 / 工作电流 (Unity default 3 V / 0.01 A = 300 ohm).
// 前向压降 is an emergent PN I-V result (the stock Is/N curve is kept), so the junction
// drop is ~0.6-0.8 V at 10 mA rather than Unity's 2.1 V constant.

static double pin_v(phy_engine::phy_lab_wrapper::pe::sample const& s, std::size_t comp, std::size_t pin)
{
    return s.pin_voltage[s.pin_voltage_ord[comp] + pin];
}

int main()
{
    using namespace phy_engine::phy_lab_wrapper;
    ::std::setvbuf(stdout, nullptr, _IONBF, 0);

    // ---- 1) Single-colour LED: forward conduction ----
    // Element order after expansion: src(0), PN(1), series R(2). Gnd code 0 has no slot.
    // R.0 is the junction cathode / resistor input, R.1 is LED pin1 (grounded).
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_led_single");
        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto led = ex.add_circuit_element("Light-Emitting Diode", position{0.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.connect(src, 0, led, 0);  // + -> anode
        ex.connect(led, 1, gnd, 0);  // cathode -> gnd
        ex.connect(src, 1, gnd, 0);

        auto run = [&](double volts) {
            ex.get_element(src).data()["Properties"]["电压"] = volts;
            auto c = pe::circuit::build_from(ex);
            c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
            c.analyze();
            return c.sample_now();
        };

        double const series_r = 300.0;
        auto s_low = run(0.7);
        double const i_low = pin_v(s_low, 2, 0) / series_r;
        double const v_led_low = pin_v(s_low, 1, 0) - pin_v(s_low, 1, 1);
        ::std::printf("[single 0.7V] V_led=%f I_led=%f A\n", v_led_low, i_low);
        // 0.7 V sits below the practical knee: only hundreds of microamps flow.
        assert(i_low > 0.0 && i_low < 5e-4);
        assert(v_led_low > 0.45 && v_led_low < 0.7);

        auto s_on = run(5.0);
        double const i_on = pin_v(s_on, 2, 0) / series_r;
        double const v_led_on = pin_v(s_on, 1, 0) - pin_v(s_on, 1, 1);
        ::std::printf("[single 5V] V_led=%f I_led=%f A\n", v_led_on, i_on);
        // 5 V across 300 ohm minus a ~0.7 V junction drop -> ~14 mA.
        assert(i_on > 8e-3 && i_on < 20e-3);
        assert(v_led_on > 0.5 && v_led_on < 0.9);
        assert(i_on > 20.0 * i_low);

        // 工作电压 / 工作电流 drives the series resistor: 2 V / 20 mA -> 100 ohm.
        ex.get_element(led).data()["Properties"]["工作电压"] = 2.0;
        ex.get_element(led).data()["Properties"]["工作电流"] = 0.02;
        auto s_custom = run(5.0);
        double const i_custom = pin_v(s_custom, 2, 0) / 100.0;
        ::std::printf("[single 5V, R=100] I_led=%f A\n", i_custom);
        assert(i_custom > 3e-2 && i_custom < 6e-2);
    }

    // ---- 2) Dual LED: anti-parallel paths ----
    // Element order after expansion: src(0), green PN(1), green R(2), red PN(3), red R(4).
    auto run_dual = [](bool forward) {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle(forward ? "pl_pe_dual_forward" : "pl_pe_dual_reverse");
        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto led = ex.add_circuit_element("Dual Light-Emitting Diode", position{0.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 5.0;
        if (forward)
        {
            ex.connect(src, 0, led, 0);
            ex.connect(led, 1, gnd, 0);
        }
        else
        {
            ex.connect(src, 0, led, 1);
            ex.connect(led, 0, gnd, 0);
        }
        ex.connect(src, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        return c.sample_now();
    };

    {
        constexpr double series_r = 300.0;
        auto sf = run_dual(true);
        double const green_i = pin_v(sf, 2, 0) / series_r;  // pin0 -> pin1
        double const red_drop = pin_v(sf, 4, 0) - pin_v(sf, 4, 1);
        double const v0 = pin_v(sf, 1, 0);   // green anode == PL pin0
        double const v1 = pin_v(sf, 2, 1);   // green series-R end == PL pin1
        ::std::printf("[dual forward] V0=%f V1=%f I_green=%f I_red_drop=%f\n", v0, v1, green_i, red_drop);
        // Pin0 is the anode for the green path, so current flows pin0 -> pin1.
        assert(v0 > 4.5 && v0 <= 5.0 + 1e-9);
        assert(::std::fabs(v1) < 1e-3);
        assert(green_i > 8e-3 && green_i < 20e-3);
        // The red path is reverse biased: no drop across its series resistor.
        assert(::std::fabs(red_drop) < 1e-6);

        auto sr = run_dual(false);
        double const red_i = pin_v(sr, 4, 0) / series_r;    // pin1 -> pin0
        double const green_drop = pin_v(sr, 2, 0) - pin_v(sr, 2, 1);
        double const v0r = pin_v(sr, 4, 1);  // red series-R end == PL pin0
        double const v1r = pin_v(sr, 3, 0);  // red anode == PL pin1
        ::std::printf("[dual reverse] V0=%f V1=%f I_red=%f I_green_drop=%f\n", v0r, v1r, red_i, green_drop);
        // Pin1 is now the anode: the same physical current is negative in pin0 -> pin1.
        assert(::std::fabs(v0r) < 1e-3);
        assert(v1r > 4.5 && v1r <= 5.0 + 1e-9);
        assert(red_i > 8e-3 && red_i < 20e-3);
        assert(::std::fabs(green_drop) < 1e-6);
        assert(-red_i < 0.0 && green_i > 0.0);
    }

    // ---- 3) Color LED: only the driven anode carries current ----
    // Element order after expansion: src(0), red PN/R (1,2), green PN/R (3,4), blue PN/R (5,6).
    auto run_color = [](int driven_pin) {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_color_led");
        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto led = ex.add_circuit_element("Color Light-Emitting Diode", position{0.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 5.0;
        ex.connect(src, 0, led, driven_pin);
        ex.connect(led, 3, gnd, 0);  // common cathode
        ex.connect(src, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        return c.sample_now();
    };

    {
        constexpr double series_r = 300.0;
        std::size_t const r_idx[3] = {2, 4, 6};
        std::size_t const pn_idx[3] = {1, 3, 5};
        for (int driven = 0; driven < 3; ++driven)
        {
            auto s = run_color(driven);
            double const v_anode = pin_v(s, pn_idx[driven], 0);
            ::std::printf("[color pin%d] V_anode=%f I=[", driven, v_anode);
            for (int color = 0; color < 3; ++color)
            {
                double const i_branch = pin_v(s, r_idx[color], 0) / series_r;
                ::std::printf("%s%f", color == 0 ? "" : ", ", i_branch);
                if (color == driven)
                {
                    assert(i_branch > 8e-3 && i_branch < 20e-3);
                }
                else
                {
                    // Off branches have no current path: no drop across their series R.
                    assert(::std::fabs(i_branch) < 1e-9);
                }
            }
            ::std::printf("]\n");
            assert(v_anode > 4.5 && v_anode <= 5.0 + 1e-9);
        }
    }

    return 0;
}
