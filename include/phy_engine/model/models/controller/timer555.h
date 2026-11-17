#pragma once
#include <fast_io/fast_io_dsal/string_view.h>
#include "../../model_refs/base.h"

namespace phy_engine::model
{
    // Conductance stamp helper that treats `node_index == SIZE_MAX` as the ground reference.
    inline constexpr void timer555_stamp_conductance(::phy_engine::MNA::MNA& mna,
                                                     ::phy_engine::model::node_t const* node_a,
                                                     ::phy_engine::model::node_t const* node_b,
                                                     double g) noexcept
    {
        if(node_a == nullptr || node_b == nullptr) { return; }
        auto const na{node_a->node_index};
        auto const nb{node_b->node_index};
        if(na == SIZE_MAX && nb == SIZE_MAX) { return; }
        if(na == SIZE_MAX)
        {
            mna.G_ref(nb, nb) += g;
        }
        else if(nb == SIZE_MAX)
        {
            mna.G_ref(na, na) += g;
        }
        else
        {
            mna.G_ref(na, na) += g;
            mna.G_ref(na, nb) -= g;
            mna.G_ref(nb, na) -= g;
            mna.G_ref(nb, nb) += g;
        }
    }

    // Unity stamps every TimerChip lead to a real node: an unconnected lead becomes its own
    // node tied to node 0 through 1e8 ohm (Circuit::Analyze floating-node pass), so it reads
    // ~0 V. An unconnected reference pin is therefore equivalent to the ground reference.
    inline constexpr void timer555_stamp_output(::phy_engine::MNA::MNA& mna,
                                                ::phy_engine::model::node_t const* node_out,
                                                ::phy_engine::model::node_t const* node_ref,
                                                double g) noexcept
    {
        if(node_out == nullptr) { return; }
        if(node_ref == nullptr)
        {
            mna.G_ref(node_out->node_index, node_out->node_index) += g;
            return;
        }
        timer555_stamp_conductance(mna, node_out, node_ref, g);
    }

    struct timer555
    {
        inline static constexpr ::fast_io::u8string_view model_name{u8"timer555"};

        // Output state depends on the previously solved pin voltages, so it is non-linear.
        inline static constexpr ::phy_engine::model::model_device_type device_type{::phy_engine::model::model_device_type::non_linear};
        inline static constexpr ::fast_io::u8string_view identification_name{u8"TIMER555"};

        // Non-standard DIP order (Unity TimerChip):
        // 0=V+, 1=Dis(charge), 2=Thr(eshold), 3=Ctrl, 4=Trig(ger), 5=Out, 6=Reset, 7=Ground.
        ::phy_engine::model::pin pins[8]{{{u8"V+"}},
                                         {{u8"Dis"}},
                                         {{u8"Thr"}},
                                         {{u8"Ctrl"}},
                                         {{u8"Trig"}},
                                         {{u8"Out"}},
                                         {{u8"Reset"}},
                                         {{u8"Ground"}}};

        // Only the output levels are exposed; the simulation thresholds are hard-coded.
        double Hl{3.0};
        double Ll{0.0};
        bool out{};

        // Internal 2/3 Vcc reference node, used only when the Ctrl pin is left unwired.
        ::phy_engine::model::node_t ctrl_internal{};
    };

    static_assert(::phy_engine::model::model<timer555>);

    inline constexpr bool
        set_attribute_define(::phy_engine::model::model_reserve_type_t<timer555>, timer555& t, ::std::size_t idx, ::phy_engine::model::variant vi) noexcept
    {
        switch(idx)
        {
            case 0:
                if(vi.type != ::phy_engine::model::variant_type::d) [[unlikely]] { return false; }
                t.Hl = vi.d;
                return true;
            case 1:
                if(vi.type != ::phy_engine::model::variant_type::d) [[unlikely]] { return false; }
                t.Ll = vi.d;
                return true;
            default: return false;
        }
        return false;
    }

    static_assert(::phy_engine::model::defines::has_set_attribute<timer555>);

    inline constexpr ::phy_engine::model::variant get_attribute_define(::phy_engine::model::model_reserve_type_t<timer555>,
                                                                       timer555 const& t,
                                                                       ::std::size_t idx) noexcept
    {
        switch(idx)
        {
            case 0: return {.d{t.Hl}, .type{::phy_engine::model::variant_type::d}};
            case 1: return {.d{t.Ll}, .type{::phy_engine::model::variant_type::d}};
            default: return {};
        }
        return {};
    }

    static_assert(::phy_engine::model::defines::has_get_attribute<timer555>);

    inline constexpr ::fast_io::u8string_view get_attribute_name_define(::phy_engine::model::model_reserve_type_t<timer555>, ::std::size_t idx) noexcept
    {
        switch(idx)
        {
            case 0: return u8"Hl";
            case 1: return u8"Ll";
            default: return {};
        }
        return {};
    }

    static_assert(::phy_engine::model::defines::has_get_attribute_name<timer555>);

    inline constexpr bool iterate_dc_define(::phy_engine::model::model_reserve_type_t<timer555>, timer555& t, ::phy_engine::MNA::MNA& mna) noexcept
    {
        auto const node_vp{t.pins[0].nodes};
        auto const node_dis{t.pins[1].nodes};
        auto const node_thr{t.pins[2].nodes};
        auto const node_ctrl{t.pins[3].nodes != nullptr ? t.pins[3].nodes : __builtin_addressof(t.ctrl_internal)};
        auto const node_trig{t.pins[4].nodes};
        auto const node_out{t.pins[5].nodes};
        auto const node_reset{t.pins[6].nodes};
        auto const node_ground{t.pins[7].nodes};

        auto const voltage{[](::phy_engine::model::node_t const* n) noexcept {
            return n != nullptr ? n->node_information.an.voltage.real() : 0.0;
        }};

        double const v_vp{voltage(node_vp)};
        double const v_thr{voltage(node_thr)};
        double const v_ctrl{voltage(node_ctrl)};
        double const v_trig{voltage(node_trig)};
        double const v_reset{voltage(node_reset)};
        double const v_ground{voltage(node_ground)};
        double const v_out{voltage(node_out)};

        // Unity TimerChip.BeginStep. An unconnected Reset lead reads ~0 V (see the
        // 1e8-to-ground note above), so a floating Reset actively forces Out low.
        t.out = v_out > v_vp * 0.5;
        if(v_thr > v_ctrl) { t.out = false; }
        if(v_ctrl * 0.5 > v_trig) { t.out = true; }
        if(v_reset < 0.7 + v_ground) { t.out = false; }

        // Unity TimerChip.Stamp: V+ --5k-- Ctrl --10k-- node 0 (the global ground, not pin 7).
        timer555_stamp_conductance(mna, node_vp, node_ctrl, 1.0 / 5000.0);
        if(node_ctrl != nullptr) { mna.G_ref(node_ctrl->node_index, node_ctrl->node_index) += 1.0 / 10000.0; }

        // Discharge switch: Dis --10 ohm-- node 0 while the output is low.
        if(!t.out && node_dis != nullptr) { mna.G_ref(node_dis->node_index, node_dis->node_index) += 1.0 / 10.0; }

        // Output driver: Out --1 ohm-- V+ (high) or the Ground pin (low).
        if(t.out) { timer555_stamp_output(mna, node_out, node_vp, 1.0); }
        else { timer555_stamp_output(mna, node_out, node_ground, 1.0); }
        return true;
    }

    static_assert(::phy_engine::model::defines::can_iterate_dc<timer555>);

    inline constexpr ::phy_engine::model::pin_view generate_pin_view_define(::phy_engine::model::model_reserve_type_t<timer555>, timer555& t) noexcept
    {
        return {t.pins, 8};
    }

    static_assert(::phy_engine::model::defines::can_generate_pin_view<timer555>);

    inline constexpr ::phy_engine::model::node_view generate_internal_node_define(::phy_engine::model::model_reserve_type_t<timer555>, timer555& t) noexcept
    {
        // Unity always gives Ctrl a real node fed by the internal divider. Allocate the
        // internal node only when the pin is unconnected; the 10 kohm leg to node 0 keeps it
        // well-defined, so no floating unknown is introduced.
        if(t.pins[3].nodes == nullptr) { return {__builtin_addressof(t.ctrl_internal), 1}; }
        return {};
    }

    static_assert(::phy_engine::model::defines::can_generate_internal_node_view<timer555>);
}  // namespace phy_engine::model
