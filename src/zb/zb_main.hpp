#ifndef ZB_MAIN_HPP_
#define ZB_MAIN_HPP_

#include "zb_types.hpp"
#include <type_traits>

namespace zb
{
    template<class T>
    constexpr Type TypeToTypeId()
    {
        if constexpr (std::is_same_v<T,zb_uint8_t>)                return Type::U8;
        else if constexpr (std::is_same_v<T,zb_uint16_t>)          return Type::U16;
        else if constexpr (std::is_same_v<T,zb_uint32_t>)          return Type::U32;
        else if constexpr (std::is_same_v<T,zb_uint64_t>)          return Type::U64;
        else if constexpr (std::is_same_v<T,zb_int8_t>)            return Type::S8;
        else if constexpr (std::is_same_v<T,zb_int16_t>)           return Type::S16;
        else if constexpr (std::is_same_v<T,zb_int32_t>)           return Type::S32;
        else if constexpr (std::is_same_v<T,zb_int64_t>)           return Type::S64;
        else if constexpr (std::is_same_v<T,std::nullptr_t>)       return Type::Null;
        else if constexpr (std::is_enum_v<T> && sizeof(T) == 1)    return Type::E8;
        else if constexpr (std::is_enum_v<T> && sizeof(T) == 2)    return Type::E16;
        else if constexpr (std::is_same_v<T,float>)                return Type::Float;
        else if constexpr (std::is_same_v<T,double>)               return Type::Double;
        else if constexpr (std::is_same_v<T,bool>)                 return Type::Bool;
        else if constexpr (requires { T::TypeId(); })              return T::TypeId();
        else 
            static_assert(sizeof(T) == 0, "Unknown type");
        return Type::Invalid;
    }

    inline constexpr zb_zcl_attr_t g_LastAttribute{
        .id = ZB_ZCL_NULL_ID,
        .type = ZB_ZCL_ATTR_TYPE_NULL,
        .access = (zb_uint8_t)Access::None,
        .manuf_code = ZB_ZCL_NON_MANUFACTURER_SPECIFIC,
        .data_p = nullptr
    };

    template<class T>
    struct ADesc
    {
        using simplified_tag = void;
        zb_uint16_t id;
        Access a;
        T *pData;
        Type type = TypeToTypeId<T>();
    };

    template<class T>
    constexpr zb_zcl_attr_t AttrDesc(ADesc<T> d)
    {
        return {
            .id = d.id, 
            .type = (zb_uint8_t)d.type, 
            .access = (zb_uint8_t)d.a, 
            .manuf_code = ZB_ZCL_NON_MANUFACTURER_SPECIFIC, 
            .data_p = d.pData
        };
    }

    template<zb_uint16_t id> constexpr zb_zcl_cluster_init_t get_cluster_init(Role r) { static_assert(id == 0xffff, "get_cluster_init not specialized for this cluster!"); return nullptr; }
#define DEFINE_GET_CLUSTER_INIT_FOR(cid) template<> constexpr zb_zcl_cluster_init_t get_cluster_init<cid>(Role r) { return r == Role::Server ? cid##_SERVER_ROLE_INIT : (r == Role::Client ? cid##_CLIENT_ROLE_INIT : NULL); }

    template<class Tag, size_t N>
    struct TAttributeList
    {
        TAttributeList(TAttributeList const&) = delete;
        TAttributeList(TAttributeList &&) = delete;
        void operator=(TAttributeList const&) = delete;
        void operator=(TAttributeList &&) = delete;


        template<class... T>
        constexpr TAttributeList(ADesc<T>... d):
            attributes{
                AttrDesc(zb::ADesc{ .id = ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID, .a = Access::Read, .pData = &rev }),
                (AttrDesc(d), ...)
                , g_LastAttribute
            },
            rev(Tag::rev())
        {
        }

        operator zb_zcl_attr_t*() { return attributes; }

        constexpr static bool is_role(Role r) { return Tag::info().role == r; }
        constexpr static size_t attributes_with_access(Access r) { return Tag::count_members_with_access(r); }

        constexpr zb_zcl_cluster_desc_t desc()
        {
            constexpr auto ci = Tag::info();
            return zb_zcl_cluster_desc_t{
                .cluster_id = ci.id,
                    .attr_count = N + 2,
                    .attr_desc_list = attributes,
                    .role_mask = (zb_uint8_t)ci.role,
                    .manuf_code = ci.manuf_code,
                    .cluster_init = get_cluster_init<Tag::info().id>(ci.role)
            };
        }

        zb_zcl_attr_t attributes[N + 2];
        zb_uint16_t rev;
    };

    template<class ClusterTag, class... T>
    constexpr auto MakeAttributeList(ClusterTag t, ADesc<T>... d)->TAttributeList<ClusterTag, sizeof...(T)>
    {
        return TAttributeList<ClusterTag, sizeof...(T)>(d...);
    }

    template<class T, class MemType>
    using mem_attr_t = MemType T::*;

    template<class T, class MemType>
    struct cluster_mem_desc_t
    {
        mem_attr_t<T, MemType> m;
        zb_uint16_t id;
        Access a = Access::Read;
        Type type = TypeToTypeId<MemType>();

        constexpr inline bool has_access(Access _a) const { return a & _a; } 
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
        return MakeAttributeList(cluster_struct_desc_t<ci, ClusterMemDescriptions...>{}, cluster_mem_to_attr_desc(s, ClusterMemDescriptions)...);
    }

    template<class zb_struct>
    constexpr auto get_cluster_description();

    template<class ZbS>
    constexpr auto to_attributes(ZbS &s) { return cluster_struct_to_attr_list(s, get_cluster_description<ZbS>()); }


    template<size_t N>
    struct TClusterList
    {
        TClusterList(TClusterList const&) = delete;
        TClusterList(TClusterList &&) = delete;
        void operator=(TClusterList const&) = delete;
        void operator=(TClusterList &&) = delete;


        template<class... T>
        constexpr TClusterList(T&... d):
            clusters{
                (d.desc(), ...)
            },
            reporting_attributes((d.attributes_with_access(Access::Report) + ... + 0)),
            cvc_level_ctrl_attributes(((d.desc().cluster_id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL ? d.attributes_with_access(Access::Report) : 0) + ... + 0)),
            server_cluster_count((d.is_role(Role::Server) + ... + 0)),
            client_cluster_count((d.is_role(Role::Client) + ... + 0))
        {
        }

        zb_zcl_cluster_desc_t clusters[N];
        size_t reporting_attributes;
        size_t cvc_level_ctrl_attributes;
        zb_uint8_t server_cluster_count;
        zb_uint8_t client_cluster_count;
    };

    template<class... ClusterAttributesDesc>
    constexpr auto to_clusters(ClusterAttributesDesc&... c) -> TClusterList<sizeof...(ClusterAttributesDesc)>
    {
        return {c...};
    }
}

#endif
