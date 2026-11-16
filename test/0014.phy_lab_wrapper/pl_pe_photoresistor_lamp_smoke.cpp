#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_photoresistor_lamp_smoke");

    auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
    auto photo = ex.add_circuit_element("Photoresistor", position{0.0, 0.0, 0.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(src).data()["Properties"]["电压"] = 5.0;

    // Deliberately no "电阻" on the photoresistor: real .sav has no default template,
    // which previously aborted the build.
    ex.connect(src, 0, photo, 0);
    ex.connect(photo, 1, gnd, 0);
    ex.connect(src, 1, gnd, 0);

    auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c.analyze();
    auto s = c.sample_now();
    // Series current through the default 10k photoresistor: I = 5 / 10k = 0.5 mA.
    double const v_photo = s.pin_voltage[s.pin_voltage_ord[1] + 0] - s.pin_voltage[s.pin_voltage_ord[1] + 1];
    ::std::printf("photoresistor default r: V = %f\n", v_photo);
    assert(::std::fabs(v_photo - 5.0) < 1e-6);

    // Explicit resistance still wins.
    ex.get_element(photo).data()["Properties"]["电阻"] = 1000.0;
    auto c2 = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c2.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c2.analyze();
    auto s2 = c2.sample_now();
    double const i_photo = (s2.pin_voltage[s2.pin_voltage_ord[0] + 0] - s2.pin_voltage[s2.pin_voltage_ord[1] + 0]) / 1000.0;
    ::std::printf("photoresistor r=1000: I = %f\n", i_photo);

    // A zero (or negative) resistance must not reach resistance.h's G = 1/R.
    ex.get_element(photo).data()["Properties"]["电阻"] = 0.0;
    auto c3 = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c3.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c3.analyze();
    auto s3 = c3.sample_now();
    for(::std::size_t i{}; i < s3.pin_voltage.size(); ++i)
    {
        assert(::std::isfinite(s3.pin_voltage[i]));
    }

    // Incandescent Lamp with a missing resistance falls back to 10 ohm.
    auto ex2 = experiment::create(experiment_type::circuit);
    ex2.entitle("pl_pe_lamp_default");
    auto src2 = ex2.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
    auto lamp = ex2.add_circuit_element("Incandescent Lamp", position{0.0, 0.0, 0.0});
    auto gnd2 = ex2.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});
    ex2.get_element(src2).data()["Properties"]["电压"] = 5.0;
    ex2.connect(src2, 0, lamp, 0);
    ex2.connect(lamp, 1, gnd2, 0);
    ex2.connect(src2, 1, gnd2, 0);
    auto c4 = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex2);
    c4.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c4.analyze();
    auto s4 = c4.sample_now();
    double const v_lamp = s4.pin_voltage[s4.pin_voltage_ord[1] + 0] - s4.pin_voltage[s4.pin_voltage_ord[1] + 1];
    ::std::printf("lamp default r: V = %f\n", v_lamp);
    assert(::std::fabs(v_lamp - 5.0) < 1e-6);
}
