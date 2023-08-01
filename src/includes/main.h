#ifndef H_MAIN_H
#define H_MAIN_H


/*********************************INCLUDE**************************************/

#include <inttypes.h>
#include <stdint.h>

/*******************************STRUCT & ENUM**********************************/
#define UUID16_SIZE             2                               /**< Size of 16 bit UUID */
#define UUID32_SIZE             4                               /**< Size of 32 bit UUID */
#define UUID128_SIZE            16                              /**< Size of 128 bit UUID */

#define APP_TIMER_PRESCALER             0                                           /**< Value of the RTC1 PRESCALER register. */ 
 

// Callback when output sink can take tx again (if flow controlled)
typedef int (*UART_TX_READY_FN_T)(void* txfn);
// Output line with N characters (may be null terminated or not)
typedef int (*UART_TX_FN_T)(uint8_t* output, int len, UART_TX_READY_FN_T txready);
// A printf type fn that takes the uart tx fn to print to.
typedef bool (*PRINTF_FN_T)(void* dev, const char* l, ...);

/******************************GLOBAL FUNCTIONS********************************/

void app_reset_request();
void app_setFlashBusy();
bool app_isFlashBusy();

bool ibb_isBeaconning();
void ibb_start();
void ibb_stop();

//EOF
#endif  /* H_MAIN_H */