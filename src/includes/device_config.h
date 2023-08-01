#ifndef H_DEVICE_CONFIG_H
#define H_DEVICE_CONFIG_H


/*********************************INCLUDE**************************************/

#include <inttypes.h>
#include <stdint.h>

typedef enum { LED_MODE_OFF, LED_MODE_IBD, LED_MODE_FINDME } ledMode_t;
#define LED_MM_ANY (0xFFFF)

void cfg_init();
void cfg_writeCheck();

void cfg_setUUID(uint8_t* value);
uint8_t* cfg_getUUID( void );
void cfg_setADV_IND(uint32_t value);
uint16_t cfg_getADV_IND( void );
void cfg_setTXPOWER_Level(int8_t level);
int8_t cfg_getTXPOWER_Level( void );
void cfg_setCompanyID(uint16_t value);
uint16_t cfg_getCompanyID( void );
uint16_t cfg_getMajor_Value( void );
uint16_t cfg_getMinor_Value( void );
void cfg_setMajor_Value( uint16_t );
void cfg_setMinor_Value( uint16_t );
char* cfg_getAdvName();
int cfg_getLEDMajor();
int cfg_getLEDMinor();
ledMode_t cfg_getLedMode();

void cfg_setConnectable(bool value);
bool cfg_getConnectable( void );
bool cfg_isIBeaconning( void );
void cfg_setIsIBeaconning(bool value);
void cfg_resetPasswordOk();
bool cfg_isPasswordOk();
bool cfg_checkPassword( char* given );
bool cfg_setPassword(char* oldp, char* newp);
void cfg_setExtra_Value(uint8_t extra);
uint8_t cfg_getExtra_Value();

int cfg_getFWMajor();
int cfg_getFWMinor();
int cfg_getCardType();

// Generalised cfgmgr access for at commands
typedef void (*PK_CB_T)(void* odev, uint16_t k, uint8_t* d, int l);
int cfg_getByKey(uint16_t key, uint8_t* vp, int maxlen);
int cfg_setByKey(uint16_t key, uint8_t* vp, int len);
int cfg_iterateKeys(void* odev, PK_CB_T pkcb);


/* generic key ids */
#define DCFG_KEY_ILLEGAL    (0)
#define DCFG_KEY_BASE       (0x0100)
#define DCFG_KEY_MAJOR      (DCFG_KEY_BASE + 0x01)
#define DCFG_KEY_MINOR      (DCFG_KEY_BASE + 0x02)
#define DCFG_KEY_ADV_INT    (DCFG_KEY_BASE + 0x03)
#define DCFG_KEY_TXPOW      (DCFG_KEY_BASE + 0x04)
#define DCFG_KEY_UUID       (DCFG_KEY_BASE + 0x05)
#define DCFG_KEY_COMP_ID    (DCFG_KEY_BASE + 0x06)
#define DCFG_KEY_PASS       (DCFG_KEY_BASE + 0x07)
#define DCFG_KEY_CONNECTABLE (DCFG_KEY_BASE + 0x08)
#define DCFG_KEY_IBEACONNING (DCFG_KEY_BASE + 0x09)
#define DCFG_KEY_LEDMAJOR      (DCFG_KEY_BASE + 0x0A)
#define DCFG_KEY_LEDMINOR      (DCFG_KEY_BASE + 0x0B)
#define DCFG_KEY_LEDMODE      (DCFG_KEY_BASE + 0x0C)

//EOF
#endif  /* H_DEVICE_CONFIG_H */