/** basic leds control */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "leds.h"
#include "wutils.h"

void leds_init(uint8_t* leds, uint8_t nLeds) {
    for (int i=0;i<nLeds;i++) {
        log_info("led %d init as output", leds[i]);
        nrf_gpio_cfg(leds[i],
            NRF_GPIO_PIN_DIR_OUTPUT,
            NRF_GPIO_PIN_INPUT_DISCONNECT,
            NRF_GPIO_PIN_NOPULL,
            NRF_GPIO_PIN_S0H1,
            NRF_GPIO_PIN_NOSENSE);
    }
}
void led_on(uint8_t led_io) {
    nrf_gpio_pin_set(led_io);
    //log_info("led %d ON", led_io);
}
void led_off(uint8_t led_io) {
    nrf_gpio_pin_clear(led_io);
    //log_info("led %d OFF", led_io);
}
void led_invert(uint8_t led_io) {
    nrf_gpio_pin_toggle(led_io);
    //log_info("led %d INV", led_io);
}
void led_flash(uint8_t led_io, uint32_t on_time_ms) {
    nrf_gpio_pin_set(led_io);
    nrf_delay_ms(on_time_ms);
    nrf_gpio_pin_clear(led_io);
}
