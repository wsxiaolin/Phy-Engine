#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
// In real usage you should link against the built `phyengine` shared/static library instead.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace
{
// Build a series divider: source(+) -> a0, a1 -> b0, b1 -> gnd, source(-) -> gnd.
// Returns the voltage across element `a` (its pin0 minus pin1).
double divider_voltage_a(double const source_volts,
                         char const* a_model,
                         char const* b_model,
                         double const load_ohms,
                         void (*configure_a)(phy_engine::phy_lab_wrapper::experiment&, std::string const&))
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_divider");

    auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
    auto a = ex.add_circuit_element(a_model, position{0.0, 0.0, 0.0});
    auto b = ex.add_circuit_element(b_model, position{1.5, 0.0, 0.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});

    ex.get_element(src).data()["Properties"]["电压"] = source_volts;
    ex.get_element(b).data()["Properties"]["电阻"] = load_ohms;
    if(configure_a) { configure_a(ex, a); }

    ex.connect(src, 0, a, 0);
    ex.connect(a, 1, b, 0);
    ex.connect(b, 1, gnd, 0);
    ex.connect(src, 1, gnd, 0);

    auto c = pe::circuit::build_from(ex);
    c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
    c.analyze();
    auto s = c.sample_now();
    return s.pin_voltage[s.pin_voltage_ord[1] + 0] - s.pin_voltage[s.pin_voltage_ord[1] + 1];
}

void set_lamp_6v_1w(phy_engine::phy_lab_wrapper::experiment& ex, std::string const& id)
{
    ex.get_element(id).data()["Properties"]["额定电压"] = 6.0;
    ex.get_element(id).data()["Properties"]["额定功率"] = 1.0;
}

void set_photo_bright_100k(phy_engine::phy_lab_wrapper::experiment& ex, std::string const& id)
{
    ex.get_element(id).data()["Properties"]["亮电阻"] = 100000.0;
}

void set_photo_explicit_1k(phy_engine::phy_lab_wrapper::experiment& ex, std::string const& id)
{
    ex.get_element(id).data()["Properties"]["电阻"] = 1000.0;
}
}  // namespace

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    // ---- Missing keys: Unity defaults are used ----
    // Unity Photoresistor has no "电阻"; it exposes "亮电阻" (default 10000).
    // A 10000 ohm series load must split the source voltage exactly in half.
    {
        double const v = divider_voltage_a(5.0, "Photoresistor", "Resistor", 10000.0, nullptr);
        ::std::printf("photoresistor default (亮电阻=10000) divider: V = %f\n", v);
        assert(::std::fabs(v - 2.5) < 1e-6);
    }
    {
        // Unity Incandescent Lamp derives R = U^2/P = 3^2/0.85 ~= 10.588 ohm.
        double const r_default = 3.0 * 3.0 / 0.85;
        double const v = divider_voltage_a(5.0, "Incandescent Lamp", "Resistor", r_default, nullptr);
        ::std::printf("lamp default (U^2/P=%.6f) divider: V = %f\n", r_default, v);
        assert(::std::fabs(v - 2.5) < 1e-6);
    }

    // ---- Explicit "电阻" still wins (legacy fixtures) ----
    {
        double const v = divider_voltage_a(5.0, "Photoresistor", "Resistor", 1000.0, &set_photo_explicit_1k);
        ::std::printf("photoresistor explicit r=1000 divider: V = %f\n", v);
        assert(::std::fabs(v - 2.5) < 1e-6);
    }

    // ---- Incandescent Lamp: R really comes from U^2/P ----
    // 6 V / 1 W => R = 36 ohm. With a 36 ohm series load the lamp must see 6 V of 12 V.
    {
        double const v = divider_voltage_a(12.0, "Incandescent Lamp", "Resistor", 36.0, &set_lamp_6v_1w);
        ::std::printf("lamp U=6,P=1 (r=36) vs 36 ohm load: V_lamp = %f\n", v);
        assert(::std::fabs(v - 6.0) < 1e-6);
    }

    // ---- Photoresistor: 亮电阻 is the static operating point ----
    // 100000 ohm bright resistance with a 100000 ohm load splits 12 V in half.
    {
        double const v = divider_voltage_a(12.0, "Photoresistor", "Resistor", 100000.0, &set_photo_bright_100k);
        ::std::printf("photoresistor 亮电阻=100000 vs 100000 ohm load: V_photo = %f\n", v);
        assert(::std::fabs(v - 6.0) < 1e-6);
    }

    // ---- A zero (or negative) resistance must not reach resistance.h's G = 1/R ----
    {
        auto ex = experiment::create(experiment_type::circuit);
        ex.entitle("pl_pe_photoresistor_lamp_zero_guard");
        auto src = ex.add_circuit_element("Student Source", position{-3.0, 0.0, 0.0});
        auto photo = ex.add_circuit_element("Photoresistor", position{0.0, 0.0, 0.0});
        auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, -1.0});
        ex.get_element(src).data()["Properties"]["电压"] = 5.0;
        ex.get_element(photo).data()["Properties"]["电阻"] = 0.0;
        ex.connect(src, 0, photo, 0);
        ex.connect(photo, 1, gnd, 0);
        ex.connect(src, 1, gnd, 0);

        auto c = pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_DC);
        c.analyze();
        auto s = c.sample_now();
        for(::std::size_t i{}; i < s.pin_voltage.size(); ++i)
        {
            assert(::std::isfinite(s.pin_voltage[i]));
        }
    }
}
