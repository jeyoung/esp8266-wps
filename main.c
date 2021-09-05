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

struct Button
{
    uint32_t pin;
    uint16_t bounces;
    uint32_t down;
    uint32_t up;
    uint32_t longpress;
    uint32_t timestamp;
};

struct Wifi
{
    uint32_t mode;
    uint32_t wps_started;
};

struct Board
{
    struct Button *button;
    struct Wifi *wifi;
};

static struct Button button = {0};
static struct Wifi wifi = {0};
static struct Board board = {0};

void ICACHE_FLASH_ATTR button_init(struct Button *button)
{
    gpio_init();
    button->pin = BUTTON_PIN;
    button->bounces = 0xFFFF;
    button->down = 0;
    button->up = 0;
    button->longpress = 0;
    button->timestamp = 0;
    GPIO_DIS_OUTPUT(button->pin);
}

void ICACHE_FLASH_ATTR button_read(struct Button *button)
{
    button->bounces = (button->bounces << 1) | (uint16_t)GPIO_INPUT_GET(button->pin);
    button->up = button->down && (button->bounces > 0xF000);
    button->longpress = button->down && (system_get_time() - button->timestamp > 5000000);
    if (button->down) {
	if (!button->timestamp)
	    button->timestamp = system_get_time();
    } else
	button->timestamp = 0;
    button->down = (button->bounces < 0xF000);
}

static ICACHE_FLASH_ATTR void wifi_init(struct Wifi *wifi)
{
    wifi->mode = STATION_MODE;
    wifi->wps_started = 0;
    wifi_wps_disable();
}

static void wifi_wps_cb(int status);

/*
 * Toggles the WiFi WPS state.
 */
static ICACHE_FLASH_ATTR void wifi_wps_toggle(struct Wifi *wifi)
{
    wifi_set_opmode(wifi->mode);
    wifi_wps_disable();
    if (!wifi->wps_started) {
	wifi->wps_started =  wifi_wps_enable(WPS_TYPE_PBC) && wifi_wps_start();
	wifi_set_wps_cb(wifi_wps_cb);
    } else
	wifi->wps_started = 0;
}

static ICACHE_FLASH_ATTR void wifi_connect(struct Wifi *wifi)
{
    wifi_wps_disable();
    wifi_station_connect();
    wifi->wps_started = 0;
}

static ICACHE_FLASH_ATTR void board_init(struct Board *board, struct Button *button, struct Wifi *wifi)
{
    board->button = button;
    board->wifi = wifi;
}

static ICACHE_FLASH_ATTR void board_tick(struct Board *board)
{
    button_read(board->button);
    if (board->button->longpress && board->button->up) {
	if (!board->wifi->wps_started) {
	    wifi_wps_toggle(board->wifi);
	    if (board->wifi->wps_started)
		os_printf("WPS is started.\n");
	    else
		os_printf("WPS cannot be started.\n");
	} else {
	    wifi_wps_toggle(board->wifi);
	    os_printf("WPS is disabled.\n");
	}
    }
    GPIO_OUTPUT_SET(LED_PIN, board->button->longpress | board->wifi->wps_started);
}

static void wifi_wps_cb(int status)
{
    switch (status) {
    case WPS_CB_ST_SUCCESS:
	wifi_connect(&wifi);
	os_printf("WPS is connected.\n");
	break;
    default:
	wifi_init(&wifi);
	os_printf("WPS failed with status code %d.\n", status);
	break;
    }
}

static void main_on_timer(void *arg)
{
    board_tick(&board);
}

void ICACHE_FLASH_ATTR user_init(void)
{
    uart_init(BIT_RATE_921600, BIT_RATE_921600);
    button_init(&button);
    wifi_init(&wifi);
    board_init(&board, &button, &wifi);

    os_timer_disarm(&os_timer);
    os_timer_setfn(&os_timer, &main_on_timer, (void *)NULL);
    os_timer_arm(&os_timer, 10, 1);
}
