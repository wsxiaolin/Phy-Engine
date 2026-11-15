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
    ex.entitle("pl_pe_spdt_smoke");

    auto src = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 0.0});
    auto sw = ex.add_circuit_element("SPDT Switch", position{0.0, 0.0, 0.0});
    auto r_no = ex.add_circuit_element("Resistor", position{2.0, 0.0, 0.0});
    auto r_nc = ex.add_circuit_element("Resistor", position{2.0, 0.0, 1.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(src).data()["Properties"]["电压"] = 5.0;
    ex.get_element(sw).data()["Properties"]["开关"] = 1;

    // SPDT pins: 0=COM, 1=NO, 2=NC
    ex.connect(src, 0, sw, 0);
    ex.connect(sw, 1, r_no, 0);
    ex.connect(sw, 2, r_nc, 0);
    ex.connect(r_no, 1, gnd, 0);
    ex.connect(r_nc, 1, gnd, 0);
    ex.connect(src, 1, gnd, 0);

    auto near = [](double a, double b) noexcept { return ::std::fabs(a - b) <= 1e-6; };

    // Non-ground component order: src(0), sw(1), r_no(2), r_nc(3).
    auto sample_switch_pins = [](phy_engine::phy_lab_wrapper::pe::circuit& c) {
        auto s = c.sample_now();
        double const v_com = s.pin_voltage[s.pin_voltage_ord[1] + 0];
        double const v_no = s.pin_voltage[s.pin_voltage_ord[1] + 1];
        double const v_nc = s.pin_voltage[s.pin_voltage_ord[1] + 2];
        return ::std::array<double, 3>{v_com, v_no, v_nc};
    };

    // 开关=1: COM-NO closed, COM-NC open.
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto const v = sample_switch_pins(c);
        assert(near(v[0], 5.0));
        assert(near(v[1], 5.0));
        assert(near(v[2], 0.0));
    }

    // The switch property is consumed at build time; rebuild from the updated experiment.
    ex.get_element(sw).data()["Properties"]["开关"] = 0;

    // 开关=0: COM-NC closed, COM-NO open.
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto const v = sample_switch_pins(c);
        assert(near(v[0], 5.0));
        assert(near(v[1], 0.0));
        assert(near(v[2], 5.0));
    }
}
