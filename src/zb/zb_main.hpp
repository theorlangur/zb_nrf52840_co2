#ifndef ZB_MAIN_HPP_
#define ZB_MAIN_HPP_

extern "C"
{
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
}

#include <type_traits>

namespace zb
{
    enum class Access: zb_uint8_t
    {
        None = 0,
        Read = 0x01,
        Write = 0x02,
        Report = 0x04,
        Singleton = 0x08,
        Scene = 0x10,
        ManuSpec = 0x20,
        Internal = 0x40,

        RW = Read | Write,
        RWP = RW | Report,
        RP = Read | Report,
        RPS = RP | Report,
    };

    constexpr Access operator|(Access a1, Access a2) { return Access(zb_uint8_t(a1) | zb_uint8_t(a2)); }

    template<class T>
    constexpr uint8_t TypeToTypeId()
    {
        if constexpr (std::is_same_v<T,zb_uint8_t>)                return ZB_ZCL_ATTR_TYPE_U8;
        else if constexpr (std::is_same_v<T,zb_uint16_t>)          return ZB_ZCL_ATTR_TYPE_U16;
        else if constexpr (std::is_same_v<T,zb_uint32_t>)          return ZB_ZCL_ATTR_TYPE_U32;
        else if constexpr (std::is_same_v<T,zb_uint64_t>)          return ZB_ZCL_ATTR_TYPE_U64;
        else if constexpr (std::is_same_v<T,zb_int8_t>)            return ZB_ZCL_ATTR_TYPE_S8;
        else if constexpr (std::is_same_v<T,zb_int16_t>)           return ZB_ZCL_ATTR_TYPE_S16;
        else if constexpr (std::is_same_v<T,zb_int32_t>)           return ZB_ZCL_ATTR_TYPE_S32;
        else if constexpr (std::is_same_v<T,zb_int64_t>)           return ZB_ZCL_ATTR_TYPE_S64;
        else if constexpr (std::is_same_v<T,std::nullptr_t>)       return ZB_ZCL_ATTR_TYPE_NULL;
        else if constexpr (std::is_enum_v<T> && sizeof(T) == 1)    return ZB_ZCL_ATTR_TYPE_8BIT_ENUM;
        else if constexpr (std::is_enum_v<T> && sizeof(T) == 2)    return ZB_ZCL_ATTR_TYPE_16BIT_ENUM;
        else if constexpr (std::is_same_v<T,float>)                return ZB_ZCL_ATTR_TYPE_SINGLE;
        else if constexpr (std::is_same_v<T,double>)               return ZB_ZCL_ATTR_TYPE_DOUBLE;
        else if constexpr (requires { (zb_uint8_t)T::TypeId(); })  return T::TypeId();
        else 
            static_assert(sizeof(T) == 0, "Unknown type");
        return ZB_ZCL_ATTR_TYPE_NULL;
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
        zb_uint8_t type = TypeToTypeId<T>();
    };

    template<class T>
    constexpr zb_zcl_attr_t AttrDesc(ADesc<T> d)
    {
        return {
            .id = d.id, 
            .type = d.type, 
            .access = (uint8_t)d.a, 
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
        zb_uint8_t type = TypeToTypeId<MemType>();
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
