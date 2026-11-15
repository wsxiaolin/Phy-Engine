#pragma once
#include <fast_io/fast_io_dsal/string_view.h>
#include "../../model_refs/base.h"

namespace phy_engine::model
{
    struct dpdt_switch
    {
        inline static constexpr ::fast_io::u8string_view model_name{u8"dpdt_switch"};

        inline static constexpr ::phy_engine::model::model_device_type device_type{::phy_engine::model::model_device_type::linear};
        inline static constexpr ::fast_io::u8string_view identification_name{u8"dpdt_switch"};

        // true: COM-NO closed, COM-NC open; false: COM-NC closed, COM-NO open. Shared by both ganged poles.
        bool cut_through{};
        ::phy_engine::model::pin pins[6]{{{u8"COM1"}}, {{u8"NO1"}}, {{u8"NC1"}}, {{u8"COM2"}}, {{u8"NO2"}}, {{u8"NC2"}}};
        ::phy_engine::model::branch branches[4]{};  // pole1: kNO1 for COM1-NO1, kNC1 for COM1-NC1; pole2: kNO2 for COM2-NO2, kNC2 for COM2-NC2
    };

    static_assert(::phy_engine::model::model<dpdt_switch>);

    inline constexpr bool set_attribute_define(::phy_engine::model::model_reserve_type_t<dpdt_switch>,
                                               dpdt_switch& dpdt,
                                               ::std::size_t n,
                                               ::phy_engine::model::variant vi) noexcept
    {
        switch(n)
        {
            case 0:
            {
                // Set dpdt_switch value
                if(vi.type != ::phy_engine::model::variant_type::boolean) [[unlikely]] { return false; }
                dpdt.cut_through = vi.boolean;
                return true;
            }
            default:
            {
                return false;
            }
        }
        return false;
    }

    static_assert(::phy_engine::model::defines::has_set_attribute<dpdt_switch>);

    inline constexpr ::phy_engine::model::variant
        get_attribute_define(::phy_engine::model::model_reserve_type_t<dpdt_switch>, dpdt_switch const& dpdt, ::std::size_t n) noexcept
    {
        switch(n)
        {
            case 0:
            {
                // Get dpdt_switch value
                return {.boolean = dpdt.cut_through, .type = ::phy_engine::model::variant_type::boolean};
            }
            default:
            {
                return {};
            }
        }
        return {};
    }

    static_assert(::phy_engine::model::defines::has_get_attribute<dpdt_switch>);

    inline constexpr ::fast_io::u8string_view get_attribute_name_define(::phy_engine::model::model_reserve_type_t<dpdt_switch>, ::std::size_t n) noexcept
    {
        switch(n)
        {
            case 0:
            {
                // dpdt_switch
                return {u8"Cut Through"};
            }
            default:
            {
                return {};
            }
        }
        return {};
    }

    static_assert(::phy_engine::model::defines::has_get_attribute_name<dpdt_switch>);

    inline constexpr bool
        iterate_dc_define(::phy_engine::model::model_reserve_type_t<dpdt_switch>, dpdt_switch const& dpdt, ::phy_engine::MNA::MNA& mna) noexcept
    {
        auto const node_com1{dpdt.pins[0].nodes};
        auto const node_no1{dpdt.pins[1].nodes};
        auto const node_nc1{dpdt.pins[2].nodes};
        auto const node_com2{dpdt.pins[3].nodes};
        auto const node_no2{dpdt.pins[4].nodes};
        auto const node_nc2{dpdt.pins[5].nodes};
        if(node_com1 && node_no1 && node_nc1 && node_com2 && node_no2 && node_nc2) [[likely]]
        {
            // Model as a variable resistance to avoid singular matrices when open.
            double const r_closed{0.0};
            double const r_no1{dpdt.cut_through ? r_closed : mna.r_open};
            double const r_nc1{dpdt.cut_through ? mna.r_open : r_closed};
            double const r_no2{dpdt.cut_through ? r_closed : mna.r_open};
            double const r_nc2{dpdt.cut_through ? mna.r_open : r_closed};

            auto const k_no1{dpdt.branches[0].index};
            mna.B_ref(node_com1->node_index, k_no1) = 1.0;
            mna.B_ref(node_no1->node_index, k_no1) = -1.0;
            mna.C_ref(k_no1, node_com1->node_index) = 1.0;
            mna.C_ref(k_no1, node_no1->node_index) = -1.0;
            mna.D_ref(k_no1, k_no1) = -r_no1;

            auto const k_nc1{dpdt.branches[1].index};
            mna.B_ref(node_com1->node_index, k_nc1) = 1.0;
            mna.B_ref(node_nc1->node_index, k_nc1) = -1.0;
            mna.C_ref(k_nc1, node_com1->node_index) = 1.0;
            mna.C_ref(k_nc1, node_nc1->node_index) = -1.0;
            mna.D_ref(k_nc1, k_nc1) = -r_nc1;

            auto const k_no2{dpdt.branches[2].index};
            mna.B_ref(node_com2->node_index, k_no2) = 1.0;
            mna.B_ref(node_no2->node_index, k_no2) = -1.0;
            mna.C_ref(k_no2, node_com2->node_index) = 1.0;
            mna.C_ref(k_no2, node_no2->node_index) = -1.0;
            mna.D_ref(k_no2, k_no2) = -r_no2;

            auto const k_nc2{dpdt.branches[3].index};
            mna.B_ref(node_com2->node_index, k_nc2) = 1.0;
            mna.B_ref(node_nc2->node_index, k_nc2) = -1.0;
            mna.C_ref(k_nc2, node_com2->node_index) = 1.0;
            mna.C_ref(k_nc2, node_nc2->node_index) = -1.0;
            mna.D_ref(k_nc2, k_nc2) = -r_nc2;
        }
        return true;
    }

    static_assert(::phy_engine::model::defines::can_iterate_dc<dpdt_switch>);

    inline constexpr ::phy_engine::model::pin_view generate_pin_view_define(::phy_engine::model::model_reserve_type_t<dpdt_switch>,
                                                                            dpdt_switch& dpdt) noexcept
    {
        return {dpdt.pins, 6};
    }

    static_assert(::phy_engine::model::defines::can_generate_pin_view<dpdt_switch>);

    inline constexpr ::phy_engine::model::branch_view generate_branch_view_define(::phy_engine::model::model_reserve_type_t<dpdt_switch>,
                                                                                   dpdt_switch& dpdt) noexcept
    {
        return {dpdt.branches, 4};
    }

    static_assert(::phy_engine::model::defines::can_generate_branch_view<dpdt_switch>);

}  // namespace phy_engine::model
