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
    ex.connect(src, 0, am, 0);
    ex.connect(am, 1, r1, 0);
    ex.connect(r1, 1, r2, 0);
    ex.connect(r2, 1, gnd, 0);
    ex.connect(src, 1, gnd, 0);
    ex.connect(vm, 0, r1, 1);
    ex.connect(vm, 1, gnd, 0);

    auto c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c.analyze();

    auto s = c.sample_now();
    // Non-ground component order: src(0), ammeter(1), r1(2), r2(3), voltmeter(4).
    double const v_r2 = s.pin_voltage[s.pin_voltage_ord[3] + 0] - s.pin_voltage[s.pin_voltage_ord[3] + 1];
    ::std::printf("voltmeter default r: r2 voltage = %f\n", v_r2);

    // Ammeter default 1e-3 ohm + 2k divider: r2 sees just under half of 10 V.
    // With a 1e6 ohm voltmeter in parallel the total is ~1998.0 ohm, so r2 = ~5.005 V.
    assert(v_r2 > 4.99 && v_r2 < 5.02);

    // Voltmeter must not short the node: r2 voltage stays near half of the source.
    // A missing template would have thrown instead of reaching this line.
    double const v_r2_no_load = v_r2;
    assert(::std::fabs(v_r2_no_load - 5.0) < 0.05);

    // Now give the voltmeter its default explicitly and confirm it still behaves.
    ex.get_element(vm).data()["Properties"]["电阻"] = 1e6;
    auto c2 = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c2.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c2.analyze();
    auto s2 = c2.sample_now();
    double const v_r2b = s2.pin_voltage[s2.pin_voltage_ord[3] + 0] - s2.pin_voltage[s2.pin_voltage_ord[3] + 1];
    ::std::printf("explicit voltmeter r=1e6: r2 voltage = %f\n", v_r2b);
    assert(::std::fabs(v_r2b - v_r2) < 1e-3);
}
