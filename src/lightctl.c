#define LIGHTCTL_C__
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "ble_gap.h"
#include "ble_advdata.h"
#include "app_error.h"
#include "leds.h"
#include "app_uart.h"
#include "app_timer.h"
#include "nrf_delay.h"

#include "wutils.h"
#include "device_config.h"

#include "main.h"
#include "lightctl.h"
#include "boards.h"

/** Scan for ibeacons or findme beacons, and light up the LEDs according to what we see and the mode selected */

// IBEACON Structure field offsets
// Note that IBEACON advertising frame is completely fixed in format even if it is a standard advert TLV...
#define IBS_IBEACON_HEADER_LENGTH 9
#define IBS_IBEACON_UUID_LENGTH (UUID128_SIZE)
#define IBS_IBEACON_MAJOR_LENGTH 2
#define IBS_IBEACON_MINOR_LENGTH 2
#define IBS_IBEACON_MEAS_POWER_LENGTH 1

#define IBS_IBEACON_UUID_OFFSET (IBS_IBEACON_HEADER_LENGTH)
#define IBS_IBEACON_MAJOR_OFFSET (IBS_IBEACON_UUID_OFFSET+IBS_IBEACON_UUID_LENGTH)
#define IBS_IBEACON_MINOR_OFFSET (IBS_IBEACON_MAJOR_OFFSET+IBS_IBEACON_MAJOR_LENGTH)
#define IBS_IBEACON_MEAS_POWER_OFFSET (IBS_IBEACON_MINOR_OFFSET+IBS_IBEACON_MINOR_LENGTH)

/**
 * @brief Parameters used when scanning.
 */
/*
static const ble_gap_scan_params_t m_scan_params =
{
    .active   = SCAN_ACTIVE,
    .interval = SCAN_INTERVAL,
    .window   = SCAN_WINDOW,
    .timeout  = SCAN_TIMEOUT,
    #if (NRF_SD_BLE_API_VERSION == 2)
        .selective   = 0,
        .p_whitelist = NULL,
    #endif
    #if (NRF_SD_BLE_API_VERSION == 3)
        .use_whitelist = 0,
    #endif
};
*/
#define SCAN_INTERVAL           0x00BA                          // < Determines scan interval in units of 0.625 millisecond. 0x00A0
#define SCAN_WINDOW             0x00B0                          // < Determines scan window in units of 0.625 millisecond. 0x0050
#define SCAN_ACTIVE             1                               // If 1, performe active scanning (scan requests).
#define SCAN_TIMEOUT            0x0000                          // < Timout when scanning. 0x0000 disables timeout.
#define SCAN_SELECTIVE          0                               // < If 1, ignore unknown devices (non whitelisted).

#define RSSI_HIST_SZ (3)
#define MIN_RSSI (-127)

static struct {
    int seen_cnt;    // +1 when I see match, -1 if I dont
    int8_t rssi_hist[RSSI_HIST_SZ];
    uint8_t rssi_hist_idx;
    bool scan_active;
    bool findme_seen;
} _ctx;

APP_TIMER_DEF(m_timer);

// Predecs
static bool _is_adv_pkt(uint8_t *data);
static void _add_ib(uint8_t* remoteaddr, uint8_t* data2, int8_t rssi);
static bool _is_findme_pkt(uint8_t *data);
static void _add_fm(uint8_t* remoteaddr, uint8_t* data2, int8_t rssi);
static void _set_leds();
static void _light_timer_cb(void* ctx);

void lightctl_init() {
    uint32_t err_code = app_timer_create(&m_timer,
                    APP_TIMER_MODE_REPEATED,
                    _light_timer_cb);
    APP_ERROR_CHECK(err_code); 
    // init leds
    uint8_t leds[] = LEDS_LIST;
    leds_init(leds, LEDS_NUMBER);
    // flash all leds for 2 secs
    for(int i=0;i<10;i++) {
        led_on(LED_1);
        led_on(LED_2);
        led_on(LED_3);
        nrf_delay_ms(200);
        led_off(LED_1);
        led_off(LED_2);
        led_off(LED_3);
        nrf_delay_ms(200);
    }
}

/**@brief Function to start scanning.
 */
bool lightctl_start() 
{
    if (_ctx.scan_active) {
        return true;    // already scanning
    }
    uint32_t delay = APP_TIMER_TICKS(3000, APP_TIMER_PRESCALER);        
    uint32_t err_code = app_timer_start(m_timer, delay, NULL);
    APP_ERROR_CHECK(err_code);

    // Flush old results
    _ctx.seen_cnt = 0;
    _ctx.findme_seen = false;
    return lightctl_restart();
}
bool lightctl_is_active() 
{
    return _ctx.scan_active;
}

// (Re)start scan at beginnning or if stopped by timeout or error. Does not reset the table of previously seen beacons
bool lightctl_restart()
{
    uint32_t err_code;
    ble_gap_scan_params_t m_scan_params;
    
    m_scan_params.interval = SCAN_INTERVAL;
    m_scan_params.window   = SCAN_WINDOW;
    m_scan_params.active = SCAN_ACTIVE;
    m_scan_params.timeout  = SCAN_TIMEOUT;
    m_scan_params.selective   = SCAN_SELECTIVE;
    m_scan_params.p_whitelist = NULL;

    err_code = sd_ble_gap_scan_start(&m_scan_params);
    
    if (err_code == NRF_SUCCESS)
    {
        _ctx.scan_active = true;
        log_info("lightctl scan started ok");
        return true;
    }
    else
    {
        _ctx.scan_active = false;
        log_info("lightctl scan failed to start %d",err_code);
        return false;
    }
}


bool lightctl_stop()
{
    uint32_t err_code;
    
    if (!_ctx.scan_active)
    {
        return false;
    }
    app_timer_stop(m_timer);

    err_code = sd_ble_gap_scan_stop();
    
    if (err_code == NRF_SUCCESS)
    {
        _ctx.scan_active = false;
        return true;
    }
    else
    {
        return false;
    }
}

void lightctl_handle_advert(ble_gap_evt_adv_report_t * p_adv_report) 
{
    if (!_ctx.scan_active)
    {
        return;
    }
    // Always checks for both ibeacons and findme beacons (findmes should be rare, and change leds to flashing)
    if (_is_adv_pkt(p_adv_report->data)) {
        _add_ib(p_adv_report->peer_addr.addr,p_adv_report->data, p_adv_report->rssi);
    } else if (_is_findme_pkt(p_adv_report->data)) {
        _add_fm(p_adv_report->peer_addr.addr,p_adv_report->data, p_adv_report->rssi);
    }

}

static void _add_ib(uint8_t* remoteaddr, uint8_t* data, int8_t rssi)
{    
    // check major, minor
    // Major and minor are BE format
    uint16_t major = Util_readBE_uint16_t(&data[IBS_IBEACON_MAJOR_OFFSET],2);
    uint16_t minor = Util_readBE_uint16_t(&data[IBS_IBEACON_MINOR_OFFSET],2);    
    if ((cfg_getLEDMajor()==LED_MM_ANY || cfg_getLEDMajor()==major)  &&
        (cfg_getLEDMinor()==LED_MM_ANY || cfg_getLEDMinor()==minor)) {
        // match
        _ctx.seen_cnt++;
        _ctx.rssi_hist[_ctx.rssi_hist_idx] = rssi;
        _ctx.rssi_hist_idx = (_ctx.rssi_hist_idx+1)%RSSI_HIST_SZ;

        // set leds appropriately
        _set_leds();

        // logging    
        uint8_t meas_pow = data[IBS_IBEACON_MEAS_POWER_OFFSET];
        // Create output line (all values in hex) : MAJHEX,MINHEX,XTRA,RSSI,remote device address
        log_info("%04x,%04x,%2x,%d,%02x%02x%02x%02x%02x%02x\r\n",
                    major, minor,
                    meas_pow, rssi,
                    remoteaddr[0],remoteaddr[1],remoteaddr[2],remoteaddr[3],remoteaddr[4],remoteaddr[5]);
    }
}

static bool _is_adv_pkt(uint8_t *data) {
	bool result = (data[0] == 0x02) && // 1st AD data length
				(data[1] == 0x01) && // Flags
				(data[2] == 0x06) && // flag = 0x06
				(data[3] == 0x1A) && // 2nd AD data length
				(data[4] == 0xff) && // Type : Manufacturer Specific Data
				(data[5] == 0x4C) && // Apple Company iD 0
				(data[6] == 0x00) && // Apple Company iD 1
				(data[7] == 0x02) && // Proximity Beacon Type 0
				(data[8] == 0x15); // data length 21

	return result;
}

static void _add_fm(uint8_t* remoteaddr, uint8_t* data, int8_t rssi) {    
    // all findmes count
    _ctx.seen_cnt++;
    _ctx.rssi_hist[_ctx.rssi_hist_idx] = rssi;
    _ctx.rssi_hist_idx = (_ctx.rssi_hist_idx+1)%RSSI_HIST_SZ;

    // Leds flash when its a find me case
    _ctx.findme_seen = true;
    // set leds appropriately
    _set_leds();

    // logging    
    // Create output line (all values in hex) : RSSI,first 6 bytes of remote device public key
    log_info("%d,%02x%02x%02x%02x%02x%02x\r\n",
                rssi,
                remoteaddr[0],remoteaddr[1],remoteaddr[2],remoteaddr[3],remoteaddr[4],remoteaddr[5]);
}

/* find me packet. Note that the peer address is actually part of the public key being advertised!
    Bytes Content (details cf. [6, § 5.1])
    0–5 BLE address ((pi[0] | (0b11  6)) || pi[1..5])
    6 Payload length in bytes (30)
    7 Advertisement type (0xFF for manufacturer-specific data)
    8–9 Company ID (0x004C)
    10 OF type (0x12)
    11 OF data length in bytes (25)
    12 Status (e.g., battery level)
    13–34 Public key bytes pi[6..27]
    35 Public key bits pi[0]  6
    36 Hint (0x00 on iOS reports)
*/
static bool _is_findme_pkt(uint8_t *data) {
    // Data is advert data ie after the peer address
	bool result = (data[0] == 0x1E) && // 1st AD data length = 30
				(data[1] == 0xff) && // Type : Manufacturer Specific Data
				(data[2] == 0x4C) && // Apple Company iD 0
				(data[3] == 0x00) && // Apple Company iD 1
				(data[4] == 0x0C) && // OF type 12
				(data[5] == 0x19); // data length 25

	return result;
}

static void _led_ctl(int led_io, bool ison, bool flash) {
    if (ison) {
        if (flash) {
            led_invert(led_io);
        } else {
            led_on(led_io);
        }
    } else {
        led_off(led_io);
    }
}

static void _set_leds() {
    bool led1 = false;
    bool led2 = false;
    bool led3 = false;
    int av_rssi = MIN_RSSI;
    if (_ctx.seen_cnt>0) {
        // sum rssis and divide by n entries to get average
        int sum_rssi = 0;
        for (int i=0;i<RSSI_HIST_SZ;i++) {
            sum_rssi += _ctx.rssi_hist[i];
        }
        av_rssi = sum_rssi/RSSI_HIST_SZ;
        if (av_rssi>-100) {
            led1 = true;
        }
        if (av_rssi>-80) {
            led2 = true;
        }
        if (av_rssi>-60) {
            led3 = true;
        }
    }
    _led_ctl(LED_1, led1, _ctx.findme_seen);
    _led_ctl(LED_2, led2, _ctx.findme_seen);
    _led_ctl(LED_3, led3, _ctx.findme_seen);
    log_info("set_leds:av rssi %d", av_rssi);
}

static void _light_timer_cb(void* ctx) {
    // If count not incremented since last time, leds off
    if (_ctx.seen_cnt<=0) {
        log_info("lightctl timer : no device seen, turning off leds");
        // all rssis set to -255 (which is leds off)
        for (int i=0;i<RSSI_HIST_SZ;i++) {
            _ctx.rssi_hist[i] = MIN_RSSI;
        }
        //update leds
        _set_leds();
    } else {
        log_info("lightctl timer : device seen %d times in last period", _ctx.seen_cnt);
    }
    _ctx.seen_cnt = 0;
    // and reset findme flag also
    _ctx.findme_seen = false;

}