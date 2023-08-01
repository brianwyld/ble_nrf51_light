/*
 * Copyright (c) 2016 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is confidential property of Nordic Semiconductor. The use,
 * copying, transfer or disclosure of such information is prohibited except by express written
 * agreement with Nordic Semiconductor.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "app_timer.h"
#include "app_error.h"
#include "app_uart.h"
#include "app_button.h"
#include "app_util.h"
#include "app_util_platform.h"
#include "bsp.h"
#include "boards.h"
#include "bsp_config.h"
#include "ble.h"
#include "ble_conn_params.h"
#include "ble_db_discovery.h"
#include "bsp_btn_ble.h"
#include "ble_gap.h"
#include "ble_nus.h"
#include "ble_hci.h"
#include "ble_gatts.h"
#include "softdevice_handler.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"

#include "timer_watchdog.h"

#include "wutils.h"
#include "main.h"
#include "bsp_minew_nrf51.h"
#include "comm_uart.h"
#include "comm_ble.h"
#include "device_config.h"

#include "ibs_scan.h"
#include "lightctl.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT 0   /**< Include the service_changed characteristic. If not enabled, the server's database cannot be changed for the lifetime of the device. */

#define CENTRAL_LINK_COUNT              1   /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1   /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#if (NRF_SD_BLE_API_VERSION == 3)
#define NRF_BLE_MAX_MTU_SIZE    GATT_MTU_SIZE_DEFAULT           /**< MTU size used in the softdevice enabling and to reply to a BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST event. */
#endif

#define APP_FEATURE_NOT_SUPPORTED       BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2        /**< Reply when unsupported features are requested. */
 
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */
 
#define APP_ADV_INTERVAL_MS_SLOW        1000                                          /**< The advertising interval when not actively beaconing (in units of ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS       60                                         /**< The advertising timeout (in units of seconds). */
 
#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */
 
#define DEAD_BEEF 0xDEADBEEF                                    /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define NUS_SERVICE_UUID_TYPE   BLE_UUID_TYPE_VENDOR_BEGIN      /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_TIMER_OP_QUEUE_SIZE 2                               /**< Size of timer operation queues. */

#define MIN_CONNECTION_INTERVAL MSEC_TO_UNITS(20, UNIT_1_25_MS) /**< Determines minimum connection interval in millisecond. */
#define MAX_CONNECTION_INTERVAL MSEC_TO_UNITS(75, UNIT_1_25_MS) /**< Determines maximum connection interval in millisecond. */
#define SLAVE_LATENCY           0                               /**< Determines slave latency in counts of connection events. */
#define SUPERVISION_TIMEOUT     MSEC_TO_UNITS(4000, UNIT_10_MS) /**< Determines supervision time-out in units of 10 millisecond. */
 
//WBeacon
#define APP_BEACON_INFO_LENGTH          0x17                                    /**< Total length of information advertised by the Beacon. */
#define APP_CFG_NON_CONN_ADV_TIMEOUT    0                                   /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_BEACON_ADV_DATA_LENGTH  (0x15)
#define APP_BEACON_ADV_DEVICE_TYPE (0x02)

static struct app_context {
    //UART Variables
    uint16_t                m_conn_handle;    /**< Handle of the current GAP connection. */
    // TODO we don't need to have a gatt server db unless acting as the central to talk to periphs? Takes up a lot of memory
//    ble_db_discovery_t      m_ble_db_discovery;             /**< Instance of database discovery module. Must be passed to all db_discovert API calls */
    // config
    uint8_t m_beacon_info[APP_BEACON_INFO_LENGTH]; 
    bool flashBusy;
    bool resetRequested;
} _ctx = {
    .m_conn_handle = BLE_CONN_HANDLE_INVALID, 
};

static void sys_evt_handler(uint32_t evt);
static bool advertising_check_start();
static uint8_t makeAdvPowerLevel( void );

//APP_TIMER_DEF(m_deinit_uart_delay); 

/**@brief Function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num     Line number of the failing ASSERT call.
 * @param[in] p_file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

 
/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);
 
    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);
 
    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}
 

/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;
    log_info("evt:conn params evt %d", p_evt->evt_type);

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(_ctx.m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
        // We will get the BLE_GAP_EVT_DISCONNECTED event to restart advertising etc
    }
}

/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

 
/**@brief Function for handling events linked to our advertising.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;
    log_info("evt:advertsing evt %d", ble_adv_evt);

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            // use leds if present to indicate state
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
        break;
        case BLE_ADV_EVT_SLOW:
            // use leds if present to indicate state
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_SLOW);
            APP_ERROR_CHECK(err_code);
            // if not being an ibeacon we are sending just NUS adverts
            break;
        case BLE_ADV_EVT_IDLE:
            // use leds if present to indicate state
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
        // BW : no sleeping on this job TODO check
//            sleep_mode_enter();
        break;
        default:
        break;
    }
}
  

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack event has
 *          been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    // First dispatch to other services to see if they need to process it
    // Dispatch to ble stack services if we want to be connected
    ble_conn_params_on_ble_evt(p_ble_evt);
    comm_ble_dispatch(p_ble_evt);           // For NUS service (we are slave)
//        comm_ble_c_dispatch(p_ble_evt);       // When discovering peers for NUS client connections

//    bsp_btn_ble_on_ble_evt(p_ble_evt);
    // If we are scanning then make db of remote guys be up to date
//    ble_db_discovery_on_ble_evt(&_ctx.m_ble_db_discovery, p_ble_evt);

    // Now our own specific processing
    uint32_t err_code;
    ble_gap_evt_t * p_gap_evt = &p_ble_evt->evt.gap_evt;
 
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            log_info("evt:gap connect");
            if (cfg_getConnectable()) {
                err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
                APP_ERROR_CHECK(err_code);
                // Stop advertising
                sd_ble_gap_adv_stop();
                _ctx.m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
                // password check has not been validated for this connection
                cfg_resetPasswordOk();
            } else {
                // Not allowed to connect
                err_code = sd_ble_gap_disconnect(_ctx.m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
                APP_ERROR_CHECK(err_code);
            }
        break; // BLE_GAP_EVT_CONNECTED
 
        case BLE_GAP_EVT_DISCONNECTED:
            log_info("evt:gap disconnect");
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            _ctx.m_conn_handle = BLE_CONN_HANDLE_INVALID;
            // Restart advertising if required
            advertising_check_start();
        break; // BLE_GAP_EVT_DISCONNECTED
        
        case BLE_GAP_EVT_ADV_REPORT: {
            //log_info("evt:adv rx");

            // Reception of an advert - could be an ibeacon
            ble_gap_evt_adv_report_t *p_adv_report = &p_gap_evt->params.adv_report;
            ibs_handle_advert(p_adv_report);
            lightctl_handle_advert(p_adv_report);
        break; // BLE_GAP_EVT_ADV_REPORT
        }
        case BLE_GAP_EVT_TIMEOUT:
            log_info("evt:gap timeout");

            advertising_check_start();
            
            if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN)
            {
                // If scan still active, restart it
                if (ibs_is_scan_active())
                {
                    ibs_scan_restart();
                }
                // If lightctl scanning still active, restart it
                if (lightctl_is_active())
                {
                    lightctl_restart();
                }
            }
        break;
            
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(_ctx.m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
        break; // BLE_GAP_EVT_SEC_PARAMS_REQUEST
 
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(_ctx.m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
        break; // BLE_GATTS_EVT_SYS_ATTR_MISSING
 
        case BLE_GATTC_EVT_TIMEOUT:
            log_info("evt:gattc timeout");
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            // And we get a EVT_DISCONNECTED when its done
        break; // BLE_GATTC_EVT_TIMEOUT
 
        case BLE_GATTS_EVT_TIMEOUT:
            log_info("evt:gatts timeout");
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            // And we get a EVT_DISCONNECTED when its done
            APP_ERROR_CHECK(err_code);
        break; // BLE_GATTS_EVT_TIMEOUT
 
        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
        break; // BLE_EVT_USER_MEM_REQUEST
 
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            ble_gatts_evt_rw_authorize_request_t req;
            ble_gatts_rw_authorize_reply_params_t auth_reply;
 
            req = p_ble_evt->evt.gatts_evt.params.authorize_request;
 
            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
 
                {
                    if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
 
 
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    }
                    else
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                    }
                    auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                    err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                               &auth_reply);
                    APP_ERROR_CHECK(err_code);
                }
            }
        }
        break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST
 
#if (NRF_SD_BLE_API_VERSION == 3)
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                       NRF_BLE_MAX_MTU_SIZE);
            APP_ERROR_CHECK(err_code);
        break; // BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST
#endif
 
        default:
            // No implementation needed.
        break;
    }
}

/**@brief Function for handling events from the BSP module.
 *
 * @param[in] event  Event generated by bsp
 */
/*
static void bsp_event_handler(bsp_event_t event)
{
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
        break;

        case BSP_EVENT_DISCONNECT:
            comm_ble_disconnect();
            advertising_check_start();
        break;
        case BSP_EVENT_WHITELIST_OFF:
            advertising_check_start();
        break;
            
        default:
        break;
    }
}
*/

// Callback when system events happen
static void sys_evt_handler(uint32_t evt)
{
    switch(evt)
    {
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
            _ctx.flashBusy = false;
        break;
        
        case NRF_EVT_FLASH_OPERATION_ERROR:
            _ctx.flashBusy = false;         // Still finished though
        break;
        
        default:
        break;
    }
}


/**@brief Function for initializing the Connection Parameters module.
 * one time boot init
 */
static void conn_params_init(void)
{
    uint32_t err_code;
    ble_conn_params_init_t cp_init;
 
    memset(&cp_init, 0, sizeof(cp_init));
 
    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;
 
    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);

}

static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;
        
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
 
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));
 
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
 
    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing services that will be used by the application.
 * One time boot init
 */
static void services_init(void)
{
    comm_ble_init();
}
 
/**@brief Function for initializing the BLE stack.
 * One time init at boot
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);
    // We have almost no GATTs
//    ble_enable_params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_MIN;
//    ble_enable_params.common_enable_params.vs_uuid_count = 1;
    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,
                                PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
#if (NRF_SD_BLE_API_VERSION == 3)
    ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);


}

/* One time boot init of ble stack */
static void ble_init(void) {
    ble_stack_init();

    gap_params_init();
    services_init();

    // Initialise connection parameters
    conn_params_init();
}

/* one time system init */
static void system_init(void)
{            
    softdevice_sys_evt_handler_set(sys_evt_handler);
        
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);
}

// Return the 'measured power @ 1m' field value for the ibeacon
static uint8_t makeAdvPowerLevel( void )
{
    // If explciitly set (eg via AT cmd) use value 
    if (cfg_getExtra_Value()!=0) {
        return cfg_getExtra_Value();
    }
    // Otherwise we use value based on tx power and battery
    uint8_t result_adv = 0;
    float battery = 0.0;
    
    // Get voltage
    hal_bsp_adc_init();
    battery = (float)hal_bsp_adc_read_u16();
    hal_bsp_adc_disable();
    battery = (battery * 3.6) / 1024.0;
 
    if(battery > 3.2)
    {
        result_adv = 0x18;
    }
    else if((battery <= 3.2) && (battery > 3.0))
    {
        result_adv = 0x10;
    }
    else if((battery <= 3.0) && (battery > 2.9))
    {
        result_adv = 0x08;
    }
    else if(battery <= 2.9)
    {
        result_adv = 0x00;
    }
    else
    {
        result_adv = 0x00;
    }
    
    // add tx power to the byte
    switch(cfg_getTXPOWER_Level())
    {
        case (int8_t)0x00: // RADIO_TXPOWER_TXPOWER_0dBm
            result_adv |= 0x00;
        break;
        
        case (int8_t)0xFC: // RADIO_TXPOWER_TXPOWER_Neg4dBm:
            result_adv |= 0x01;
        break;
        
        case (int8_t)0xF8: // RADIO_TXPOWER_TXPOWER_Neg8dBm:
            result_adv |= 0x02;
        break;
        
        case (int8_t)0xF4: // RADIO_TXPOWER_TXPOWER_Neg12dBm:
            result_adv |= 0x03;
        break;
        
        case (int8_t)0xF0: // RADIO_TXPOWER_TXPOWER_Neg16dBm:
            result_adv |= 0x04;
        break;
        
        case (int8_t)0xEC: // RADIO_TXPOWER_TXPOWER_Neg20dBm:
            result_adv |= 0x05;
        break;
        
        case (int8_t)0xD8: // RADIO_TXPOWER_TXPOWER_Neg30dBm:
            result_adv |= 0x06;
        break;
        
        case (int8_t)0x04UL: //RADIO_TXPOWER_TXPOWER_Pos4dBm:
            result_adv |= 0x07;
        break;
        
        default:
            result_adv |= 0x05;
        break;
    }
 
    return result_adv;
}
 
/**@brief Function for initializing the Advertising functionality params
 * call before each start of beaconning to update from config in case it changed
 */
static void advertising_setup(void)
{
    uint32_t               err_code;
    ble_advdata_t          advdata;
    ble_advdata_t          scanrsp;
    ble_adv_modes_config_t options;
    ble_advdata_manuf_data_t manuf_specific_data; 
    ble_uuid_t m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}};     // Advertise NUS server for at commands over NUS
    int8_t txPowerLevel = cfg_getTXPOWER_Level();

    memset(&advdata, 0, sizeof(advdata));
    memset(&scanrsp, 0, sizeof(scanrsp));
    memset(&options, 0, sizeof(options));

    manuf_specific_data.company_identifier = cfg_getCompanyID();
    // iBeacons are a BLE advert with a specific 'manufacteur specific' block, which is a TLV essentially
    _ctx.m_beacon_info[0] = APP_BEACON_ADV_DEVICE_TYPE;     // Tag - this is a ibeacon
    _ctx.m_beacon_info[1] = APP_BEACON_ADV_DATA_LENGTH;     // length of rest of block

    memcpy(&_ctx.m_beacon_info[2], cfg_getUUID(), UUID128_SIZE);
    _ctx.m_beacon_info[18] = MSB_16(cfg_getMajor_Value());
    _ctx.m_beacon_info[19] = LSB_16(cfg_getMajor_Value());
    _ctx.m_beacon_info[20] = MSB_16(cfg_getMinor_Value());
    _ctx.m_beacon_info[21] = LSB_16(cfg_getMinor_Value());    
    _ctx.m_beacon_info[22] = makeAdvPowerLevel();

    manuf_specific_data.data.p_data = (uint8_t *) (&_ctx.m_beacon_info[0]);
    manuf_specific_data.data.size   = APP_BEACON_INFO_LENGTH;
 
    // Build advertising data struct to pass into @ref ble_advertising_init.
    if (cfg_isIBeaconning()) {
        // Only make our advert look like an ibeacon if we are wanting it to be one
        advdata.name_type             = BLE_ADVDATA_NO_NAME;
        advdata.p_manuf_specific_data = &manuf_specific_data;
    } else {
        advdata.name_type             = BLE_ADVDATA_FULL_NAME;
        // And no addition manuf data so it doesn't look like an ibeacon
    }

    // depending on if we accept connections or not, advert data is different
    if (cfg_getConnectable()) {
        advdata.flags                 = (BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE |
                                        BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED);
        scanrsp.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
        scanrsp.uuids_complete.p_uuids  = m_adv_uuids;
    } else {
        advdata.flags                 = (BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED);
        // Not connectable, no uuids advertised to avoid access to NUS
        scanrsp.uuids_complete.uuid_cnt = 0;
        scanrsp.uuids_complete.p_uuids  = NULL;
    }
    scanrsp.name_type = BLE_ADVDATA_FULL_NAME;
    scanrsp.short_name_len = 8;      // get at least id part (maj/min in hex)
    scanrsp.p_tx_power_level = &txPowerLevel;

    // Give both options for advert speed (the one used depends on whether isIBeaconning() is true or not)
    options.ble_adv_fast_enabled  = true;
    options.ble_adv_fast_interval = MSEC_TO_UNITS(cfg_getADV_IND(), UNIT_0_625_MS);
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;
    options.ble_adv_slow_enabled = true;
    options.ble_adv_slow_interval = MSEC_TO_UNITS(APP_ADV_INTERVAL_MS_SLOW, UNIT_0_625_MS);     //1285*1.6;
    options.ble_adv_slow_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;
 
//    err_code = ble_advdata_set(&advdata, NULL); Done in ble_advertising_init using advdata
//    APP_ERROR_CHECK(err_code);
    // TODO : using ble_advertising module means out ibeacons are always flagged as connectable 
    // (never sets type to BLE_GAP_ADV_TYPE_ADV_NONCONN_IND)
    err_code = ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}
 
 

static bool advertising_check_start()
{
    uint32_t err_code;
    // Can't start if currently got an active BLE connection (not enough channels)
    if (_ctx.m_conn_handle!=BLE_CONN_HANDLE_INVALID) {
        return false;
    }
    // in all cases, setup to be ready otherwise the ble_advertisting_start() will fail with wrong state error
    ble_gap_conn_sec_mode_t sec_mode;    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    // password check has not been validated
    cfg_resetPasswordOk();
    // Change name
    sd_ble_gap_device_name_set(&sec_mode,
                            (const uint8_t *)(cfg_getAdvName()),
                                strlen(cfg_getAdvName()));

    // Change tx power level
    sd_ble_gap_tx_power_set(cfg_getTXPOWER_Level());

    advertising_setup();
    // only advertise if we want to be connectable, or we are ibeaconning, and we are NOT scanning
    if ((cfg_getConnectable() || ibb_isBeaconning())) // && ibs_is_scan_active()==false)
    {
        err_code = ble_advertising_start(ibb_isBeaconning()?BLE_ADV_MODE_FAST:BLE_ADV_EVT_SLOW);
        APP_ERROR_CHECK(err_code);
        log_info("adv check : connectable [%s] beaconning : %s", cfg_getAdvName(), ibb_isBeaconning()?"yes":"no");
        return true;
    } else {
        //idle the adverts
        err_code = ble_advertising_start(BLE_ADV_MODE_IDLE);
        APP_ERROR_CHECK(err_code);
        sd_ble_gap_adv_stop();
        log_info("adv check: no beaconning");
        return false;
    }
}
 


/** @brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


 
/*
static void deinit_uart_timeout_handler(void * p_context)
{
    comm_uart_deinit();
    
    //
    //examples to setup TX/RX in different mode :
    //
    //nrf_gpio_cfg_default(RX_PIN_NUMBER);
    //nrf_gpio_cfg_default(TX_PIN_NUMBER);
    //
    //nrf_gpio_cfg_output(RX_PIN_NUMBER);
    //nrf_gpio_pin_set(RX_PIN_NUMBER);
    //nrf_gpio_cfg_output(TX_PIN_NUMBER);
    //nrf_gpio_pin_set(TX_PIN_NUMBER);
    //
    //nrf_gpio_cfg_input(RX_PIN_NUMBER, GPIO_PIN_CNF_PULL_Pulldown);
    //nrf_gpio_cfg_input(TX_PIN_NUMBER, GPIO_PIN_CNF_PULL_Pulldown);
}
*/


// Control of beaconning activity
bool ibb_isBeaconning() {
    return (cfg_isIBeaconning());
}

void ibb_start() {
    cfg_setIsIBeaconning(true);
    // To change the beaconning, we have to stop anything currently running (connectable or ibeaconning)
    // TODO (the current phase will timeout in a while anyway and then we'll re-check it)
}
void ibb_stop() {
    cfg_setIsIBeaconning(false);
    // To change the beaconning, we have to stop anything currently running (connectable or ibeaconning)
    // TODO (the current phase will timeout in a while anyway and then we'll re-check it)
}

/** request a reset asap */
void app_reset_request() {
    _ctx.resetRequested = true;    
}

void app_setFlashBusy() {
    _ctx.flashBusy = true;      // someone somewhere is writing to the flash
}
bool app_isFlashBusy() {
    return _ctx.flashBusy;      // they would like to know if its busy right now
}

int main(void)
{
    system_init();

    // Setup gpio
    uint32_t err_code;
    if(!nrf_drv_gpiote_is_init())
    {
        err_code = nrf_drv_gpiote_init();
    }
    // Init uart asap in case using for logs also
    comm_uart_init(); 

#ifdef RELEASE_BUILD
    wlog_init(-1);      // replace by 0 to get logs on uart
#else 
    wlog_init(0);      // replace by 0 to get logs on uart - warning will disrupt remote guy if connected..
#endif
    cfg_init();              
   log_info("config/log init done");
    
    ble_init();
    log_info("ble system init done");

    lightctl_init();
    // Tell host we are ready to rock
    comm_uart_tx((uint8_t *)"+READY\r\n",8, NULL);
    // Init adv parameters and start if required
    advertising_check_start();

    //todo remove uart disabled at 10s of ibeaconning for power
    /*                    
    app_timer_create(&m_deinit_uart_delay,
                        APP_TIMER_MODE_SINGLE_SHOT,
                        deinit_uart_timeout_handler);    
        uint32_t deinit_delay = APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER);//32 * 10000;          
        app_timer_start(m_deinit_uart_delay, deinit_delay, NULL);
    */
   lightctl_start();
   log_info("waiting for commands");
    for (;;)
    {        
        if (_ctx.resetRequested)
        {
            NVIC_SystemReset();
        }
        // Check if flash needs updating
        cfg_writeCheck();
        // Check if UART has data to process
        comm_uart_processRX();
        // Go in lowpower only if flash isn't busy
        if(!app_isFlashBusy())
        {
            power_manage();
        }
    }
}
