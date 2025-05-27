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
#include <zephyr/devicetree.h>
#include <soc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/regulator.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/settings/settings.h>
#include <ram_pwrdn.h>
#include <zephyr/drivers/adc.h>
#include "lib/lib_msgq_typed.hpp"
#include "led.h"

//#define FORCE_FMT
//#define PRINTF_FUNC(...) printk(__VA_ARGS__)
#define ALARM_LIST_LOCK_TYPE thread::DummyLock

#include "zb/zb_main.hpp"
#include "zb/zb_std_cluster_desc.hpp"
#include "zb/zb_co2_cluster_desc.hpp"
#include "zb/zb_power_config_cluster_desc.hpp"
#include "zb/zb_temp_cluster_desc.hpp"
#include "zb/zb_humid_cluster_desc.hpp"
#include "zb/zb_poll_ctrl_cluster_desc.hpp"

#include "zb/zb_alarm.hpp"

constexpr bool kPowerSaving = false;

/* Manufacturer name (32 bytes). */
#define BULB_INIT_BASIC_MANUF_NAME      "TheOrlangur"

/* Model number assigned by manufacturer (32-bytes long string). */
#define BULB_INIT_BASIC_MODEL_ID        "CO2_v0.1"


/* Button used to enter the Bulb into the Identify mode. */
#define IDENTIFY_MODE_BUTTON            DK_BTN1_MSK

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON IDENTIFY_MODE_BUTTON

/* Device endpoint, used to receive light controlling commands. */
constexpr uint8_t kCO2_EP = 10;

constexpr uint16_t kDEV_ID = 0xDEAD;

/* Main application customizable context.
 * Stores all settings and static values.
 */
typedef struct {
    zb::zb_zcl_basic_names_t basic_attr;
    zb::zb_zcl_power_cfg_battery_info_t battery_attr;
    zb::zb_zcl_poll_ctrl_basic_t poll_ctrl;
    zb::zb_zcl_co2_basic_t co2_attr;
    zb::zb_zcl_temp_basic_t temp_attr;
    zb::zb_zcl_rel_humid_basic_t humid_attr;
} bulb_device_ctx_t;

constexpr auto kAttrCO2Value = &zb::zb_zcl_co2_basic_t::measured_value;
constexpr auto kAttrTempValue = &zb::zb_zcl_temp_basic_t::measured_value;
constexpr auto kAttrRelHValue = &zb::zb_zcl_rel_humid_basic_t::measured_value;
constexpr auto kAttrBattVoltage = &zb::zb_zcl_power_cfg_battery_info_t::batt_voltage;
constexpr auto kAttrBattPercentage = &zb::zb_zcl_power_cfg_battery_info_t::batt_percentage_remaining;

constexpr uint32_t kPowerCycleThresholdSeconds = 6 * 60 - 1; //Just under 6 minutes

using namespace zb::literals;
/* Zigbee device application context storage. */
static constinit bulb_device_ctx_t dev_ctx{
    .poll_ctrl = {
	.check_in_interval = 2_min_to_qs,
	.long_poll_interval = 0xffffffff,//disabled
	//.short_poll_interval = 1_sec_to_qs,
    },
    .co2_attr{
	.measured_value = 0
    }
};

constinit static auto co2_ctx = zb::make_device( 
	zb::make_ep_args<{.ep=kCO2_EP, .dev_id=kDEV_ID, .dev_ver=1}>(
	    dev_ctx.basic_attr
	    , dev_ctx.battery_attr
	    , dev_ctx.poll_ctrl
	    , dev_ctx.co2_attr
	    , dev_ctx.temp_attr
	    , dev_ctx.humid_attr
	    )
	);

constinit static auto &co2_ep = co2_ctx.ep<kCO2_EP>();

/**********************************************************************/
/* Devices                                                            */
/**********************************************************************/
constinit const struct device *co2sensor = nullptr;
constinit const struct device *co2_power = nullptr;

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

/**********************************************************************/
/* Message Queue definitions + commands                               */
/**********************************************************************/
enum class CO2Commands
{
    Initial
    , Fetch
    , ManualFetch
};

using CO2Q = msgq::Queue<CO2Commands,4>;
K_MSGQ_DEFINE_TYPED(CO2Q, co2v2);

/**********************************************************************/
/* Battery                                                            */
/**********************************************************************/
int32_t g_BatteryVoltage = 0;

/**********************************************************************/
/* CO2 measuring thread                                               */
/**********************************************************************/
void co2_thread_entry(void *, void *, void *);
void update_co2_readings_in_zigbee(uint8_t id);

constexpr size_t CO2_THREAD_STACK_SIZE = 512;
constexpr size_t CO2_THREAD_PRIORITY=7;

K_THREAD_DEFINE(co2_thread, CO2_THREAD_STACK_SIZE,
	co2_thread_entry, NULL, NULL, NULL,
	CO2_THREAD_PRIORITY, 0, -1);

bool update_measurements()
{
    bool needsPowerCycle = false;
    bool res = false;
    if ((needsPowerCycle = !regulator_is_enabled(co2_power)))
    {
	regulator_enable(co2_power);
	device_init(co2sensor);
    }
    if (device_is_ready(co2sensor)) {
	int err = -EAGAIN;
	int max_attempts = 3;
	while((err == -EAGAIN) && max_attempts)
	{
	    err = sensor_sample_fetch(co2sensor);
	    if (err == -EAGAIN)
		k_sleep(K_MSEC(1000));
	    --max_attempts;
	}

	if (err == 0)
	{
	    if (needsPowerCycle) //after power up 2 fetches are needed
		sensor_sample_fetch(co2sensor);
	}

	{
	    uint16_t buf;
	    struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	    };
	    (void)adc_sequence_init_dt(&adc_channels[0], &sequence);

	    int err = adc_read_dt(&adc_channels[0], &sequence);
	    if (err == 0)
	    {
		g_BatteryVoltage = (int32_t)buf;
		adc_raw_to_millivolts_dt(&adc_channels[0],
			&g_BatteryVoltage);
	    }
	}
	res = true;
    }

    if (zb::qs_to_s(dev_ctx.poll_ctrl.check_in_interval) >= kPowerCycleThresholdSeconds)//seconds
    {
	//check in interval is big enough to power down
	regulator_disable(co2_power);
    }
    return res;
}

void co2_thread_entry(void *, void *, void *)
{
    CO2Commands cmd;
    bool needsPowerCycle = false;
    static uint32_t g_last_mark = 0; 
    while(1)
    {
	co2v2 >> cmd;
	switch(cmd)
	{
	    using enum CO2Commands;
	    case Initial:
	    {
		if (update_measurements())
		    zigbee_enable();
	    }
	    break;
	    case Fetch:
	    {
		auto now = k_uptime_seconds();
		if (g_last_mark && ((now - g_last_mark) < (zb::qs_to_s(dev_ctx.poll_ctrl.check_in_interval) / 2)))
		{
		    //to often
		    continue;
		}
		g_last_mark = now;
	    }
	    [[fallthrough]];
	    case ManualFetch:
	    if (update_measurements())
	    {
		//post to zigbee thread
		zigbee_schedule_callback(update_co2_readings_in_zigbee, 0);
	    }
	    break;
	}
    }
}

zb::ZbTimerExt g_FactoryResetDoneChecker;

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
	    g_FactoryResetDoneChecker.Setup([]{
		    if (was_factory_reset_done()) {
			/* The long press was for Factory Reset */
			led::show_pattern(led::kPATTERN_2_BLIPS_NORMED, 2000);
			k_sleep(K_MSEC(2100));
			sys_reboot(SYS_REBOOT_COLD);
			return false;
		    }
		    return true;
	    }, 1000);
	} else {
	    /* Button changed its state to released */
	    if (!was_factory_reset_done()) {
		/* Button released before Factory Reset */
		g_FactoryResetDoneChecker.Cancel();
		co2v2 << CO2Commands::ManualFetch;
		led::show_pattern(led::kPATTERN_2_BLIPS_NORMED, 500);
	    }
	}
    }

    check_factory_reset_button(button_state, has_changed);
}

/**@brief Function for initializing LEDs and Buttons. */
static void configure_gpio(void)
{
    int err;

    err = dk_buttons_init(button_changed);
    if (err) {
    }
}

static int configure_adc(void)
{
    if (!adc_is_ready_dt(&adc_channels[0])) {
	//printk("ADC controller device %s not ready\n", adc_channels[0].dev->name);
	return -1;
    }

    int err = adc_channel_setup_dt(&adc_channels[0]);
    if (err < 0) {
	//printk("Could not setup channel #%d (%d)\n", i, err);
	return err;
    }
    return 0;
}

/**@brief Function for initializing all clusters attributes.
*/
static void bulb_clusters_attr_init(void)
{
    /* Basic cluster attributes data */
    dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
    dev_ctx.basic_attr.manufacturer = BULB_INIT_BASIC_MANUF_NAME;
    dev_ctx.basic_attr.model = BULB_INIT_BASIC_MODEL_ID;
    dev_ctx.basic_attr.power_source = zb::zb_zcl_basic_min_t::PowerSource::Battery;
}

void on_zigbee_start()
{
    zb_zcl_poll_control_start(0, kCO2_EP);
    zb_zcl_poll_controll_register_cb([](uint8_t){co2v2 << CO2Commands::Fetch;});
    

    //should be there already
    update_co2_readings_in_zigbee(0);
}

void update_co2_readings_in_zigbee(uint8_t id)
{
    if (co2sensor)
    {
	sensor_value v;
	sensor_channel_get(co2sensor, SENSOR_CHAN_CO2, &v);
	co2_ep.attr<kAttrCO2Value>() = float(v.val1) / 1'000'000.f;
	co2_ep.attr<kAttrBattVoltage>() = uint8_t(g_BatteryVoltage / 100);
	co2_ep.attr<kAttrBattPercentage>() = uint8_t(g_BatteryVoltage * 200 / 1600);
	sensor_channel_get(co2sensor, SENSOR_CHAN_AMBIENT_TEMP, &v);
	co2_ep.attr<kAttrTempValue>() = zb::zb_zcl_temp_t::FromC(float(v.val1) + float(v.val2) / 1000'000.f);
	sensor_channel_get(co2sensor, SENSOR_CHAN_HUMIDITY, &v);
	co2_ep.attr<kAttrRelHValue>() = zb::zb_zcl_rel_humid_t::FromRelH(float(v.val1) + float(v.val2) / 1000'000.f);
    }else
    {
	//co2_ep.attr<kAttrCO2Value>() = float(200) / 1'000'000.f;
    }
}

/**@brief Zigbee stack event handler.
 *
 * @param[in]   bufid   Reference to the Zigbee stack buffer
 *                      used to pass signal.
 */
void zboss_signal_handler(zb_bufid_t bufid)
{
    auto ret = zb::tpl_signal_handler<{
	    .on_leave = []{ zb_zcl_poll_control_stop(); },
	    .on_error = []{ led::show_pattern(led::kPATTERN_3_BLIPS_NORMED, 1000); },
	    .on_dev_reboot = on_zigbee_start,
	    .on_steering = on_zigbee_start,
	    .on_can_sleep = zb_sleep_now,
    }>(bufid);
    const uint32_t LOCAL_ERR_CODE = (uint32_t) (-ret);	
    if (LOCAL_ERR_CODE != RET_OK) {				
	zb_osif_abort();				
    }							
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

int main(void)
{
    int blink_status = 0;
    int err;

    co2sensor = DEVICE_DT_GET(DT_NODELABEL(co2sensor));
    co2_power = DEVICE_DT_GET(DT_NODELABEL(scd41_power));
    if (!device_is_ready(co2_power)) {
    	//printk("Power reg not ready");
    	return 0;
    }

    /* Initialize */
    configure_gpio();
    if (led::setup() < 0)
    {
	return 0;
    }
    if (configure_adc() < 0)
    {
	return 0;
    }
    err = settings_subsys_init();
    if (err) {
    }
    register_factory_reset_button(FACTORY_RESET_BUTTON);

    zigbee_erase_persistent_storage(false);
    zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    zb_set_keepalive_mode(ED_KEEPALIVE_DISABLED);
    if constexpr (kPowerSaving)
    {
	zb_set_rx_on_when_idle(false);
	zigbee_configure_sleepy_behavior(true);
    }

    /* Register callback for handling ZCL commands. */
    auto dev_cb = zb::tpl_device_cb<>;
    ZB_ZCL_REGISTER_DEVICE_CB(dev_cb);

    /* Register dimmer switch device context (endpoints). */
    ZB_AF_REGISTER_DEVICE_CTX(co2_ctx);

    bulb_clusters_attr_init();

    /* Settings should be loaded after zcl_scenes_init */
    err = settings_load();
    if (err) {
    }

    if constexpr (kPowerSaving)
    {
	power_down_unused_ram();
    }
    k_thread_start(co2_thread);
    led::start();

    co2v2 << CO2Commands::Initial;

    while (1) {
	k_sleep(K_FOREVER);
    }
    return 0;
}
