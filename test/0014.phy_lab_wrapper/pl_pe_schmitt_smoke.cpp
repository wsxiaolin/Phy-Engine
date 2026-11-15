#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <utility>

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_schmitt_smoke");

    auto vin = ex.add_circuit_element("Student Source", position{-1.0, 0.0, 0.0});
    auto st = ex.add_circuit_element("Schmitt Trigger", position{0.0, 0.0, 0.0});
    auto y = ex.add_circuit_element("Logic Output", position{1.0, 0.0, 0.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{1.0, 0.0, 1.0});

    ex.get_element(vin).data()["Properties"]["电压"] = 0.0;

    // Student Source pins: 0=+, 1=-. Schmitt Trigger pins: 0=i, 1=o. Logic Output pin: i=0.
    ex.connect(vin, 0, st, 0);
    ex.connect(vin, 1, gnd, 0);
    ex.connect(st, 1, y, 0);

    // Non-ground PL elements keep insertion order in the PE component list.
    std::size_t st_index = ex.elements().size();
    for (std::size_t i = 0; i < ex.elements().size(); ++i)
    {
        if (ex.elements()[i].identifier() == st) { st_index = i; }
    }
    assert(st_index < ex.elements().size());

    // Adapter defaults: Vth_low=1, Vth_high=2, inverted=0, Ll=0, Hl=5.
    auto settle_step = [&](double vin_v) -> std::pair<int, int> {
        ex.get_element(vin).data()["Properties"]["电压"] = vin_v;
        phy_engine::phy_lab_wrapper::pe::circuit c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
        c.set_analyze_type(PHY_ENGINE_ANALYZE_TR);
        c.set_tr(1e-9, 1e-9);
        int prev = -1;
        int out = -1;
        for (int step = 0; step < 4; ++step)
        {
            c.digital_clk();
            c.analyze();
            auto s = c.sample_now();
            prev = out;
            out = s.pin_digital[s.pin_digital_ord[st_index] + 1] != 0 ? 1 : 0;
            c.write_back_to_pl(ex, s);
        }
        auto const& out_props = ex.get_element(y).data()["Properties"];
        assert(out_props.value("状态", -1.0) == static_cast<double>(out));
        return {prev, out};
    };

    auto [low_prev, low_last] = settle_step(0.0);
    std::cout << "vin=0 out prev=" << low_prev << " last=" << low_last << "\n";
    assert(low_prev == 0);
    assert(low_last == 0);

    auto [high_prev, high_last] = settle_step(5.0);
    std::cout << "vin=5 out prev=" << high_prev << " last=" << high_last << "\n";
    assert(high_prev == 1);
    assert(high_last == 1);

    auto [hold_prev, hold_last] = settle_step(5.0);
    std::cout << "vin=5 hold out prev=" << hold_prev << " last=" << hold_last << "\n";
    assert(hold_prev == 1);
    assert(hold_last == 1);
}
