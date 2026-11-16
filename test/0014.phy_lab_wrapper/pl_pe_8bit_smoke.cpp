#include <phy_engine/phy_lab_wrapper/pe_sim.h>

// Bring in the C ABI implementation for this test binary.
#include "../../src/dll_main.cpp"

#include <cassert>

int main()
{
    using namespace phy_engine::phy_lab_wrapper;

    auto ex = experiment::create(experiment_type::circuit);
    ex.entitle("pl_pe_8bit_smoke");

    auto in = ex.add_circuit_element("8bit Input", position{0.0, 0.0, 0.0});
    auto disp = ex.add_circuit_element("8bit Display", position{1.0, 0.0, 0.0});

    // 0xA5 = b7..b0 = 1,0,1,0,0,1,0,1
    ex.get_element(in).data()["Properties"]["十进制"] = 165;

    // PL pin 0 = bit7 ... PL pin 7 = bit0 (MSB-first), matching PE pin order 1:1.
    for (int pin = 0; pin < 8; ++pin)
    {
        ex.connect(in, pin, disp, pin);
    }

    phy_engine::phy_lab_wrapper::pe::circuit c = phy_engine::phy_lab_wrapper::pe::circuit::build_from(ex);
    c.set_analyze_type(PHY_ENGINE_ANALYZE_TR);
    c.set_tr(1e-9, 1e-9);
    c.analyze();
    c.digital_clk();

    auto s = c.sample_now();

    // Component order matches element order: comp 0 = 8bit Input, comp 1 = 8bit Display (8 pins).
    assert(s.pin_digital_ord.size() == 3);
    auto const disp_first = s.pin_digital_ord[1];
    int const expected[8] = {1, 0, 1, 0, 0, 1, 0, 1};
    for (int pin = 0; pin < 8; ++pin)
    {
        assert(s.pin_digital[disp_first + pin] == static_cast<std::uint8_t>(expected[pin]));
    }
}
