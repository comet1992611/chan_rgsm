/*
   Copyright (C) 2012 Right and Above, LLC
   www.rightandabove.com
*/
#ifndef CHAN_RGSM_H
#define CHAN_RGSM_H

#include <asterisk.h>
#include <asterisk/lock.h>
#include <asterisk/cli.h>
#include <asterisk/frame.h>
#include <asterisk/channel.h>
#include <asterisk/manager.h>
#include <asterisk/app.h>
#include <asterisk/pbx.h>

#include "rgsm_defs.h"
#include "ggw8_hal.h"
#include "rgsm_timers.h"

//relative to AST_CONFIG_DIR
#define CONFIG_FILE		"chan_rgsm.conf"

#define DEVICE_FRAME_SIZE	    320
#define CHANNEL_FRAME_SIZE	    320
#define DEVICE_FRAME_FORMAT     AST_FORMAT_SLINEAR

//driver periodically search for new devices, this is a perisod in seconds
#define GGW8_DISCOVERY_PERIOD   10

//driver periodically pings the emulator, this is a perisod in seconds
#define SIMULATOR_PING_PERIOD	2

#define GSMPVT_NAME_PREFIX     "slot_"
#define GSMPVT_CHNAME_PREFIX   "ch_"

#define RST_DELAY_SIMSEARCH     10
#define MAX_EXPIRE_CMD_COUNTER  10
#define MAX_ERROR_CMD_COUNTER   20

#define RST_CHANNEL(pvt, delay_sec) do { \
    pvt->rst_delay_sec = delay_sec; \
    pvt->flags.restart = 1; \
    pvt->flags.restart_now = 1; \
} while (0)

struct msg_queue_entry;

extern const struct ast_channel_tech rgsm_tech;

//!at cmd queue
struct rgsm_atcmd {
	struct at_command *at;
	int id;
	uint32_t oper;
	int sub_cmd;
	char cmd_buf[256];
	int cmd_len;
	int timeout;
	struct rgsm_timer timer;
	int show;
	AST_LIST_ENTRY(rgsm_atcmd) entry;
};

typedef struct gsm_pvt gsm_pvt_t;

typedef void (*atp_handle_response_function) (gsm_pvt_t* pvt);
typedef void (*set_sim_poll_function) (gsm_pvt_t* pvt);
typedef void (*gsm_query_sim_data_function) (gsm_pvt_t* pvt);
typedef void (*check_sim_status_function) (gsm_pvt_t* pvt);
typedef void (*hangup_function) (gsm_pvt_t* pvt);
typedef void (*change_imei_function) (gsm_pvt_t* pvt);
typedef void (*setup_audio_channel_function) (gsm_pvt_t* pvt) ;
typedef void (*send_ussd_function) (gsm_pvt_t* pvt, unsigned char sub_cmd, char* ussd_str) ;
typedef void (*check_init_status_function) (gsm_pvt_t* pvt);


struct driver_callbacks
{
    atp_handle_response_function atp_handle_response;
    set_sim_poll_function set_sim_poll;
    gsm_query_sim_data_function gsm_query_sim_data;
    check_sim_status_function check_sim_status;
    hangup_function hangup;
    change_imei_function change_imei;
    setup_audio_channel_function setup_audio_channel;
    send_ussd_function send_ussd;
    check_init_status_function check_init_status;
};

//! This is a private structure to maintain the state for single GSM module
struct gsm_pvt
{
    struct ggw8_device  *ggw8_device;
	struct ast_channel  *owner;			    // Channel we belong to, possibly NULL
	struct ast_frame    fr;				    // "null" frame
    struct timeval      curr_tv;            // pvt time
    struct timezone     curr_tz;            // pvt timezone

	ast_mutex_t         lock;				// pvt lock

	pthread_t           channel_thread;		//module monitor thread handle
	int                 pwr_delay_sec;      //a timeout sleep before running the channel loop at power_on
	int                 rst_delay_sec;      //a timeout sleep before restarting the channel loop without exiting

	/*! queue for messages we are expecting */
    AST_LIST_HEAD_NOLOCK(rgsm_atcmd_queue, rgsm_atcmd) cmd_queue;
    int             cmd_queue_length;
	int             cmd_done;

	int             unique_id;              //unique id
	uint8_t         modem_id;               //modem's position on board
	char            chname[31];			    //unique channel name "ch_<unique_id>"
	char            name[31];				//user configurable channel name, like slot_X
	int             group;					// group number for group dialling

	//char io_buf[CHANNEL_FRAME_SIZE + AST_FRIENDLY_OFFSET];
	//struct ast_smoother *smoother;			//our smoother, for making 320 byte frames

	int             at_fd;                       //file descriptor to read/write at stream
	int             voice_fd;                    //file descriptor to read/write voice data

    debug_ctl_t     debug_ctl;

    rgsm_timers_t   timers;
    mdm_state_t     mdm_state;              //modem management state
    reg_state_t     reg_state;              //gsm regisration state
    call_state_t    call_state;             //call state

    //following four properties will be populated either from pvt_config or slot_XXX config
	//char context[AST_MAX_CONTEXT];			//the context for incoming calls
    //char incomingto[AST_MAX_EXTENSION];     //extention to route incoming call to
	//outgoing_type_t outgoing_type;
	//incoming_type_t incoming_type;

	chnl_config_t   chnl_config;

    call_dir_t      call_dir;
    address_t       calling_number;
    address_t       called_number;

    module_type_t   module_type;

   	int func_test_run;
	int func_test_done;
	int func_test_rate;

	// registration attempt
	int reg_try_cnt_conf;
	int reg_try_cnt_curr;

	// sim attempt
	int sim_try_cnt_conf;
	int sim_try_cnt_curr;

	int mic_gain_conf;
	int mic_gain_curr;

	int spk_gain_conf;
	int spk_gain_curr;

	int spk_audg_conf;

	int mic_amp1_conf;
	int tx_lvl_conf;
	int rx_lvl_conf;
	int tx_gain_conf;
	int rx_gain_conf;
	int ussd_dcs_conf;
	int init_delay_conf;

	int hidenum_conf;
	int hidenum_set;
	int hidenum_stat;

	struct timeval  start_call_time_mark;

    // AT channel buffer
    char            recv_buf[2048];
    int             recv_len;
    char            *recv_ptr;
    int             recv_buf_valid;
    int             recv_en;
    unsigned char	ussd_text_param;

	address_t       smsc_number;

	int             sms_user_length;
	char            sms_user_pdubuf[1024];
	int             sms_user_pdulen;
	int             sms_user_valid;
	int             sms_user_done;
	int             sms_user_inuse;

    //sms state variables
  	int             cmt_pdu_wait;
	int             cds_pdu_wait;
	int             pdu_len;
	int             now_send_pdu_id;
	int             now_send_pdu_len;
	int             now_send_attempt;

    //sms buffers
	char            now_send_pdu_buf[512];
	char            prepare_pdu_buf[512];

	int rssi;
	int ber;
	int linkmode;
	int linksubmode;

/*
	int sms_storage_position;
	unsigned int auto_delete_sms:1;
	unsigned int use_ucs2_encoding:1;
	int u2diag;
	char number[1024];
	int fr_format;
*/
	//data from gsm modem
	char        imei[32];
	char        model[32];
	char        manufacturer[32];
	char        firmware[64];

	char        new_imei[32];

	//data from simcard
	char        imsi[32];
	char        iccid[32];
	char        iccid_ban[32];
	char        operator_name[32];
	char        operator_code[32];
	address_t   subscriber_number;

    //buffer for parsing various data
    char parser_buf[256];

   	int callwait_conf;
	int callwait_status;
	int active_line;
	int active_line_stat;
	int active_line_contest;
	int wait_line;
	char wait_line_num[AST_MAX_EXTENSION];
	int wait_line_contest;
	int is_play_tone;

	// USSD
	char ussd_databuf[320];
	int ussd_datalen;
	int ussd_dcs;
	int ussd_valid;
	int ussd_done;
	subcmd_ussd_t ussd_subcmd;

	char balance_str[512];
	char balance_req_str[BALANCE_REQ_STR_SIZE];

    // SMS
	struct timeval sms_sendinterval;
	int sms_sendattempt;
	int sms_maxpartcount;
	int sms_autodelete;
	int sms_ref;
	int sms_notify_enable;
	char sms_notify_context[AST_MAX_CONTEXT];
	char sms_notify_extension[AST_MAX_EXTENSION];

	// voice channel data
	//int voice_fd;

	struct ast_frame frame;
	char voice_recv_buf[RGSM_VOICE_BUF_LEN + AST_FRIENDLY_OFFSET];
	// rtp (voice)
#if ASTERISK_VERSION_NUM < 10800
	int frame_format;
#else
	format_t frame_format;
#endif
	int payload_type;

    //dtmf symbol received
    char dtmf_sym;
	// rtp event receiving
	unsigned int event_is_now_recv_begin;
	unsigned int event_is_now_recv_end;
	//unsigned short event_recv_seq_num;
	// rtp event transmit
	unsigned int event_is_now_send;
	//

	u_int32_t send_sid_curr;
	u_int32_t send_drop_curr;
	u_int32_t send_frame_curr;
	u_int32_t recv_frame_curr;

	u_int32_t send_sid_total;
	u_int32_t send_drop_total;
	u_int32_t send_frame_total;
	u_int32_t recv_frame_total;

	time_t last_time_incoming;
	time_t last_time_outgoing;

	time_t call_time_incoming;
	time_t call_time_outgoing;

    query_sig_t     querysig;

    init_settings_t init_settings;

	/* flags */
	pvt_flags_t     flags;

	ggw8_baudrate_t baudrate;

    int stk_capabilities;          //the number of menu items in STK menu
    int stk_cmd_done;
    int stk_sync_notification_count;
    char stk_status[256];
	char stk_action_id[128];
    struct mansession* stk_mansession;

    int power_man_disable;      //set to 1 to disable a user to on/off/restart a channel
    man_chstate_t man_chstate;  //channel's event manager state
    //May 31, 2013: fix for Bz1940 - Recover a channel when gsm module hangup
    int exp_cmd_counter;
    //Sept, 2014: Recover a channel when gsm module alway returns ERROR on at command
    int err_cmd_counter;

    volatile int busy;           //set to 1 when busy while flashing
    struct ast_cli_args *args;
    struct driver_callbacks functions;

    unsigned char last_mdm_state;

    // May 12, 2017: allow to send AT commands after then a RDY received and until power off or reset
    int allow_cli_at;
};

struct gateway {
    struct ggw8_device*         ggw8_device;        //the pointer to Gateway board HAL structure
    struct gsm_pvt*             gsm_pvts[MAX_MODEMS_ON_BOARD];        //max eight gsm modules may consist on ggw8 board
    int                         uid;                 //unique ID assigned at initialization
    //Jul 6, 2014: propagate the gateway uid to simsimulator
    int				uid_propagated;
    AST_LIST_ENTRY(gateway)     link;
};

typedef AST_LIST_HEAD_NOLOCK(gateways, gateway) gateways_t;

extern ast_mutex_t      rgsm_lock;
extern gateways_t       gateways;
extern struct timeval   rgsm_start_time;
extern uint32_t         channel_id;

//configs
extern gen_config_t     gen_config;
extern me_config_t      sim900_config;
extern me_config_t      sim5320_config;
extern me_config_t      uc15_config;
extern hw_config_t      hw_config;
extern pvt_config_t     pvt_config ;

void rgsm_init_pvt_state(struct gsm_pvt *pvt);

//! gsm_pvt power management routines
void rgsm_pvt_power_on(struct ast_cli_args *a, struct gsm_pvt *chnl, int pwr_delay_sec, ggw8_baudrate_t baudrate);
void rgsm_pvt_power_off(struct ast_cli_args *a, struct gsm_pvt *chnl);
void rgsm_pvt_power_reset(struct ast_cli_args *a, struct gsm_pvt *chnl);

//resets the sim card fields in pvt struct to ther initial values
void gsm_reset_sim_data(struct gsm_pvt* pvt);
void gsm_reset_modem_data(struct gsm_pvt* pvt);
void gsm_shutdown_channel(struct gsm_pvt* pvt);
void gsm_abort_channel(struct gsm_pvt* pvt, man_chstate_t reason);
void gsm_next_sim_search(struct gsm_pvt* pvt);
void gsm_query_sim_data(struct gsm_pvt* pvt);
void gsm_start_simpoll(struct gsm_pvt* pvt);

//! AT Command Queue access
int rgsm_atcmd_queue_append(struct gsm_pvt* pvt,
										int id,
										u_int32_t oper,
										int subcmd,
										int timeout,
										int show,
										const char *fmt, ...);

int rgsm_atcmd_trysend(struct gsm_pvt* pvt);
int rgsm_atcmd_queue_free(struct gsm_pvt* pvt, struct rgsm_atcmd *cmd);
int rgsm_atcmd_queue_flush(struct gsm_pvt* pvt);

/**
    Helper function to perform sleeps.
    pvt->lock and rgsm_lock MUST BE LOCKED before calling this function
 */
void rgsm_usleep(struct gsm_pvt *pvt, int us);


#endif // CHAN_RGSM_H

