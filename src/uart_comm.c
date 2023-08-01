/* uart_comm.c : handle the UART used for the communication with host ie AT commands
 */
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include <stdio.h>
#include "nrf_drv_gpiote.h"

#include "app_uart.h"

#include "wutils.h"

#include "main.h"
#include "bsp_minew_nrf51.h"
#include "at_process.h"
#include "device_config.h"

#define COMM_UART_NB    (0)
#define COMM_UART_BAUDRATE  (UART_BAUDRATE_BAUDRATE_Baud115200)     // MUST USE THE CONSTANT NOT A SIMPLE VALUE
#define MAX_RX_LINE (100)            // AT+IB_START E2C56DB5DFFB48D2B060D0F5A71096E0,8201,135C,00,0200,-10   is longest command and is 67

static struct {
    int uartNb;
    bool isOpen;
    bool rxDataReady;
    uint8_t rx_buf[MAX_RX_LINE+2];      // wriggle space for the \0
    uint8_t rx_index;
    UART_TX_READY_FN_T tx_ready_fn;     // in case caller wants to be told
    uint32_t rxC;
    uint32_t rxL;
    uint32_t txL;
    uint32_t rxFerr;
    uint32_t rxLerr;
} _ctx;

// predecs
static void uart_event_handler(app_uart_evt_t * p_event);
static void uart_gpio_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static bool uart_gpio_init();

/**@brief Function for initializing the UART.
 */
bool comm_uart_init() {
    _ctx.uartNb = COMM_UART_NB;
//    _ctx.isOpen = hal_bsp_uart_init(_ctx.uartNb, COMM_UART_BAUDRATE, uart_event_handler);

    // TODO need way to decide if io input to decide if uart is to be enabled (revE board only)
    // setup input on pin X, used to signal remote end wants to talk or not
    uart_gpio_init();
    // Check if uart is initialled running or not
    uart_gpio_handler(0,0);     // as though had a vcc_uart state change
    return _ctx.isOpen;
}

void comm_uart_deinit(void) {
    _ctx.isOpen = false;
    hal_bsp_uart_deinit(_ctx.uartNb);
}
int comm_uart_tx(uint8_t* data, int len, UART_TX_READY_FN_T tx_ready) {
    if (_ctx.isOpen) {
        _ctx.tx_ready_fn = tx_ready;        // in case of..
        // check if disconnecting 
        if (data!=NULL)  {
            _ctx.txL++;
            return (hal_bsp_uart_tx(_ctx.uartNb, data, len));
        } else {
            // can't disconnect from uart... but let them know on the other end
            return (hal_bsp_uart_tx(_ctx.uartNb, (uint8_t*)"AT+DISC\r\n", 10));
        }
        return 0;       // ok mate
    } else {
        // soz
        return -1;
    }
}

// Call this from main loop to check if uart has input data to process
void comm_uart_processRX() {
    if (_ctx.isOpen) {
        // Read all the bytes we can and pass any complete lines to the at command processor
        while(app_uart_get(&_ctx.rx_buf[_ctx.rx_index])==NRF_SUCCESS) 
        {
            // Don't take nulls
            if (_ctx.rx_buf[_ctx.rx_index]!=0) {
                _ctx.rxC++;
                // End of line? or buiffer full?
                if( (_ctx.rx_buf[_ctx.rx_index] == '\r') || (_ctx.rx_buf[_ctx.rx_index] == '\n') || (_ctx.rx_index >= (MAX_RX_LINE-2)) )
                {
                    // Don't process empty lines (eg the \n from people who do "<blah>\r\n" -> "<blah>\n","\n" after processing) 
                    if (_ctx.rx_index>0) {
                        _ctx.rx_buf[_ctx.rx_index] = '\n';  // make sure its got a LF on end
                        _ctx.rx_index++;
                        _ctx.rx_buf[_ctx.rx_index] = 0; // null terminate the data in buffer (not overwriting the \r or \n though)
                        // And process
                        at_process_input((char*)(&_ctx.rx_buf[0]), &comm_uart_tx);
                        _ctx.rxL++;
                    }
                    // reset our line buffer to start
                    _ctx.rx_index = 0;
                    // Continue for rest of data in input
                } else {
                    _ctx.rx_index++;
                }
            }
        }
        _ctx.rxDataReady = false;       // as we ate all the data
    }
}

void comm_uart_print_stats(PRINTF_FN_T printf, void* odev) {
    (*printf)(odev, "U:%d,%d,-,%d,%d,%d", _ctx.rxC, _ctx.rxL, _ctx.txL, _ctx.rxFerr, _ctx.rxLerr);
}


// handler for reinit timer
static void reinit_uart_timeout_handler(void * p_context)
{
    if (_ctx.isOpen==false) {
        comm_uart_init(false);
    } // else ignore
}

// Timer to try a reinit in case UART is playing up...
static void start_reopen_timer() {
    /*
    app_timer_create(&_ctx->m_reinit_uart_delay,
                        APP_TIMER_MODE_SINGLE_SHOT,
                        reinit_uart_timeout_handler);    
        uint32_t reinit_delay = APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER);//32 * 10000;          
        app_timer_start(_ctx->m_reinit_uart_delay, reinit_delay, NULL);
        */
}

/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be prcoessed when the last character received was a
 *          'new line' i.e '\n' (hex 0x0A) or carriage return (\r 0x0D) or if the string has reached the end of the rx buffer
 */
static void uart_event_handler(app_uart_evt_t * p_event)
{    
    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
        {
            _ctx.rxDataReady = true;
            break;
        }
        break;
 
        case APP_UART_COMMUNICATION_ERROR:
            // This is for errors such as parity, framing, overrun or break (all 0 on the line)
            // None of these are fatal so we can ignore them
//            APP_ERROR_HANDLER(p_event->data.error_communication);
            // but we will flush input buffer / input fifo
            _ctx.rxLerr++;
            _ctx.rx_index = 0;
            app_uart_flush();
        break;
 
        case APP_UART_FIFO_ERROR:
                // Information only, we'll cope!
//            comm_uart_deinit();
//            start_reopen_timer();
//            APP_ERROR_HANDLER(p_event->data.error_code);
            _ctx.rxFerr++;
        break;
        
        case APP_UART_TX_EMPTY:
            // if tx had been full, can tell the user they can restart...
            if (_ctx.tx_ready_fn!=NULL) {
                (*_ctx.tx_ready_fn)(&comm_uart_tx);
            }
            break;
 
        default:
        break;
    }
}

static bool uart_gpio_init() {
    uint32_t err_code;
    nrf_drv_gpiote_out_config_t led1_config = GPIOTE_CONFIG_OUT_SIMPLE(false);
    err_code = nrf_drv_gpiote_out_init(BSP_LED_0, &led1_config);
    if (err_code!=NRF_SUCCESS) {
        return false;
    }
    nrf_drv_gpiote_out_config_t led2_config = GPIOTE_CONFIG_OUT_SIMPLE(false);
    err_code = nrf_drv_gpiote_out_init(BSP_LED_1, &led2_config);
    if (err_code!=NRF_SUCCESS) {
        return false;
    }
    // Only define uart control io pin if on a card supporting it
    if (cfg_getCardType()>=5 && cfg_getCardType()<10) {
        nrf_drv_gpiote_in_config_t vccuart_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
        vccuart_config.pull = NRF_GPIO_PIN_PULLDOWN;
        err_code = nrf_drv_gpiote_in_init(BSP_VCCUART, &vccuart_config, uart_gpio_handler);
        if (err_code!=NRF_SUCCESS) {
            return false;
        }
        // Must enable 'event' for input io to be seen
        nrf_drv_gpiote_in_event_enable(BSP_VCCUART, true);
    }
    return true;
}

// Handle level change on the input IO used to enable/disable uart
static void uart_gpio_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    // Get current state of enable pin if on a card that supports this (revE fille and onwards)
    if (cfg_getCardType()>=5 && cfg_getCardType()<10) {
        bool state = nrf_drv_gpiote_in_is_set(BSP_VCCUART);
        if (state==false) {
            _ctx.isOpen = false;
            hal_bsp_uart_deinit(_ctx.uartNb);
            nrf_drv_gpiote_out_clear(BSP_LED_0);
        } else {
            _ctx.isOpen = hal_bsp_uart_init(_ctx.uartNb, COMM_UART_BAUDRATE, uart_event_handler);
            nrf_drv_gpiote_out_set(BSP_LED_0);
        }
    } else {
        _ctx.isOpen = hal_bsp_uart_init(_ctx.uartNb, COMM_UART_BAUDRATE, uart_event_handler);
    }
}

/* to get libc to be ok */
static size_t
stdin_read(FILE *fp, char *bp, size_t n)
{
    return 0;
}

static size_t
stdout_write(FILE *fp, const char *bp, size_t n)
{
    comm_uart_tx((uint8_t*)bp, n,NULL);
    return n;
}

static struct File_methods _stdin_methods = {
    .write = stdout_write,
    .read = stdin_read
};

static struct File _stdin = {
    .vmt = &_stdin_methods
};

struct File *const stdin = &_stdin;
struct File *const stdout = &_stdin;
struct File *const stderr = &_stdin;

