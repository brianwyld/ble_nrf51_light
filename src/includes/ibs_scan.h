#ifndef IBS_SCAN_H__
#define IBS_SCAN_H__

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

#define IBS_SCAN_LIST_LENGTH 120

// TODO : given we use this table only to avoid sending duplicates, no point in keeping rssi in it? unless we're tracking rssi and resending if it changes?
// Also the structure is now 5 bytes, which the compilier is gonna pad to 8 bytes I bet...
typedef struct {
	uint16_t major;
	uint16_t minor;
	uint8_t rssi;
} ibs_scan_result_t;


bool ibs_is_scan_active();
bool ibs_scan_start(UART_TX_FN_T dest_tx_fn);
bool ibs_scan_restart(void);
bool ibs_scan_stop(void);
void ibs_scan_set_uuid_filter(uint8_t* uuid);
void ibs_handle_advert(ble_gap_evt_adv_report_t *p_adv_report);
int ibs_scan_getTableSize();
#endif
