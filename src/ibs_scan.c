#define IBS_SCAN_C__
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "ble_gap.h"
#include "ble_advdata.h"
#include "app_error.h"
#include "bsp.h"
#include "app_uart.h"

#include "wutils.h"

#include "main.h"
#include "ibs_scan.h"


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

static struct {
    uint8_t ibs_scan_filter_uuid[UUID128_SIZE];
    ibs_scan_result_t ibs_scan_results[IBS_SCAN_LIST_LENGTH];
    int ibs_scan_result_index;
    bool ibs_scan_active;
    bool ibs_scan_filter_uuid_active;
    UART_TX_FN_T output_tx_fn;          // Where to write current scan results to
} _ctx;



// Predecs
static bool ibs_is_adv_pkt(uint8_t *data);
static void ibs_scan_add(uint8_t* remoteaddr, uint8_t* data2, int8_t rssi);
static bool ibs_scan_uuid_match(uint8_t* uuid);

/**@brief Function to start scanning.
 */
bool ibs_scan_start(UART_TX_FN_T dest_tx_fn) 
{
    if (_ctx.ibs_scan_active) {
        return false;
    }
    // Flush old table
    _ctx.ibs_scan_result_index = 0;
    // Record where the output is to go to
    _ctx.output_tx_fn = dest_tx_fn;
    return ibs_scan_restart();
}
bool ibs_is_scan_active() 
{
    return _ctx.ibs_scan_active;
}

// (Re)start scan at beginnning or if stopped by timeout or error. Does not reset the table of previously seen beacons
bool ibs_scan_restart()
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
        _ctx.ibs_scan_active = true;
        return true;
    }
    else
    {
        return false;
    }
}


bool ibs_scan_stop()
{
    uint32_t err_code;
    
    if (!_ctx.ibs_scan_active)
    {
        return false;
    }
    
    err_code = sd_ble_gap_scan_stop();
    
    if (err_code == NRF_SUCCESS)
    {
        _ctx.ibs_scan_active = false;
        return true;
    }
    else
    {
        return false;
    }
}

int ibs_scan_getTableSize() {
    return _ctx.ibs_scan_result_index;
}

void ibs_handle_advert(ble_gap_evt_adv_report_t * p_adv_report) 
{
    if (!_ctx.ibs_scan_active)
    {
        return;
    }
    if (ibs_is_adv_pkt(p_adv_report->data))
    {
        if (ibs_scan_uuid_match(&p_adv_report->data[IBS_IBEACON_UUID_OFFSET]))
        {
            ibs_scan_add(p_adv_report->peer_addr.addr,p_adv_report->data, p_adv_report->rssi);
        }
    }

}

void ibs_scan_set_uuid_filter(uint8_t* uuid)
{
    if (uuid == NULL)
    {
        _ctx.ibs_scan_filter_uuid_active = false;
    }
    else
    {
        memcpy(_ctx.ibs_scan_filter_uuid, uuid, IBS_IBEACON_UUID_LENGTH);
        _ctx.ibs_scan_filter_uuid_active = true;
    }
}



static bool ibs_scan_uuid_match(uint8_t* uuid)
{
    if (_ctx.ibs_scan_filter_uuid_active)
    {
        return (memcmp(uuid, _ctx.ibs_scan_filter_uuid, IBS_IBEACON_UUID_LENGTH) == 0);
    }
    else
    {
        return true;
    }
}

static void ibs_scan_add(uint8_t* remoteaddr, uint8_t* data, int8_t rssi)
{    
    // copy major, minor, measpow
    if (_ctx.ibs_scan_result_index >= IBS_SCAN_LIST_LENGTH)
    {
        return;     // list is full..
    }
    // Major and minor are BE format
    uint16_t major = Util_readBE_uint16_t(&data[IBS_IBEACON_MAJOR_OFFSET],2);
    uint16_t minor = Util_readBE_uint16_t(&data[IBS_IBEACON_MINOR_OFFSET],2);    
    // Check if same major/minor/uuid not in the list already
    for (int i = 0; i < _ctx.ibs_scan_result_index; i++)
    {
        if (_ctx.ibs_scan_results[i].major==major && _ctx.ibs_scan_results[i].minor==minor)
        {
            // Note we don't keep the UUID in the table to save space -> issue if ibeacons with same maj/min and not using uuid filter...
            // update RSSI
            // If rssi changes 'significantly' from the first time, then resend on uart?
            _ctx.ibs_scan_results[i].rssi = rssi;
            return;
        }
    }
    ibs_scan_result_t* ib = &_ctx.ibs_scan_results[_ctx.ibs_scan_result_index];
    // Store info in new slot
    ib->major = major;
    ib->minor = minor;
    ib->rssi = rssi;
    
    uint8_t meas_pow = data[IBS_IBEACON_MEAS_POWER_OFFSET];
    // Create output line (all values in hex) : MAJHEX,MINHEX,XTRA,RSSI,remote device address
    char line[32] = {0};
    sprintf(line, "%04x,%04x,%2x,%2x,%02x%02x%02x%02x%02x%02x\r\n",
                ib->major, ib->minor,
                meas_pow, ib->rssi,
                remoteaddr[0],remoteaddr[1],remoteaddr[2],remoteaddr[3],remoteaddr[4],remoteaddr[5]);
    // And send to our preferred serial output
    if ((*_ctx.output_tx_fn)((uint8_t*)line, strlen(line), NULL)==0) {
        _ctx.ibs_scan_result_index++;
    } else {
        // Failed to send line (fifo probably full)
        // Deal with it by NOT adding this guy to list (don't inc index), and hopefully we'll get him on his next advert
    }
}


static bool ibs_is_adv_pkt(uint8_t *data) {
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
