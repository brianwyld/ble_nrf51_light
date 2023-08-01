#ifndef LIGHTCTL_H__
#define LIGHTCTL_H__

void lightctl_init();
bool lightctl_is_active();
bool lightctl_start();
bool lightctl_restart(void);
bool lightctl_stop(void);
void lightctl_handle_advert(ble_gap_evt_adv_report_t * p_adv_report);
#endif
