#ifndef RGSM_TYPES_H_INCLUDED
#define RGSM_TYPES_H_INCLUDED

#include <stdint.h>
#include <stdint-gcc.h>
#include <asterisk.h>
#include <asterisk/abstract_jb.h>

#ifndef RGSM_VERSION_STR
#define RGSM_VERSION_STR                "0.0.SNAPSHOT"
#endif

#ifndef RGSM_VERSION_DATE
#define RGSM_VERSION_DATE               "N/A"
#endif

#ifndef ASTERISK_VERSION_NUM
#define ASTERISK_VERSION_NUM            10800
#endif

#define ITEMS_OF(x) (sizeof(x)/sizeof((x)[0]))
#define XSTR(x)     #x
#define STR(x)      XSTR(x)

#define MAX_ADDRESS_LENGTH              64

#define DEFAULT_TIMEOUT_AT_RESPONSE     2
#define RGSM_MAX_DTMF_LEN               256
#define RGSM_VOICE_BUF_LEN              256
#define RGSM_MAX_TRUNK_NAME_LEN         256
#define BALANCE_REQ_STR_SIZE            32


#define SP      0x20
#define TAB     0x09

#define DEFAULT_REG_TRY_CNT             5

#define ERROR_OUT_OF_MEM                -255

// this code is written by Roman
typedef int64_t format_t;

//debug macroses

#define DEBUG_AT(fmt, ...) do { \
    if (pvt->debug_ctl.at) \
        ast_log(LOG_DEBUG, fmt, ## __VA_ARGS__); \
} while (0)

#define DEBUG_CALL_SM(fmt, ...) do { \
    if (pvt->debug_ctl.callsm) \
        ast_log(LOG_DEBUG, fmt, ## __VA_ARGS__); \
} while (0)

#define DEBUG_VOICE(fmt, ...) do { \
    if (pvt->debug_ctl.voice) \
        ast_log(LOG_DEBUG, fmt, ## __VA_ARGS__); \
} while (0)


#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)

typedef struct rgsm_voice_frame_header {
    uint8_t     modem_id;
    uint16_t    data_len;
}
__attribute__ ((__packed__)) rgsm_voice_frame_header_t;

typedef struct debug_ctl {
	unsigned int at:1;
	unsigned int callsm:1;
	unsigned int rcvr:1;
	unsigned int hal:1;
	unsigned int voice:1;
} debug_ctl_t;

typedef struct gen_config {
    int                 timeout_at_response;
    debug_ctl_t         debug_ctl;
    struct ast_jb_conf  jbconf;
    int                 dev_jbsize;
} gen_config_t;

typedef struct hw_config {
    uint16_t    vid;
    uint16_t    pid;
    char        product_string[32];
} hw_config_t;

typedef struct me_config {
    int mic_gain;
    int spk_gain;
    int spk_audg;
    int mic_amp1;
    int rx_lvl;
    int tx_lvl;
    int rx_gain;
    int tx_gain;
    int ussd_dcs;
    int init_delay;
    char hex_download[128];
    char fw_image[128];
    int at_send_rate;
} me_config_t;

typedef enum {
	INC_TYPE_UNKNOWN = -1,
	INC_TYPE_DENY = 0,
	INC_TYPE_DTMF = 1,
	INC_TYPE_SPEC = 2,
	INC_TYPE_DYN = 3,
} incoming_type_t;

typedef enum {
	OUT_CALL_UNKNOWN = -1,
	OUT_CALL_DENY = 0,
	OUT_CALL_ALLOW = 1,
} outgoing_type_t;

typedef enum ggw8_baudrate {
    BR_AUTO_POWER_ON = -3,
    BR_UNKNOWN = -2,
    BR_AUTO = -1,
    BR_1200 = 0,
    BR_2400 = 1,
    BR_4800 = 2,
    BR_9600 = 3,
    BR_19200 = 4,
    BR_38400 = 5,
    BR_57600 = 6,
    BR_115200 = 7,
} ggw8_baudrate_t;

#define DEFAULT_BAUDRATE    BR_115200

typedef struct pvt_config {
    char context[80];
    incoming_type_t incoming_type;
    char incomingto[80];                         //extention to route incoming call to
    outgoing_type_t outgoing_type;
    int reg_try_cnt;
    int sim_try_cnt;
    char balance_req_str[BALANCE_REQ_STR_SIZE];
    int sms_sendinterval;
    int sms_sendattempt;
    int sms_maxpartcount;
    int sms_autodelete;
    int voice_codec;
    int sim_toolkit;                            //May 21, 2014, enable|disable
    int auto_start;                             //Oct 20, 2014: "power on" a channel on rgsm module load, defaults disable
    unsigned char autoclose_ussd_menu;
} pvt_config_t;

typedef struct chnl_config {
    char context[80];
    incoming_type_t incoming_type;
    char incomingto[80];                         //extention to route incoming call to
    outgoing_type_t outgoing_type;
    //May 17, 2013: Bz1927 - channel alias
    char alias[31];
    int sim_toolkit;                            //May 21, 2014, enable|disable
    int auto_start;                             //Oct 20, 2014: "power on" a channel on rgsm module load, defaults disable
} chnl_config_t;

typedef struct pvt_flags {
	unsigned int enable:1;
	unsigned int shutdown:1;
	unsigned int shutdown_now:1;
	unsigned int restart:1;
	unsigned int restart_now:1;
    unsigned int hard_reset:1;    //if this flag set then modem power will be OFF/ONN instead of RESET during on channel restart
	unsigned int suspend_now:1;
	unsigned int resume_now:1;
	unsigned int init:1;
	unsigned int balance_req:1;
	unsigned int subscriber_number_req:1;
	unsigned int cpin_checked:1;
	unsigned int changesim:1;
	unsigned int testsim:1;
	unsigned int sim_present:1;
	unsigned int sim_startup:1;
	unsigned int adjust_baudrate:1;
	unsigned int stk_capabilities_req:1;
	unsigned int module_type_discovered:1;  //added at Aug 2014
} pvt_flags_t;

typedef enum {
    MODULE_TYPE_UNKNOWN = -1,
    MODULE_TYPE_SIM900 = 0,
    MODULE_TYPE_SIM300 = 1,
    MODULE_TYPE_M10 = 2,
    MODULE_TYPE_SIM5320 = 3,
    MODULE_TYPE_UC15 = 4,
} module_type_t;

// gsm registartion status
typedef enum reg_state {
	REG_STATE_NOTREG_NOSEARCH = 0,
	REG_STATE_REG_HOME_NET = 1,
	REG_STATE_NOTREG_SEARCH = 2,
	REG_STATE_REG_DENIED = 3,
	REG_STATE_UNKNOWN = 4,
	REG_STATE_REG_ROAMING = 5,
} reg_state_t;

typedef enum mdm_state {
    MDM_STATE_INVAL = -1,
    MDM_STATE_DISABLE = 0,
    MDM_STATE_PWR_DOWN,
    MDM_STATE_RESET,
    MDM_STATE_WAIT_RDY,
    MDM_STATE_WAIT_CFUN,
    MDM_STATE_TEST_FUN,
    MDM_STATE_CHECK_PIN,
    MDM_STATE_WAIT_CALL_READY,
    MDM_STATE_INIT,
    MDM_STATE_RUN,
    MDM_STATE_SUSPEND,
    MDM_STATE_WAIT_SUSPEND,
    MDM_STATE_AUTO_BAUDING,
    MDM_STATE_WAIT_MODULE_DATA,
} mdm_state_t;

typedef enum call_state {
	CALL_STATE_INVAL = -1,
	CALL_STATE_DISABLE = 0,
	CALL_STATE_ONSERVICE,
	CALL_STATE_NULL,
	CALL_STATE_OUT_CALL_PROC,
	CALL_STATE_CALL_DELIVERED,
	CALL_STATE_CALL_PRESENT,
	CALL_STATE_CALL_RECEIVED,
	CALL_STATE_IN_CALL_PROC,
	CALL_STATE_ACTIVE,
	CALL_STATE_RELEASE_IND,
	CALL_STATE_OVERLAP_RECEIVING
} call_state_t;

typedef enum call_msg {
	CALL_MSG_INVAL = -1,
	CALL_MSG_NONE = 0,
	CALL_MSG_SETUP_REQ,
	CALL_MSG_PROCEEDING_IND,
	CALL_MSG_ALERTING_IND,
	CALL_MSG_SETUP_CONFIRM,
	CALL_MSG_RELEASE_REQ,
	CALL_MSG_RELEASE_IND,
	CALL_MSG_SETUP_IND,
	CALL_MSG_INFO_IND,
	CALL_MSG_SETUP_RESPONSE,
} call_msg_t;

typedef enum call_dir {
	CALL_DIR_NONE = 0,
	CALL_DIR_OUT = 1,
	CALL_DIR_IN = 2,
} call_dir_t;

#define TYPE_OF_NUMBER_UNKNOWN			0
#define TYPE_OF_NUMBER_INTERNATIONAL	1
#define TYPE_OF_NUMBER_NATIONAL			2
#define TYPE_OF_NUMBER_NETWORK			3
#define TYPE_OF_NUMBER_SUBSCRIBER		4
#define TYPE_OF_NUMBER_ALPHANUMGSM7		5
#define TYPE_OF_NUMBER_ABBREVIATED		6
#define TYPE_OF_NUMBER_RESERVED			7

#define NUMBERING_PLAN_UNKNOWN			0x0
#define NUMBERING_PLAN_ISDN_E164		0x1
#define NUMBERING_PLAN_DATA_X121		0x3
#define NUMBERING_PLAN_TELEX			0x4
#define NUMBERING_PLAN_NATIONAL			0x8
#define NUMBERING_PLAN_PRIVATE			0x9
#define NUMBERING_PLAN_ERMES			0xA
#define NUMBERING_PLAN_RESERVED			0xF

union toa {
	struct
	{
		unsigned char numbplan:4;
		unsigned char typenumb:3;
		unsigned char reserved:1;
    } __attribute__((packed)) bits;
	unsigned char full;
} __attribute__((packed));

typedef struct address {
    union toa type;
    char value[MAX_ADDRESS_LENGTH];
    int length;
} address_t;

typedef struct query_sig {
	unsigned int imei:1;
	unsigned int callwait:1;
	unsigned int hidenum:1;
	unsigned int spk_gain:1;
	unsigned int mic_gain:1;
	unsigned int cmgl:1;
} query_sig_t;

typedef struct init_settings {
	unsigned int ready:1;
	unsigned int clip:1;
	unsigned int chfa:1;
	unsigned int colp:1;
	unsigned int clvl:1;
	unsigned int cmic:1;
	unsigned int creg:1;
	unsigned int cscs:1;
	unsigned int cmee:1;
	unsigned int cclk:1;
	unsigned int cmgf:1;
	unsigned int cnmi:1;
	unsigned int fallback:1;
	unsigned int cfun:1;
	unsigned int imei_change:1;
	unsigned int imei_cmp:1;
	unsigned int init:1;	// status bit -- set after startup initialization
} init_settings_t;

enum {
	HIDENUM_UNKNOWN = -1,
	HIDENUM_SUBSCRIPTION = 0,
	HIDENUM_INVOCATION = 1,
	HIDENUM_SUPPRESSION = 2,
	HIDENUM_QUERY = 3,
};

enum {
	HIDENUM_STATUS_NOT_PROVISIONED = 0,
	HIDENUM_STATUS_PERM_MODE_PROVISIONED = 1,
	HIDENUM_STATUS_UNKNOWN = 2,
	HIDENUM_STATUS_TEMP_MODE_RESTRICTED = 3,
	HIDENUM_STATUS_TEMP_MODE_ALLOWED = 4,
};

enum {
	CALLWAIT_STATUS_UNKNOWN = -1,
	CALLWAIT_STATUS_DISABLE = 0,
	CALLWAIT_STATUS_ENABLE = 1,
	CALLWAIT_STATUS_QUERY = 2,
};

typedef enum {
	SUBCMD_CUSD_GET_BALANCE = 1,
	SUBCMD_CUSD_USER,
	SUBCMD_CUSD_MANAGER,
} subcmd_ussd_t;

enum {
	SUBCMD_CMGR_USER = 1,
};

typedef enum {
    STK_SETUP_MENU = 0,
    STK_COMMAND_REJECTED = 1,
    STK_NOTIFICATION = 2,
    STK_DISPLAY_TEXT = 3,
    STK_SELECT_ITEM = 4,
    STK_MENU_SELECTION = 5,
    STK_GET_ITEM_LIST = 6,
} stk_response_t;

typedef enum {
	MAN_CHSTATE_DISABLED = 0,
	MAN_CHSTATE_STARTING = 1,
	MAN_CHSTATE_IDLE = 2,
	MAN_CHSTATE_INCOMING_CALL = 3,
	MAN_CHSTATE_OUTGOING_CALL = 4,
	MAN_CHSTATE_ABORTED_REGFAILURE = 20,
	MAN_CHSTATE_ABORTED_NOSIM = 21,
	MAN_CHSTATE_ABORTED_HWFAILURE = 22,
	MAN_CHSTATE_ONSERVICE = 40,
} man_chstate_t;

#endif // RGSM_TYPES_H_INCLUDED
