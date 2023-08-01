/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 * Redirection of custom board to specific wyres boards pinout definitions
 */
#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

// Include the relevant custom boards we know about
#ifdef BOARD_W_FILLE_MINEW_MS49SF2
#include "board_ms49SF2_w_fille.h"      // nRF51822 mdule on a Wyres W_FILLE card
#endif
#ifdef BOARD_W_FILLE_MINEW_MS50SFA
#include "board_ms50SFA_w_fille.h"      // nrf52832 module on a Wyres W_FILLE card
#endif
#ifdef BOARD_W_BLE_LIGHT_MINEW_MS50SFA2
#include "board_ms49SF2_w_ble_light.h"      // nrf52832 module on a Wyres W_FILLE card connected to LEDs as a light
#endif

#endif // CUSTOM_BOARD_H
