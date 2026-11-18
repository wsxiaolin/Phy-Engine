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
    ex.entitle("pl_pe_rheostat_smoke");

    auto src = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 0.0});
    auto rheo = ex.add_circuit_element("Slide Rheostat", position{0.0, 0.0, 0.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(src).data()["Properties"]["电压"] = 5.0;
    ex.get_element(rheo).data()["Properties"]["电阻"] = 10000.0;

    // Adapter convention (catalog pins are unnamed): 0=end A, 1/2=wiper taps, 3=end B.
    // Slider fraction is the resistance share below the wiper: V_tap = f * (V_A - V_B).
    ex.connect(src, 0, rheo, 0);
    ex.connect(rheo, 3, gnd, 0);
    ex.connect(src, 1, gnd, 0);

    auto near = [](double a, double b) noexcept { return ::std::fabs(a - b) <= 1e-6; };

    // Non-ground component order: src(0), R-top(1), R-bottom(2); tap = R-top pin 1.
    auto tap_v = [](phy_engine::phy_lab_wrapper::pe::circuit& c) -> double {
        auto s = c.sample_now();
        return s.pin_voltage[s.pin_voltage_ord[1] + 1];
    };

    // Missing slider key: default fraction 0.5 -> half of the source voltage.
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        double const v = tap_v(c);
        ::std::printf("rheostat default tap: %f\n", v);
        assert(near(v, 2.5));
    }

    // Slider key "滑块" = 0.25 -> 25% of the source voltage (rebuild needed).
    ex.get_element(rheo).data()["Properties"]["滑块"] = 0.25;
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        double const v = tap_v(c);
        ::std::printf("rheostat slide=0.25 tap: %f\n", v);
        assert(near(v, 1.25));
    }

    // Fallback key "比例" (no "滑块") = 0.75 -> 75% of the source voltage.
    ex.get_element(rheo).data()["Properties"].erase("滑块");
    ex.get_element(rheo).data()["Properties"]["比例"] = 0.75;
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        double const v = tap_v(c);
        ::std::printf("rheostat ratio=0.75 tap: %f\n", v);
        assert(near(v, 3.75));
    }
}
