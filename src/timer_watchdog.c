#include "app_util_platform.h"
#include "nrf_drv_clock.h"
#include "sdk_config.h"
#include "timer_watchdog.h"

#if 0
static uint16_t limit_counter = 0;
#define MAX_LIMIT       2000

//Configure and start timer
void WATCHDOG_TimerConfig( void )
{
    NRF_TIMER2->MODE = TIMER_MODE_MODE_Timer;  // Set the timer in Counter Mode
    NRF_TIMER2->TASKS_CLEAR = 1;               // clear the task first to be usable for later
    NRF_TIMER2->PRESCALER = 9;                          //Set prescaler. Higher number gives slower timer. Prescaler = 0 gives 16MHz timer
    NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_32Bit;//TIMER_BITMODE_BITMODE_16Bit;  //Set counter to 16 bit resolution
    NRF_TIMER2->CC[0] = (31250*10000);                         //Set value for TIMER2 compare register 0
    NRF_TIMER2->CC[1] = 1;                               //Set value for TIMER2 compare register 1

    // Enable interrupt on Timer 2, both for CC[0] and CC[1] compare match events
    NRF_TIMER2->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos) | (TIMER_INTENSET_COMPARE1_Enabled << TIMER_INTENSET_COMPARE1_Pos);
    NVIC_EnableIRQ(TIMER2_IRQn);
}

void WATCHDOG_TimerStart( void )
{
    NRF_TIMER2->TASKS_START = 1; // Start TIMER2
}

void WATCHDOG_TimerStop( void )
{
    NRF_TIMER2->TASKS_STOP = 1; // Stop TIMER2
}

/** TIMTER2 peripheral interrupt handler. This interrupt handler is called whenever there it a TIMER2 interrupt
 */
void TIMER2_IRQHandler(void)
{
    if ((NRF_TIMER2->EVENTS_COMPARE[0] != 0) && ((NRF_TIMER2->INTENSET & TIMER_INTENSET_COMPARE0_Msk) != 0))
    {
        NRF_TIMER2->EVENTS_COMPARE[0] = 0;           //Clear compare register 0 event
        
        limit_counter++;
        
        if(limit_counter >= MAX_LIMIT)
        {
            limit_counter = 0;
            NVIC_SystemReset();
        }
        
    }
}
#endif 
