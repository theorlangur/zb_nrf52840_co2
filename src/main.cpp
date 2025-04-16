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

#define FORCE_FMT
#define PRINTF_FUNC(...) printk(__VA_ARGS__)

#include "zb/zb_main.hpp"
#include "zb/zb_std_cluster_desc.hpp"
#include "zb_dimmable_light.h"
#include "zb/zb_alarm.hpp"
#include "zb/zb_co2_cluster_desc.hpp"

#include "lib_misc_helpers.hpp"

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
#define BULB_INIT_BASIC_MANUF_NAME      "TheOrlangur"

/* Model number assigned by manufacturer (32-bytes long string). */
#define BULB_INIT_BASIC_MODEL_ID        "CO2_v0.1"

/* First 8 bytes specify the date of manufacturer of the device
 * in ISO 8601 format (YYYYMMDD). The rest (8 bytes) are manufacturer specific.
 */
#define BULB_INIT_BASIC_DATE_CODE       "20200329"

/* Type of power sources available for the device.
 * For possible values see section 3.2.2.2.8 of ZCL specification.
 */
#define BULB_INIT_BASIC_POWER_SOURCE    ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE

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
	zb::zb_zcl_basic_names_t basic_attr;
	zb::zb_zcl_co2_basic_t co2_attr;
} bulb_device_ctx_t;

/* Zigbee device application context storage. */
static bulb_device_ctx_t dev_ctx;

constinit static auto dimmable_light_ctx = zb::make_device( 
		zb::make_ep_args<{.ep=DIMMABLE_LIGHT_ENDPOINT, .dev_id=ZB_DIMMABLE_LIGHT_DEVICE_ID, .dev_ver=ZB_DEVICE_VER_DIMMABLE_LIGHT}>(
		dev_ctx.basic_attr
		, dev_ctx.co2_attr
		)
);
constinit static auto &dim_ep = dimmable_light_ctx.ep<DIMMABLE_LIGHT_ENDPOINT>();

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

/**@brief Function for initializing all clusters attributes.
 */
static void bulb_clusters_attr_init(void)
{
	/* Basic cluster attributes data */
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.manufacturer = BULB_INIT_BASIC_MANUF_NAME;
	dev_ctx.basic_attr.model = BULB_INIT_BASIC_MODEL_ID;
	dev_ctx.basic_attr.power_source = zb::zb_zcl_basic_min_t::PowerSource::DC;
}

static void test_device_cb(zb_zcl_device_callback_param_t *device_cb_param)
{
	zb_uint8_t cluster_id;
	zb_uint8_t attr_id;
	LOG_INF("%s id %hd", __func__, device_cb_param->device_cb_id);

	/* Set default response value. */
	device_cb_param->status = RET_OK;

	switch (device_cb_param->device_cb_id) {
	case ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID:
		//LOG_INF("Level control setting to %d",
		//	device_cb_param->cb_param.level_control_set_value_param
		//	.new_value);
		//level_control_set_value(
		//	device_cb_param->cb_param.level_control_set_value_param
		//	.new_value);
		break;
	default:
		break;
	}

	LOG_INF("%s status: %hd", __func__, device_cb_param->status);
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

	ZB_ERROR_CHECK(zb::tpl_signal_handler<{}>(bufid));
}

K_MUTEX_DEFINE(rtt_term_mutex);
extern "C" void zephyr_rtt_mutex_lock()
{
	k_mutex_lock(&rtt_term_mutex, K_FOREVER);
}

extern "C" void zephyr_rtt_mutex_unlock()
{
	k_mutex_unlock(&rtt_term_mutex);
}

zb::ZbAlarmExt<> g_TestAlarm;

int main(void)
{
	int blink_status = 0;
	int err;

	FMT_PRINTLN("Test print");
	LOG_INF("Starting ZBOSS Light Bulb example");

	/* Initialize */
	configure_gpio();
	err = settings_subsys_init();
	if (err) {
		LOG_ERR("settings initialization failed");
	}
	register_factory_reset_button(FACTORY_RESET_BUTTON);

	/* Register callback for handling ZCL commands. */
	auto dev_cb = zb::tpl_device_cb<
	    {test_device_cb} //default generic
	>;
	ZB_ZCL_REGISTER_DEVICE_CB(dev_cb);

	FMT_PRINTLN("Dev cb: {}", dev_cb);

	/* Register dimmer switch device context (endpoints). */
	ZB_AF_REGISTER_DEVICE_CTX(dimmable_light_ctx);

	bulb_clusters_attr_init();

	//g_TestAlarm.Setup([err]{
	//		level_control_set_value(err);
	//		}, 5000);

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
