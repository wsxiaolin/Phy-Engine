#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string_view>

// Sensor macros, verified against the Unity sources:
//   Sensor3Axies.cs : 3x (DC source + series `输出阻抗`) driving Pins[0..2];
//                     static axis voltage = 偏移 (measured = 0).
//
// After expansion a three-axis sensor contributes VDC/R pairs in axis order, so the
// non-ground component indices are: axis0 VDC=0, axis0 R=1, axis1 VDC=2, axis1 R=3,
// axis2 VDC=4, axis2 R=5.
static double pin_v(phy_engine::phy_lab_wrapper::pe::sample const& s, std::size_t comp, std::size_t pin)
{
    return s.pin_voltage[s.pin_voltage_ord[comp] + pin];
}

static bool near(double a, double b, double rel)
{
    return ::std::fabs(a - b) <= ::std::fabs(b) * rel;
}

int main()
{
    using namespace phy_engine::phy_lab_wrapper;
    ::std::setvbuf(stdout, nullptr, _IONBF, 0);

    // ---- 1) Accelerometer defaults: each axis sits at OffsetDefault = 0.75 V ----
    // High-impedance loads (1e9) so the 10k series resistance drops almost nothing.
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_sensor_accel_default");
        auto sensor = ex.add_circuit_element("Accelerometer", position{0.0, 0.0, 0.0});
        auto ld0 = ex.add_circuit_element("Resistor", position{1.0, 1.0, 0.0});
        auto ld1 = ex.add_circuit_element("Resistor", position{1.0, 0.0, 0.0});
        auto ld2 = ex.add_circuit_element("Resistor", position{1.0, -1.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(ld0).data()["Properties"]["电阻"] = 1e9;
        ex.get_element(ld1).data()["Properties"]["电阻"] = 1e9;
        ex.get_element(ld2).data()["Properties"]["电阻"] = 1e9;

        ex.connect(sensor, 0, ld0, 0);
        ex.connect(sensor, 1, ld1, 0);
        ex.connect(sensor, 2, ld2, 0);
        ex.connect(ld0, 1, gnd, 0);
        ex.connect(ld1, 1, gnd, 0);
        ex.connect(ld2, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();

        double const expected = 0.75;  // Accelerometer.prefab OffsetDefault
        for (int axis = 0; axis < 3; ++axis)
        {
            double const v = pin_v(s, static_cast<std::size_t>(1 + 2 * axis), 1);
            ::std::printf("[accel default] axis%d = %f V (expect %f)\n", axis, v, expected);
            assert(near(v, expected, 0.05));
        }
    }

    // ---- 2) Editing 偏移 moves every axis ----
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_sensor_accel_offset");
        auto sensor = ex.add_circuit_element("Accelerometer", position{0.0, 0.0, 0.0});
        auto ld = ex.add_circuit_element("Resistor", position{1.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(sensor).data()["Properties"]["偏移"] = 1.5;
        ex.get_element(ld).data()["Properties"]["电阻"] = 1e9;

        ex.connect(sensor, 0, ld, 0);
        ex.connect(ld, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();

        double const v = pin_v(s, 1, 1);
        ::std::printf("[accel offset 1.5] axis0 = %f V\n", v);
        assert(near(v, 1.5, 0.05));
    }

    // ---- 3) 输出阻抗 is a real series resistance: 10k load halves the output ----
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_sensor_accel_series_r");
        auto sensor = ex.add_circuit_element("Accelerometer", position{0.0, 0.0, 0.0});
        auto ld = ex.add_circuit_element("Resistor", position{1.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(sensor).data()["Properties"]["偏移"] = 1.5;
        ex.get_element(ld).data()["Properties"]["电阻"] = 10000.0;

        ex.connect(sensor, 0, ld, 0);
        ex.connect(ld, 1, gnd, 0);

        auto run = [&]() {
            auto c = pe::circuit::build_from(ex);
            c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
            c.analyze();
            return c.sample_now();
        };

        auto s_half = run();  // default 输出阻抗 = 10000
        double const v_half = pin_v(s_half, 1, 1);
        ::std::printf("[accel 10k load, Rz=10k] axis0 = %f V (expect 0.75)\n", v_half);
        assert(near(v_half, 0.75, 0.05));

        ex.get_element(sensor).data()["Properties"]["输出阻抗"] = 30000.0;
        auto s_quarter = run();  // 30k series + 10k load -> 1/4
        double const v_quarter = pin_v(s_quarter, 1, 1);
        ::std::printf("[accel 10k load, Rz=30k] axis0 = %f V (expect 0.375)\n", v_quarter);
        assert(near(v_quarter, 0.375, 0.05));
    }

    return 0;
}
