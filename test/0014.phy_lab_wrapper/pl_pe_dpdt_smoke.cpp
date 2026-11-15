#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <array>
#include <cassert>
#include <cmath>

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_dpdt_smoke");

    auto src1 = ex.add_circuit_element("Student Source", position{-4.0, 0.0, 0.0});
    auto src2 = ex.add_circuit_element("Student Source", position{-4.0, 0.0, 1.0});
    auto sw = ex.add_circuit_element("DPDT Switch", position{0.0, 0.0, 0.0});
    auto r_no1 = ex.add_circuit_element("Resistor", position{2.0, 0.0, 0.0});
    auto r_nc1 = ex.add_circuit_element("Resistor", position{2.0, 0.0, 1.0});
    auto r_no2 = ex.add_circuit_element("Resistor", position{2.0, 0.0, 2.0});
    auto r_nc2 = ex.add_circuit_element("Resistor", position{2.0, 0.0, 3.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(src1).data()["Properties"]["电压"] = 5.0;
    ex.get_element(src2).data()["Properties"]["电压"] = 3.0;
    ex.get_element(sw).data()["Properties"]["开关"] = 1;

    // DPDT pin convention: 0=COM1, 1=NO1, 2=NC1, 3=COM2, 4=NO2, 5=NC2
    ex.connect(src1, 0, sw, 0);
    ex.connect(src2, 0, sw, 3);
    ex.connect(sw, 1, r_no1, 0);
    ex.connect(sw, 2, r_nc1, 0);
    ex.connect(sw, 4, r_no2, 0);
    ex.connect(sw, 5, r_nc2, 0);
    ex.connect(r_no1, 1, gnd, 0);
    ex.connect(r_nc1, 1, gnd, 0);
    ex.connect(r_no2, 1, gnd, 0);
    ex.connect(r_nc2, 1, gnd, 0);
    ex.connect(src1, 1, gnd, 0);
    ex.connect(src2, 1, gnd, 0);

    auto near = [](double a, double b) noexcept { return ::std::fabs(a - b) <= 1e-6; };

    // Non-ground component order: src1(0), src2(1), sw(2), r_no1(3), r_nc1(4), r_no2(5), r_nc2(6).
    auto sample_switch_pins = [](phy_engine::phy_lab_wrapper::pe::circuit& c) {
        auto s = c.sample_now();
        ::std::array<double, 6> v{};
        for(::std::size_t p{}; p < 6; ++p) { v[p] = s.pin_voltage[s.pin_voltage_ord[2] + p]; }
        return v;
    };

    // 开关=1: both poles COM-NO closed, COM-NC open (ganged).
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto const v = sample_switch_pins(c);
        assert(near(v[0], 5.0));  // COM1
        assert(near(v[1], 5.0));  // NO1
        assert(near(v[2], 0.0));  // NC1
        assert(near(v[3], 3.0));  // COM2
        assert(near(v[4], 3.0));  // NO2
        assert(near(v[5], 0.0));  // NC2
    }

    // The switch property is consumed at build time; rebuild from the updated experiment.
    ex.get_element(sw).data()["Properties"]["开关"] = 0;

    // 开关=0: both poles COM-NC closed, COM-NO open (ganged).
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto const v = sample_switch_pins(c);
        assert(near(v[0], 5.0));  // COM1
        assert(near(v[1], 0.0));  // NO1
        assert(near(v[2], 5.0));  // NC1
        assert(near(v[3], 3.0));  // COM2
        assert(near(v[4], 0.0));  // NO2
        assert(near(v[5], 3.0));  // NC2
    }
}
