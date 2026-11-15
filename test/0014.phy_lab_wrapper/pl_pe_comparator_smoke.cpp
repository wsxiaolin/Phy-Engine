#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
#include "../../src/dll_main.cpp"

#include <cassert>
#include <cstddef>
#include <iostream>

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_comparator_smoke");

    auto va = ex.add_circuit_element("Student Source", position{-1.0, 0.0, 0.0});
    auto vb = ex.add_circuit_element("Student Source", position{1.0, 0.0, 0.0});
    auto cmp = ex.add_circuit_element("Comparator", position{0.0, 0.0, 0.0});
    auto y = ex.add_circuit_element("Logic Output", position{0.0, 0.0, 1.0});
    auto gnd = ex.add_circuit_element("Ground Component", position{0.0, 0.0, 2.0});

    ex.get_element(va).data()["Properties"]["电压"] = 5.0;
    ex.get_element(vb).data()["Properties"]["电压"] = 0.0;

    // Student Source pins: 0=+, 1=-. Comparator pins: 0=A, 1=B, 2=o. Logic Output pin: i=0.
    ex.connect(va, 0, cmp, 0);
    ex.connect(va, 1, gnd, 0);
    ex.connect(vb, 0, cmp, 1);
    ex.connect(vb, 1, gnd, 0);
    ex.connect(cmp, 2, y, 0);

    // Non-ground PL elements keep insertion order in the PE component list.
    std::size_t cmp_index = ex.elements().size();
    for (std::size_t i = 0; i < ex.elements().size(); ++i)
    {
        if (ex.elements()[i].identifier() == cmp) { cmp_index = i; }
    }
    assert(cmp_index < ex.elements().size());

    auto settle_step = [&](int vav, int vbv) -> int {
        ex.get_element(va).data()["Properties"]["电压"] = vav;
        ex.get_element(vb).data()["Properties"]["电压"] = vbv;
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
            out = s.pin_digital[s.pin_digital_ord[cmp_index] + 2] != 0 ? 1 : 0;
            c.write_back_to_pl(ex, s);
        }
        assert(prev == out);
        auto const& out_props = ex.get_element(y).data()["Properties"];
        assert(out_props.value("状态", -1.0) == static_cast<double>(out));
        return out;
    };

    int out_high = settle_step(5, 0);
    std::cout << "vA=5 vB=0 out=" << out_high << "\n";
    assert(out_high == 1);

    int out_low = settle_step(0, 5);
    std::cout << "vA=0 vB=5 out=" << out_low << "\n";
    assert(out_low == 0);
}
