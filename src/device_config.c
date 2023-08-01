
/** Device config module */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "bsp_minew_nrf51.h"
#include "wutils.h"
#include "main.h"
#include "device_config.h"

#ifndef FW_MAJOR
#define FW_MAJOR (2)
#endif
#ifndef FW_MINOR 
#define FW_MINOR (0)
#endif
// Do we have card type set via makefile? if not, assume fille revE
#ifndef CARD_TYPE
#define CARD_TYPE 5
#endif

#define DEVICE_NAME_BASE             "W"                                       /**< Name of device when for connection beacons. Will be included in the advertising data. */
#define DEVICE_NAME_LEN             20      // 00000000_W
#define PASSWORD_LEN    (4)
#define MAGIC_CFG_SAVED (0x60671520)    // magic number meaning full saved config present in flash
#define MAGIC_CFG_PROD (0x60671519)     // magic number meaning just production saved config present in flash
#ifndef LED_MAJOR
#define LED_MAJOR (1)
#endif
#ifndef LED_MINOR 
#define LED_MINOR LED_MM_ANY
#endif

#define STR2(x) #x
#define STR(x) STR2(x)
// string in binary that shows verion build and date
const char* BUILD="(@)Build v" STR(FW_MAJOR) "." STR(FW_MINOR) " card type " STR(CARD_TYPE) " on " __DATE__ " " __TIME__;

// Device config structure
static struct {
    uint32_t magic;
    //iBeacon config
    uint16_t major_value;
    uint16_t minor_value;
    uint16_t advertisingInterval_ms; // Default advertising interval 100 ms
    uint16_t company_id; // apple 0x0059 -> Nordic
    uint8_t beacon_uuid_tab[UUID128_SIZE];
    // config for connectable adverts
    char nameAdv[DEVICE_NAME_LEN];
    uint8_t passwordTab[PASSWORD_LEN]; // Password
    uint8_t masterPasswordTab[PASSWORD_LEN]; // Password
    bool isConnectable;        // is it allowed to connect via GAP for config or uart pass-thru?
    bool isIBeaconning;        // is it currently beaconning (in case of unexpected reset)
    int8_t txPowerLevel; // Default -4dBm
    uint8_t extra_value;    // usually related to tx power
    bool flashWriteReq;
    ledMode_t ledMode;
    uint16_t ledIBMajor;
    uint16_t ledIBMinor;
} _ctx = {
    .magic=MAGIC_CFG_SAVED,             // So that if config updated and saved, the next reboot will find it        
    .advertisingInterval_ms = 300, 
    .txPowerLevel = -20,        // RADIO_TXPOWER_TXPOWER_Neg20dBm,  
    .major_value = 0xFFFF,
    .minor_value = 0xFFFF,
    .beacon_uuid_tab = {0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2, 0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0},
    .nameAdv = "FFFFFFFF_W", 
    .isConnectable=true,
    .isIBeaconning=false,
    .company_id = 0x004C, 
    .extra_value = 0xC3,
    .passwordTab = {'1', '5', '1', '9'},
    .masterPasswordTab = {'6', '0', '6', '7'},
    .ledMode=LED_MODE_IBD,
    .ledIBMajor=LED_MAJOR,
    .ledIBMinor=LED_MINOR,
};
// Not in config
static bool _isPasswordOK = false;

// Refresh advertised name (eg when change maj/minor)
// Note we put maj/min at front so can see it with short name of 8 chars
static void makeNameAdv() {
    sprintf(_ctx.nameAdv, "%04x%04x_%s", cfg_getMajor_Value(), cfg_getMinor_Value(), DEVICE_NAME_BASE);
}
/** Config handling
 */
// Get config from NVM into _ctx
void cfg_init() {
    // Simple flash config management here - just write the entire structrue in the flash page
    // Check if our magic number is there and read from page (ie valid config present) otherwise use compile time defaults
    uint32_t magic=0;
    hal_bsp_nvmRead(0, 4, (uint8_t*)&magic);
    if (magic==MAGIC_CFG_PROD) {
        // Only the major/minor were written by production process
        _ctx.major_value = hal_bsp_nvmRead16(4);
        _ctx.minor_value = hal_bsp_nvmRead16(6);
        // set default name
        makeNameAdv();
        log_warn("initialised config from production flash setup [%04x,%04x]", _ctx.major_value, _ctx.minor_value);
    } else if (magic==MAGIC_CFG_SAVED) {
        // Proper saved config present
        // load full structure
        hal_bsp_nvmRead(0, sizeof(_ctx), (uint8_t*)&_ctx);
        log_info("config initialised from flash [%s]", _ctx.nameAdv);
    } else {
        // go with defaults
        log_info("config initialised from defaults[%s]", _ctx.nameAdv);
    }
}
// Request update of NVM with new config
static void configUpdateRequest() {
    // Set flag to do it in main loop (for ble timing reasons)
    _ctx.flashWriteReq = true;
}
void cfg_writeCheck() {
    if (_ctx.flashWriteReq) {
        app_setFlashBusy();     // not sure if this works
        // Write entire structure
        // TODO ADD PAGE ERASE ETC
        hal_bsp_nvmWrite(0, sizeof(_ctx), (uint8_t*)&_ctx);
    }
    _ctx.flashWriteReq = false;
}
/*!
* Set And Get methods
*/
int cfg_getFWMajor() {
    return FW_MAJOR;
}
int cfg_getFWMinor() {
    return FW_MINOR;
}
int cfg_getCardType() {
    return CARD_TYPE;
}
int cfg_getLEDMajor() {
    return _ctx.ledIBMajor;
}
void cfg_setLEDMajor(uint16_t value)
{
    if (value!=_ctx.ledIBMajor) {
        _ctx.ledIBMajor = value;
        configUpdateRequest();
    }
}
int cfg_getLEDMinor() {
    return _ctx.ledIBMinor;
}
void cfg_setLEDMinor(uint16_t value)
{
    if (value!=_ctx.ledIBMinor) {
        _ctx.ledIBMinor = value;
        configUpdateRequest();
    }
}
 
ledMode_t cfg_getLedMode() {
    return _ctx.ledMode;
}
void cfg_setLedMode(uint8_t value)
{
    ledMode_t mv = (ledMode_t)value;
    if (mv!=_ctx.ledMode) {
        _ctx.ledMode = mv;
        configUpdateRequest();
    }
}
void cfg_setUUID(uint8_t* value)
{
    if (memcmp(value, _ctx.beacon_uuid_tab, UUID128_SIZE)!=0) {
        memcpy(_ctx.beacon_uuid_tab, value, UUID128_SIZE);
        configUpdateRequest();
    }
}
 
uint8_t* cfg_getUUID( void )
{
    return _ctx.beacon_uuid_tab;
}
void cfg_setADV_IND(uint32_t value)
{
    if (value!=_ctx.advertisingInterval_ms) {
        _ctx.advertisingInterval_ms = value;
        configUpdateRequest();
    }
}
 
uint16_t cfg_getADV_IND( void )
{
    return _ctx.advertisingInterval_ms;
}
 
void cfg_setTXPOWER_Level(int8_t value)
{
    if (value!=_ctx.txPowerLevel) {
        _ctx.txPowerLevel = value;
        configUpdateRequest();
    }
}
 
int8_t cfg_getTXPOWER_Level( void )
{
    return _ctx.txPowerLevel;
}
 
void cfg_setConnectable(bool value)
{
    if (value!=_ctx.isConnectable) {
        _ctx.isConnectable = value;
        configUpdateRequest();
    }
}
 
bool cfg_getConnectable( void )
{
    return _ctx.isConnectable;
}

bool cfg_isIBeaconning( void )
{
    return _ctx.isIBeaconning;
}
void cfg_setIsIBeaconning(bool value)
{
    if (value!=_ctx.isIBeaconning) {
        _ctx.isIBeaconning = value;
        configUpdateRequest();
    }
}
 
void cfg_setMinor_Value(uint16_t value)
{
    if (value!=_ctx.minor_value) {
        _ctx.minor_value = value;
        makeNameAdv();
        configUpdateRequest();
    }
}
 
uint16_t cfg_getMinor_Value( void )
{
    return _ctx.minor_value;
}
 
void cfg_setMajor_Value(uint16_t value)
{
    if (value!=_ctx.major_value) {
        _ctx.major_value = value;
        makeNameAdv();
        configUpdateRequest();
    }
}
 
uint16_t cfg_getMajor_Value( void )
{
    return _ctx.major_value;
}

char* cfg_getAdvName() {
    return _ctx.nameAdv;
}

void cfg_resetPasswordOk()
{
   _isPasswordOK = false;
}

bool cfg_isPasswordOk() {
    return _isPasswordOK;
} 
bool cfg_checkPassword( char* given )
{
    if (given==NULL) {
        return false;
    }
    for(int i=0;i<PASSWORD_LEN;i++) {
        if (_ctx.passwordTab[i]!=given[i]) {
            _isPasswordOK = false;
            return false;
        }
    }
    _isPasswordOK = true;
    return true;
}
bool cfg_setPassword(char* oldp, char* newp) 
{
    if (newp==NULL) {
        return false;
    }
    if (cfg_isPasswordOk() || cfg_checkPassword(oldp)) {
        for(int i=0;i<PASSWORD_LEN;i++) {
            _ctx.passwordTab[i]=newp[i];
        }
        configUpdateRequest();
        return true;
    }
    return false;
} 
void cfg_setCompanyID(uint16_t value)
{
    if (value!=_ctx.company_id) {
        _ctx.company_id = value;
        configUpdateRequest();
    }
}
 
uint16_t cfg_getCompanyID( void )
{
    return _ctx.company_id;
}
 
void cfg_setExtra_Value(uint8_t value) {
    if (value!=_ctx.extra_value) {
        _ctx.extra_value = value;
        configUpdateRequest();
    }
}
uint8_t cfg_getExtra_Value() {
    return _ctx.extra_value;
}


// Generic access by keys
// Get key value or 0 if not found
int cfg_getByKey(uint16_t key, uint8_t* vp, int maxlen) {
    switch (key) {
        case DCFG_KEY_MAJOR: {
            *((uint16_t*)vp) = cfg_getMajor_Value();
            return sizeof(uint16_t);
        }
        case DCFG_KEY_MINOR: {
            *((uint16_t*)vp) = cfg_getMinor_Value();
            return sizeof(uint16_t);
        }
        case DCFG_KEY_ADV_INT: {
            *((uint16_t*)vp) = cfg_getADV_IND();
            return sizeof(uint16_t);
        }
        case DCFG_KEY_COMP_ID: {
            *((uint16_t*)vp) = cfg_getCompanyID();
            return sizeof(uint16_t);
        }
        case DCFG_KEY_TXPOW: {
            *((int8_t*)vp) = cfg_getTXPOWER_Level();
            return sizeof(int8_t);
        }
        case DCFG_KEY_CONNECTABLE: {
            *((bool*)vp) = cfg_getConnectable();
            return sizeof(bool);
        }
        case DCFG_KEY_IBEACONNING: {
            *((bool*)vp) = cfg_isIBeaconning();
            return sizeof(bool);
        }
        case DCFG_KEY_UUID: {
            int l = (maxlen<UUID128_SIZE ? maxlen : UUID128_SIZE);
            memcpy(vp, _ctx.beacon_uuid_tab, l);
            return UUID128_SIZE;
        }
        case DCFG_KEY_PASS: {
            int l = (maxlen<PASSWORD_LEN ? maxlen : PASSWORD_LEN);
            memcpy(vp, _ctx.passwordTab, l);
            return PASSWORD_LEN;
        }
        case DCFG_KEY_LEDMAJOR: {
            *((uint16_t*)vp) = cfg_getLEDMajor();
            return sizeof(uint16_t);
        }
        case DCFG_KEY_LEDMINOR: {
            *((uint16_t*)vp) = cfg_getLEDMinor();
            return sizeof(uint16_t);
        }
        case DCFG_KEY_LEDMODE: {
            *((uint8_t*)vp) = cfg_getLedMode();
            return sizeof(uint8_t);
        }
        default:
            return 0;
    }
}
// Set key value, return real key length
int cfg_setByKey(uint16_t key, uint8_t* vp, int len) {
    switch (key) {
        case DCFG_KEY_MAJOR: {
            cfg_setMajor_Value(*((uint16_t*)vp));
            return sizeof(uint16_t);
        }
        case DCFG_KEY_MINOR: {
            cfg_setMinor_Value(*((uint16_t*)vp));
            return sizeof(uint16_t);
        }
        case DCFG_KEY_ADV_INT: {
            cfg_setADV_IND(*((uint16_t*)vp));
            return sizeof(uint16_t);
        }
        case DCFG_KEY_COMP_ID: {
            cfg_setCompanyID(*((uint16_t*)vp));
            return sizeof(uint16_t);
        }
        case DCFG_KEY_TXPOW: {
            cfg_setTXPOWER_Level(*((int8_t*)vp));
            return sizeof(int8_t);
        }
        case DCFG_KEY_CONNECTABLE: {
            cfg_setConnectable(*((bool*)vp));
            return sizeof(bool);
        }
        case DCFG_KEY_IBEACONNING: {
            cfg_setIsIBeaconning(*((bool*)vp));
            return sizeof(bool);
        }
        
        case DCFG_KEY_UUID: {
            int l = (len<UUID128_SIZE ? len : UUID128_SIZE);
            memcpy(_ctx.beacon_uuid_tab, vp, l);
            return UUID128_SIZE;
        }
        case DCFG_KEY_PASS: {
            int l = (len<PASSWORD_LEN ? len : PASSWORD_LEN);
            memcpy(_ctx.passwordTab, vp, l);
            return PASSWORD_LEN;
        }
        case DCFG_KEY_LEDMAJOR: {
            cfg_setLEDMajor(*((uint16_t*)vp));
            return sizeof(uint16_t);
        }
        case DCFG_KEY_LEDMINOR: {
            cfg_setLEDMinor(*((uint16_t*)vp));
            return sizeof(uint16_t);
        }
        case DCFG_KEY_LEDMODE: {
            cfg_setLedMode(*((uint8_t*)vp));
            return sizeof(uint8_t);
        }
        default:
            return 0;       // not found
    }
}
int cfg_iterateKeys(void* odev, PK_CB_T pkcb) {
    static uint16_t KEYS[] = {DCFG_KEY_MAJOR, DCFG_KEY_MINOR, DCFG_KEY_ADV_INT, DCFG_KEY_TXPOW, 
                        DCFG_KEY_UUID, DCFG_KEY_COMP_ID, DCFG_KEY_PASS, DCFG_KEY_CONNECTABLE, DCFG_KEY_IBEACONNING,
                        DCFG_KEY_LEDMAJOR, DCFG_KEY_LEDMINOR, DCFG_KEY_LEDMODE};
    uint8_t d[16];
    for(int i=0; i<(sizeof(KEYS)/sizeof(KEYS[0]));i++) {
        int l = cfg_getByKey(KEYS[i], &d[0], 16);
        (*pkcb)(odev, KEYS[i], d, l);
    }
    return sizeof(KEYS)/sizeof(KEYS[0]);
}
