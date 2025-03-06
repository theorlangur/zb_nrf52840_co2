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
    enum class AttAccess: zb_uint8_t
    {
        None = 0,
        Read = 0x01,
        Write = 0x02,
        Report = 0x04,
        Singleton = 0x08,
        Scene = 0x10,
        ManuSpec = 0x20,
        Internal = 0x40,
    };

    template<class T>
    constexpr uint8_t TypeToTypeId()
    {
        if constexpr (std::is_same_v<T,zb_uint8_t>) return ZB_ZCL_ATTR_TYPE_U8;
        else if constexpr (std::is_same_v<T,zb_uint16_t>) return ZB_ZCL_ATTR_TYPE_U16;
        else if constexpr (std::is_same_v<T,zb_uint32_t>) return ZB_ZCL_ATTR_TYPE_U32;
        else if constexpr (std::is_same_v<T,zb_uint64_t>) return ZB_ZCL_ATTR_TYPE_U64;
        else if constexpr (std::is_same_v<T,zb_int8_t>)   return ZB_ZCL_ATTR_TYPE_S8;
        else if constexpr (std::is_same_v<T,zb_int16_t>)  return ZB_ZCL_ATTR_TYPE_S16;
        else if constexpr (std::is_same_v<T,zb_int32_t>)  return ZB_ZCL_ATTR_TYPE_S32;
        else if constexpr (std::is_same_v<T,zb_int64_t>)  return ZB_ZCL_ATTR_TYPE_S64;
        else if constexpr (std::is_same_v<T,std::nullptr_t>)   return ZB_ZCL_ATTR_TYPE_NULL;
        else if constexpr (requires { (zb_uint8_t)T::TypeId(); })  return T::TypeId();
        else 
            static_assert(sizeof(T) == 0, "Unknown type");
        return ZB_ZCL_ATTR_TYPE_NULL;
    }

    inline constexpr zb_zcl_attr_t g_LastAttribute{
        .id = ZB_ZCL_NULL_ID,
        .type = ZB_ZCL_ATTR_TYPE_NULL,
        .access = (zb_uint8_t)AttAccess::None,
        .manuf_code = ZB_ZCL_NON_MANUFACTURER_SPECIFIC,
        .data_p = nullptr
    };

    template<class T>
    struct AttrDescSimplified
    {
        using simplified_tag = void;
        zb_uint16_t id;
        AttAccess a;
        T *pData;
    };
    template<class C>
    concept IsAttrSimplified = requires{ typename C::simplified_tag; };

    template<class T>
    constexpr zb_zcl_attr_t AttrDesc(AttrDescSimplified<T> d)
    {
        return {
            .id = d.id, 
            .type = TypeToTypeId<T>(), 
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


        template<class... Attributes> requires (IsAttrSimplified<Attributes> && ...)
        constexpr TAttributeList(zb_uint16_t r, Attributes... d):
            attributes{
                AttrDesc(AttrDescSimplified{ .id = ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID, .a = AttAccess::Read, .pData = &rev }),
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

    template<class... Attributes> requires (IsAttrSimplified<Attributes> && ...)
    constexpr auto MakeAttributeList(zb_uint16_t r, Attributes... d)->TAttributeList<sizeof...(Attributes)>
    {
        return TAttributeList<sizeof...(Attributes)>(r, d...);
    }
}

#endif
