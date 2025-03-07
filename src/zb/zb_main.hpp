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

    template<size_t N>
    struct TAttributeList
    {
        TAttributeList(TAttributeList const&) = delete;
        TAttributeList(TAttributeList &&) = delete;
        void operator=(TAttributeList const&) = delete;
        void operator=(TAttributeList &&) = delete;


        template<class... T>
        constexpr TAttributeList(zb_uint16_t r, ADesc<T>... d):
            attributes{
                AttrDesc(zb::ADesc{ .id = ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID, .a = Access::Read, .pData = &rev }),
                (AttrDesc(d), ...)
                , g_LastAttribute
            },
            rev(r)
        {
        }

        operator zb_zcl_attr_t*() { return attributes; }

        zb_zcl_attr_t attributes[N + 2];
        zb_uint16_t rev;
    };

    template<class... T>
    constexpr auto MakeAttributeList(zb_uint16_t r, ADesc<T>... d)->TAttributeList<sizeof...(T)>
    {
        return TAttributeList<sizeof...(T)>(r, d...);
    }

    template<class T, class MemType>
    using mem_attr_t = MemType T::*;

    template<class T, class MemType>
    struct cluster_mem_desc_t
    {
        mem_attr_t<T, MemType> m;
        zb_uint16_t id;
        Access a;
        Type type = TypeToTypeId<MemType>();
    };

    template<zb_uint16_t rev, auto... ClusterMemDescriptions>
    struct cluster_struct_desc_t
    {
        template<zb_uint16_t rev2, auto... ClusterMemDescriptions2>
        friend constexpr auto operator+(cluster_struct_desc_t<rev, ClusterMemDescriptions...> lhs, cluster_struct_desc_t<rev2, ClusterMemDescriptions2...> rhs)
        {
            static_assert(rev == rev2, "Must be the same revision");
            return cluster_struct_desc_t<
                rev,
                ClusterMemDescriptions...,
                ClusterMemDescriptions2...
                >{};
        }
    };

    template<class T, class DestT, class MemType> requires std::is_base_of_v<DestT, T>
    constexpr ADesc<MemType> cluster_mem_to_attr_desc(T& s, cluster_mem_desc_t<DestT,MemType> d) { return {.id = d.id, .a = d.a, .pData = &(s.*d.m), .type = d.type}; }

    template<class T,zb_uint16_t rev, auto... ClusterMemDescriptions>
    constexpr auto cluster_struct_to_attr_list(T &s, cluster_struct_desc_t<rev, ClusterMemDescriptions...>)
    {
        return MakeAttributeList(rev, cluster_mem_to_attr_desc(s, ClusterMemDescriptions)...);
    }

    template<class zb_struct>
    constexpr auto get_cluster_description();

    template<class ZbS>
    constexpr auto get_attributes_from_cluster_struct(ZbS &s) { return cluster_struct_to_attr_list(s, get_cluster_description<ZbS>()); }
}

#endif
