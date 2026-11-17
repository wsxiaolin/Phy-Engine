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
    ex.entitle("pl_pe_meter_defaults_smoke");

    auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
    auto r1 = ex.add_circuit_element("Resistor", position{-1.0, 0.0, 0.0});
    auto r2 = ex.add_circuit_element("Resistor", position{1.0, 0.0, 0.0});
    auto vm = ex.add_circuit_element("Simple Voltmeter", position{1.0, 0.0, 1.0});
    auto am = ex.add_circuit_element("Simple Ammeter", position{-2.0, 0.0, 0.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(src).data()["Properties"]["电压"] = 10.0;
    ex.get_element(r1).data()["Properties"]["电阻"] = 1000.0;
    ex.get_element(r2).data()["Properties"]["电阻"] = 1000.0;

    // Deliberately do NOT set "电阻" on the meters: real PL .sav has no default
    // resistance template for instruments, which previously aborted the build.

    // src + -- ammeter -- r1 -- r2 -- gnd, voltmeter across r2.
    // Unity meters: pin0 is the common negative terminal, pin1 the positive tap, so the
    // voltmeter's common goes to ground and its + goes to the r1/r2 junction.
    ex.connect(src, 0, am, 1);
    ex.connect(am, 0, r1, 0);
    ex.connect(r1, 1, r2, 0);
    ex.connect(r2, 1, gnd, 0);
    ex.connect(src, 1, gnd, 0);
    ex.connect(vm, 1, r1, 1);
    ex.connect(vm, 0, gnd, 0);

    auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c.analyze();

    auto s = c.sample_now();
    // Non-ground PL components: src(0), ammeter(1), r1(2), r2(3), voltmeter(4).
    double const v_r2 = s.pin_voltage[s.pin_voltage_ord[3] + 0] - s.pin_voltage[s.pin_voltage_ord[3] + 1];
    ::std::printf("voltmeter default r: r2 voltage = %f\n", v_r2);

    // Series ammeter movement (1/72 ohm) + 2k divider. The voltmeter sits across r2 with
    // its Unity topology: 3000 ohm movement on pin1-pin0 plus the open 15 V tap.
    // Parallel 3000||1000 = 750 ohm, so r2 sees 10 * 750/(1000+750) = ~4.286 V.
    assert(v_r2 > 4.20 && v_r2 < 4.35);

    // Voltmeter must not short the node: r2 voltage stays near half of the source.
    // A missing template would have thrown instead of reaching this line.
    // The voltmeter must not short the node: the reading stays in the volts range.
    double const v_r2_no_load = v_r2;
    assert(v_r2_no_load > 4.0);

    // Now switch the voltmeter to Unity's ideal mode and confirm the loading disappears.
    ex.get_element(vm).data()["Properties"]["理想模式"] = 1.0;
    auto c2 = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c2.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c2.analyze();
    auto s2 = c2.sample_now();
    double const v_r2b = s2.pin_voltage[s2.pin_voltage_ord[3] + 0] - s2.pin_voltage[s2.pin_voltage_ord[3] + 1];
    ::std::printf("explicit voltmeter r=1e6: r2 voltage = %f\n", v_r2b);
    assert(v_r2b > v_r2 + 0.1);
    assert(::std::fabs(v_r2b - 5.0) < 0.05);
}
