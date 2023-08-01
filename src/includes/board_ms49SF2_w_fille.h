/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#ifndef BOARD_MS49SF2_W_FILLE_H
#define BOARD_MS49SF2_W_FILLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// LEDs definitions for Minew module MS49SF2 on a wyres w_fille revD/E board
#define LEDS_NUMBER    2

#define LED_START      22
#define LED_1          22
#define LED_2          23
#define LED_3          -1
#define LED_4          -1
#define LED_STOP       23

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2
#define BSP_LED_2      LED_3
#define BSP_LED_3      LED_4

#define BUTTONS_NUMBER 0

#define BUTTON_START   24
#define BUTTON_1       24
#define BUTTON_2       -1
#define BUTTON_3       -1
#define BUTTON_4       -1
#define BUTTON_STOP    24
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4 }

#define BSP_BUTTON_0   BUTTON_1
#define BSP_BUTTON_1   BUTTON_2
#define BSP_BUTTON_2   BUTTON_3
#define BSP_BUTTON_3   BUTTON_4

#define BSP_VCCUART     12
#define BSP_EXT_IO      8

#define RX_PIN_NUMBER  16
#define TX_PIN_NUMBER  15
#define CTS_PIN_NUMBER UART_PIN_DISCONNECTED
#define RTS_PIN_NUMBER UART_PIN_DISCONNECTED
#define HWFC           false

// Low frequency clock source to be used by the SoftDevice
#ifdef S210
#define NRF_CLOCK_LFCLKSRC      NRF_CLOCK_LFCLKSRC_XTAL_20_PPM
#else
#define NRF_CLOCK_LFCLKSRC      {.source        = NRF_CLOCK_LF_SRC_XTAL,            \
                                 .rc_ctiv       = 0,                                \
                                 .rc_temp_ctiv  = 0,                                \
                                 .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM}
#endif

#ifdef __cplusplus
}
#endif

#endif // BOARD_MS49SF2_W_FILLE_H
