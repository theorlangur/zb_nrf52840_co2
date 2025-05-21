#include "led.h"
#include <cstddef>
#include <zephyr/kernel.h>
#include <dk_buttons_and_leds.h>

#define STATUS_LED        DK_LED1

namespace led
{
    struct msg
    {
	uint32_t p;
	uint32_t tms;//millis
    };
    constexpr size_t MSGQ_LED_ENTRY_SIZE = sizeof(msg);
    constexpr size_t MSGQ_LED_LENGTH = 4;

    K_MSGQ_DEFINE(led_msgq, MSGQ_LED_ENTRY_SIZE, MSGQ_LED_LENGTH, 4);


    void led_thread_entry(void *, void *, void *);

    constexpr size_t LED_THREAD_STACK_SIZE = 512;
    constexpr size_t LED_THREAD_PRIORITY=7;

    K_THREAD_DEFINE(led_thread, LED_THREAD_STACK_SIZE,
	    led_thread_entry, NULL, NULL, NULL,
	    LED_THREAD_PRIORITY, 0, -1);

    void led_thread_entry(void *, void *, void *)
    {
	msg pattern;
	while(1)
	{
	    k_msgq_get(&led_msgq, &pattern, K_FOREVER);
	    size_t bitDuration = pattern.tms / 32;
	    for(int i = 0; i < 32; ++i)
	    {
		bool state = pattern.p & 1;
		dk_set_led(STATUS_LED, state);
		size_t dur = 0;
		while((pattern.p & 1) == state && i < 32)
		{
		    dur += bitDuration;
		    pattern.p >>= 1;
		    ++i;
		}
		--i;
		k_sleep(K_MSEC(dur));
	    }
	}
    }

    int setup()
    {
	int err = dk_leds_init();
	if (err) {
	    //LOG_ERR("Cannot init LEDs (err: %d)", err);
	    return err;
	}
	return 0;
    }

    void start()
    {
	k_thread_start(led_thread);
    }

    void show_pattern(uint32_t p, uint32_t tms)
    {
	msg _msg{p, tms};
	(void)k_msgq_put(&led_msgq, &_msg, K_FOREVER);
    }
}
