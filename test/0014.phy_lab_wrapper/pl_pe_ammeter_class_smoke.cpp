#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

// Unity Physics Lab reference:
//   SimpleAmmeter      : realistic delta R1(pin1->pin0)=1/72, R2(pin1->pin2)=0.1, R3(pin2->pin0)=0.025
//                        ideal single 1e-6 movement.
//   Galvanometer       : R1(pin1->pin0)=100, R2(pin2->pin1)=2400.
//   Microammeter       : R1(pin1->pin0)=500, R2(pin2->pin1)=2000.
//
// The adapter exposes pin0 as common (-), pin1 as the low-range (+) tap and pin2 as the
// high-range (+) tap. Terminals that share a PE node must read the same voltage, and an
// ammeter loop must drop a current-driven IR this test can predict from the schematic.
int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto near_rel = [](double a, double b, double rel) noexcept {
        return ::std::fabs(a - b) <= rel * (::std::fabs(a) + ::std::fabs(b)) + 1e-12;
    };

    // ---- Simple Ammeter in a series loop: expects the delta's pin1->pin0 equivalent ----
    // Circuit: src 3 V -- ammeter(pin1 .. pin0) -- 3 ohm load -- gnd.
    // Equivalent ammeter resistance between pin1 and pin0:
    //   R_movement || (R2 + R3) = (1/72) || (0.1 + 0.025) = 0.0138889 || 0.125 = ~0.0125 ohm.
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_ammeter_class_smoke");

        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto am = ex.add_circuit_element("Simple Ammeter", position{-1.0, 0.0, 0.0});
        auto load = ex.add_circuit_element("Resistor", position{1.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 3.0;
        ex.get_element(load).data()["Properties"]["电阻"] = 3.0;

        // pin1 = low-range +, pin0 = common -, pin2 wired to the 3 A tap (left open here).
        ex.connect(src, 0, am, 1);
        ex.connect(am, 0, load, 0);
        ex.connect(load, 1, gnd, 0);
        ex.connect(src, 1, gnd, 0);

        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();

        // Non-ground components: src(0), R1(1), R2(2), R3(3), load(4).
        double const v_pin1 = s.pin_voltage[s.pin_voltage_ord[1] + 0];
        double const v_pin0 = s.pin_voltage[s.pin_voltage_ord[1] + 1];
        double const v_pin2 = s.pin_voltage[s.pin_voltage_ord[2] + 1];
        double const v_load_hi = s.pin_voltage[s.pin_voltage_ord[4] + 0];
        ::std::printf("ammeter: pin1=%f pin0=%f pin2=%f V_pin1-pin0=%f\n", v_pin1, v_pin0, v_pin2,
                      v_pin1 - v_pin0);

        // The ammeter drop must be the expected IR, much smaller than the load's.
        double const r_eq = (1.0 / 72.0) * 0.125 / (1.0 / 72.0 + 0.125);  // ~0.0125 ohm
        double const i = 3.0 / (3.0 + r_eq);
        double const v_drop = i * r_eq;
        ::std::printf("ammeter: expected drop=%f (r_eq=%f, i=%f)\n", v_drop, r_eq, i);
        assert(near_rel(v_pin1 - v_pin0, v_drop, 1e-3));
        // Load sees the source minus the ammeter's own drop (a few mV here).
        assert(near_rel(v_load_hi, 3.0 - v_drop, 1e-3));

        // The 3 A tap (pin2) sits on the R2+R3 branch of the delta, so only part of the
        // loop current flows through it: i_branch = i * R1 / (R1 + R2 + R3).
        double const r1 = 1.0 / 72.0;
        double const i_branch = i * r1 / (r1 + 0.1 + 0.025);
        assert(near_rel(v_pin1 - v_pin2, i_branch * 0.1, 1e-2));
        assert(near_rel(v_pin2 - v_pin0, i_branch * 0.025, 1e-2));
    }

    // ---- Ideal mode shortens the ammeter to a single 1e-6 ohm movement ----
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_ammeter_class_smoke");

        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto am = ex.add_circuit_element("Simple Ammeter", position{-1.0, 0.0, 0.0});
        auto load = ex.add_circuit_element("Resistor", position{1.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 3.0;
        ex.get_element(load).data()["Properties"]["电阻"] = 3.0;
        ex.get_element(am).data()["Properties"]["理想模式"] = 1.0;

        ex.connect(src, 0, am, 1);
        ex.connect(am, 0, load, 0);
        ex.connect(load, 1, gnd, 0);
        ex.connect(src, 1, gnd, 0);

        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();
        double const v_pin1 = s.pin_voltage[s.pin_voltage_ord[1] + 0];
        double const v_pin0 = s.pin_voltage[s.pin_voltage_ord[1] + 1];
        ::std::printf("ideal ammeter: drop=%g\n", v_pin1 - v_pin0);
        // 3 V over ~3 ohm -> ~1 A through the 1e-6 ohm movement: the drop must be in the
        // microvolt ballpark, far below the realistic 12 mV measured above.
        assert(v_pin1 - v_pin0 > 0.0);
        assert(v_pin1 - v_pin0 < 2e-6);
    }

    // ---- Galvanometer / Microammeter: pin2 range resistor in series with the movement ----
    // Source + -- pin2 --> R2(2400) --> pin1 --> R1(100) --> pin0 --> load 1000 --> gnd.
    // Serial sum = 100 + 2400 + 1000; tap at the movement's own drop.
    auto check_series_meter = [&](char const* model, double r_movement, double r_series) {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_ammeter_class_smoke");

        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto m = ex.add_circuit_element(model, position{-1.0, 0.0, 0.0});
        auto load = ex.add_circuit_element("Resistor", position{1.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

        ex.get_element(src).data()["Properties"]["电压"] = 5.0;
        ex.get_element(load).data()["Properties"]["电阻"] = 1000.0;

        ex.connect(src, 0, m, 2);  // pin2 range +
        ex.connect(m, 0, load, 0); // pin0 common -
        ex.connect(load, 1, gnd, 0);
        ex.connect(src, 1, gnd, 0);

        auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();

        // Non-ground: src(0), R1(1), R2(2), load(3).
        double const v_pin2 = s.pin_voltage[s.pin_voltage_ord[2] + 0];  // R2 pin0 = pin2
        double const v_pin1 = s.pin_voltage[s.pin_voltage_ord[1] + 0];  // R1 pin0 = pin1
        double const v_pin0 = s.pin_voltage[s.pin_voltage_ord[1] + 1];  // R1 pin1 = pin0

        double const i = 5.0 / (r_movement + r_series + 1000.0);
        ::std::printf("%s: v_pin2=%f v_pin1=%f v_pin0=%f (i=%g)\n", model, v_pin2, v_pin1, v_pin0, i);
        // Movement drop must match i * r_movement.
        assert(near_rel(v_pin1 - v_pin0, i * r_movement, 1e-3));
        // Series resistor drop must match i * r_series.
        assert(near_rel(v_pin2 - v_pin1, i * r_series, 1e-3));
    };

    check_series_meter("Galvanometer", 100.0, 2400.0);
    check_series_meter("Microammeter", 500.0, 2000.0);

    ::std::printf("pl_pe_ammeter_class_smoke: OK\n");
}
