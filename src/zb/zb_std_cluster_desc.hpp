#ifndef ZB_STD_CLUSTER_DESC_HPP_
#define ZB_STD_CLUSTER_DESC_HPP_

#include "zb_main.hpp"
#include "zb_str.hpp"

extern "C"
{
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
}

namespace zb
{

struct zb_zcl_basic_min_t
{
    enum PowerSource: zb_uint8_t
    {
        Unknown               = 0x00,
        Mains1Phase           = 0x01,
        Mains3Phase           = 0x02,
        Battery               = 0x03,
        DC                    = 0x04,
        EmergencyConst        = 0x05,
        EmergencySwitch       = 0x06,
        BackupUnknown         = 0x80,
        BackupMains1Phase     = 0x81,
        BackupMains3Phase     = 0x82,
        BackupBattery         = 0x83,
        BackupDC              = 0x84,
        BackupEmergencyConst  = 0x85,
        BackupEmergencySwitch = 0x86,
    };
    zb_uint8_t zcl_version;
    PowerSource power_source;
} ;

template<> constexpr auto get_cluster_description<zb_zcl_basic_min_t>()
{
	using T = zb_zcl_basic_min_t;
	return cluster_struct_desc_t<
		0,
		cluster_mem_desc_t{.m = &T::zcl_version , .id = ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID },
		cluster_mem_desc_t{.m = &T::power_source, .id = ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID}
	>{};
}

struct zb_zcl_basic_names_t: zb_zcl_basic_min_t
{
    ZigbeeStr<33> manufacturer;
    ZigbeeStr<33> model;
};

template<> constexpr auto get_cluster_description<zb_zcl_basic_names_t>()
{
	using T = zb_zcl_basic_names_t;
	return get_cluster_description<zb_zcl_basic_min_t>() + 
        cluster_struct_desc_t<
		0,
		cluster_mem_desc_t{.m = &T::manufacturer, .id = ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID},
		cluster_mem_desc_t{.m = &T::model       , .id = ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID }
	>{};
}

template<> constexpr auto get_cluster_description<zb_zcl_basic_attrs_ext_t>()
{
	using T = zb_zcl_basic_attrs_ext_t;
	return cluster_struct_desc_t<
		0,
		cluster_mem_desc_t{.m = &T::zcl_version,   .id = ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID},
		cluster_mem_desc_t{.m = &T::app_version,   .id = ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID},
		cluster_mem_desc_t{.m = &T::stack_version, .id = ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID},
		cluster_mem_desc_t{.m = &T::hw_version,    .id = ZB_ZCL_ATTR_BASIC_HW_VERSION_ID},
		cluster_mem_desc_t{.m = &T::mf_name,       .id = ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,    .a = Access::Read, .type=Type::CharStr},
		cluster_mem_desc_t{.m = &T::model_id,      .id = ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,     .a = Access::Read, .type=Type::CharStr},
		cluster_mem_desc_t{.m = &T::date_code,     .id = ZB_ZCL_ATTR_BASIC_DATE_CODE_ID,            .a = Access::Read, .type=Type::CharStr},
		cluster_mem_desc_t{.m = &T::power_source,  .id = ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID,         .a = Access::Read, .type=Type::E8},
		cluster_mem_desc_t{.m = &T::location_id,   .id = ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID, .a = Access::RW,   .type=Type::CharStr},
		cluster_mem_desc_t{.m = &T::ph_env,        .id = ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID, .a = Access::RW,   .type=Type::E8},
		cluster_mem_desc_t{.m = &T::sw_ver,        .id = ZB_ZCL_ATTR_BASIC_SW_BUILD_ID,             .a = Access::Read, .type=Type::CharStr}
	>{};
}

template<> constexpr auto get_cluster_description<zb_zcl_identify_attrs_t>()
{
	using T = zb_zcl_identify_attrs_t;
	return cluster_struct_desc_t<
		0,
		cluster_mem_desc_t{.m = &T::identify_time, .id = ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, .a = Access::RW}
	>{};
}

template<> constexpr auto get_cluster_description<zb_zcl_on_off_attrs_t>()
{
	using T = zb_zcl_on_off_attrs_t;
	return cluster_struct_desc_t<
		0,
		cluster_mem_desc_t{.m = &T::on_off, .id = ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, .a = Access::RPS, .type=Type::Bool}
	>{};
}

}

#endif
