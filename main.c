#include <stdio.h>
#include <stdint.h>

#include "mem.h"
#include "osapi.h"
#include "uart.h"
#include "user_interface.h"

#include "main.h"

#define BUTTON_PIN 2
#define LED_PIN 0

static os_timer_t os_timer;

/* This program demonstrates WPS pairing.
 *
 * Press the button for 5 seconds to enable
 * WPS. The LED will light up. Press the
 * button for another 5 seconds to stop WPS.
 * The LED will switch off.
 */

static volatile uint16_t bounces = 0xFFFF;
static volatile uint32_t button_pressed_at = 0;
static volatile uint8_t wps_enabled = 0;

static void wifi_wps_cb(int status)
{
    switch (status) {
    case WPS_CB_ST_SUCCESS:
	wifi_wps_disable();
	wifi_station_connect();
	wps_enabled = 0;
	os_printf("WPS is connected.");
	break;
    default:
	wifi_wps_disable();
	wps_enabled = 0;
	os_printf("WPS failed with status code %d\n", status);
	break;
    }
}

static void main_on_timer(void *arg)
{
    uint32_t now = system_get_time();
    uint32_t button_pressed = 0;
    uint32_t button_state = GPIO_INPUT_GET(BUTTON_PIN);

    bounces = (bounces << 1) | button_state;
    button_pressed = bounces < 0xFF00;
    if (button_pressed) {
	if (button_pressed_at == 0)
	    button_pressed_at = now;
	else if (now - button_pressed_at >= 5000000) {
	    if (!wps_enabled) {
		wps_enabled = wifi_wps_enable(WPS_TYPE_PBC);
		if (wps_enabled) {
		    wifi_set_wps_cb(wifi_wps_cb);
		    if (wifi_wps_start())
			os_printf("WPS is started\n");
		    else
			os_printf("WPS cannot be started\n");
		} else
		    os_printf("WPS cannot be enabled\n");
	    } else {
		wifi_wps_disable();
		wps_enabled = 0;
		os_printf("WPS is disabled\n");
	    }
	    button_pressed_at = 0;
	}
    } else
	button_pressed_at = 0;
    GPIO_OUTPUT_SET(LED_PIN, wps_enabled);
    os_timer_arm(&os_timer, 100, 0);
}

static void configure_wifi()
{
    wifi_set_opmode(STATION_MODE);
}

void ICACHE_FLASH_ATTR user_init(void)
{
    /* UART0 is the default debugging interface, so we must initialise UART
     * with the desired baud rate
     */
    uart_init(BIT_RATE_921600, BIT_RATE_921600);

    gpio_init();
    GPIO_DIS_OUTPUT(BUTTON_PIN);

    configure_wifi();

    os_timer_disarm(&os_timer);
    os_timer_setfn(&os_timer, &main_on_timer, (void *)NULL);
    os_timer_arm(&os_timer, 100, 0);
}
