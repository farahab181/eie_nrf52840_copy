/*
 * src/main.c
 * Zephyr: robust multi-LED blinker with batch init, logging, and clean error handling.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Build-time checks keep failures obvious. */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

BUILD_ASSERT(DT_NODE_EXISTS(LED0_NODE), "DT alias 'led0' missing");
BUILD_ASSERT(DT_NODE_EXISTS(LED1_NODE), "DT alias 'led1' missing");
BUILD_ASSERT(DT_NODE_EXISTS(LED2_NODE), "DT alias 'led2' missing");
BUILD_ASSERT(DT_NODE_EXISTS(LED3_NODE), "DT alias 'led3' missing");

/* Configure to INACTIVE so boards with active-low LEDs don't flash at boot. */
static const struct gpio_dt_spec leds[] = {
		GPIO_DT_SPEC_GET(LED0_NODE, gpios),
		GPIO_DT_SPEC_GET(LED1_NODE, gpios),
		GPIO_DT_SPEC_GET(LED2_NODE, gpios),
		GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};
#define NUM_LEDS (sizeof(leds) / sizeof(leds[0]))

/* Timing config */
#define STEP_DELAY_MS   150U   /* delay between each LED toggle in a step */
#define LAP_DELAY_MS    400U   /* delay after a full lap */
#define STARTUP_FLASHES 2U     /* brief startup indication */

/* Return the first nonzero Zephyr error code or 0. */
static int init_leds(void)
{
	for (size_t i = 0; i < NUM_LEDS; ++i) {
		if (!gpio_is_ready_dt(&leds[i])) {
			LOG_ERR("LED%u GPIO device not ready", (unsigned)i);
			return -ENODEV; /* why: early abort; hardware not ready */
		}
		int err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		if (err) {
			LOG_ERR("LED%u configure failed: %d", (unsigned)i, err);
			return err; /* why: propagate exact failure */
		}
	}
	return 0;
}

/* Quick visual heartbeat on boot. */
static void startup_blink(void)
{
	for (uint32_t n = 0; n < STARTUP_FLASHES; ++n) {
		for (size_t i = 0; i < NUM_LEDS; ++i) {
			gpio_pin_set_dt(&leds[i], 1);
		}
		k_msleep(000);
		for (size_t i = 0; i < NUM_LEDS; ++i) {
			gpio_pin_set_dt(&leds[i], 0);
		}
		k_msleep(100);
	}
}

/* One forward-and-back “chase” lap. */
static void do_chase_lap(void)
{
	/* Forward  */
	for (size_t i = 0; i < NUM_LEDS; ++i) {
		gpio_pin_toggle_dt(&leds[i]);
		k_msleep(STEP_DELAY_MS);
		gpio_pin_toggle_dt(&leds[i]);
	}
	/* Backward (skip ends to avoid double-toggling same LED) */
	for (size_t i = NUM_LEDS ; i-- > 0; ) {
		gpio_pin_toggle_dt(&leds[i]);
		k_msleep(STEP_DELAY_MS);
		gpio_pin_toggle_dt(&leds[i]);
	}
	k_msleep(LAP_DELAY_MS);
}

int main(void)
{
	int err = init_leds();
	if (err) {
		/* Fall back to printk so errors are visible even if logging is off. */
		printk("LED init failed: %d\n", err);
		return err;
	}

	LOG_INF("LEDs ready: %u", (unsigned)NUM_LEDS);
	startup_blink();

	/* Continuous pattern */
	for (;;) {
		do_chase_lap();
	}
	/* Unreachable */
	/* return 0; */
}
