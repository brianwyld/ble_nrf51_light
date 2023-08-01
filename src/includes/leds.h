#ifndef LEDS_H__
#define LEDS_H__

void leds_init(uint8_t* leds, uint8_t nLeds);
void led_on(uint8_t led_io);
void led_off(uint8_t led_io);
void led_invert(uint8_t led_io);
void led_flash(uint8_t led_io, uint32_t on_time_ms);

#endif
