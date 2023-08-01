/**
 * AT command processing
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "ble_nus.h"
#include "app_util_platform.h"
#include "ble_hci.h"
#include "app_error.h"
#include "app_uart.h"
#include "ibs_scan.h"
#include "nrf_delay.h"
#include "softdevice_handler.h"

#include "wutils.h"
#include "device_config.h"

#include "main.h"
#include "at_process.h"
#include "comm_uart.h"
#include "comm_ble.h"

#include "nrf_drv_gpiote.h"
#include "bsp_minew_nrf51.h"

#define MAX_TXSZ (100)
#define MAX_ARGS (8)

// per at command we have a definiton:
typedef enum { ATCMD_OK, ATCMD_GENERR, ATCMD_BADARG, ATCMD_PROCESSED } ATRESULT;
typedef ATRESULT (*ATCMD_CBFN_t)(uint8_t nargs, char* argv[], void* odev);
typedef struct {
    const char* cmd;
    const char* desc;
    ATCMD_CBFN_t fn;
} ATCMD_DEF_t;

// Write output to console on given device
static bool wconsole_println(void* udev, const char* l, ...);

// predec at command handlers
static ATRESULT atcmd_hello(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_who(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_type(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_reset(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_listcmds(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_info(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_getcfg(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_setcfg(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_checkconnect(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_connect(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_disconnect(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_password(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_start_scan(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_stop_scan(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_start_ib(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_stop_ib(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_enable_conn(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_disable_conn(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_push(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_out(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_in(uint8_t nargs, char* argv[], void* odev);
static ATRESULT atcmd_debug_stats(uint8_t nargs, char* argv[], void* odev);

static ATCMD_DEF_t ATCMDS[] = {
    { .cmd="AT", .desc="Wakeup", .fn=atcmd_hello},
    { .cmd="AT+WHO", .desc="Card mode", .fn=atcmd_who},
    { .cmd="AT+TYPE", .desc="Card type", .fn=atcmd_type},
    { .cmd="AT+HELP", .desc="List commands", .fn=atcmd_listcmds},
    { .cmd="ATZ", .desc="Reset card", .fn=atcmd_reset},
    { .cmd="AT+INFO", .desc="Show info", .fn=atcmd_info},
    { .cmd="AT+GETCFG", .desc="Show config", .fn=atcmd_getcfg},     
    { .cmd="AT+SETCFG", .desc="Set config", .fn=atcmd_setcfg},
    { .cmd="AT+VERSION", .desc="FW version", .fn=atcmd_info},         
    { .cmd="AT+CONN?", .desc="Check connected", .fn=atcmd_checkconnect},
    { .cmd="AT+CONN", .desc="Connect", .fn=atcmd_connect},  
    { .cmd="AT+DISC", .desc="Disconnect", .fn=atcmd_disconnect},  
    { .cmd="AT+PASS", .desc="Check password", .fn=atcmd_password},
    { .cmd="AT+START", .desc="Start scan", .fn=atcmd_start_scan},
    { .cmd="AT+STOP", .desc="Stop scan", .fn=atcmd_stop_scan},
    { .cmd="AT+IB_START", .desc="Start ibeaconning", .fn=atcmd_start_ib},
    { .cmd="AT+IB_STOP", .desc="Stop ibeaconning", .fn=atcmd_stop_ib},
    { .cmd="AT+CONN_EN", .desc="Enable remote connection", .fn=atcmd_enable_conn},
    { .cmd="AT+CONN_DIS", .desc="Disable remote connection", .fn=atcmd_disable_conn},
    { .cmd="AT+PUSH", .desc="Push scan data", .fn=atcmd_push},
    { .cmd="AT+D?", .desc="Output debug stats", .fn=atcmd_debug_stats},
    { .cmd="AT+O", .desc="Set output state", .fn=atcmd_out},
    { .cmd="AT+I", .desc="Get input state", .fn=atcmd_in},
};

// Our local data context
static struct {
    uint8_t txbuf[MAX_TXSZ+3];          // space for terminating \r\n\0
    UART_TX_FN_T passThru_txfn1;        // If in passthru, this is output to one side
    UART_TX_FN_T passThru_txfn2;        // and this the other
    uint8_t ncmds;
    ATCMD_DEF_t* cmds;
} _ctx = {
    .cmds=ATCMDS,
    .ncmds= sizeof(ATCMDS)/sizeof(ATCMDS[0]),
};

// External api

// Deal with received data : line string either of max length or \r or \n terminated. utx_fn is the fn to send data back to this originator
// If in 'pass-thru' mode the data is just copied to the other guy (see AT+CONN) unless we find a "AT+DISC" in either direction
void at_process_input(char* data, UART_TX_FN_T source_txfn) {
    int dlen = strlen(data);
    if (dlen>1) {
        if (_ctx.passThru_txfn1!=NULL && _ctx.passThru_txfn2!=NULL) {
            // see which is the other guy..
            UART_TX_FN_T dest_txfn = _ctx.passThru_txfn1;  
            if (_ctx.passThru_txfn1==source_txfn) {
                // ah nope its the other
                dest_txfn = _ctx.passThru_txfn2;
            }
            // Check if command is one that must be processed locally for correct result
            // check for disconnect from either side, for a who (used as uart check) or the cross-connection status check
            if (strncmp("AT+DISC", data,7)==0  || strncmp("AT+WHO", data,6)==0 || strncmp("AT+CONN?", data,8)==0 || strncmp("AT+D?", data,5)==0) {
                // process it locally as AT command
                at_process_line(data, source_txfn);
            } else {
                // Give it all to dest
                if ((*dest_txfn)((uint8_t*)data, dlen, NULL)<0) {
                    // broken
                    (*source_txfn)(NULL, -1, NULL);        // tell guy who sent me data
                    _ctx.passThru_txfn1 = NULL;      // ie disconnect
                    _ctx.passThru_txfn2 = NULL;      // ie disconnect
                }
            }
        } else {
            // process it locally as AT command
            at_process_line(data, source_txfn);
        }
    } else {
        // brokenness
    }
}

//Process an input line terminated by \0, and write any output back to the given tx fn
void at_process_line(char* line, UART_TX_FN_T utx_fn) {
    // parse line into : command, args
    char* els[MAX_ARGS];
    char* s = line;
    int elsi = 0;
    // first segment
    els[elsi++] = s;
    // Process input until end of string, newlone or got max allow args
    while (*s!=0 && elsi<MAX_ARGS) {
        // Separators on line are space, = and , (so can accept "AT+X P1 P1", "AT+X=P1 P2" and "AT+X=P1,P2, P3" etc )
        if (*s==' ' || *s=='=' || *s==',') {
            // make end of string at white space
            *s='\0';
            s++;
            // consume blank space but NOT explicit seperators (so AT ,,,,, still creates 6 args or 0 length...)
            while(*s==' ') {
                s++;
            }
            // If more string, then its the next element..
            if (*s!='\0') {
                els[elsi++] = s;
            } // else leave it on the end of string as we're done
        } else if (*s=='\r' || *s=='\n') {
            *s='\0';
            break;      // end of processing for this line
        } else {
            s++;
        }
    }
    if (strlen(els[0])==0) {
        // empty bad command, ignore it
        return;
    }
    // find it in the list
    for(int i=0;i<_ctx.ncmds;i++) {
        if (strcmp(els[0], _ctx.cmds[i].cmd)==0) {
            // gotcha
//            log_debug("got cmd %s with %d args", els[0], elsi-1);
            // call the specific command processor function as registered
            ATRESULT ret = (*_ctx.cmds[i].fn)(elsi, els, utx_fn);
            // generic processing of return for OK and GENERR, other cases the cmd processing sent the return
            if (ret==ATCMD_OK) {
                wconsole_println(utx_fn, "OK");
            } else if (ret==ATCMD_GENERR) {
                wconsole_println(utx_fn, "ERROR");
//                wconsole_println(utx_fn, "Bad args for %s : %s", _ctx.cmds[i].cmd, _ctx.cmds[i].desc);
            }
            return;
        }
    }
    // not found
    wconsole_println(utx_fn, "ERROR");
    wconsole_println(utx_fn, "Unknown command [%s].", els[0]);
//    log_debug("no cmd %s with %d args", els[0], elsi-1);
}

// internals

// AT command processing
static ATRESULT atcmd_listcmds(uint8_t nargs, char* argv[], void* odev) {
    for(int i=0;i<_ctx.ncmds;i++) {
        wconsole_println(odev, "%s: %s", _ctx.cmds[i].cmd, _ctx.cmds[i].desc);
    }
    return ATCMD_OK;
}

static ATRESULT atcmd_hello(uint8_t nargs, char* argv[], void* odev) {
    wconsole_println(odev, "Hello.");
    return ATCMD_OK;
}

// Return decimal integer with firmware version
static ATRESULT atcmd_who(uint8_t nargs, char* argv[], void* odev) {
    wconsole_println(odev, "%d", (((cfg_getFWMajor() & 0xFF)<<8) + (cfg_getFWMinor() & 0xff)));
    return ATCMD_PROCESSED;     // don't need to says ok
}
static ATRESULT atcmd_type(uint8_t nargs, char* argv[], void* odev) {
    wconsole_println(odev, "%d", cfg_getCardType());
    return ATCMD_PROCESSED;     // don't need to says ok
}

static ATRESULT atcmd_push(uint8_t nargs, char* argv[], void* odev) {
    // always in push mode anyway
    return ATCMD_OK;
}

static ATRESULT atcmd_reset(uint8_t nargs, char* argv[], void* odev) {
    app_reset_request();
    return ATCMD_OK;
}

static ATRESULT atcmd_info(uint8_t nargs, char* argv[], void* odev) {
    wconsole_println(odev, "Wyres BLE v%d.%d", cfg_getFWMajor(), cfg_getFWMinor());
    wconsole_println(odev, "id:%04x:%04x (%d:%d) name [%s]", cfg_getMajor_Value(), cfg_getMinor_Value(),cfg_getMajor_Value(), cfg_getMinor_Value(), cfg_getAdvName());
    wconsole_println(odev, "Scan: %s, Beacon: %s", (ibs_is_scan_active()?"YES":"NO"), (ibb_isBeaconning()?"YES":"NO"));
    wconsole_println(odev, "nbIBs[%d]", ibs_scan_getTableSize());
    wconsole_println(odev, "Lightctl mode : %d, active : %d", cfg_getLedMode(), (lightctl_is_active()?"YES":"NO"));
    wconsole_println(odev, "LightCtl ibeacon:%04x:%04x (%d:%d)", cfg_getLEDMajor(), cfg_getLEDMinor(), cfg_getLEDMajor(), cfg_getLEDMinor());
    return ATCMD_OK;
}

static void printKey(void* odev, uint16_t k, uint8_t* d, int l) {
    // Must explicitly check for illegal key (0000) as used for error check ie assert in configmgr
    if (k==DCFG_KEY_ILLEGAL) {
        wconsole_println(odev, "Key[%04x]=NOT FOUND", k);
        return;
    }
    // print element max data length 16
    switch(l) {
        case 0:
        case -1:  {
            wconsole_println(odev, "Key[%04x]=NOT FOUND", k);
            break;
        }
        case 1: {
            // print as decimal
            wconsole_println(odev, "Key[%04x]=%d", k, d[0]);
            break;
        }
        case 2: {
            // print as decimal
            uint16_t* vp = (uint16_t *)(&d[0]);     // avoid the overly keen anti-aliasing check
            wconsole_println(odev, "Key[%04x]=%d / 0x%04x", k, *vp, *vp);
            break;
        }
        case 4: {
            // print as decimal
            uint32_t* vp = (uint32_t *)(&d[0]);     // avoid the overly keen anti-aliasing check
            wconsole_println(odev, "Key[%04x]=%d / 0x%08x", k, *vp, *vp);
            break;
        }
        default: {
            // dump as hex up to 16 bytes
            char hs[34]; // 16*2 + 1 (terminator) + 1 (for luck)
            int sz = (l<16?l:16);       // in case its longer!
            for(int i=0;i<sz;i++) {
                sprintf(&hs[i*2], "%02x", d[i]);
            }
            hs[sz*2]='\0';
            wconsole_println(odev, "Key[%04x]=0x%s", k, hs);
            break;
        }
    }
}

static ATRESULT atcmd_getcfg(uint8_t nargs, char* argv[], void* odev) {
    // Check args - if 1 present then show just that config element else show all
    if (nargs>2) {
        return ATCMD_BADARG;
    }
    if (nargs==1) {
        // get al config elements and print them
        // Currently not possible as uart output buffer can't hold them all
        cfg_iterateKeys(odev, &printKey);
    } else if (nargs==2) {
        int k=0;
        int kl = strlen(argv[1]);
        if (kl==4) {
            if (sscanf(argv[1], "%4x", &k)<1) {
                wconsole_println(odev, "ERROR");
                wconsole_println(odev, "Bad key [%s] must be 4 hex digits", argv[1]);
                return ATCMD_BADARG;
            } else {
                uint8_t d[16];
                int l = cfg_getByKey(k, &d[0], 16);
                printKey(odev, k, &d[0], l);
            }
        } else {
            wconsole_println(odev, "ERROR");
            wconsole_println(odev, "Must give key of 4 digits");
            return ATCMD_BADARG;
        }
    }
    return ATCMD_OK;
}
static ATRESULT atcmd_setcfg(uint8_t nargs, char* argv[], void* odev) {
    // Must have done a password login to set params
    if (!cfg_isPasswordOk()) {
        return ATCMD_GENERR;
    }

    // args : cmd, config key (always 4 digits hex), config value as hex string (0x prefix) or decimal
    if (nargs<3) {
        return ATCMD_BADARG;
    }
    // parse key
    if (strlen(argv[1])!=4) {
        wconsole_println(odev, "Key[%s] must be 4 digits", argv[1]);
        return ATCMD_BADARG;
    }
    int k=0;
    if (sscanf(argv[1], "%4x", &k)<1) {
        wconsole_println(odev, "Key[%s] must be 4 digits", argv[1]);
    } else {
        // parse value
        // Get the length it should be by a dummy get
        uint32_t d=0;
        int l = cfg_getByKey(k, (uint8_t*)&d, 4);
        switch(l) {
            case 0: {
                wconsole_println(odev, "Key[%s] does not exist", argv[1]);
                // TODO ? create in this case? maybe if a -c arg at the end? maybe not useful?
                break;
            }
            case 1: 
            case 2:
            case 3:
            case 4: {
                int v = 0;
                char *vp = argv[2];
                if (*vp=='0' && *(vp+1)=='x') {
                    if (strlen((vp+2))!=(l*2)) {
                        wconsole_println(odev, "Key[%04x]=%s value is incorrect length. (expected %d bytes)", k, argv[2], l);
                        return ATCMD_BADARG;
                    }
                    if (sscanf((vp+2), "%x", &v)<1) {
                        wconsole_println(odev, "Key[%04x] bad hex value:%s",k,vp);
                        return ATCMD_BADARG;
                    }
                } else {
                    if (sscanf(vp, "%d", &v)<1) {
                        wconsole_println(odev, "Key[%04x] bad dec value:%s",k,vp);
                        return ATCMD_BADARG;
                    }
                }
                cfg_setByKey(k, (uint8_t*)&v, l);
                printKey(odev, k, (uint8_t*)&v, l);        // Show the value now in the config 
                break;
            }

            default: {
                // parse as hex string
                char* vp = argv[2];
                // Skip 0x if user put it in
                if (*vp=='0' && *(vp+1)=='x') {
                    vp+=2;
                }
                //Check got enough digits
                if (strlen(vp)!=(l*2)) {
                    wconsole_println(odev, "Key[%04x]=%s value is incorrect length. (expected %d bytes)", k, argv[2], l);
                    return ATCMD_BADARG;
                }
                if (l>16) {
                    wconsole_println(odev, "Key[%04x] has length %d : cannot set (max 16)", k, l);
                    return ATCMD_BADARG;
                }
                // gonna allow up to 16 bytes
                uint8_t val[16];
                for(int i=0;i<l;i++) {
                    // sscanf into int (4 bytes) then copy just LSB as value for each byte
                    unsigned int b=0;
                    if (sscanf(vp, "%02x", &b)<1) {
                        wconsole_println(odev, "Key[%04x] bad hex : %s", k, vp);
                        return ATCMD_BADARG;
                    }
                    val[i] = b;
                    vp+=2;
                }
                cfg_setByKey(k, &val[0], l);
                printKey(odev, k, &val[0], l);
                break;
            }
        }
    }
    return ATCMD_OK;
}

// Check if connected to BLE (0=no, 1=yes but not cross, 2=cross)
static ATRESULT atcmd_checkconnect(uint8_t nargs, char* argv[], void* odev) {
    if (_ctx.passThru_txfn1!=NULL && _ctx.passThru_txfn2!=NULL) {
        wconsole_println(odev, "2");
    } else if (comm_ble_isConnected()) {
        wconsole_println(odev, "1");
    } else {
        wconsole_println(odev, "0");
    }
    return ATCMD_PROCESSED;
}

// Request to cross-connect to another comm port
// 1st Parameter indicates the device to connect to: 
//  - U = physical uart (logically only from a remote BLE NUS service...) - this is also the default
//  - NC = NUS remote client (must be already connected to us)
//  - NS = NUS remote server (connection will be initiated, parameter 2 must indicate devAddr)
static ATRESULT atcmd_connect(uint8_t nargs, char* argv[], void* odev) {

    if (nargs==1 || (nargs==2 && argv[1][0]=='U')) {
        // Must have done a password login to be allowed to connect if coming from remote BLE guy
        if (!cfg_isPasswordOk()) {
            return ATCMD_GENERR;
        }
        // the sender is one side (assumed to be a remote BLE) and the comm UART is forced as the other side
        _ctx.passThru_txfn1 = (UART_TX_FN_T)odev;
        _ctx.passThru_txfn2 = &comm_uart_tx;
        // Setting these 2 attributes will mean that the pass-thru handling takes place in the at_process_line() method
        return ATCMD_OK;    
    }
    if (nargs>=2) {
        if (strncmp("NC", argv[1], 2)==0) {
            // Must have BLE NUS client connected currently
            if (comm_ble_isConnected()) {
                _ctx.passThru_txfn1 = (UART_TX_FN_T)odev;
                _ctx.passThru_txfn2 = &comm_ble_tx;
                // Setting these 2 attributes will mean that the pass-thru handling takes place in the at_process_line() method
                return ATCMD_OK;    
            } else {
                // don't connect when noone there to listen
                return ATCMD_GENERR;
            }
        } else if (strncmp("NS", argv[1], 2)==0) {
            wconsole_println(odev, "ERROR");
            wconsole_println(odev, "Not yet implemented");
            return ATCMD_BADARG;
        }
    }
    return ATCMD_GENERR;
}
// And tear down the cross-connect
static ATRESULT atcmd_disconnect(uint8_t nargs, char* argv[], void* odev) {
    // Are we connected currently?
    if (_ctx.passThru_txfn1!=NULL && _ctx.passThru_txfn2!=NULL) {
        (*_ctx.passThru_txfn1)(NULL, -1, NULL);        // Tell remote dest we're done
        (*_ctx.passThru_txfn2)(NULL, -1, NULL);       // and confirm to source
        _ctx.passThru_txfn1 = NULL;
        _ctx.passThru_txfn2 = NULL;
    } else {
        wconsole_println(odev, "Not connected");
        return ATCMD_GENERR;
    }
//    sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    return ATCMD_OK;
}

// AT+PASS,V,<password> or AT+PASS,S,<newpassword> (must already have had a password 'V' ok)
static ATRESULT atcmd_password(uint8_t nargs, char* argv[], void* odev) {
    if (nargs==2) {
        // Check password
        if (cfg_checkPassword(argv[1])) {
            return ATCMD_OK;
        }
    } else if (nargs==3) {
        if (strcmp("V", argv[1])==0) {
            if (cfg_checkPassword(argv[2])) {
                return ATCMD_OK;
            }
        } else if (strcmp("S", argv[1])==0) {
            // set password if already logged in (old pass is passed as NULL)
            if (cfg_setPassword(NULL, argv[2])) {
                return ATCMD_OK;
            }
        }
    }
    // Else not
    cfg_resetPasswordOk();
    return ATCMD_GENERR;
}

static ATRESULT atcmd_start_scan(uint8_t nargs, char* argv[], void* odev) {
    // check if a UUID is provided
    // expected pattern is AT+START,<UUID> UUID is 32 hex digits long
    // parser cuts into argv[0]=AT+START, argv[1]=<UUID>
    if (nargs>1) 
    {
        // Parse hex string to build bytes
        uint8_t uuid[16];
        if (Util_scanhex(argv[1], 16, uuid)!=16) 
//        if (sscanf(argv[1], "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
//                    &uuid[0],&uuid[1],&uuid[2],&uuid[3],&uuid[4],&uuid[5],&uuid[6],&uuid[7],
//                    &uuid[8],&uuid[9],&uuid[10],&uuid[11],&uuid[12],&uuid[13],&uuid[14],&uuid[15])!=16) 
        {
            // oops
            wconsole_println(odev, "ERROR");
            wconsole_println(odev, "Failed to parse UUID [%s]", argv[1]);
            return ATCMD_BADARG;
        }
        
        ibs_scan_set_uuid_filter(&uuid[0]);
    }
    else
    {
        ibs_scan_set_uuid_filter(NULL);
    }
    
    if (!ibs_scan_start((UART_TX_FN_T)odev)) 
    {
        return ATCMD_GENERR;
    }
    return ATCMD_OK;
}

static ATRESULT atcmd_stop_scan(uint8_t nargs, char* argv[], void* odev) {
    if (!ibs_scan_stop()) 
    {
        return ATCMD_GENERR;
    }
    return ATCMD_OK;
}

static ATRESULT atcmd_start_ib(uint8_t nargs, char* argv[], void* odev) {
    // set all the params from the args optionally
    if (nargs==7) {
        // AT_IB_START <uuid>,<major>,<minor>,<extrabyte>,<interval in ms>,<txpower>
        // All values in hex
        uint8_t uuid[16];
        unsigned int major;
        unsigned int minor;
        unsigned int extra;
        unsigned int interMS;
        int txpower;
        // any param can be ignored by making it empty eg AT+IB_START,,,,,,-10
        if (Util_scanhex(argv[1], 16, uuid)==16) {
            cfg_setUUID(uuid);
        }
        if (sscanf(argv[2], "%04x", &major)==1) {
            cfg_setMajor_Value(major);
        }
        if (sscanf(argv[3], "%04x", &minor)==1) {
            cfg_setMinor_Value(minor);
        }
        if (sscanf(argv[4], "%02x", &extra)==1) {
            cfg_setExtra_Value(extra);
        }
        if (sscanf(argv[5], "%04x", &interMS)==1) {
            cfg_setADV_IND(interMS);
        }
        if (sscanf(argv[6], "%d", &txpower)==1) {
            cfg_setTXPOWER_Level(txpower);
        }
    }
    ibb_start();
    return ATCMD_OK;
}

static ATRESULT atcmd_stop_ib(uint8_t nargs, char* argv[], void* odev) {
    ibb_stop();
    return ATCMD_OK;
}
static ATRESULT atcmd_enable_conn(uint8_t nargs, char* argv[], void* odev) {
    cfg_setConnectable(true);
    return ATCMD_OK;
}
static ATRESULT atcmd_disable_conn(uint8_t nargs, char* argv[], void* odev) {
    cfg_setConnectable(false);
    return ATCMD_OK;
}
static ATRESULT atcmd_debug_stats(uint8_t nargs, char* argv[], void* odev) {
    comm_ble_print_stats(wconsole_println, odev);
    comm_uart_print_stats(wconsole_println, odev);
    return ATCMD_PROCESSED;
}
static ATRESULT atcmd_out(uint8_t nargs, char* argv[], void* odev) {
    if (nargs<3) {
        return ATCMD_GENERR;
    }
    int pin = atoi(argv[1]);
    if( argv[2][0]=='0') {
        nrf_drv_gpiote_out_clear(pin);
    } else {
        nrf_drv_gpiote_out_set(pin);
    }
    return ATCMD_OK;
}
static ATRESULT atcmd_in(uint8_t nargs, char* argv[], void* odev) {
    if (nargs<2) {
        return ATCMD_GENERR;
    }
    int pin = atoi(argv[1]);
    wconsole_println(odev, "Input[%d] is [%s]", pin, nrf_drv_gpiote_in_is_set(pin)?"HIGH":"LOW");
    wconsole_println(odev, "%d", nrf_drv_gpiote_in_is_set(pin)?1:0);
    return ATCMD_PROCESSED;
}

// internal processing
static bool wconsole_println(void* dev, const char* l, ...) {
    UART_TX_FN_T utx_fn = (UART_TX_FN_T)dev;
    bool ret = true;
    va_list vl;
    va_start(vl, l);
    _ctx.txbuf[0] = '+';        // All AT response lines MUST start with a "+"
    vsprintf((char*)&_ctx.txbuf[1], l, vl);     // response goes in at offset 1
    int len = strlen((const char*)&_ctx.txbuf[0]);  //, MAX_TXSZ);        why no strnlen in gcc libc??
    if (len>=MAX_TXSZ) {
        // oops might just have broken stuff...
        len = MAX_TXSZ-1;
        ret = false;        // caller knows there was an issue
    }
    _ctx.txbuf[len]='\n';
    _ctx.txbuf[len+1]='\r';
    _ctx.txbuf[len+2]=0;
    len+=2;     // Don't send the null byte!
    if (utx_fn!=NULL) {
        int res = (*utx_fn)(&_ctx.txbuf[0], len, NULL);
        if (res!=0) {       // Error or flow control
            _ctx.txbuf[0] = '*';
            (*utx_fn)(&_ctx.txbuf[0], 1, NULL);      // so user knows he missed something.
            ret = false;        // caller knows there was an issue
            // Not dealing with tx ready upcalls just now
           log_noout_fn("console write FAIL %d", res);      // just for debugger to watch
       }
    } else {
        ret = false;        // caller knows there was an issue
        log_noout_fn("console write no open uart");      // just for debugger to watch
    }
    va_end(vl);
    return ret;
}
