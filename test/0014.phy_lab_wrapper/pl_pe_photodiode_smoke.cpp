#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

// Photodiode macro expansion:
//   pin0 = anode, pin1 = cathode.
//   PN junction (anode..cathode) + series 100 ohm to pin1 + parallel 1e8 ohm.
//   Photocurrent IDC (Unity: 亮度 * 响应系数 * 1e-4) is read from the adapter-side
//   optional property `环境亮度` (lux); it defaults to 0 (dark).

static double pin_v(phy_engine::phy_lab_wrapper::pe::sample const& s, std::size_t comp, std::size_t pin)
{
    return s.pin_voltage[s.pin_voltage_ord[comp] + pin];
}

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    // ---- 1) Dark, forward biased: 0.7 V source, current limited by 1 kOhm ----
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_photodiode_forward");
        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto pd = ex.add_circuit_element("Photodiode", position{0.0, 0.0, 0.0});
        auto r = ex.add_circuit_element("Resistor", position{2.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 0.7;
        ex.get_element(r).data()["Properties"]["电阻"] = 1000.0;

        // Rule out the wrong polarity: wiring A->K with the forward diode.
        ex.connect(src, 0, pd, 0);   // + -> anode
        ex.connect(pd, 1, r, 0);     // cathode -> R
        ex.connect(r, 1, gnd, 0);
        ex.connect(src, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();

        double const v_anode = pin_v(s, 1, 0);
        double const v_cathode = pin_v(s, 1, 1);
        double const v_diode = v_anode - v_cathode;
        ::std::printf("[forward] V_anode=%f V_cathode=%f V_diode=%f\n", v_anode, v_cathode, v_diode);

        // The forward drop is the diode's own junction drop, not the full 0.7 V source.
        assert(v_diode > 0.5 && v_diode < 0.7);
    }

    // ---- 2) Dark, reverse biased: cathode toward +, anode toward gnd; ~no current ----
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_photodiode_reverse");
        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto pd = ex.add_circuit_element("Photodiode", position{0.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 5.0;

        ex.connect(src, 0, pd, 1);   // + -> cathode
        ex.connect(pd, 0, gnd, 0);   // anode -> gnd
        ex.connect(src, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();

        double const v_cathode = pin_v(s, 1, 1);
        double const v_anode = pin_v(s, 1, 0);
        ::std::printf("[reverse dark] V_cathode=%f V_anode=%f\n", v_cathode, v_anode);
        // Cathode sits essentially at the source voltage: no reverse current path.
        assert(::std::fabs(v_cathode - 5.0) < 1e-3);
        assert(::std::fabs(v_anode) < 1e-3);
    }

    // ---- 3) Illuminated, reverse biased: photocurrent flows cathode -> anode through the
    //         external source, so the cathode node drops below the source voltage. ----
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_photodiode_photo");
        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto pd = ex.add_circuit_element("Photodiode", position{0.0, 0.0, 0.0});
        auto r = ex.add_circuit_element("Resistor", position{2.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 5.0;
        // 1 kOhm series resistor turns the photocurrent into a measurable node drop.
        ex.get_element(r).data()["Properties"]["电阻"] = 1000.0;

        // Adapter-side convention: 环境亮度 (lux) + 响应系数 drive the photocurrent.
        // lux=10, coeff=1.0 -> I = 10 * 1.0 * 1e-4 = 1 mA, which 1 kOhm converts to 1 V.
        // (The source is ideal and pins the cathode, so the drop must be measured on R.)
        ex.get_element(pd).data()["Properties"]["环境亮度"] = 10.0;
        ex.get_element(pd).data()["Properties"]["响应系数"] = 1.0;

        ex.connect(src, 0, pd, 1);   // + -> cathode
        ex.connect(pd, 0, r, 0);     // anode -> R
        ex.connect(r, 1, gnd, 0);
        ex.connect(src, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();

        double const v_cathode = pin_v(s, 1, 1);
        double const v_anode = pin_v(s, 1, 0);
        // The photodiode expands into several internal elements, so address the load by
        // the anode potential instead of a hard-coded internal index.
        double const i_r = v_anode / 1000.0;
        ::std::printf("[photo] V_cathode=%f V_anode=%f I_R=%f\n", v_cathode, v_anode, i_r);

        // 1 mA photocurrent (10 lux * 1.0 * 1e-4) shows up as 1 V across the 1 kOhm load,
        // with the anode side positive; the 100 ohm series resistor adds a 0.1 V drop, so
        // the cathode pin (outside that resistor) sits at 5 - 0.1 = 4.9 V.
        assert(::std::fabs(v_cathode - 4.9) < 1e-2);
        assert(v_anode > 0.99 && v_anode < 1.01);
        assert(::std::fabs(i_r) > 9e-4 && ::std::fabs(i_r) < 1.1e-3);
        assert(::std::isfinite(v_cathode) && ::std::isfinite(v_anode));
    }

    return 0;
}
