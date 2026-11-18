#pragma once
#include <fast_io/fast_io_dsal/string_view.h>
#include "../../model_refs/base.h"

namespace phy_engine::model
{
    struct spdt_switch
    {
        inline static constexpr ::fast_io::u8string_view model_name{u8"spdt_switch"};

        inline static constexpr ::phy_engine::model::model_device_type device_type{::phy_engine::model::model_device_type::linear};
        inline static constexpr ::fast_io::u8string_view identification_name{u8"spdt_switch"};

        // true: COM-NO closed, COM-NC open; false: COM-NC closed, COM-NO open
        bool cut_through{};
        ::phy_engine::model::pin pins[3]{{{u8"COM"}}, {{u8"NO"}}, {{u8"NC"}}};
        ::phy_engine::model::branch branches[2]{};  // kNO for COM-NO path, kNC for COM-NC path
    };

    static_assert(::phy_engine::model::model<spdt_switch>);

    inline constexpr bool set_attribute_define(::phy_engine::model::model_reserve_type_t<spdt_switch>,
                                               spdt_switch& spdt,
                                               ::std::size_t n,
                                               ::phy_engine::model::variant vi) noexcept
    {
        switch(n)
        {
            case 0:
            {
                // Set spdt_switch value
                if(vi.type != ::phy_engine::model::variant_type::boolean) [[unlikely]] { return false; }
                spdt.cut_through = vi.boolean;
                return true;
            }
            default:
            {
                return false;
            }
        }
        return false;
    }

    static_assert(::phy_engine::model::defines::has_set_attribute<spdt_switch>);

    inline constexpr ::phy_engine::model::variant
        get_attribute_define(::phy_engine::model::model_reserve_type_t<spdt_switch>, spdt_switch const& spdt, ::std::size_t n) noexcept
    {
        switch(n)
        {
            case 0:
            {
                // Get spdt_switch value
                return {.boolean = spdt.cut_through, .type = ::phy_engine::model::variant_type::boolean};
            }
            default:
            {
                return {};
            }
        }
        return {};
    }

    static_assert(::phy_engine::model::defines::has_get_attribute<spdt_switch>);

    inline constexpr ::fast_io::u8string_view get_attribute_name_define(::phy_engine::model::model_reserve_type_t<spdt_switch>, ::std::size_t n) noexcept
    {
        switch(n)
        {
            case 0:
            {
                // spdt_switch
                return {u8"Cut Through"};
            }
            default:
            {
                return {};
            }
        }
        return {};
    }

    static_assert(::phy_engine::model::defines::has_get_attribute_name<spdt_switch>);

    inline constexpr bool
        iterate_dc_define(::phy_engine::model::model_reserve_type_t<spdt_switch>, spdt_switch const& spdt, ::phy_engine::MNA::MNA& mna) noexcept
    {
        auto const node_com{spdt.pins[0].nodes};
        auto const node_no{spdt.pins[1].nodes};
        auto const node_nc{spdt.pins[2].nodes};
        if(node_com && node_no && node_nc) [[likely]]
        {
            // Model as a variable resistance to avoid singular matrices when open.
            double const r_closed{0.0};
            double const r_no{spdt.cut_through ? r_closed : mna.r_open};
            double const r_nc{spdt.cut_through ? mna.r_open : r_closed};

            auto const k_no{spdt.branches[0].index};
            mna.B_ref(node_com->node_index, k_no) = 1.0;
            mna.B_ref(node_no->node_index, k_no) = -1.0;
            mna.C_ref(k_no, node_com->node_index) = 1.0;
            mna.C_ref(k_no, node_no->node_index) = -1.0;
            mna.D_ref(k_no, k_no) = -r_no;

            auto const k_nc{spdt.branches[1].index};
            mna.B_ref(node_com->node_index, k_nc) = 1.0;
            mna.B_ref(node_nc->node_index, k_nc) = -1.0;
            mna.C_ref(k_nc, node_com->node_index) = 1.0;
            mna.C_ref(k_nc, node_nc->node_index) = -1.0;
            mna.D_ref(k_nc, k_nc) = -r_nc;
        }
        return true;
    }

    static_assert(::phy_engine::model::defines::can_iterate_dc<spdt_switch>);

    inline constexpr ::phy_engine::model::pin_view generate_pin_view_define(::phy_engine::model::model_reserve_type_t<spdt_switch>,
                                                                            spdt_switch& spdt) noexcept
    {
        return {spdt.pins, 3};
    }

    static_assert(::phy_engine::model::defines::can_generate_pin_view<spdt_switch>);

    inline constexpr ::phy_engine::model::branch_view generate_branch_view_define(::phy_engine::model::model_reserve_type_t<spdt_switch>,
                                                                                  spdt_switch& spdt) noexcept
    {
        return {spdt.branches, 2};
    }

    static_assert(::phy_engine::model::defines::can_generate_branch_view<spdt_switch>);

}  // namespace phy_engine::model
