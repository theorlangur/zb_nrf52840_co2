#ifndef ZB_DESC_HELPER_TYPES_CLUSTER_HPP_
#define ZB_DESC_HELPER_TYPES_CLUSTER_HPP_

#include "zb_desc_helper_types_attr.hpp"

namespace zb
{
    template<class T, class MemType>
    using mem_attr_t = MemType T::*;

    template<class T, class MemType>
    struct cluster_mem_desc_t
    {
        using MemT = MemType;

        mem_attr_t<T, MemType> m;
        zb_uint16_t id;
        Access a = Access::Read;
        Type type = TypeToTypeId<MemType>();

        constexpr inline bool has_access(Access _a) const { return a & _a; } 
    };

    template<auto memPtr, auto...ClusterMemDescriptions>
    struct find_cluster_mem_desc_t;

    template<auto memPtr, auto MemDesc, auto...ClusterMemDescriptions> requires (MemDesc.m == memPtr)
    struct find_cluster_mem_desc_t<memPtr, MemDesc, ClusterMemDescriptions...>
    {
        static constexpr auto mem_desc() { return MemDesc; }
    };

    template<auto memPtr, auto MemDesc, auto...ClusterMemDescriptions> requires (MemDesc.m != memPtr)
    struct find_cluster_mem_desc_t<memPtr, MemDesc, ClusterMemDescriptions...>: find_cluster_mem_desc_t<memPtr, ClusterMemDescriptions...>
    {
    };

    template<auto memPtr>
    struct find_cluster_mem_desc_t<memPtr>
    {
        static constexpr auto mem_desc() { static_assert(sizeof(memPtr) == 0, "Pointer to a member is not an attribute"); }
    };

    struct cluster_info_t
    {
        zb_uint16_t id;
        zb_uint16_t rev = 0;
        Role        role = Role::Server;
        zb_uint16_t manuf_code = ZB_ZCL_MANUF_CODE_INVALID;

        constexpr bool operator==(cluster_info_t const&) const = default;
    };

    template<cluster_info_t ci, auto... ClusterMemDescriptions>
    struct cluster_struct_desc_t
    {
        static constexpr inline zb_uint16_t rev() { return ci.rev; }
        static constexpr inline auto info() { return ci; }
        static constexpr inline size_t count_members_with_access(Access a) { return ((size_t)ClusterMemDescriptions.has_access(a) + ... + 0); }

        template<auto memPtr>
        static constexpr inline auto get_member_description() { return find_cluster_mem_desc_t<memPtr, ClusterMemDescriptions...>::mem_desc(); }

        template<cluster_info_t ci2, auto... ClusterMemDescriptions2>
        friend constexpr auto operator+(cluster_struct_desc_t<ci, ClusterMemDescriptions...> lhs, cluster_struct_desc_t<ci2, ClusterMemDescriptions2...> rhs)
        {
            static_assert(ci == ci2, "Must be the same revision");
            return cluster_struct_desc_t<
                ci,
                ClusterMemDescriptions...,
                ClusterMemDescriptions2...
                >{};
        }
    };

    template<class T, class DestT, class MemType> requires std::is_base_of_v<DestT, T>
    constexpr ADesc<MemType> cluster_mem_to_attr_desc(T& s, cluster_mem_desc_t<DestT,MemType> d) { return {.id = d.id, .a = d.a, .pData = &(s.*d.m), .type = d.type}; }

    template<class T,cluster_info_t ci, auto... ClusterMemDescriptions>
    constexpr auto cluster_struct_to_attr_list(T &s, cluster_struct_desc_t<ci, ClusterMemDescriptions...>)
    {
        return MakeAttributeList(&s, cluster_mem_to_attr_desc(s, ClusterMemDescriptions)...);
    }

    template<class... T>
    struct TClusterList
    {
        static constexpr size_t N = sizeof...(T);

        TClusterList(TClusterList const&) = delete;
        TClusterList(TClusterList &&) = delete;
        void operator=(TClusterList const&) = delete;
        void operator=(TClusterList &&) = delete;

        static constexpr size_t reporting_attributes_count() { return (T::attributes_with_access(Access::Report) + ... + 0); }
        static constexpr size_t cvc_level_ctrl_attributes_count() { return ((T::info().id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL ? T::attributes_with_access(Access::Report) : 0) + ... + 0); }
        static constexpr size_t server_cluster_count() { return (T::is_role(Role::Server) + ... + 0); }
        static constexpr size_t client_cluster_count() { return (T::is_role(Role::Client) + ... + 0); }
        static constexpr bool has_info(cluster_info_t ci) { return ((T::info() == ci) || ...); }

        constexpr TClusterList(T&... d):
            clusters{ d.desc()... }
        {
        }

        zb_zcl_cluster_desc_t clusters[N];
    };
}
#endif
