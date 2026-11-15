#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_joystick_smoke");

    auto src_x = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 0.0});
    auto src_y = ex.add_circuit_element("Student Source", position{-2.0, 0.0, 1.0});
    auto joy = ex.add_circuit_element("Analog Joystick", position{0.0, 0.0, 0.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(src_x).data()["Properties"]["电压"] = 5.0;
    ex.get_element(src_y).data()["Properties"]["电压"] = 5.0;
    ex.get_element(joy).data()["Properties"]["电阻"] = 10000.0;

    // Joystick pins (catalog order): 0=X1, 1=X2, 2=X3, 3=Y1, 4=Y2, 5=Y3.
    // Student Source pins: 0=positive, 1=negative.
    ex.connect(src_x, 0, joy, 0);
    ex.connect(joy, 2, gnd, 0);
    ex.connect(src_x, 1, gnd, 0);
    ex.connect(src_y, 0, joy, 3);
    ex.connect(joy, 5, gnd, 0);
    ex.connect(src_y, 1, gnd, 0);

    auto near = [](double a, double b) noexcept { return ::std::fabs(a - b) <= 1e-6; };

    // The anonymous internal resistors occupy components in build order after the
    // sources: src_x(0), src_y(1), X-top(2), X-bottom(3), Y-top(4), Y-bottom(5).
    auto pin_v = [](phy_engine::phy_lab_wrapper::pe::circuit& c, std::size_t comp, int pin) -> double {
        auto s = c.sample_now();
        return s.pin_voltage[s.pin_voltage_ord[comp] + static_cast<std::size_t>(pin)];
    };

    // Unloaded wipers sit at the exact divider midpoint.
    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();

        double const v_x1 = pin_v(c, 2, 0);
        double const v_x2 = pin_v(c, 2, 1);
        double const v_x3 = pin_v(c, 3, 1);
        double const v_y1 = pin_v(c, 4, 0);
        double const v_y2 = pin_v(c, 4, 1);
        double const v_y3 = pin_v(c, 5, 1);
        ::std::printf("joystick X: %f %f %f, Y: %f %f %f\n", v_x1, v_x2, v_x3, v_y1, v_y2, v_y3);
        assert(near(v_x1, 5.0));
        assert(near(v_x2, 2.5));
        assert(near(v_x3, 0.0));
        assert(near(v_y1, 5.0));
        assert(near(v_y2, 2.5));
        assert(near(v_y3, 0.0));
    }

    // Light 1M load on each wiper also exercises the external PL wiper pins (X2/Y2).
    auto r_ld_x = ex.add_circuit_element("Resistor", position{2.0, 0.0, 0.0});
    auto r_ld_y = ex.add_circuit_element("Resistor", position{2.0, 0.0, 1.0});
    ex.get_element(r_ld_x).data()["Properties"]["电阻"] = 1e6;
    ex.get_element(r_ld_y).data()["Properties"]["电阻"] = 1e6;
    ex.connect(joy, 1, r_ld_x, 0);
    ex.connect(joy, 4, r_ld_y, 0);
    ex.connect(r_ld_x, 1, gnd, 0);
    ex.connect(r_ld_y, 1, gnd, 0);

    {
        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();

        // Bottom 5k half paralleled by the 1M load, fed by the top 5k half.
        double const rp = 5e3 * 1e6 / (5e3 + 1e6);
        double const expect = 5.0 * rp / (5e3 + rp);
        double const v_x2 = pin_v(c, 2, 1);
        double const v_y2 = pin_v(c, 4, 1);
        ::std::printf("joystick loaded wipers: %f %f (expect %f)\n", v_x2, v_y2, expect);
        assert(near(v_x2, expect));
        assert(near(v_y2, expect));
    }
}
