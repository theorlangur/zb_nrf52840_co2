/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *
 * @brief Simple Zigbee light bulb implementation.
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/settings/settings.h>
#include <zephyr/drivers/gpio.h>

#include "zb/zb_main.hpp"
#include "zb/zb_std_cluster_desc.hpp"
#include "zb_dimmable_light.h"

#define RUN_STATUS_LED                  DK_LED1
#define RUN_LED_BLINK_INTERVAL          1000

/* Device endpoint, used to receive light controlling commands. */
#define DIMMABLE_LIGHT_ENDPOINT         10

/* Version of the application software (1 byte). */
#define BULB_INIT_BASIC_APP_VERSION     01

/* Version of the implementation of the Zigbee stack (1 byte). */
#define BULB_INIT_BASIC_STACK_VERSION   10

/* Version of the hardware of the device (1 byte). */
#define BULB_INIT_BASIC_HW_VERSION      11

/* Manufacturer name (32 bytes). */
#define BULB_INIT_BASIC_MANUF_NAME      "Nordic"

/* Model number assigned by manufacturer (32-bytes long string). */
#define BULB_INIT_BASIC_MODEL_ID        "Dimable_Light_v0.1"

/* First 8 bytes specify the date of manufacturer of the device
 * in ISO 8601 format (YYYYMMDD). The rest (8 bytes) are manufacturer specific.
 */
#define BULB_INIT_BASIC_DATE_CODE       "20200329"

/* Type of power sources available for the device.
 * For possible values see section 3.2.2.2.8 of ZCL specification.
 */
#define BULB_INIT_BASIC_POWER_SOURCE    ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE

/* Describes the physical location of the device (16 bytes).
 * May be modified during commissioning process.
 */
#define BULB_INIT_BASIC_LOCATION_DESC   "Office desk"

/* Describes the type of physical environment.
 * For possible values see section 3.2.2.2.10 of ZCL specification.
 */
#define BULB_INIT_BASIC_PH_ENV          ZB_ZCL_BASIC_ENV_UNSPECIFIED

/* LED indicating that light switch successfully joind Zigbee network. */
#define ZIGBEE_NETWORK_STATE_LED        DK_LED3

/* LED immitaing dimmable light bulb - define for informational
 * purposes only.
 */
#define BULB_LED                        DK_LED4

/* Button used to enter the Bulb into the Identify mode. */
#define IDENTIFY_MODE_BUTTON            DK_BTN4_MSK

/* Use onboard led4 to act as a light bulb.
 * The app.overlay file has this at node label "pwm_led3" in /pwmleds.
 */
#define PWM_DK_LED4_NODE                DT_ALIAS(pwm_led0)

#if DT_NODE_HAS_STATUS(PWM_DK_LED4_NODE, okay)
static const struct pwm_dt_spec led_pwm = PWM_DT_SPEC_GET(PWM_DK_LED4_NODE);
#else
#error "Choose supported PWM driver"
#endif

#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
//static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Led PWM period, calculated for 100 Hz signal - in microseconds. */
#define LED_PWM_PERIOD_US               (USEC_PER_SEC / 100U)

#ifndef ZB_ROUTER_ROLE
#error Define ZB_ROUTER_ROLE to compile router source code.
#endif

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON IDENTIFY_MODE_BUTTON

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* Main application customizable context.
 * Stores all settings and static values.
 */
typedef struct {
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_scenes_attrs_t scenes_attr;
	zb_zcl_groups_attrs_t groups_attr;
	zb_zcl_on_off_attrs_t on_off_attr;
	zb_zcl_level_control_attrs_t level_control_attr;
} bulb_device_ctx_t;

/* Zigbee device application context storage. */
static bulb_device_ctx_t dev_ctx;

constinit static auto dimmable_light_ctx = zb::make_device( 
		zb::make_ep_args<{.ep=DIMMABLE_LIGHT_ENDPOINT, .dev_id=ZB_DIMMABLE_LIGHT_DEVICE_ID, .dev_ver=ZB_DEVICE_VER_DIMMABLE_LIGHT}>(
		dev_ctx.identify_attr
		, dev_ctx.on_off_attr
		, dev_ctx.basic_attr
		, dev_ctx.groups_attr
		, dev_ctx.scenes_attr
		, dev_ctx.level_control_attr
		)
);
constinit static auto &dim_ep = dimmable_light_ctx.ep<DIMMABLE_LIGHT_ENDPOINT>();

/**@brief Starts identifying the device.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void start_identifying(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	if (ZB_JOINED()) {
		/* Check if endpoint is in identifying mode,
		 * if not, put desired endpoint in identifying mode.
		 */
		if (dev_ctx.identify_attr.identify_time ==
		    ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE) {

			zb_ret_t zb_err_code = zb_bdb_finding_binding_target(
				DIMMABLE_LIGHT_ENDPOINT);

			if (zb_err_code == RET_OK) {
				LOG_INF("Enter identify mode");
			} else if (zb_err_code == RET_INVALID_STATE) {
				LOG_WRN("RET_INVALID_STATE - Cannot enter identify mode");
			} else {
				ZB_ERROR_CHECK(zb_err_code);
			}
		} else {
			LOG_INF("Cancel identify mode");
			zb_bdb_finding_binding_target_cancel();
		}
	} else {
		LOG_WRN("Device not in a network - cannot enter identify mode");
	}
}

/**@brief Callback for button events.
 *
 * @param[in]   button_state  Bitmask containing the state of the buttons.
 * @param[in]   has_changed   Bitmask containing buttons that have changed their state.
 */
static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (IDENTIFY_MODE_BUTTON & has_changed) {
		if (IDENTIFY_MODE_BUTTON & button_state) {
			/* Button changed its state to pressed */
		} else {
			/* Button changed its state to released */
			if (was_factory_reset_done()) {
				/* The long press was for Factory Reset */
				LOG_DBG("After Factory Reset - ignore button release");
			} else   {
				/* Button released before Factory Reset */

				/* Start identification mode */
				ZB_SCHEDULE_APP_CALLBACK(start_identifying, 0);
			}
		}
	}

	check_factory_reset_button(button_state, has_changed);
}

/**@brief Function for initializing additional PWM leds. */
static void pwm_led_init(void)
{
	if (!device_is_ready(led_pwm.dev)) {
		LOG_ERR("Error: PWM device %s is not ready",
			led_pwm.dev->name);
	}

	//gpio_is_ready_dt(&led);
	//gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
}

/**@brief Function for initializing LEDs and Buttons. */
static void configure_gpio(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
	}

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	pwm_led_init();
}

/**@brief Sets brightness of bulb luminous executive element
 *
 * @param[in] brightness_level Brightness level, allowed values 0 ... 255,
 *                             0 - turn off, 255 - full brightness.
 */
static void light_bulb_set_brightness(zb_uint8_t brightness_level)
{
	uint32_t pulse = brightness_level * LED_PWM_PERIOD_US / 255U;

	//if (brightness_level == 0)
	//	gpio_pin_set_dt(&led, 0);
	//else
	//	gpio_pin_set_dt(&led, 1);
	if (pwm_set_dt(&led_pwm, PWM_USEC(LED_PWM_PERIOD_US), PWM_USEC(pulse))) {
		LOG_ERR("Pwm led 4 set fails:\n");
		return;
	}
}

/**@brief Function for setting the light bulb brightness.
 *
 * @param[in] new_level   Light bulb brightness value.
 */
static void level_control_set_value(zb_uint16_t new_level)
{
	LOG_INF("Set level value: %i", new_level);

	dim_ep.attr<&zb_zcl_level_control_attrs_t::current_level>() = new_level;

	light_bulb_set_brightness(new_level);
}

/**@brief Function for turning ON/OFF the light bulb.
 *
 * @param[in]   on   Boolean light bulb state.
 */
static void on_off_set_value(zb_bool_t on)
{
	LOG_INF("Set ON/OFF value: %i", on);

	dim_ep.attr<&zb_zcl_on_off_attrs_t::on_off>() = on;

	if (on) {
		light_bulb_set_brightness(dev_ctx.level_control_attr.current_level);
	} else {
		light_bulb_set_brightness(0U);
	}
}

/**@brief Function to toggle the identify LED - BULB_LED is used for this.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void toggle_identify_led(zb_bufid_t bufid)
{
	static int blink_status;

	light_bulb_set_brightness(((++blink_status) % 2) ? (255U) : (0U));
	ZB_SCHEDULE_APP_ALARM(toggle_identify_led, bufid, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void identify_cb(zb_bufid_t bufid)
{
	zb_ret_t zb_err_code;

	if (bufid) {
		/* Schedule a self-scheduling function that will toggle the LED. */
		ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
	} else {
		/* Cancel the toggling function alarm and restore current Zigbee LED state. */
		zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
		ZVUNUSED(zb_err_code);

		if (dev_ctx.on_off_attr.on_off) {
			light_bulb_set_brightness(dev_ctx.level_control_attr.current_level);
		} else {
			light_bulb_set_brightness(0U);
		}
	}
}

/**@brief Function for initializing all clusters attributes.
 */
static void bulb_clusters_attr_init(void)
{
	/* Basic cluster attributes data */
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.app_version = BULB_INIT_BASIC_APP_VERSION;
	dev_ctx.basic_attr.stack_version = BULB_INIT_BASIC_STACK_VERSION;
	dev_ctx.basic_attr.hw_version = BULB_INIT_BASIC_HW_VERSION;

	/* Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte
	 * should contain string length without trailing zero.
	 *
	 * For example "test" string will be encoded as:
	 *   [(0x4), 't', 'e', 's', 't']
	 */
	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.mf_name,
		BULB_INIT_BASIC_MANUF_NAME,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.model_id,
		BULB_INIT_BASIC_MODEL_ID,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MODEL_ID));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.date_code,
		BULB_INIT_BASIC_DATE_CODE,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_DATE_CODE));

	dev_ctx.basic_attr.power_source = BULB_INIT_BASIC_POWER_SOURCE;

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.location_id,
		BULB_INIT_BASIC_LOCATION_DESC,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_LOCATION_DESC));

	dev_ctx.basic_attr.ph_env = BULB_INIT_BASIC_PH_ENV;

	/* Identify cluster attributes data. */
	dev_ctx.identify_attr.identify_time =
		ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	/* On/Off cluster attributes data. */
	dev_ctx.on_off_attr.on_off = (zb_bool_t)ZB_ZCL_ON_OFF_IS_ON;

	dev_ctx.level_control_attr.current_level =
		ZB_ZCL_LEVEL_CONTROL_LEVEL_MAX_VALUE;
	dev_ctx.level_control_attr.remaining_time =
		ZB_ZCL_LEVEL_CONTROL_REMAINING_TIME_DEFAULT_VALUE;

	dim_ep.attr<&zb_zcl_on_off_attrs_t::on_off>() = dev_ctx.on_off_attr.on_off;
	dim_ep.attr<&zb_zcl_level_control_attrs_t::current_level>() = dev_ctx.level_control_attr.current_level;
}

/**@brief Callback function for handling ZCL commands.
 *
 * @param[in]   bufid   Reference to Zigbee stack buffer
 *                      used to pass received data.
 */
static void zcl_device_cb(zb_bufid_t bufid)
{
	zb_uint8_t cluster_id;
	zb_uint8_t attr_id;
	zb_zcl_device_callback_param_t  *device_cb_param =
		ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);

	LOG_INF("%s id %hd", __func__, device_cb_param->device_cb_id);

	/* Set default response value. */
	device_cb_param->status = RET_OK;

	switch (device_cb_param->device_cb_id) {
	case ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID:
		LOG_INF("Level control setting to %d",
			device_cb_param->cb_param.level_control_set_value_param
			.new_value);
		level_control_set_value(
			device_cb_param->cb_param.level_control_set_value_param
			.new_value);
		break;

	case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
		cluster_id = device_cb_param->cb_param.
			     set_attr_value_param.cluster_id;
		attr_id = device_cb_param->cb_param.
			  set_attr_value_param.attr_id;

		if (cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF) {
			uint8_t value =
				device_cb_param->cb_param.set_attr_value_param
				.values.data8;

			LOG_INF("on/off attribute setting to %hd", value);
			if (attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
				on_off_set_value((zb_bool_t)value);
			}
		} else if (cluster_id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
			uint16_t value = device_cb_param->cb_param.
					 set_attr_value_param.values.data16;

			LOG_INF("level control attribute setting to %hd",
				value);
			if (attr_id ==
			    ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
				level_control_set_value(value);
			}
		} else {
			/* Other clusters can be processed here */
			LOG_INF("Unhandled cluster attribute id: %d",
				cluster_id);
			device_cb_param->status = RET_NOT_IMPLEMENTED;
		}
		break;

	default:
		break;
	}

	LOG_INF("%s status: %hd", __func__, device_cb_param->status);
}

struct BufPtr
{
	~BufPtr() { if (bufid) zb_buf_free(bufid); }
	zb_bufid_t bufid = 0;
};

struct sig_handlers_t
{
	using generic_handler_t = void(*)(zb_ret_t status);
	using dev_annce_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_device_annce_params_t *p);
	using leave_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_leave_params_t *p);
	using dev_associated_handler_t = void(*)(zb_ret_t status, zb_nwk_signal_device_associated_params_t *p);
	using leave_indication_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_leave_indication_params_t *p);
	//using zgp_commisioning_handler_t = void(*)(zb_ret_t status, zb_zgp_signal_commissioning_params_t *p);
	using can_sleep_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_can_sleep_params_t *p);
	using dev_authorized_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_device_authorized_params_t *p);
	using dev_update_handler_t = void(*)(zb_ret_t status, zb_zdo_signal_device_update_params_t *p);
	//using pan_id_conflict_handler_t = void(*)(zb_ret_t status, zb_start_pan_id_conflict_resolution_t *p);
	using nlme_status_indication_handler_t = void(*)(zb_ret_t status, zb_nwk_command_status_t *p);
	using nwk_permit_join_status_handler_t = void(*)(zb_ret_t status, zb_uint8_t *p);

	generic_handler_t on_skip_startup = nullptr;
	dev_annce_handler_t on_dev_annce = nullptr;
	leave_handler_t on_leave = nullptr;
	generic_handler_t on_error = nullptr;
	generic_handler_t on_dev_first_start = nullptr;
	generic_handler_t on_dev_reboot = nullptr;
	generic_handler_t on_steering = nullptr;
	generic_handler_t on_formation = nullptr;
	generic_handler_t on_finding_and_binding_target_finished = nullptr;
	generic_handler_t on_finding_and_binding_initiator_finished = nullptr;
	dev_associated_handler_t on_dev_associated = nullptr;
	leave_indication_handler_t on_leave_indication = nullptr;
	generic_handler_t on_wwah_rejoined_started = nullptr;
	can_sleep_handler_t on_can_sleep = nullptr;
	generic_handler_t on_production_config_ready = nullptr;
	generic_handler_t on_nwk_no_active_links_left = nullptr;
	generic_handler_t on_sub_ghz_suspend = nullptr;
	generic_handler_t on_sub_ghz_resume = nullptr;
	dev_authorized_handler_t on_dev_authorized = nullptr;
	dev_update_handler_t on_dev_update = nullptr;
	nlme_status_indication_handler_t on_nlme_status_indication = nullptr;
	generic_handler_t on_tcswap_db_backup_required = nullptr;
	generic_handler_t on_tcswapped = nullptr;
	generic_handler_t on_tc_rejoin_done = nullptr;
	nwk_permit_join_status_handler_t on_nwk_permit_join = nullptr;
	generic_handler_t on_steering_cancelled = nullptr;
	generic_handler_t on_formation_cancelled = nullptr;
	generic_handler_t on_ready_to_shut = nullptr;
};

template<sig_handlers_t h = {}>
void tpl_signal_handler(zb_bufid_t bufid)
{
	BufPtr b{bufid};
	zb_zdo_app_signal_hdr_t *pHdr;
	auto signalId = zb_get_app_signal(bufid, &pHdr);
	zb_ret_t status = zb_buf_get_status(bufid);
	switch(signalId)
	{
		//case ZB_ZDO_SIGNAL_DEFAULT_START://deprecated
		//break;
		case ZB_ZDO_SIGNAL_SKIP_STARTUP:
		if constexpr (h.on_skip_startup) h.on_skip_startup(status);
		break;
		case ZB_ZDO_SIGNAL_DEVICE_ANNCE://zb_zdo_signal_device_annce_params_t
		if constexpr (h.on_dev_annce) h.on_dev_annce(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_device_annce_params_t));
		break;
		case ZB_ZDO_SIGNAL_LEAVE://zb_zdo_signal_leave_params_t
		if constexpr (h.on_leave) h.on_leave(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_leave_params_t));
		break;
		case ZB_ZDO_SIGNAL_ERROR:
		if constexpr (h.on_error) h.on_error(status);
		break;
		case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
		if constexpr (h.on_dev_first_start) h.on_dev_first_start(status);
		break;
		case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		if constexpr (h.on_dev_reboot) h.on_dev_reboot(status);
		break;
		case ZB_BDB_SIGNAL_STEERING:
		if constexpr (h.on_steering) h.on_steering(status);
		break;
		case ZB_BDB_SIGNAL_FORMATION:
		if constexpr (h.on_formation) h.on_formation(status);
		break;
		case ZB_BDB_SIGNAL_FINDING_AND_BINDING_TARGET_FINISHED:
		if constexpr (h.on_finding_and_binding_target_finished) h.on_finding_and_binding_target_finished(status);
		break;
		case ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED:
		if constexpr (h.on_finding_and_binding_initiator_finished) h.on_finding_and_binding_initiator_finished(status);
		break;
		case ZB_NWK_SIGNAL_DEVICE_ASSOCIATED://zb_nwk_signal_device_associated_params_t, deprecated
		if constexpr (h.on_dev_associated) h.on_dev_associated(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_nwk_signal_device_associated_params_t));
		break;
		case ZB_ZDO_SIGNAL_LEAVE_INDICATION://zb_zdo_signal_leave_indication_params_t
		if constexpr (h.on_leave_indication) h.on_leave_indication(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_leave_indication_params_t));
		break;
		case ZB_BDB_SIGNAL_WWAH_REJOIN_STARTED:
		if constexpr (h.on_wwah_rejoined_started) h.on_wwah_rejoined_started(status);
		break;
		//case ZB_ZGP_SIGNAL_COMMISSIONING://zb_zgp_signal_commissioning_params_s
		//break;
		case ZB_COMMON_SIGNAL_CAN_SLEEP://zb_zdo_signal_can_sleep_params_t
		if constexpr (h.on_can_sleep) h.on_can_sleep(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_can_sleep_params_t));
		break;
		case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
		if constexpr (h.on_production_config_ready) h.on_production_config_ready(status);
		break;
		case ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT:
		if constexpr (h.on_nwk_no_active_links_left) h.on_nwk_no_active_links_left(status);
		break;
		case ZB_SIGNAL_SUBGHZ_SUSPEND:
		if constexpr (h.on_sub_ghz_suspend) h.on_sub_ghz_suspend(status);
		break;
		case ZB_SIGNAL_SUBGHZ_RESUME:
		if constexpr (h.on_sub_ghz_resume) h.on_sub_ghz_resume(status);
		break;
		case ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED://zb_zdo_signal_device_authorized_params_t
		if constexpr (h.on_dev_authorized) h.on_dev_authorized(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_device_authorized_params_t));
		break;
		case ZB_ZDO_SIGNAL_DEVICE_UPDATE://zb_zdo_signal_device_update_params_t
		if constexpr (h.on_dev_update) h.on_dev_update(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_zdo_signal_device_update_params_t));
		break;
		//case ZB_NWK_SIGNAL_PANID_CONFLICT_DETECTED://zb_start_pan_id_conflict_resolution
		//break;
		case ZB_NLME_STATUS_INDICATION://zb_nwk_command_status_t
		if constexpr (h.on_nlme_status_indication) h.on_nlme_status_indication(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_nwk_command_status_t));
		break;
		case ZB_TCSWAP_DB_BACKUP_REQUIRED_SIGNAL:
		if constexpr (h.on_tcswap_db_backup_required) h.on_tcswap_db_backup_required(status);
		break;
		case ZB_TC_SWAPPED_SIGNAL:
		if constexpr (h.on_tcswapped) h.on_tcswapped(status);
		break;
		case ZB_BDB_SIGNAL_TC_REJOIN_DONE:
		if constexpr (h.on_tc_rejoin_done) h.on_tc_rejoin_done(status);
		break;
		case ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS://zb_uint8_t
		if constexpr (h.on_nwk_permit_join) h.on_nwk_permit_join(status, ZB_ZDO_SIGNAL_GET_PARAMS(pHdr, zb_uint8_t));
		break;
		case ZB_BDB_SIGNAL_STEERING_CANCELLED:
		if constexpr (h.on_steering_cancelled) h.on_steering_cancelled(status);
		break;
		case ZB_BDB_SIGNAL_FORMATION_CANCELLED:
		if constexpr (h.on_formation_cancelled) h.on_formation_cancelled(status);
		break;
		case ZB_SIGNAL_READY_TO_SHUT:
		if constexpr (h.on_ready_to_shut) h.on_ready_to_shut(status);
		break;
	}

	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
}

/**@brief Zigbee stack event handler.
 *
 * @param[in]   bufid   Reference to the Zigbee stack buffer
 *                      used to pass signal.
 */
void zboss_signal_handler(zb_bufid_t bufid)
{
	/* Update network status LED. */
	zigbee_led_status_update(bufid, ZIGBEE_NETWORK_STATE_LED);

	/* No application-specific behavior is required.
	 * Call default signal handler.
	 */
	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));

	/* All callbacks should either reuse or free passed buffers.
	 * If bufid == 0, the buffer is invalid (not passed).
	 */
	if (bufid) {
		zb_buf_free(bufid);
	}
}

int main(void)
{
	//K_ERR_KERNEL_PANIC;
	int blink_status = 0;
	int err;

	LOG_INF("Starting ZBOSS Light Bulb example");

	/* Initialize */
	configure_gpio();
	err = settings_subsys_init();
	if (err) {
		LOG_ERR("settings initialization failed");
	}
	register_factory_reset_button(FACTORY_RESET_BUTTON);

	/* Register callback for handling ZCL commands. */
	ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);

	/* Register dimmer switch device context (endpoints). */
	ZB_AF_REGISTER_DEVICE_CTX(dimmable_light_ctx);

	bulb_clusters_attr_init();
	level_control_set_value(dev_ctx.level_control_attr.current_level);

	/* Register handler to identify notifications. */
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(DIMMABLE_LIGHT_ENDPOINT, identify_cb);

	/* Settings should be loaded after zcl_scenes_init */
	err = settings_load();
	if (err) {
		LOG_ERR("settings loading failed");
	}

	/* Start Zigbee default thread */
	zigbee_enable();

	LOG_INF("ZBOSS Light Bulb example started");

	while (1) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}
