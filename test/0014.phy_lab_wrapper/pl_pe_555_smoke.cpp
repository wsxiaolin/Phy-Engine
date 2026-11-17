#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cstddef>
#include <cstdio>

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_555_smoke");

    auto vcc = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 0.0});
    auto trig = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 1.0});
    auto thr = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 2.0});
    auto rst = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 3.0});
    auto r_out = ex.add_circuit_element("Resistor", position{2.0, 0.0, 0.0});
    auto r_dis = ex.add_circuit_element("Resistor", position{2.0, 0.0, 1.0});
    auto timer = ex.add_circuit_element("555 Timer", position{0.0, 0.0, 0.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(vcc).data()["Properties"]["电压"] = 5.0;
    ex.get_element(trig).data()["Properties"]["电压"] = 0.0;
    ex.get_element(thr).data()["Properties"]["电压"] = 0.0;
    ex.get_element(rst).data()["Properties"]["电压"] = 5.0;
    ex.get_element(r_out).data()["Properties"]["电阻"] = 1e6;
    ex.get_element(r_dis).data()["Properties"]["电阻"] = 1e3;

    // 555 pins (Unity TimerChip order):
    // 0=V+, 1=Dis, 2=Thr, 3=Ctrl, 4=Trig, 5=Out, 6=Reset, 7=Ground.
    // Ctrl is intentionally left unconnected to exercise the internal 5k/10k divider.
    ex.connect(vcc, 0, timer, 0);
    ex.connect(r_dis, 0, timer, 1);
    ex.connect(thr, 0, timer, 2);
    ex.connect(trig, 0, timer, 4);
    ex.connect(r_out, 0, timer, 5);
    ex.connect(rst, 0, timer, 6);
    ex.connect(timer, 7, gnd, 0);
    ex.connect(vcc, 1, gnd, 0);
    ex.connect(trig, 1, gnd, 0);
    ex.connect(thr, 1, gnd, 0);
    ex.connect(rst, 1, gnd, 0);
    ex.connect(r_out, 1, gnd, 0);
    ex.connect(r_dis, 1, timer, 0);

    // Locate the PE component index: non-ground elements keep insertion order.
    std::size_t timer_index = ex.elements().size();
    {
        std::size_t pe_index{};
        for (auto const& e : ex.elements())
        {
            if (e.identifier() == timer) { timer_index = pe_index; break; }
            if (e.data().value("ModelID", "") != "Ground Component") { ++pe_index; }
        }
    }
    assert(timer_index < ex.elements().size());

    auto sample_pin = [&](pe::circuit& c, int pin) -> double {
        auto s = c.sample_now();
        return s.pin_voltage[s.pin_voltage_ord[timer_index] + static_cast<std::size_t>(pin)];
    };

    auto run = [&]() {
        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        return c;
    };

    // a) Reset high, Trig low (< Ctrl/2 = Vcc/3): Out is driven high through the 1 ohm switch.
    {
        auto c = run();
        double const v_out = sample_pin(c, 5);
        double const v_dis = sample_pin(c, 1);
        std::printf("555 trigger: out=%f dis=%f\n", v_out, v_dis);
        assert(v_out > 4.9 && v_out <= 5.0);
        // Discharge is open, so Dis only sees the 1k pull-up to V+.
        assert(v_dis > 4.9 && v_dis <= 5.0);
    }

    // b) Thr above Ctrl while Trig stays above Ctrl/2: Out goes low and Dis is pulled to ground.
    ex.get_element(trig).data()["Properties"]["电压"] = 5.0;
    ex.get_element(thr).data()["Properties"]["电压"] = 5.0;
    {
        auto c = run();
        double const v_out = sample_pin(c, 5);
        double const v_dis = sample_pin(c, 1);
        std::printf("555 threshold: out=%f dis=%f\n", v_out, v_dis);
        assert(v_out > -0.1 && v_out < 0.1);
        // Dis is grounded through 10 ohm against the 1k pull-up: ~5 * 10 / 1010.
        assert(v_dis > 0.01 && v_dis < 0.1);
    }

    // c) Reset low overrides the low trigger: Out is forced low.
    ex.get_element(trig).data()["Properties"]["电压"] = 0.0;
    ex.get_element(thr).data()["Properties"]["电压"] = 0.0;
    ex.get_element(rst).data()["Properties"]["电压"] = 0.0;
    {
        auto c = run();
        double const v_out = sample_pin(c, 5);
        double const v_dis = sample_pin(c, 1);
        std::printf("555 reset: out=%f dis=%f\n", v_out, v_dis);
        assert(v_out > -0.1 && v_out < 0.1);
        assert(v_dis > 0.01 && v_dis < 0.1);
    }
}
