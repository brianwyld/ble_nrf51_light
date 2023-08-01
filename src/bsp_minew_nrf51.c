/**
 * BSP functions for the Minex nRF51xxx module mounted on the Wyres dcard
 * Some of these are specific to the board, others for the nrf51 SOC
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "nrf_soc.h"
#include "app_uart.h"
#include "main.h"

#include "bsp_minew_nrf51.h"

// UARTs config
static struct {
    bool isOpen;
    app_uart_comm_params_t uart_comm_params;
} _uarts[UART_CNT] = 
    {{
        .isOpen = false,
        .uart_comm_params = {
            .rx_pin_no = UART0_RX_PIN_NUMBER, // RX_PIN_NUMBER A
            .tx_pin_no = UART0_TX_PIN_NUMBER, // TX_PIN_NUMBER A
            .rts_pin_no = UART0_RTS_PIN_NUMBER,
            .cts_pin_no = UART0_CTS_PIN_NUMBER,
            .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
            .use_parity = false,
            .baud_rate = UART_BAUDRATE_BAUDRATE_Baud115200,     // NOTE : MUST USE SPECIFIC DEFINES AS ITS NOT JUST '115200'
        }
    }
    };


// On this module we define 2 UARTs:
// - communication with external cards (AT command set processing) 
// - debug logging
// NOTE: baudrate is the define from nrf51_bitfields.h
bool hal_bsp_uart_init(int uartNb, int baudrate_selector, app_uart_event_handler_t uart_event_handler) {
    uint32_t  err_code=0;
    if (uartNb<0 || uartNb>=UART_CNT) {
        return NULL;
    }
    // Protect against multiple inits of same uart
    if (_uarts[uartNb].isOpen==false) {
        _uarts[uartNb].uart_comm_params.baud_rate = baudrate_selector;
        APP_UART_FIFO_INIT( (&_uarts[uartNb].uart_comm_params),
                        UART_RX_BUF_SIZE,
                        UART_TX_BUF_SIZE,
                        uart_event_handler,
                        APP_IRQ_PRIORITY_LOW,
                        err_code);
        APP_ERROR_CHECK(err_code);
        _uarts[uartNb].isOpen = true;
    }
    return true;
}

void hal_bsp_uart_deinit(int uartNb) {
    if (uartNb<0 || uartNb >= UART_CNT) {
        return;
    }
    if (_uarts[uartNb].isOpen==true) {
        // only 1 uart in their api..
        app_uart_close();
        _uarts[uartNb].isOpen = false;
    }
}

// Tx line. returns number of bytes not sent due to flow control or -1 for error
int hal_bsp_uart_tx(int uartNb, uint8_t* d, int len) {
    if (uartNb<0 || uartNb >= UART_CNT) {
        return -1;      // fatal error
    }
    if (_uarts[uartNb].isOpen) {
        for(int i=0;i<len;i++) {
            // nrf uart api only allows one uart, sorry
            if (app_uart_put(d[i])!=NRF_SUCCESS) {
                return (len-i);       // Number of bytes not txd
            }
        }
        return 0;
    }
    return -1;
}

// UART enable IO
bool hal_bsp_comm_uart_active_request() {
    // Should the external comm uart be init or deinit?
    return true;
}

// LED config
void hal_bsp_leds_init(void) {

}
void hal_bsp_leds_deinit(void) {
    
}

// buttons
void hal_bsp_buttons_init(void) {

}
void hal_bsp_buttons_deinit(void) {
    
}

// NVM for config management
// The NVM used for config on the nrf51 is pages of flash
// Flash related constants for nRF51 with softdevice and app
// See nrf51_xxac.ld for where these come from...
extern volatile uint32_t __FLASH_CONFIG_BASE_ADDR[];
extern volatile uint32_t __FLASH_CONFIG_SZ[];
static uint32_t FLASH_CONFIG_SZ = ((uint32_t)__FLASH_CONFIG_SZ);      //(0x1000)                    // 4K
static uint32_t FLASH_CONFIG_BASE_ADDR = (((uint32_t)__FLASH_CONFIG_BASE_ADDR));   //0x338A0)        // FLASH_CFG in .ld file

uint16_t hal_bsp_nvmSize() {
    return (uint16_t)FLASH_CONFIG_SZ;
}
// Lock flash, we're done writing
bool hal_bsp_nvmLock() {
    return true;
}
// Unlock flash, we're going to be writing stuff
bool hal_bsp_nvmUnlock() {
    return true;
}

uint8_t hal_bsp_nvmRead8(uint16_t off) {
    return *((volatile uint8_t*)(FLASH_CONFIG_BASE_ADDR+off));
}
uint16_t hal_bsp_nvmRead16(uint16_t off) {
    return *((volatile uint16_t*)(FLASH_CONFIG_BASE_ADDR+off));
}
bool hal_bsp_nvmRead(uint16_t off, uint8_t len, uint8_t* buf) {
    for(int i=0;i<len;i++) {
        *(buf+i) = hal_bsp_nvmRead8(off+i);
    }
    return true;
}
bool hal_bsp_nvmWrite8(uint16_t off, uint8_t v) {
    uint32_t status=NRF_SUCCESS;
    // Sanity check
    if (off > hal_bsp_nvmSize()) {
        return false;       // no writing on the code
    }
    // softdevice access function can onyl access in 32 bit chunks. Need to map and merge...
    uint16_t off_u32aligned = (off/4)*4;
    uint32_t* addr_u32aligned = (uint32_t*)(FLASH_CONFIG_BASE_ADDR+off_u32aligned);
    // Get current value
    uint32_t currentU32Val = *addr_u32aligned;
    // Overwrite the correct byte
    switch (off % 4) {
        case 0:
            currentU32Val = ((currentU32Val & 0xFFFFFF00) | v);
            break;
        case 1:
            currentU32Val = ((currentU32Val & 0xFFFF00FF) | (v<<8));
            break;
        case 2:
            currentU32Val = ((currentU32Val & 0xFF00FFFF) | (v<<16));
            break;
        case 3:
            currentU32Val = ((currentU32Val & 0x00FFFFFF) | (v<<24));
            break;
    }
    do
    {
        status = sd_flash_write(addr_u32aligned, &currentU32Val, 1);        // Write 1 32 bit value
    } while (status == NRF_ERROR_BUSY);     // In case its still processing the previous write
    /* not required to wait
        while(app_isFlashBusy())
        {
            sd_app_evt_wait();
        }
    */
    return (status==NRF_SUCCESS);
}

bool hal_bsp_nvmWrite16(uint16_t off, uint16_t v) {
    bool ret = true;
    // Just write as 2 bytes, too hard to think about optimisation (may be within 1 32 bit word, or split...)
    ret &= hal_bsp_nvmWrite8(off, v & 0xff);
    ret &= hal_bsp_nvmWrite8(off+1, (v>>8) & 0xff);
    return ret;
}

bool hal_bsp_nvmWrite(uint16_t off, uint8_t len, uint8_t* buf) {
    // If writing a block to offset 0, we assume you are writing the entire page rather than just bits
    if (off==0) {
        uint32_t* addr_u32aligned = (uint32_t*)(FLASH_CONFIG_BASE_ADDR);
        uint32_t status=NRF_SUCCESS;
        // Erase page
        app_setFlashBusy();
        do
        {
            status = sd_flash_page_erase(((uint32_t)addr_u32aligned)/1024);
        } while (status == NRF_ERROR_BUSY);     // In case its still processing the previous write
        while(app_isFlashBusy()) {
            sd_app_evt_wait();
        }
        // Write block
        app_setFlashBusy();
        do
        {
            status = sd_flash_write(addr_u32aligned, (uint32_t*)buf, (len/4)+1);        // Write rounded up to nearest 32 bit length
        } while (status == NRF_ERROR_BUSY);     // In case its still processing the previous write
        while(app_isFlashBusy()) {
            sd_app_evt_wait();
        }
        return true;
    } else {
        // Writing N bytes, worth the optimisation? TODO later
        bool ret = true;
        for(int i=0;i<len;i++) {
            ret &= hal_bsp_nvmWrite8(off+i, *(buf+i));
        }
        return ret;
    }
}


// ADC

void hal_bsp_adc_init(void)
{
    //NRF_ADC->POWER = ADC_POWER_POWER_Enabled << ADC_POWER_POWER_Pos;
    NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled << ADC_ENABLE_ENABLE_Pos;
    
    NRF_ADC->CONFIG = (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos) |
                      (ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos) |
                      (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos) |
                      (ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos) |
                      (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
}
 
void hal_bsp_adc_disable(void)
{
    NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Disabled << ADC_ENABLE_ENABLE_Pos;
    //NRF_ADC->POWER = ADC_POWER_POWER_Disabled << ADC_POWER_POWER_Pos;
    
    NRF_ADC->CONFIG = (ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos) |
                      (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
}
 
uint16_t hal_bsp_adc_read_u16(void)
{
    NRF_ADC->CONFIG     &= ~ADC_CONFIG_PSEL_Msk;
    NRF_ADC->CONFIG     |= ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos;
    NRF_ADC->TASKS_START = 1;
    while (((NRF_ADC->BUSY & ADC_BUSY_BUSY_Msk) >> ADC_BUSY_BUSY_Pos) == ADC_BUSY_BUSY_Busy) {};
    NRF_ADC->TASKS_STOP = 1;
    return (uint16_t)NRF_ADC->RESULT; // 10 bit
}

#if 0 
/* Interrupt handler for ADC data ready event. It will be executed when ADC sampling is complete */
void ADC_IRQHandler(void)
{
    /* Clear dataready event */
    NRF_ADC->EVENTS_END = 0;
}
#endif
