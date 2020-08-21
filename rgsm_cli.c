#include "chan_rgsm.h"
#include "rgsm_defs.h"
#include "rgsm_utilities.h"
#include "termios.h"
#include "at.h"
#include "rgsm_dao.h"

#include <sys/stat.h>
#include <fcntl.h>
#include "rgsm_sim900.h"
#include "rgsm_manager.h"
#include "rgsm_dfu.h"

#include "pthread.h"
#include "asterisk.h"
#include <asterisk/logger.h>
#include <asterisk/lock.h>
#include <asterisk/linkedlists.h>
#include <asterisk/select.h>

//#define LOG_PREPARE_ERROR() ast_log(LOG_ERROR, "<%s>: sqlite3_prepare_v2(): %d: %s\n", pvt->name, res, sqlite3_errmsg(smsdb))
//#define LOG_STEP_ERROR()    ast_log(LOG_ERROR, "<%s>: sqlite3_step(): %d: %s\n", pvt->name, res, sqlite3_errmsg(smsdb))

#if ASTERISK_VERSION_NUM < 10800
typedef cli_fn cli_fn_type;
#else
typedef char*(*cli_fn_type)(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
#endif

struct rgsm_cli_action {
	char name[AST_MAX_CMD_LEN];
	cli_fn_type handler;
};

#define RGSM_CLI_CH_ACTION(act, fn) {.name = act, .handler = fn}
#define RGSM_CLI_DEV_ACTION(act, fn) {.name = act, .handler = fn}

struct rgsm_cli_channel_param {
	int id;
	char name[AST_MAX_CMD_LEN];
};

#define RGSM_CLI_CH_PARAM(prm, prmid) {.id = prmid, .name = prm}

#define RGSM_CLI_PARAM_COUNT(prm) sizeof(prm) / sizeof(prm[0])

// rgsm CLI channel actions handlers proto
static char *channel_action_power(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *channel_action_ussd(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *channel_action_at(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *channel_action_sms(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *channel_action_play(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *channel_action_imei(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *channel_action_flash(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);

//! Unused actions. Uncomment and implement if need
//static char *channel_action_suspresume(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
//static char *channel_action_param(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
//static char *chanel_action_debug(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);

static char *device_action_show(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static char *device_action_dfu(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);

//rgsm CLI channel actions
static struct rgsm_cli_action rgsm_cli_channel_acts[] = {
	RGSM_CLI_CH_ACTION("enable", channel_action_power),
	RGSM_CLI_CH_ACTION("disable", channel_action_power),
	RGSM_CLI_CH_ACTION("restart", channel_action_power),
	RGSM_CLI_CH_ACTION("power", channel_action_power),
	RGSM_CLI_CH_ACTION("ussd", channel_action_ussd),
	RGSM_CLI_CH_ACTION("at", channel_action_at),
	RGSM_CLI_CH_ACTION("sms", channel_action_sms),
    RGSM_CLI_CH_ACTION("play", channel_action_play),
    RGSM_CLI_CH_ACTION("imei", channel_action_imei),
    RGSM_CLI_CH_ACTION("flash", channel_action_flash),

    //! Unused actions. Uncomment and implement if need
	//RGSM_CLI_CH_ACTION("suspend", channel_action_suspresume),
	//RGSM_CLI_CH_ACTION("resume", channel_action_suspresume),
	//RGSM_CLI_CH_ACTION("get", channel_action_param),
	//RGSM_CLI_CH_ACTION("set", channel_action_param),
	//RGSM_CLI_CH_ACTION("query", channel_action_param),
	//RGSM_CLI_CH_ACTION("debug", channel_action_debug),
};

//rgsm CLI device actions
static struct rgsm_cli_action
rgsm_cli_device_acts[] = {
	RGSM_CLI_DEV_ACTION("show", device_action_show),
	RGSM_CLI_DEV_ACTION("dfu", device_action_dfu),
};

static char channel_actions_usage[256];
static char device_actions_usage[256];

static char *complete_ch_name(const char *begin, int count, int all)
{
    struct gateway* gw;
	struct gsm_pvt *pvt = NULL;
	char *res = NULL;
	int beginlen;
	int which = 0;
	int i;

    beginlen = strlen(begin);

	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            pvt = gw->gsm_pvts[i];
            if (pvt && !strncasecmp((const char *)pvt->name, begin, beginlen) && (++which > count)) {
                res = ast_strdup(pvt->name);
                goto exit_;
            }
        }
	}

	// compare with special case "all"
	if(all && !strncmp(begin, "all", beginlen) && (++which > count)) {
        res = ast_strdup("all");
    }
exit_:
	return res;
}

static char *complete_ch_act(const char *begin, int count)
{
	char *res = NULL;
	int beginlen;
	int which = 0;
	int i;

	beginlen = strlen(begin);
	for(i=0; i < RGSM_CLI_PARAM_COUNT(rgsm_cli_channel_acts); i++) {
		// get actions name
		if((!strncmp(begin, rgsm_cli_channel_acts[i].name, beginlen)) && (++which > count)){
			res = ast_strdup(rgsm_cli_channel_acts[i].name);
			break;
		}
    }

	return res;
}

static char *complete_ch_act_on_off(const char *begin, int count){

	char *res;
	int beginlen;
	int which;

	//
	res = NULL;
	which = 0;
	beginlen = strlen(begin);
	//
	if((!res) && (!strncmp(begin, "on", beginlen)) && (++which > count))
		res = ast_strdup("on");
	//
	if((!res) && (!strncmp(begin, "off", beginlen)) && (++which > count))
		res = ast_strdup("off");

	return res;
}

static char *complete_ch_act_flash(const char *begin, int count){

	char *res;
	int beginlen;
	int which;

	//
	res = NULL;
	which = 0;
	beginlen = strlen(begin);
	//
	if((!res) && (!strncmp(begin, "sim900", beginlen)) && (++which > count)) {
		res = ast_strdup("sim900");
	}

	return res;
}

static char *complete_dev_act(const char *begin, int count)
{
	char *res = NULL;
	int beginlen;
	int which = 0;
	int i;

	beginlen = strlen(begin);
	for(i=0; i < RGSM_CLI_PARAM_COUNT(rgsm_cli_device_acts); i++) {
		// get actions name
		if((!strncmp(begin, rgsm_cli_device_acts[i].name, beginlen)) && (++which > count)){
			res = ast_strdup(rgsm_cli_device_acts[i].name);
			break;
		}
    }

	return res;
}

char *cli_show_modinfo(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{

	struct timeval curr_time_mark;
	char buf[32];
	//
	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm show modinfo";
			e->usage = "Usage: rgsm show modinfo\n";
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
		}

	if(a->argc != 3)
		return CLI_SHOWUSAGE;

	// -- module info
	ast_cli(a->fd, "  RGSM Module Info:\n");
	// show rgsm module version
	ast_cli(a->fd, "  -- module version: %s\n", STR(RGSM_VERSION_STR));
	// date
	ast_cli(a->fd, "  -- module version date: %s\n", STR(RGSM_VERSION_DATE));
	// show rgsm module uptime
	gettimeofday(&curr_time_mark, NULL);

	ast_cli(a->fd, "  -- started: %s", ctime(&rgsm_start_time.tv_sec));
	curr_time_mark.tv_sec -= rgsm_start_time.tv_sec;
	ast_cli(a->fd, "  -- uptime: %s (%ld sec)\n",
			second_to_dhms(buf, curr_time_mark.tv_sec), curr_time_mark.tv_sec);

	return CLI_SUCCESS;
}

//------------------------------------------------------------------------------
// cli_show_channels()
//------------------------------------------------------------------------------
char* cli_show_channels(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
    struct gateway *gw;
	struct gsm_pvt *pvt;
	int i;
	char remote_number[MAX_ADDRESS_LENGTH];
    ggw8_info_t    *ggw8_info;
    uint16_t       sys_id;

	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm show channels";
			e->usage = "Usage: rgsm show channels\n";
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
		}

	if(a->argc != 3)
		return CLI_SHOWUSAGE;

//ast_cli(a->fd,"show channels\n");

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);

	if (!gateways.first) {
        ast_cli(a->fd, "  There are no GGW-8 cards installed\n");
	} else {
        //
        AST_LIST_TRAVERSE (&gateways, gw, link) {
            //
            ggw8_info = ggw8_get_device_info(gw->ggw8_device);
            sys_id = ggw8_get_device_sysid(gw->ggw8_device);

            ast_cli(a->fd, "GGW-8: UID=%d, SYSID=%.3u:%.3u, HWRev=%s, FWRev=%s, AvChnls=%s%s%s%s%s%s%s%s, Mode=%s\n",
                    gw->uid,
                    (sys_id >> 8),
                    (sys_id & 0xff),
                    ggw8_info->hw_version,
                    ggw8_info->fw_version,
                    ((ggw8_info->gsm_presence & 0x80) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x40) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x20) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x10) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x08) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x04) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x02) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x01) ? "Y" : "-"),
                    ((ggw8_get_device_mode(gw->ggw8_device) == DFU) ? "DFU" : "GSM"));


            ast_cli(a->fd, "| %-2.2s | %-8.8s | %-16.16s | %-5.5s | %-8.8s | %-10.10s | %-15.15s | %-8.8s | %-16.16s |\n",
                    "ID",
                    "Name",
                    "Number",
                    "Power",
                    "SIM",
                    "Registered",
                    "CallState",
                    "Dir",
                    "Call To/From");

            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
                //ast_cli(a->fd, "modem_indx=%d, pvt=%p\n", i, gw->gsm_pvts[i]);
                if ((pvt = gw->gsm_pvts[i])) {
                    // lock pvt channel
                    ast_mutex_lock(&pvt->lock);
                    if (pvt->call_dir == CALL_DIR_OUT) {
                        sprintf(remote_number, "%s%s", pvt->calling_number.type.full == 145 ? "+" : "", pvt->calling_number.value);
                    } else if (pvt->call_dir == CALL_DIR_IN) {
                        sprintf(remote_number, "%s%s", pvt->called_number.type.full == 145 ? "+" : "", pvt->called_number.value);
                    } else {
                        remote_number[0] = '\0';
                    }
                    ast_cli(a->fd, "| %-2.2d | %-8.8s | %-16.16s | %-5.5s | %-8.8s | %-10.10s | %-15.15s | %-8.8s | %-16.16s |\n",
                            pvt->unique_id,
                            pvt->name,
                            pvt->subscriber_number.value,
                            (pvt->power_man_disable ? "N/A" : onoff_str(pvt->flags.enable)),
                            (pvt->flags.sim_present)?("inserted"):(""),
                            reg_state_print_short(pvt->reg_state),
                            rgsm_call_state_str(pvt->call_state),
                            rgsm_call_dir_str(pvt->call_dir),
                            remote_number);
                    // unlock pvt channel
                    ast_mutex_unlock(&pvt->lock);
                }
            }
        }
	}

	// unlock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);

	return CLI_SUCCESS;
}

char* cli_show_channel(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	struct gsm_pvt *chnl;
	char buf[256];


	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm show channel";
			e->usage = "Usage: rgsm show channel <channel_name>\n";
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			// try to generate complete channel name
			if(a->pos == 3)
				return complete_ch_name(a->word, a->n, 0);

			return NULL;
		}

	if(a->argc != 4)
		return CLI_SHOWUSAGE;

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);
	// get channel by name
	chnl = find_ch_by_name(a->argv[3]);
	if(chnl) {
		// lock pvt channel
		ast_mutex_lock(&chnl->lock);
		ast_cli(a->fd, "  Channel %02d: %s\n", chnl->unique_id, chnl->name);
		ast_cli(a->fd, "  -- device = %p\n", chnl->ggw8_device);
		ast_cli(a->fd, "  -- power = %s\n", chnl->power_man_disable ? "N/A" : onoff_str(chnl->flags.enable));
		// module props
        ast_cli(a->fd, "  -- gsm module = %s\n", chnl->model);
        //ast_cli(a->fd, "  -- manufacturer = %s\n", chnl->manufacturer);
        ast_cli(a->fd, "  -- firmware = %s\n", chnl->firmware);
		ast_cli(a->fd, "  -- imei = %s\n", chnl->imei);
		ast_cli(a->fd, "  -- RSSI = %s\n", rssi_print(buf, chnl->rssi));
		ast_cli(a->fd, "  -- BER = %s\n", ber_print(chnl->ber));
        //sim mprops
        ast_cli(a->fd, "  -- sim status = %s\n", (chnl->flags.sim_present)?("inserted"):(""));
		ast_cli(a->fd, "  -- imsi = %s\n", chnl->imsi);
		ast_cli(a->fd, "  -- iccid = %s\n", chnl->iccid);
		ast_cli(a->fd, "  -- operator = %s (%s)\n", chnl->operator_name, chnl->operator_code);
		ast_cli(a->fd, "  -- reg status = %s\n", reg_state_print(chnl->reg_state));
		ast_cli(a->fd, "  -- subscriber number = %s\n", chnl->subscriber_number.value);

//		ast_cli(a->fd, "  -- call wait status = %s\n", eggsm_callwait_status_str(chnl->callwait_status));
//		ast_cli(a->fd, "  -- balance request string = %s\n", chnl->balance_req_str);
		ast_cli(a->fd, "  -- balance = [%s]\n", chnl->balance_str);

        ast_cli(a->fd, "  -- context = %s\n", chnl->chnl_config.context);
        ast_cli(a->fd, "  -- incoming = %s\n", incoming_type_str(chnl->chnl_config.incoming_type));

		if(chnl->chnl_config.incoming_type == INC_TYPE_SPEC) {
		  ast_cli(a->fd, "  -- incomingto = %s\n", chnl->chnl_config.incomingto);
		}
        ast_cli(a->fd, "  -- outgoing = %s\n", outgoing_type_str(chnl->chnl_config.outgoing_type));
        ast_cli(a->fd, "  -- sim_toolkit = %s,%d\n",
                               chnl->chnl_config.sim_toolkit ? "enable" : "disable",
                               chnl->stk_capabilities);


		ast_cli(a->fd, "  -- last incoming call time = %s (%ld sec)\n",
				second_to_dhms(buf, chnl->last_time_incoming), chnl->last_time_incoming);
		ast_cli(a->fd, "  -- last outgoing call time = %s (%ld sec)\n",
				second_to_dhms(buf, chnl->last_time_outgoing), chnl->last_time_outgoing);

		ast_cli(a->fd, "  -- total incoming call time = %s (%ld sec)\n",
				second_to_dhms(buf, chnl->call_time_incoming), chnl->call_time_incoming);
		ast_cli(a->fd, "  -- total outgoing call time = %s (%ld sec)\n",
				second_to_dhms(buf, chnl->call_time_outgoing), chnl->call_time_outgoing);
		ast_cli(a->fd, "  -- total call time = %s (%ld sec)\n",
				second_to_dhms(buf, (chnl->call_time_incoming + chnl->call_time_outgoing)),
										(chnl->call_time_incoming + chnl->call_time_outgoing));

		ast_cli(a->fd, "  -- TX pass (last) = %u frames\n", chnl->send_frame_curr);
		ast_cli(a->fd, "  -- TX drop (last) = %u frames\n", chnl->send_drop_curr);
		ast_cli(a->fd, "  -- TX sid (last) = %u frames\n", chnl->send_sid_curr);
		ast_cli(a->fd, "  -- RX (last) = %u frames\n", chnl->recv_frame_curr);

		ast_cli(a->fd, "  -- TX pass (total) = %u frames\n", chnl->send_frame_total);
		ast_cli(a->fd, "  -- TX drop (total) = %u frames\n", chnl->send_drop_total);
		ast_cli(a->fd, "  -- TX sid (total) = %u frames\n", chnl->send_sid_total);
		ast_cli(a->fd, "  -- RX (total) = %u frames\n", chnl->recv_frame_total);
		// unlock pvt channel
		ast_mutex_unlock(&chnl->lock);
    }
	else
		ast_cli(a->fd, "  Channel <%s> not found\n", a->argv[3]);
	// unlock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);
	return CLI_SUCCESS;
}

char* cli_show_netinfo(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
    struct gateway *gw;
	struct gsm_pvt *pvt;
	int i;
	char buf[256];
    ggw8_info_t    *ggw8_info;
    uint16_t       sys_id;

	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm show netinfo";
			e->usage = "Usage: rgsm show netinfo\n";
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
    }

	if(a->argc != 3) return CLI_SHOWUSAGE;

	ast_mutex_lock(&rgsm_lock);

	if (!gateways.first) {
        ast_cli(a->fd, "  There are no GGW-8 cards installed\n");
	} else {
        //
        AST_LIST_TRAVERSE (&gateways, gw, link) {
            //
            ggw8_info = ggw8_get_device_info(gw->ggw8_device);
            sys_id = ggw8_get_device_sysid(gw->ggw8_device);

            ast_cli(a->fd, "GGW-8: UID=%d, SYSID=%.3u:%.3u, HWRev=%s, FWRev=%s, AvChnls=%s%s%s%s%s%s%s%s, Mode=%s\n",
                    gw->uid,
                    (sys_id >> 8),
                    (sys_id & 0xff),
                    ggw8_info->hw_version,
                    ggw8_info->fw_version,
                    ((ggw8_info->gsm_presence & 0x80) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x40) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x20) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x10) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x08) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x04) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x02) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x01) ? "Y" : "-"),
                    ((ggw8_get_device_mode(gw->ggw8_device) == DFU) ? "DFU" : "GSM"));

            ast_cli(a->fd, "| %-2.2s | %-8.8s | %-5.5s | %-8.8s | %-10.10s | %-15.15s | %-20.20s | %-15.15s | %-5.5s | %-5.5s |\n",
				"ID",
				"Name",
				"Power",
				"SIM",
				"Registered",
				"IMSI",
				"ICCID",
				"Number",
				"RSSI",
				"BER");

            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
                if ((pvt = gw->gsm_pvts[i])) {
                    // lock pvt channel
                    ast_mutex_lock(&pvt->lock);
                    ast_cli(a->fd, "| %-2.2d | %-8.8s | %-5.5s | %-8.8s | %-10.10s | %-15.15s | %-20.20s | %-15.15s | %-5.5s | %-5.5s |\n",
                            pvt->unique_id,
                            pvt->name,
                            (pvt->power_man_disable ? "N/A" : onoff_str(pvt->flags.enable)),
                            (pvt->flags.sim_present)?("inserted"):(""),
                            reg_state_print_short(pvt->reg_state),
                            pvt->imsi,
                            pvt->iccid,
                            pvt->subscriber_number.value,
                            rssi_print_short(buf, pvt->rssi),
                            ber_print_short(pvt->ber));
                    // unlock pvt channel
                    ast_mutex_unlock(&pvt->lock);
                }
            }
        }
	}

	// unlock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);

	return CLI_SUCCESS;
}

char* cli_show_calls(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
    struct gateway *gw;
	struct gsm_pvt *pvt;
	int i;
    ggw8_info_t    *ggw8_info;
    uint16_t       sys_id;
    struct timeval time_mark;
	char calling_number[MAX_ADDRESS_LENGTH];
    char called_number[MAX_ADDRESS_LENGTH];

    switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm show calls";
			e->usage = "Usage: rgsm show calls\n";
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
    }

	if(a->argc != 3) return CLI_SHOWUSAGE;

	ast_mutex_lock(&rgsm_lock);

	if (!gateways.first) {
        ast_cli(a->fd, "  There are no GGW-8 cards installed\n");
	} else {
        //
        gettimeofday(&time_mark, NULL);
        AST_LIST_TRAVERSE (&gateways, gw, link) {
            //
            ggw8_info = ggw8_get_device_info(gw->ggw8_device);
            sys_id = ggw8_get_device_sysid(gw->ggw8_device);

            ast_cli(a->fd, "GGW-8: UID=%d, SYSID=%.3u:%.3u, HWRev=%s, FWRev=%s, AvChnls=%s%s%s%s%s%s%s%s, Mode=%s\n",
                    gw->uid,
                    (sys_id >> 8),
                    (sys_id & 0xff),
                    ggw8_info->hw_version,
                    ggw8_info->fw_version,
                    ((ggw8_info->gsm_presence & 0x80) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x40) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x20) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x10) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x08) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x04) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x02) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x01) ? "Y" : "-"),
                    ((ggw8_get_device_mode(gw->ggw8_device) == DFU) ? "DFU" : "GSM"));

            ast_cli(a->fd, "| %-2.2s | %-8.8s | %-15.15s | %-9.9s | %-20.20s | %-20.20s | %-8.8s |\n",
				"ID",
				"Channel",
				"State",
				"Direction",
				"Calling",
				"Called",
				"Duration");

            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
                if ((pvt = gw->gsm_pvts[i])) {
                    // lock pvt channel
                    ast_mutex_lock(&pvt->lock);
                    if (pvt->call_state > CALL_STATE_NULL) {
                        //
                        sprintf(calling_number, "%s%s", pvt->calling_number.type.full == 145 ? "+" : "", pvt->calling_number.value);
                        sprintf(called_number, "%s%s", pvt->called_number.type.full == 145 ? "+" : "", pvt->called_number.value);

                        ast_cli(a->fd, "| %-2.2d | %-8.8s | %-15.15s | %-9.9s | %-20.20s | %-20.20s | %8.d |\n",
                                pvt->unique_id,
                                pvt->name,
                                rgsm_call_state_str(pvt->call_state),
                                rgsm_call_dir_str(pvt->call_dir),
                                calling_number,
                                called_number,
                                //Jul 2, 2013: fix Bz1990 - "show calls" may display wrong duration
                                pvt->start_call_time_mark.tv_sec ? (int)(time_mark.tv_sec - pvt->start_call_time_mark.tv_sec) : 0);
                    }
                    // unlock pvt channel
                    ast_mutex_unlock(&pvt->lock);
                }
            }
        }
	}

	// unlock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);

	return CLI_SUCCESS;
}

char* cli_show_devinfo(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
    struct gateway *gw;
	struct gsm_pvt *pvt;
	int i;
    ggw8_info_t    *ggw8_info;
    uint16_t       sys_id;

    switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm show devinfo";
			e->usage = "Usage: rgsm show devinfo\n";
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
    }

	if(a->argc != 3) return CLI_SHOWUSAGE;

	ast_mutex_lock(&rgsm_lock);

	if (!gateways.first) {
        ast_cli(a->fd, "  There are no GGW-8 cards installed\n");
	} else {
        //
        AST_LIST_TRAVERSE (&gateways, gw, link) {
            //
            ggw8_info = ggw8_get_device_info(gw->ggw8_device);
            sys_id = ggw8_get_device_sysid(gw->ggw8_device);

            ast_cli(a->fd, "GGW-8: UID=%d, SYSID=%.3u:%.3u, HWRev=%s, FWRev=%s, AvChnls=%s%s%s%s%s%s%s%s, Mode=%s\n",
                    gw->uid,
                    (sys_id >> 8),
                    (sys_id & 0xff),
                    ggw8_info->hw_version,
                    ggw8_info->fw_version,
                    ((ggw8_info->gsm_presence & 0x80) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x40) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x20) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x10) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x08) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x04) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x02) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x01) ? "Y" : "-"),
                    ((ggw8_get_device_mode(gw->ggw8_device) == DFU) ? "DFU" : "GSM"));



            ast_cli(a->fd, "| %-2.2s | %-8.8s | %-5.5s | %-15.15s | %-20.20s | %-50.50s |\n",
				"ID",
				"Name",
				"Power",
				"IMEI",
				"Hardware",
				"Firmware");

            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
                if ((pvt = gw->gsm_pvts[i])) {
                    // lock pvt channel
                    ast_mutex_lock(&pvt->lock);
                    ast_cli(a->fd, "| %-2.2d | %-8.8s | %-5.5s | %-15.15s | %-20.20s | %-50.50s |\n",
                            pvt->unique_id,
                            pvt->name,
                            (pvt->power_man_disable ? "N/A" : onoff_str(pvt->flags.enable)),
                            pvt->flags.enable ? pvt->imei : "N/A",
                            pvt->flags.enable ? pvt->model : "N/A",
                            pvt->flags.enable ? pvt->firmware : "N/A");
                    // unlock pvt channel
                    ast_mutex_unlock(&pvt->lock);
                }
            }
        }
	}

	// unlock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);

	return CLI_SUCCESS;
}

char *cli_channel_actions(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){

	int i;
	cli_fn_type subhandler;
	//
	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm channel";
			sprintf(channel_actions_usage, "Usage: rgsm channel <channel_name> <action> [...]\n");
			e->usage = channel_actions_usage;
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			// try to generate complete channel name
			if(a->pos == 2)
				return complete_ch_name(a->word, a->n, 1);
			// try to generate complete channel action
			else if(a->pos == 3)
				return complete_ch_act(a->word, a->n);
			// generation channel action parameters ...
			else if(a->pos >= 4) {
				// from this point delegate generation function to
				// action depended CLI entries
				subhandler = NULL;
				// search action CLI entry
				for(i=0; i < RGSM_CLI_PARAM_COUNT(rgsm_cli_channel_acts); i++) {
					// get actions by name
					if(strstr(a->line, rgsm_cli_channel_acts[i].name)) {
						subhandler = rgsm_cli_channel_acts[i].handler;
						break;
                    }
                }
				if(subhandler) return subhandler(e, cmd, a);
            }
			//
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
		}

	if(a->argc <= 3){
		sprintf(channel_actions_usage, "Usage: rgsm channel <channel_name> <action> [...]\n");
		return CLI_SHOWUSAGE;
    }

	subhandler = NULL;
	// search action CLI entry
	for(i=0; i < RGSM_CLI_PARAM_COUNT(rgsm_cli_channel_acts); i++){
		// get actions by name
		if(!strcmp(a->argv[3], rgsm_cli_channel_acts[i].name)){
			subhandler =rgsm_cli_channel_acts[i].handler;
			break;
        }
    }
	if (subhandler) return subhandler(e, cmd, a);

	// if command not handled
	return CLI_FAILURE;
}


//channel actions implementation
//------------------------------------------------------------------------------
// _generating_prepare()
// get from asterisk-1.6.0.x main/cli.c parse_args()
//------------------------------------------------------------------------------
static int _generating_prepare(char *s, int *argc, char *argv[]){

	char *cur;
	int x = 0;
	int quoted = 0;
	int escaped = 0;
	int whitespace = 1;

	if(s == NULL)	/* invalid, though! */
		return -1;

	cur = s;
	/* scan the original string copying into cur when needed */
	for (; *s ; s++) {
		if (x >= AST_MAX_ARGS - 1) {
			ast_log(LOG_WARNING, "Too many arguments, truncating at %s\n", s);
			break;
		}
		if (*s == '"' && !escaped) {
			quoted = !quoted;
			if (quoted && whitespace) {
				/* start a quoted string from previous whitespace: new argument */
				argv[x++] = cur;
				whitespace = 0;
			}
		} else if ((*s == ' ' || *s == '\t') && !(quoted || escaped)) {
			/* If we are not already in whitespace, and not in a quoted string or
			   processing an escape sequence, and just entered whitespace, then
			   finalize the previous argument and remember that we are in whitespace
			*/
			if (!whitespace) {
				*cur++ = '\0';
				whitespace = 1;
			}
		} else if (*s == '\\' && !escaped) {
			escaped = 1;
		} else {
			if (whitespace) {
				/* we leave whitespace, and are not quoted. So it's a new argument */
				argv[x++] = cur;
				whitespace = 0;
			}
			*cur++ = *s;
			escaped = 0;
		}
	}
	/* Null terminate */
	*cur++ = '\0';
	/* XXX put a NULL in the last argument, because some functions that take
	 * the array may want a null-terminated array.
	 * argc still reflects the number of non-NULL entries.
	 */
	argv[x] = NULL;
	*argc = x;
	return 0;
}

#define CH_PWR_OFF      0
#define CH_PWR_ON       1
#define CH_PWR_RESET    2

//------------------------------------------------------------------------------
// cli_channel_action_power()
//------------------------------------------------------------------------------
static char *channel_action_power(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int pwr_delay;
	int op, i;
    struct gateway* gw;
	struct gsm_pvt *chnl;

	char *gline;
	char *gargv[AST_MAX_ARGS];
	int gargc;

	//
	switch(cmd) {
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "is ch_act_pwr subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			gline = ast_strdupa(a->line);
			if(!(_generating_prepare(gline, &gargc, gargv))) {
				if((a->pos == 4) && (!strcmp(gargv[3], "power")))
					return complete_ch_act_on_off(a->word, a->n);
            }
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
    }

	//power without on/off
	if((a->argc == 4) && (!strcmp(a->argv[3], "power"))){
		sprintf(channel_actions_usage, "Usage: rgsm channel <channel_name> power <on|off>\n");
		return CLI_SHOWUSAGE;
    }

	op = 0;
	// get enable|disable
	if(a->argc == 4){
		if(!strcmp(a->argv[3], "enable"))
			op = CH_PWR_ON;
		else if(!strcmp(a->argv[3], "disable"))
			op = CH_PWR_OFF;
		else if(!strcmp(a->argv[3], "restart"))
			op = CH_PWR_RESET;
		else
			return CLI_FAILURE;
    } else if((a->argc == 5) && (!strcmp(a->argv[3], "power"))) {
		if(!strcmp(a->argv[4], "on"))
			op = CH_PWR_ON;
		else if(!strcmp(a->argv[4], "off"))
			op = CH_PWR_OFF;
		else
			return CLI_FAILURE;
		}
	else
		return CLI_FAILURE;

	// check name param for wildcard "all"
	if (!strcmp(a->argv[2], "all")) {
        pwr_delay = 0;
        ast_mutex_lock(&rgsm_lock);
	    AST_LIST_TRAVERSE (&gateways, gw, link)
        {
            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
                chnl = gw->gsm_pvts[i];
                if (!chnl) continue;
                switch (op) {
                    case CH_PWR_OFF:
                        rgsm_pvt_power_off(a, chnl);
                        break;
                    case CH_PWR_ON:
                        ast_cli(a->fd, "  <%s>: ", chnl->name);
                        rgsm_pvt_power_on(a, chnl, pwr_delay++, BR_115200);
                        break;
                    case CH_PWR_RESET:
                        rgsm_pvt_power_reset(a, chnl);
                        break;
                    default:
                        break;
                }
            }
        }
        ast_mutex_unlock(&rgsm_lock);
		return CLI_SUCCESS;
	} // all

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);
	// get channel by name
	chnl = find_ch_by_name(a->argv[2]);
	if (chnl) {
        switch (op) {
            case CH_PWR_OFF:
                rgsm_pvt_power_off(a, chnl);
                break;
            case CH_PWR_ON:
                ast_cli(a->fd, "  <%s>: ", chnl->name);
                rgsm_pvt_power_on(a, chnl, 1, BR_115200);
                break;
            case CH_PWR_RESET:
                rgsm_pvt_power_reset(a, chnl);
                break;
            default:
                break;
        }
	} else {
		ast_cli(a->fd, "  channel <%s> not found\n", a->argv[2]);
	}
	// unlock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);
	//
	return CLI_SUCCESS;
}

static char *channel_action_ussd(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct gsm_pvt *pvt;

	int is_ussd_queued;
	int ussd_timeout;

	const char *msg;

	//
	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "is action_ussd subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
    }

	if(a->argc != 5){
		sprintf(channel_actions_usage, "Usage: rgsm channel <channel_name> ussd <ussd string>\n");
		return CLI_SHOWUSAGE;
    }

	// check name param for wildcard "all"
	if(!strcmp(a->argv[2], "all")){
		ast_cli(a->fd, "wildcard \"all\" not supported -- use channel name\n");
		return CLI_SUCCESS;
    }

	//
	is_ussd_queued = 0;
	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);
	// get channel by name
	pvt = find_ch_by_name(a->argv[2]);

	//
	if(pvt){
		// lock pvt channel
		ast_mutex_lock(&pvt->lock);

		msg = send_ussd(pvt, a->argv[4], &is_ussd_queued, SUBCMD_CUSD_USER);

		if (is_ussd_queued) {
            ast_cli(a->fd, "  <%s>: queued USSD \"%s\"...\n", pvt->name, a->argv[4]);
		} else {
			ast_cli(a->fd, "  <%s>: unable to queue USSD \"%s\": %s\n", pvt->name, a->argv[4], msg);
		}

		if(is_ussd_queued){
		    //resp_valid = 0;
		    //resp_datalen = 0;
			ussd_timeout = 60*2;
			//resp_done = 0;
			while (ussd_timeout-- > 0) {
			    rgsm_usleep(pvt, 500000);
				//
				if(pvt->ussd_done){
				    break;
                }
            }
			// check for time is out
			if(ussd_timeout > 0){
				if(pvt->ussd_valid/*resp_valid*/) {
				    //
				    ast_cli(a->fd, "  <%s>: \"%s\"\n", pvt->name, pvt->ussd_databuf);
                } else {
					ast_cli(a->fd, "  <%s>: ussd response invalid\n", pvt->name);
                }
            } else {
				ast_cli(a->fd, "  <%s>: wait for USSD response - time is out\n", pvt->name);
            }
			//
			pvt->call_state = CALL_STATE_NULL;
        }
		// unlock pvt channel
		ast_mutex_unlock(&pvt->lock);
    } else {
		ast_cli(a->fd, "  Channel <%s> not found\n", a->argv[2]);
    }
	// unlock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);

	return CLI_SUCCESS;
}

static char *channel_action_at(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct gsm_pvt *pvt;
	char *cp;
	struct rgsm_atcmd *ac;
	int i;

	switch(cmd)
	{
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "is action_at subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
	}

	if (a->argc < 4) {
		sprintf(channel_actions_usage, "Usage: rgsm channel <channel_name> at [<at command>]\n");
		return CLI_SHOWUSAGE;
	}

	// check name param for wildcard "all"
	if (!strcmp(a->argv[2], "all")) {
		ast_cli(a->fd, "wildcard \"all\" not supported -- use channel name\n");
		return CLI_SUCCESS;
	}

    //ast_cli(a->fd, "  AT for <%s>: begin\n", a->argv[2]);

	// lock eggsm subsystem
	ast_mutex_lock(&rgsm_lock);

    //ast_cli(a->fd, "  AT for <%s>: mutex rgsm acquired\n", a->argv[2]);

	// get channel by name
	pvt = find_ch_by_name(a->argv[2]);
	//
	if (!pvt) {
   		ast_cli(a->fd, "Channel <%s> not found\n", a->argv[2]);
        goto _cleanup;
	}

    ast_mutex_lock(&pvt->lock);

    //<<< May 12, 2017: allow to send AT commands after then a RDY/START received and until power off or reset
	//if (pvt->mdm_state != MDM_STATE_RUN) {
	if (pvt->allow_cli_at == 0) {
    //>>>
   		ast_cli(a->fd, "Could not send AT command at a moment. Try when modem RDY/START\n");
        goto _cleanup_2;
	}

    if (a->argc == 4) {
        ast_cli(a->fd, "total=%d: %s\n", pvt->cmd_queue_length, pvt->cmd_done?"done":"in progress");
        i = 0;
        AST_LIST_TRAVERSE(&pvt->cmd_queue, ac, entry)
        {
            i++;
            ast_cli(a->fd, "|%03d|%.*s|\n", i, ac->cmd_len-1, ac->cmd_buf);
        }
    } else if ((a->argc == 5) && (!strcasecmp(a->argv[4], "flush"))) {
        // flush at command queue
        rgsm_atcmd_queue_flush(pvt);
    } else {

        cp = (char *)a->argv[4];
        while (*cp)
        {
            if(*cp == '!') *cp = '?';
            cp++;
        }
        ast_cli(a->fd, "  <%s>: send command \"%s\"...", pvt->name, a->argv[4]);
        /* check management state */
        if (rgsm_atcmd_queue_append(pvt, AT_UNKNOWN, AT_OPER_EXEC, 0, 30, 1, "%s", a->argv[4]) > 0) {
            ast_cli(a->fd, " - ok\n");
        } else {
            ast_cli(a->fd, " - failed\n");
        }
    }
_cleanup_2:
    ast_mutex_unlock(&pvt->lock);
_cleanup:
	ast_mutex_unlock(&rgsm_lock);
	return CLI_SUCCESS;
}


static char *channel_action_play(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct gsm_pvt *pvt;
	int fd, psize, prate, nbytes, pcount, codec, n;
	char *pbuf;
	char rbuf[256];
	struct rgsm_timer timer;
	struct timeval timeout;

	switch(cmd)
	{
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "is ch_act_at subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
	}

	if (a->argc < 8) {
		sprintf(channel_actions_usage, "Usage: rgsm channel <channel_name> play <file> <packet_size> <packet_rate_ms> <codec>\n");
		return CLI_SHOWUSAGE;
	}

	// check name param for wildcard "all"
	if (!strcmp(a->argv[2], "all")) {
		ast_cli(a->fd, "wildcard \"all\" not supported -- use channel name\n");
		return CLI_SUCCESS;
	}

    //ast_cli(a->fd, "  AT for <%s>: begin\n", a->argv[2]);

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);
	// get channel by name
	pvt = find_ch_by_name(a->argv[2]);
    ast_mutex_unlock(&rgsm_lock);
	//
	if (!pvt) {
   		ast_cli(a->fd, "  Channel <%s> not found\n", a->argv[2]);
        return CLI_SUCCESS;
	}

	if (pvt->voice_fd != -1) {
   		ast_cli(a->fd, "  Can't play for busy channel\n");
        return CLI_SUCCESS;
	}


    if ((fd = open(a->argv[4], O_RDONLY)) == -1) {
        ast_cli(a->fd, "Cannot open voice file [%s]\n", a->argv[4]);
        goto _cleanup;
    }

    psize = atoi(a->argv[5]);
    if (psize <= 10 || psize > 255) {
        ast_cli(a->fd, "Invalid arg value: packet_size=[%s], shouls be between 10 to 255 bytes\n", a->argv[5]);
        goto _cleanup;
    }

    prate = atoi(a->argv[6]);
    if (prate < 5) {
        ast_cli(a->fd, "Invalid arg value: packet_rate_ms=[%s], should not be less than 5 ms\n", a->argv[6]);
        goto _cleanup;
    }

    codec = atoi(a->argv[7]);

    ast_cli(a->fd, "Start play [%s]\n", a->argv[4]);

    pbuf = malloc(psize);
    if (!pbuf) {
        ast_cli(a->fd, "No memory\n");
        goto _cleanup;
    }

    pvt->voice_fd = ggw8_open_voice(pvt->ggw8_device, pvt->modem_id, (uint16_t)codec, gen_config.dev_jbsize);
    if (pvt->voice_fd == -1) {
        ast_cli(a->fd, "Can't open voice for channel channel\n");
    }

    pcount = 0;
    tv_set(timeout, 0, prate*1000);
    rgsm_timer_set(timer, timeout);
    rgsm_timer_stop(timer);

    while (1) {
        nbytes = read(fd, pbuf, psize);
        if (nbytes == -1) {
            ast_cli(a->fd, "Error reading voice file: err=[%s]\n", strerror(errno));
            break;
        } else if (nbytes == 0) {
            break;
        }

        if (is_rgsm_timer_enable(timer)) {
            while (is_rgsm_timer_active(timer)) us_sleep(500);
        }

        if(write(pvt->voice_fd, pbuf, nbytes) < 0) {
            ast_cli(a->fd, "Error writing voice data to device: err=[%s]\n", strerror(errno));
            break;
        }

        pcount++;
        if (nbytes < psize) break;

        rgsm_timer_set(timer, timeout);

        //dummy read
        n = read(pvt->voice_fd, rbuf, sizeof(rbuf));
    }

    ast_cli(a->fd, "Stop play [%s]: packets transmitted [%d]\n", a->argv[4], pcount);

    free(pbuf);

_cleanup:
	if (fd != -1) close(fd);
	if (pvt->voice_fd != -1) {
        ggw8_close_voice(pvt->ggw8_device, pvt->modem_id);
        pvt->voice_fd = -1;
	}
	return CLI_SUCCESS;
}

char *channel_action_imei(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
    char msgbuf[256];
    int rc;
    int completed = 1;

    ast_cli(a->fd, "rgsm: <%s>: IMEI change started. It may take few minutes. !!! DON'T INTERRUPT IT !!!\n", a->argv[2]);

    *msgbuf = '\0';
    rc = imei_change(a->argv[2], a->argv[4], msgbuf, sizeof(msgbuf), &completed, a);
    if (rc) {
        ast_cli(a->fd, "rgsm: <%s>: IMEI change FAILED: %s. Log follows >>>\n", a->argv[2], msgbuf);
        //in case of failure send AMI event immediately
        rgsm_man_event_imei_change_complete(a->argv[2], rc, msgbuf);
    } else {
        //!!! SIM900 will complete imei change asynchronously in channel work thread
        if (!completed) {
            ast_cli(a->fd, "rgsm: <%s>: IMEI change continue. Log follows >>>\n", a->argv[2]);
        }
        else {
            ast_cli(a->fd, "rgsm: <%s>: IMEI change SUCCEED\n", a->argv[2]);
            rgsm_man_event_imei_change_complete(a->argv[2], 0, a->argv[4]);
        }
    }

    return CLI_SUCCESS;
}

void *get_imei(struct gsm_pvt* pvt)
{
	int i;
	char msgbuf[256];

	pvt->man_chstate = MAN_CHSTATE_STARTING;
	rgsm_man_event_channel_state(pvt);

	//0.1sec
	us_sleep(100000);

	// set enable flag
	//pvt->flags.enable = 1;
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: set baudrate 115200\n", pvt->name);
	if (ggw8_baudrate_ctl(pvt->ggw8_device, pvt->modem_id, BR_115200))
	  {
	     strncpy(msgbuf, "Couldn't set baudrate 115200", sizeof(msgbuf));
	     ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
	     goto _exit;
	 }
           // send boot control command
	 ast_log(AST_LOG_DEBUG, "rgsm: <%s>: supply modem power for boot\n", pvt->name);
	 if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_POWERON))
	 {
	     strncpy(msgbuf, "Couldn't supply modem power", sizeof(msgbuf));
	     ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
	     goto _exit;
	 }
         pvt->at_fd = ggw8_open_at(pvt->ggw8_device, pvt->modem_id);
	 if (pvt->at_fd == -1) {
	     strncpy(msgbuf, "Can't open AT channel", sizeof(msgbuf));
	     ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
	     //pvt->flags.enable = 0;
	     goto _exit;
	 }

	 us_sleep(1000000);

	 for(i = 0; i<100; i++){
		if (pvt->mdm_state == MDM_STATE_WAIT_CALL_READY) break;
		us_sleep(100000);
	 }
	 if (pvt->mdm_state == MDM_STATE_WAIT_CALL_READY){
	     rgsm_atcmd_queue_append(pvt, AT_GSN, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
	 } else ast_cli(pvt->args->fd, "get_imei: GSM module %s not MDM_STATE_RUN\n", pvt->name);
	 for(i = 0; i<50; i++){
		if (strlen(pvt->imei)) break;
		us_sleep(100000);
	 }
	 if(strlen(pvt->imei)) ast_cli(pvt->args->fd, "get_imei: GSM module IMEI = %s\n", pvt->imei);
	    else ast_cli(pvt->args->fd, "get_imei: GSM module IMEI not get.\n");

	 gsm_shutdown_channel(pvt);
	 ast_log(AST_LOG_DEBUG, "rgsm: <%s> wait for channel disabling...\n", pvt->name);
	// wait for channel shutdown
	 while (1) {
	    //
	    if (!pvt->flags.enable) break;
	    us_sleep(10000);   //sleep 10ms
	 }
	 ast_log(AST_LOG_DEBUG, "rgsm: <%s> disabled\n", pvt->name);
	 // reset shutdown flag
	 pvt->flags.shutdown = 0;
	 pvt->flags.shutdown_now = 0;
	 _exit:
	 us_sleep(2000000);   //sleep 2s
         return CLI_SUCCESS;
}


void *flash_one_channel(void *arg)
{
    struct gsm_pvt *pvt = (struct gsm_pvt*)arg;
    char msgbuf[256];
//    char oldimeibuf[20];
    int rc = -1;
    int was_enabled = 0;
    pvt->busy = 1;
    module_type_t module_type = MODULE_TYPE_UNKNOWN;

//    int completed;

    if (pvt->power_man_disable) {
        strcpy(msgbuf, "Flash GSM module already in progress");
//        ast_mutex_unlock(&pvt->lock);
//        ast_mutex_unlock(&rgsm_lock);
    }
    else {
	    ast_mutex_lock(&pvt->lock);
	    ast_mutex_lock(&rgsm_lock);
	    was_enabled = pvt->flags.enable;
            //disable manual power managemeent
	    pvt->power_man_disable = 1;
//            ast_mutex_lock(&pvt->lock);
	    ast_mutex_unlock(&pvt->lock);
	    ast_mutex_unlock(&rgsm_lock);

	    // get imei
//	    if (!strlen(pvt->imei)) {
//		get_imei(pvt);
//	    }
//            if (strlen(pvt->imei)) memcpy(oldimeibuf, pvt->imei, strlen(pvt->imei));

//	    ast_log(AST_LOG_ERROR, "FOC: Starting flashing channels %s\n", pvt->name);
            ast_cli(pvt->args->fd, "rgsm: <%s>: GSM module flash started. It may take few minutes. !!! DON'T INTERRUPT IT !!!\n", pvt->name);
            //remove all locks

	    *msgbuf = '\0';
	    module_type = MODULE_TYPE_SIM900;

            switch (module_type) {
            case MODULE_TYPE_SIM900:
                rc = sim900_fw_update(pvt, msgbuf, sizeof(msgbuf));
                break;
            default:
                break;
	    }

//	    pvt->imei[0] = 0;
//	    get_imei(pvt);
//	    if (strlen(pvt->imei)) {
/*		if(!strcmp((const char *)pvt->imei, oldimeibuf))
		{
	          *msgbuf = '\0';
  	          rc = imei_change(pvt->name, oldimeibuf, msgbuf, sizeof(msgbuf), &completed);
	          if (rc) {
	              ast_cli(pvt->args->fd, "rgsm: <%s>: IMEI change FAILED: %s\n", pvt->name, msgbuf);
	              //in case of failure send AMI event immediately
	              rgsm_man_event_imei_change_complete(pvt->name, rc, msgbuf);
  	          } else {
  	              //!!! SIM900 will complete imei change asynchronously in channel work thread
	              if (!completed) {
	                  ast_cli(pvt->args->fd, "rgsm: <%s>: IMEI change continue...\n", pvt->name);
	              }
	              else {
	                  ast_cli(pvt->args->fd, "rgsm: <%s>: IMEI change SUCCEED\n", pvt->name);
	                  rgsm_man_event_imei_change_complete(pvt->name, 0, oldimeibuf);
	              }
	            }
		} else {
                     ast_cli(pvt->args->fd, "\n");
		  }
	    }
*/

	    }

            //enable manual power management
            ast_mutex_lock(&pvt->lock);
            pvt->power_man_disable = 0;
            ast_mutex_unlock(&pvt->lock);

            if (was_enabled) {
                // lock rgsm subsystem before power on a channel
                ast_mutex_lock(&rgsm_lock);
                //May 23, 2013: skip fixed baud rates and demand the auto-bauding
                //This valid for SIM900
                rgsm_pvt_power_on(NULL, pvt, 0, BR_AUTO_POWER_ON);
                ast_mutex_unlock(&rgsm_lock);
            }
//        }
	if (rc) {
        	  ast_cli(pvt->args->fd, "rgsm: <%s>: Failed to flash GSM module: %s\n", pvt->name, msgbuf);
		} else {
			  ast_cli(pvt->args->fd, "rgsm: <%s>: GSM module flashed\n", pvt->name);
		       }
	if (was_enabled) {
	  	           ast_cli(pvt->args->fd, "rgsm: <%s>: re-enable channel and wait for registering\n", pvt->name);
			 }
      pvt->busy = 0;
      return CLI_SUCCESS;
}

char *channel_action_flash(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
    struct gateway* gw;
    struct gsm_pvt* pvt[MAX_MODEMS_ON_BOARD];
    char msgbuf[256];
    char *gline;
    char *gargv[AST_MAX_ARGS];
    int gargc;
    unsigned short i;

    module_type_t module_type = MODULE_TYPE_UNKNOWN;

    pthread_t  thread[MAX_MODEMS_ON_BOARD];



	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "is ch_act_at subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			gline = ast_strdupa(a->line);
			if(!(_generating_prepare(gline, &gargc, gargv))) {
				if((a->pos == 4) && (!strcmp(gargv[3], "flash")))
					return complete_ch_act_flash(a->word, a->n);
            }
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
		}

	if(a->argc < 5){
		sprintf(channel_actions_usage, "Usage: rgsm channel <channel_name> flash <sim900|...>\n");
		return CLI_SHOWUSAGE;
        }

	if (!strcmp(a->argv[4], "sim900")) {
        module_type = MODULE_TYPE_SIM900;
	} else {
		ast_cli(a->fd, "Only sim900 now allowed to flash\n");
		return CLI_SUCCESS;
	}

	// check name param for wildcard "all"
	if (!strcmp(a->argv[2], "all")) {

//		ast_cli(a->fd, "wildcard \"all\" not supported -- use channel name\n");

//		   ast_log(AST_LOG_ERROR, "Starting flash all channels\n");

	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            	ast_mutex_lock(&rgsm_lock);
                pvt[i] = gw->gsm_pvts[i];
            	ast_mutex_unlock(&rgsm_lock);

                if (!pvt[i]) continue;
                pvt[i]->args = a;
                thread[i] = AST_PTHREADT_NULL;
//		   ast_cli(a->fd, "Try flash channel: %s\n", pvt[i]->name);
                if (ast_pthread_create_detached(&thread[i], NULL, flash_one_channel, pvt[i]) < 0) {
		   ast_log(AST_LOG_ERROR, "Unable to create flash_one_channel() thread: %s\n", pvt[i]->name);
//		   ast_cli(a->fd, "Unable to create flash_one_channel() thread: %s\n", pvt[i]->name);
		}
	    	us_sleep(1000000);
	    }
	}
	    while(1) { // Check that flashing for all channei is completed.
	    	us_sleep(500000);
	    	for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) if(pvt[i]->busy) break;
	    	if(i>=MAX_MODEMS_ON_BOARD) break;
	    }
//	    	ast_mutex_unlock(&rgsm_lock);
	    	us_sleep(100000);
//	    ast_log(AST_LOG_ERROR, "Flash all modems completed.\n");
	    		   ast_cli(a->fd, "Flash all modems completed.\n");

	    return CLI_SUCCESS;
	}


	ast_mutex_lock(&rgsm_lock);
	pvt[0] = find_ch_by_name(a->argv[2]);
        ast_mutex_unlock(&rgsm_lock);
	//
	if(!pvt[0]){
	    strcpy(msgbuf, "Channel not found");
	}
	else {
//	        ast_log(AST_LOG_ERROR, "Starting flash channel %s\n", pvt[0]->name);

		pvt[0]->args = a;
		flash_one_channel((void*)pvt[0]);
	     }

    return CLI_SUCCESS;
}

static char *cli_sms_complete_group(const char *begin, int count){

	char *res;
	int beginlen;
	int which;

	//
	res = NULL;
	which = 0;
	beginlen = strlen(begin);

	// sent storage
	if((!res) && (!strncmp(begin, "sent", beginlen)) && (++which > count))
		res = ast_strdup("sent");
	// outbox storage
	if((!res) && (!strncmp(begin, "outbox", beginlen)) && (++which > count))
		res = ast_strdup("outbox");
	// inbox storage
	if((!res) && (!strncmp(begin, "inbox", beginlen)) && (++which > count))
		res = ast_strdup("inbox");
	// discard storage
	if((!res) && (!strncmp(begin, "discard", beginlen)) && (++which > count))
		res = ast_strdup("discard");

	return res;
}

static char *cli_sms_complete_operation(const char *begin, int count){

	char *res;
	int beginlen;
	int which;

	//
	res = NULL;
	which = 0;
	beginlen = strlen(begin);

	// read message
	if((!res) && (!strncmp(begin, "read", beginlen)) && (++which > count))
		res = ast_strdup("read");
	// delete message
	if((!res) && (!strncmp(begin, "delete", beginlen)) && (++which > count))
		res = ast_strdup("delete");
	// prompt to show specified count of last messages
	if((!res) && (!strncmp(begin, "last", beginlen)) && (++which > count))
		res = ast_strdup("last");
	//
	return res;
}

static char *channel_action_sms(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
    //ast_cli(a->fd, "  Channel acttion sms subhandler NOT INMPEMENTED\n");

	struct gsm_pvt*	pvt;
	struct gateway* gw;
	int i, row, res, smscnt, part, index;
	char *str0, *str1;
	struct sqlite3_stmt *sql0;

	struct timeval time_data, time_data2;
	struct ast_tm time_buf, time_buf2, *time_ptr, *time_ptr2;


	//
	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "This is a channel acttion sms subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			if(a->pos == 4)
				return cli_sms_complete_group(a->word, a->n);
			else if(a->pos == 5)
				return cli_sms_complete_operation(a->word, a->n);
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
    }

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);

	// show SMS stat
	if(a->argc == 4){
		// check for channel is exist
		if(strcmp(a->argv[2], "all") && (!find_ch_by_name(a->argv[2]))){
			ast_cli(a->fd, "  channel \"%s\" not found\n", a->argv[2]);
			// unlock rgsm subsystem
			ast_mutex_unlock(&rgsm_lock);
			return CLI_SUCCESS;
        }
		//
		AST_LIST_TRAVERSE (&gateways, gw, link)
        {
            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
                pvt = gw->gsm_pvts[i];
                if (!pvt) continue;

                // get channel for stat
                if(!strcmp(a->argv[2], "all") || !strcmp(a->argv[2], pvt->name)){
                    //channel lock required to be consistent with dao_query_int()
                    ast_mutex_lock(&pvt->lock);
                    // get inbox unread message
                    smscnt = 0;
                    str0 = sqlite3_mprintf("SELECT COUNT(DISTINCT msgid) FROM '%q-inbox' WHERE status=1;", pvt->chname);
                    dao_query_int(str0, 1, pvt, &smscnt);
                    // print result
                    if(smscnt) {
                        ast_cli(a->fd, "  \"%s\": %d new messages\n", pvt->name, smscnt);
                    } else {
                        ast_cli(a->fd, "  \"%s\": no new messages\n", pvt->name);
                    }
                    ast_mutex_unlock(&pvt->lock);
                }
            }
        }

		// unlock rgsm subsystem
		ast_mutex_unlock(&rgsm_lock);
		return CLI_SUCCESS;
    }

	// check name param for wildcard "all"
	if(!strcmp(a->argv[2], "all")){
		ast_cli(a->fd, "  \"all\" not supported for operations -- use channel name\n");
		// unlock eggsm subsystem
		ast_mutex_unlock(&rgsm_lock);
		return CLI_SUCCESS;
    }

	// get channel by name
	pvt = find_ch_by_name(a->argv[2]);
	if(!pvt){
		ast_cli(a->fd, "  channel \"%s\" not found\n", a->argv[2]);
		ast_mutex_unlock(&rgsm_lock);
		return CLI_SUCCESS;
    }

	// inbox
	if(!strcmp(a->argv[4], "inbox")){
		// check for presented arguments
		if((a->argc == 5) ||
			((a->argc == 6) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete")))) ||
				((a->argc >= 6) && (!strcmp(a->argv[5], "last"))) ||
					((a->argc >= 7) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))) && (!strcmp(a->argv[6], "last")))){

			// set message count for print
			smscnt = 0;
			if((a->argc == 7) && (!strcmp(a->argv[5], "last")))
				smscnt = atoi(a->argv[6]);
			else if((a->argc == 8) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))) && (!strcmp(a->argv[6], "last")))
				smscnt = atoi(a->argv[7]);

			if(smscnt <= 0) smscnt = 10;

			// get list of messages
			str0 = sqlite3_mprintf("SELECT msgid,oatype,oaname,received,status FROM '%q-inbox' GROUP BY msgid ORDER BY received DESC;",
                          pvt->chname);
			while(1){
				res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
				if(res == SQLITE_OK){
					row = 0;
					while(1){
						res = sqlite3_step(sql0);
						if(res == SQLITE_ROW){
							row++;
							if(row > smscnt) break;
							time_data.tv_sec = sqlite3_column_int64(sql0, 3);
							if((time_ptr = ast_localtime(&time_data, &time_buf, NULL)))
								ast_cli(a->fd, "%0d: %04d-%02d-%02d-%02d:%02d:%02d \"%s%s\"%s\n",
												sqlite3_column_int(sql0, 0),
												time_ptr->tm_year + 1900,
												time_ptr->tm_mon+1,
												time_ptr->tm_mday,
												time_ptr->tm_hour,
												time_ptr->tm_min,
												time_ptr->tm_sec,
												(sqlite3_column_int(sql0, 1) == 145)?("+"):(""),
												sqlite3_column_text(sql0, 2),
												(sqlite3_column_int(sql0, 4))?(" (new)"):(""));
							else
								ast_cli(a->fd, "%0d: %ld \"%s%s\"%s\n",
												sqlite3_column_int(sql0, 0),
												time_data.tv_sec,
												(sqlite3_column_int(sql0, 1) == 145)?("+"):(""),
												sqlite3_column_text(sql0, 2),
												(sqlite3_column_int(sql0, 4))?(" (new)"):(""));
                        }
						else if(res == SQLITE_DONE)
							break;
						else if(res == SQLITE_BUSY){
							// unlock eggsm subsystem
							ast_mutex_unlock(&rgsm_lock);
							usleep(1);
							// lock eggsm subsystem
							ast_mutex_lock(&rgsm_lock);
							continue;
                        }
						else{
						    LOG_STEP_ERROR();
							break;
                        }
                    }
					//
					if(!row) ast_cli(a->fd, "  no messages in inbox\n");
					sqlite3_finalize(sql0);
					break;
                }
				else if(res == SQLITE_BUSY){
					ast_mutex_unlock(&rgsm_lock);
					usleep(1);
					ast_mutex_lock(&rgsm_lock);
					continue;
                }
				else{
					LOG_PREPARE_ERROR();
					break;
                }
            }
			sqlite3_free(str0);
        }
		else if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))))) {
			//
			if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read"))))) {
				if(a->argc == 6)
					index = atoi(a->argv[5]);
				else
					index = atoi(a->argv[6]);

				// get message content
				str0 = sqlite3_mprintf("SELECT scatype,scaname,oatype,oaname,sent,received,part,content FROM '%q-inbox' WHERE msgid=%d ORDER BY part;",
                           pvt->chname, index);
				while(1) {
					res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
					if(res == SQLITE_OK){
						row = 0;
						part = 0;
						while(1) {
							res = sqlite3_step(sql0);
							if(res == SQLITE_ROW){
								row++;
								part++;
								if(row == 1) {
									// SMS Center Address
									ast_cli(a->fd, "SMS Center Address: %s%s\n",
													(sqlite3_column_int(sql0, 0) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 1));
									// Originating Address
									ast_cli(a->fd, "From: %s%s\n",
													(sqlite3_column_int(sql0, 2) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 3));
									// Sent time
									time_data.tv_sec = sqlite3_column_int64(sql0, 4);
									if((time_ptr = ast_localtime(&time_data, &time_buf, NULL))) {
										ast_cli(a->fd, "Sent: %04d-%02d-%02d %02d:%02d:%02d\n",
												time_ptr->tm_year + 1900,
												time_ptr->tm_mon+1,
												time_ptr->tm_mday,
												time_ptr->tm_hour,
												time_ptr->tm_min,
												time_ptr->tm_sec);
									} else {
										ast_cli(a->fd, "Sent timestamp: %ld\n", time_data.tv_sec);
									}
									// Received time
									time_data.tv_sec = sqlite3_column_int64(sql0, 5);
									if((time_ptr = ast_localtime(&time_data, &time_buf, NULL))) {
										ast_cli(a->fd, "Received: %04d-%02d-%02d %02d:%02d:%02d\n",
												time_ptr->tm_year + 1900,
												time_ptr->tm_mon+1,
												time_ptr->tm_mday,
												time_ptr->tm_hour,
												time_ptr->tm_min,
												time_ptr->tm_sec);
									} else {
										ast_cli(a->fd, "Received timestamp: %ld\n", time_data.tv_sec);
									}
									// start border
									ast_cli(a->fd, ">> ");
                                }
								// missed text mark
								if(part != sqlite3_column_int(sql0, 6)){
									ast_cli(a->fd, "*some text missed*");
									part = sqlite3_column_int(sql0, 6);
                                }
								// print message
								ast_cli(a->fd, "%s", sqlite3_column_text(sql0, 7));
                            }
							else if(res == SQLITE_DONE) {
								break;
							}
							else if(res == SQLITE_BUSY) {
								ast_mutex_unlock(&rgsm_lock);
								usleep(1);
								ast_mutex_lock(&rgsm_lock);
								continue;
                            }
							else{
								LOG_STEP_ERROR();
								break;
                            }
                        }
						//
						if(!row)
							ast_cli(a->fd, "  message \"%s\" not found\n", (a->argc == 6)?(a->argv[5]):(a->argv[6]));
						else{
							// end border
							ast_cli(a->fd, " <<\n");
							// mark this message as read
							str1 = sqlite3_mprintf("UPDATE '%q-inbox' SET status=0 WHERE msgid=%d;", pvt->chname, index);
							ast_mutex_lock(&pvt->lock);
							dao_exec_stmt(str1, 1, pvt);
							ast_mutex_unlock(&pvt->lock);
                        }
						sqlite3_finalize(sql0);
						break;
                    }
					else if(res == SQLITE_BUSY){
						ast_mutex_unlock(&rgsm_lock);
						usleep(1);
						ast_mutex_lock(&rgsm_lock);
						continue;
                    }
					else{
						LOG_PREPARE_ERROR();
						break;
                    }
                }
				sqlite3_free(str0);
            }
			else if(!strcmp(a->argv[5], "delete")){
				// delete message
				index = atoi(a->argv[6]);
				str0 = sqlite3_mprintf("DELETE FROM '%q-inbox' WHERE msgid=%d;", pvt->chname, index);
				ast_mutex_lock(&pvt->lock);
				dao_exec_stmt(str0, 1, pvt);
				ast_mutex_unlock(&pvt->lock);
            }
			else {
				ast_cli(a->fd, "  unknown operation \"%s\" in inbox\n", a->argv[5]);
			}
        }
    } // end inbox
	// outbox
	else if(!strcmp(a->argv[4], "outbox")){
		// check for presented arguments
		if((a->argc == 5) || ((a->argc == 6) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))))) {
			// get list of messages
			str0 = sqlite3_mprintf("SELECT enqueued,msgno,destination,flash FROM '%q-outbox' ORDER BY enqueued;",
                          pvt->chname);
			res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
			while(1){
				if(res == SQLITE_OK){
					row = 0;
					while(1){
						res = sqlite3_step(sql0);
						if(res == SQLITE_ROW){
							row++;
							time_data.tv_sec = sqlite3_column_int64(sql0, 0);
							if((time_ptr = ast_localtime(&time_data, &time_buf, NULL)))
								ast_cli(a->fd, "%0d: %04d-%02d-%02d-%02d:%02d:%02d \"%s\"%s\n",
												sqlite3_column_int(sql0, 1),
												time_ptr->tm_year + 1900,
												time_ptr->tm_mon+1,
												time_ptr->tm_mday,
												time_ptr->tm_hour,
												time_ptr->tm_min,
												time_ptr->tm_sec,
												sqlite3_column_text(sql0, 2),
												(sqlite3_column_int(sql0, 3))?(" (flash)"):(""));
							else
								ast_cli(a->fd, "%0d: %ld \"%s\"%s\n",
												sqlite3_column_int(sql0, 1),
												time_data.tv_sec,
												sqlite3_column_text(sql0, 2),
												(sqlite3_column_int(sql0, 3))?(" (flash)"):(""));
                        }
						else if(res == SQLITE_DONE) {
							break;
						}
						else if(res == SQLITE_BUSY){
							ast_mutex_unlock(&rgsm_lock);
							usleep(1);
							ast_mutex_lock(&rgsm_lock);
							continue;
                        }
						else{
							LOG_STEP_ERROR();
							break;
                        }
                    }
					//
					if(!row) ast_cli(a->fd, "  no messages in outbox\n");
					sqlite3_finalize(sql0);
					break;
                }
				else if(res == SQLITE_BUSY){
					ast_mutex_unlock(&rgsm_lock);
					usleep(1);
					ast_mutex_lock(&rgsm_lock);
					continue;
                }
				else{
					LOG_PREPARE_ERROR();
					break;
                }
            }
			sqlite3_free(str0);
        }
		else if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))))) {
			//
			if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read"))))) {
				if(a->argc == 6)
					index = atoi(a->argv[5]);
				else
					index = atoi(a->argv[6]);

				// get message content
				str0 = sqlite3_mprintf("SELECT destination,enqueued,content FROM '%q-outbox' WHERE msgno=%d;",
                           pvt->chname, index);
				while(1){
					res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
					if(res == SQLITE_OK){
						row = 0;
						while(1){
							res = sqlite3_step(sql0);
							if(res == SQLITE_ROW) {
								row++;
								// Destination Address
								ast_cli(a->fd, "Destination Address: %s\n", sqlite3_column_text(sql0, 0));
								// Enqueued time
								time_data.tv_sec = sqlite3_column_int64(sql0, 1);
								if((time_ptr = ast_localtime(&time_data, &time_buf, NULL))) {
									ast_cli(a->fd, "Enqueued: %04d-%02d-%02d %02d:%02d:%02d\n",
											time_ptr->tm_year + 1900,
											time_ptr->tm_mon+1,
											time_ptr->tm_mday,
											time_ptr->tm_hour,
											time_ptr->tm_min,
											time_ptr->tm_sec);
								} else {
									ast_cli(a->fd, "Enqueued timestamp: %ld\n", time_data.tv_sec);
								}
								// print message
								ast_cli(a->fd, ">> %s <<\n", sqlite3_column_text(sql0, 2));
                            }
							else if(res == SQLITE_DONE) {
								break;
							}
							else if(res == SQLITE_BUSY){
								ast_mutex_unlock(&rgsm_lock);
								usleep(1);
								ast_mutex_lock(&rgsm_lock);
								continue;
                            }
							else{
								LOG_STEP_ERROR();
								break;
                            }
                        }
						//
						if(!row) ast_cli(a->fd, "  message \"%s\" not found\n", (a->argc == 6)?(a->argv[5]):(a->argv[6]));
						sqlite3_finalize(sql0);
						break;
                    }
					else if(res == SQLITE_BUSY){
						ast_mutex_unlock(&rgsm_lock);
						usleep(1);
						ast_mutex_lock(&rgsm_lock);
						continue;
                    }
					else{
						LOG_PREPARE_ERROR();
						break;
                    }
                }
				sqlite3_free(str0);
            }
			else if(!strcmp(a->argv[5], "delete")){
				// delete message
				index = atoi(a->argv[6]);
				str0 = sqlite3_mprintf("DELETE FROM '%q-outbox' WHERE msgno=%d;",
                           pvt->chname, index);
				ast_mutex_lock(&pvt->lock);
				dao_exec_stmt(str0, 1, pvt);
				ast_mutex_unlock(&pvt->lock);
            }
			else {
				ast_cli(a->fd, "  unknown operation \"%s\" in outbox\n", a->argv[5]);
			}
        }
    } // end outbox
	// sent
	else if(!strcmp(a->argv[4], "sent")){
		// check for presented arguments
		if((a->argc == 5) ||
			((a->argc == 6) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete")))) ||
				((a->argc >= 6) && (!strcmp(a->argv[5], "last"))) ||
					((a->argc >= 7) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))) && (!strcmp(a->argv[6], "last")))) {
			// set message count for print
			smscnt = 0;
			if((a->argc == 7) && (!strcmp(a->argv[5], "last"))) {
				smscnt = atoi(a->argv[6]);
			}
			else if((a->argc == 8) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))) && (!strcmp(a->argv[6], "last"))) {
				smscnt = atoi(a->argv[7]);
			}

			if(smscnt <= 0) smscnt = 10;
			// get list of messages
			str0 = sqlite3_mprintf("SELECT status,msgid,sent,received,datype,daname FROM '%q-sent' WHERE owner='this' GROUP BY msgid ORDER BY msgno DESC;",
                          pvt->chname);
			while(1){
				res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
				if(res == SQLITE_OK) {
					row = 0;
					while(1){
						res = sqlite3_step(sql0);
						if(res == SQLITE_ROW){
							row++;
							if(row > smscnt) break;
							// check status
							if(sqlite3_column_int(sql0, 0)){
								// delivered
								time_data.tv_sec = sqlite3_column_int64(sql0, 2); // sent
								time_data2.tv_sec = sqlite3_column_int64(sql0, 3); // received
								time_ptr = ast_localtime(&time_data, &time_buf, NULL);
								time_ptr2 = ast_localtime(&time_data2, &time_buf2, NULL);
								if(time_ptr && time_ptr2) {
									ast_cli(a->fd, "%0d: \"%s%s\" - sent: %04d-%02d-%02d-%02d:%02d:%02d - delivered: %04d-%02d-%02d-%02d:%02d:%02d\n",
													sqlite3_column_int(sql0, 1),
													(sqlite3_column_int(sql0, 4) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 5),
													time_ptr->tm_year + 1900,
													time_ptr->tm_mon+1,
													time_ptr->tm_mday,
													time_ptr->tm_hour,
													time_ptr->tm_min,
													time_ptr->tm_sec,
													time_ptr2->tm_year + 1900,
													time_ptr2->tm_mon+1,
													time_ptr2->tm_mday,
													time_ptr2->tm_hour,
													time_ptr2->tm_min,
													time_ptr2->tm_sec);
								} else {
									ast_cli(a->fd, "%0d: \"%s%s\" - sent: %ld - delivered: %ld\n",
													sqlite3_column_int(sql0, 1),
													(sqlite3_column_int(sql0, 4) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 5),
													time_data.tv_sec,
													time_data2.tv_sec);
								}
                            }
							else{
								// waiting
								time_data.tv_sec = sqlite3_column_int64(sql0, 2); // sent
								time_ptr = ast_localtime(&time_data, &time_buf, NULL);
								if(time_ptr) {
									ast_cli(a->fd, "%0d: \"%s%s\" - sent: %04d-%02d-%02d-%02d:%02d:%02d - waiting\n",
													sqlite3_column_int(sql0, 1),
													(sqlite3_column_int(sql0, 4) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 5),
													time_ptr->tm_year + 1900,
													time_ptr->tm_mon+1,
													time_ptr->tm_mday,
													time_ptr->tm_hour,
													time_ptr->tm_min,
													time_ptr->tm_sec);
								} else {
									ast_cli(a->fd, "%0d: \"%s%s\" - sent: %ld - waiting\n",
													sqlite3_column_int(sql0, 1),
													(sqlite3_column_int(sql0, 4) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 5),
													time_data.tv_sec);
								}
                            }
                        }
						else if(res == SQLITE_DONE) {
							break;
						}
						else if(res == SQLITE_BUSY){
							ast_mutex_unlock(&rgsm_lock);
							usleep(1);
							ast_mutex_lock(&rgsm_lock);
							continue;
                        }
						else {
							LOG_STEP_ERROR();
							break;
                        }
                    }
					//
					if(!row) ast_cli(a->fd, "  no sent messages\n");
					sqlite3_finalize(sql0);
					break;
                }
				else if(res == SQLITE_BUSY) {
					ast_mutex_unlock(&rgsm_lock);
					usleep(1);
					ast_mutex_lock(&rgsm_lock);
					continue;
                }
				else {
					LOG_PREPARE_ERROR();
					break;
                }
            }
			sqlite3_free(str0);
        }
		else if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))))){
			// read message from sent
			if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read"))))) {
				if(a->argc == 6) {
					index = atoi(a->argv[5]);
				}
				else {
					index = atoi(a->argv[6]);
				}
				// get message content
				str0 = sqlite3_mprintf("SELECT scatype,scaname,datype,daname,sent,received,part,content FROM '%q-sent' WHERE owner='this' AND msgid=%d ORDER BY part;",
                           pvt->chname, index);
				while(1){
					res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
					if(res == SQLITE_OK){
						row = 0;
						part = 0;
						while(1){
							res = sqlite3_step(sql0);
							if(res == SQLITE_ROW) {
								row++;
								part++;
								if(row == 1){
									// SMS Center Address
									ast_cli(a->fd, "SMS Center Address: %s%s\n",
													(sqlite3_column_int(sql0, 0) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 1));
									// Destination Address
									ast_cli(a->fd, "To: %s%s\n",
													(sqlite3_column_int(sql0, 2) == 145)?("+"):(""),
													sqlite3_column_text(sql0, 3));
									// Sent time
									time_data.tv_sec = sqlite3_column_int64(sql0, 4);
									if((time_ptr = ast_localtime(&time_data, &time_buf, NULL))) {
										ast_cli(a->fd, "Sent: %04d-%02d-%02d %02d:%02d:%02d\n",
												time_ptr->tm_year + 1900,
												time_ptr->tm_mon+1,
												time_ptr->tm_mday,
												time_ptr->tm_hour,
												time_ptr->tm_min,
												time_ptr->tm_sec);
									}
									else {
										ast_cli(a->fd, "Sent timestamp: %ld\n", time_data.tv_sec);
									}
									// Received time
									time_data.tv_sec = sqlite3_column_int64(sql0, 5);
									if(time_data.tv_sec){
										if((time_ptr = ast_localtime(&time_data, &time_buf, NULL))) {
											ast_cli(a->fd, "Delivered: %04d-%02d-%02d %02d:%02d:%02d\n",
													time_ptr->tm_year + 1900,
													time_ptr->tm_mon+1,
													time_ptr->tm_mday,
													time_ptr->tm_hour,
													time_ptr->tm_min,
													time_ptr->tm_sec);
										}
										else {
											ast_cli(a->fd, "Received timestamp: %ld\n", time_data.tv_sec);
										}
                                    }
									else {
										ast_cli(a->fd, "Delivered: Waiting...\n");
									}
									// start border
									ast_cli(a->fd, ">> ");
                                }
								// missed text mark
								if(part != sqlite3_column_int(sql0, 6)) {
									ast_cli(a->fd, "*some text missed*");
									part = sqlite3_column_int(sql0, 6);
                                }
								// print message
								ast_cli(a->fd, "%s", sqlite3_column_text(sql0, 7));
                            }
							else if(res == SQLITE_DONE) {
								break;
							}
							else if(res == SQLITE_BUSY) {
								// unlock eggsm subsystem
								ast_mutex_unlock(&rgsm_lock);
								usleep(1);
								// lock eggsm subsystem
								ast_mutex_lock(&rgsm_lock);
								continue;
                            }
							else {
								LOG_STEP_ERROR();
								break;
                            }
                        }
						//
						if(!row) {
							ast_cli(a->fd, "  message \"%s\" not found\n", (a->argc == 6)?(a->argv[5]):(a->argv[6]));
						}
						else {
							ast_cli(a->fd, " <<\n"); // end border
						}
						sqlite3_finalize(sql0);
						break;
                    }
					else if(res == SQLITE_BUSY) {
						ast_mutex_unlock(&rgsm_lock);
						usleep(1);
						ast_mutex_lock(&rgsm_lock);
						continue;
                    }
					else {
						LOG_PREPARE_ERROR();
						break;
                    }
                }
				sqlite3_free(str0);
            }
			else if(!strcmp(a->argv[5], "delete")) {
				// delete message from sent
				index = atoi(a->argv[6]);
				str0 = sqlite3_mprintf("DELETE FROM '%q-sent' WHERE owner='this' AND msgid=%d;",
                           pvt->chname, index);
				ast_mutex_lock(&pvt->lock);
				dao_exec_stmt(str0, 1, pvt);
				ast_mutex_unlock(&pvt->lock);
            }
			else {
				ast_cli(a->fd, "  unknown operation \"%s\" in sent\n", a->argv[5]);
			}
        }
    } // end of sent
	// discard
	else if(!strcmp(a->argv[4], "discard")){
		// check for presented arguments
		if((a->argc == 5) ||
			((a->argc == 6) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete")))) ||
				((a->argc >= 6) && (!strcmp(a->argv[5], "last"))) ||
					((a->argc >= 7) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))) && (!strcmp(a->argv[6], "last")))) {
			// set message count for print
			smscnt = 0;
			if((a->argc == 7) && (!strcmp(a->argv[5], "last"))) {
				smscnt = atoi(a->argv[6]);
			}
			else if((a->argc == 8) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))) && (!strcmp(a->argv[6], "last"))) {
				smscnt = atoi(a->argv[7]);
			}
			if(smscnt <= 0) smscnt = 10;
			// get list of messages
			str0 = sqlite3_mprintf("SELECT id,timestamp,destination,cause FROM '%q-discard' ORDER BY timestamp;",
                          pvt->chname);
			while(1) {
				res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
				if(res == SQLITE_OK) {
					row = 0;
					while(1) {
						res = sqlite3_step(sql0);
						if(res == SQLITE_ROW){
							row++;
							if(row > smscnt) break;
							time_data.tv_sec = sqlite3_column_int64(sql0, 1);
							time_ptr = ast_localtime(&time_data, &time_buf, NULL);
							if(time_ptr) {
								ast_cli(a->fd, "%0d: %04d-%02d-%02d-%02d:%02d:%02d - \"%s\" - cause: \"%s\"\n",
													sqlite3_column_int(sql0, 0),
													time_ptr->tm_year + 1900,
													time_ptr->tm_mon+1,
													time_ptr->tm_mday,
													time_ptr->tm_hour,
													time_ptr->tm_min,
													time_ptr->tm_sec,
													sqlite3_column_text(sql0, 2),
													sqlite3_column_text(sql0, 3));
							}
							else {
								ast_cli(a->fd, "%0d: %ld - \"%s\" - cause: \"%s\"\n",
													sqlite3_column_int(sql0, 0),
													time_data.tv_sec,
													sqlite3_column_text(sql0, 2),
													sqlite3_column_text(sql0, 3));
							}
                        }
						else if(res == SQLITE_DONE) {
							break;
						}
						else if(res == SQLITE_BUSY){
							ast_mutex_unlock(&rgsm_lock);
							usleep(1);
							ast_mutex_lock(&rgsm_lock);
							continue;
                        }
						else {
							LOG_STEP_ERROR();
							break;
                        }
                    }
					//
					if(!row) ast_cli(a->fd, "  no messages in discard\n");
					sqlite3_finalize(sql0);
					break;
                }
				else if(res == SQLITE_BUSY){
					ast_mutex_unlock(&rgsm_lock);
					usleep(1);
					ast_mutex_lock(&rgsm_lock);
					continue;
                }
				else{
					LOG_PREPARE_ERROR();
					break;
                }
            }
			sqlite3_free(str0);
        }
		else if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read")) || (!strcmp(a->argv[5], "delete"))))) {
			// read message from discard
			if((a->argc == 6) || ((a->argc == 7) && ((!strcmp(a->argv[5], "read"))))){
				if(a->argc == 6) {
					index = atoi(a->argv[5]);
				}
				else {
					index = atoi(a->argv[6]);
				}
				// get message content
				str0 = sqlite3_mprintf("SELECT timestamp,cause,destination,content FROM '%q-discard' WHERE id=%d;",
                           pvt->chname, index);
				while(1){
					res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
					if(res == SQLITE_OK){
						row = 0;
						part = 0;
						while(1){
							res = sqlite3_step(sql0);
							if(res == SQLITE_ROW){
								row++;
								// Timestamp
								time_data.tv_sec = sqlite3_column_int64(sql0, 0);
								if((time_ptr = ast_localtime(&time_data, &time_buf, NULL))) {
									ast_cli(a->fd, "Timestamp: %04d-%02d-%02d %02d:%02d:%02d\n",
												time_ptr->tm_year + 1900,
												time_ptr->tm_mon+1,
												time_ptr->tm_mday,
												time_ptr->tm_hour,
												time_ptr->tm_min,
												time_ptr->tm_sec);
								}
								else {
									ast_cli(a->fd, "Timestamp: %ld\n", time_data.tv_sec);
								}
								// Cause
								ast_cli(a->fd, "Cause: \"%s\"\n", sqlite3_column_text(sql0, 1));
								// Destination
								ast_cli(a->fd, "Destination Address: \"%s\"\n", sqlite3_column_text(sql0, 2));
								// Content
								ast_cli(a->fd, ">> %s <<\n", sqlite3_column_text(sql0, 3));
                            }
							else if(res == SQLITE_DONE) {
								break;
							}
							else if(res == SQLITE_BUSY) {
								ast_mutex_unlock(&rgsm_lock);
								usleep(1);
								ast_mutex_lock(&rgsm_lock);
								continue;
                            }
							else {
								LOG_STEP_ERROR();
								break;
                            }
                        }
						//
						if(!row) ast_cli(a->fd, "  message \"%s\" not found\n", (a->argc == 6)?(a->argv[5]):(a->argv[6]));
						sqlite3_finalize(sql0);
						break;
                    }
					else {
						LOG_PREPARE_ERROR();
						break;
                    }
                }
				sqlite3_free(str0);
            }
			else if(!strcmp(a->argv[5], "delete")){
				// delete message from discard
				index = atoi(a->argv[6]);
				str0 = sqlite3_mprintf("DELETE FROM '%q-discard' WHERE id=%d;", pvt->chname, index);
				ast_mutex_lock(&pvt->lock);
				dao_exec_stmt(str0, 1, pvt);
				ast_mutex_unlock(&pvt->lock);
            }
			else {
				ast_cli(a->fd, "  unknown operation \"%s\" in discard\n", a->argv[5]);
			}
        }
    } // end of discard
	else {
		ast_cli(a->fd, "  unknown message group \"%s\"\n", a->argv[4]);
	}

	// unlock eggsm subsystem
	ast_mutex_unlock(&rgsm_lock);
    return CLI_SUCCESS;
}

//!Device actions handler
char *cli_device_actions(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){

	int i;
	cli_fn_type subhandler;
	//
	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			e->command = "rgsm device";
			sprintf(device_actions_usage, "Usage: rgsm device <action> [...]\n");
			e->usage = device_actions_usage;
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
            if(a->pos == 2)
				return complete_dev_act(a->word, a->n);
			// generation channel action parameters ...
			else if(a->pos >= 3) {
				// from this point delegate generation function to
				// action depended CLI entries
				subhandler = NULL;
				// search action CLI entry
				for(i=0; i < RGSM_CLI_PARAM_COUNT(rgsm_cli_device_acts); i++) {
					// get actions by name
					if(strstr(a->line, rgsm_cli_device_acts[i].name)) {
						subhandler = rgsm_cli_device_acts[i].handler;
						break;
                    }
                }
				if(subhandler) return subhandler(e, cmd, a);
            }
			//
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
		}

	if(a->argc <= 2){
		sprintf(device_actions_usage, "Usage: rgsm device <action> [...]\n");
		return CLI_SHOWUSAGE;
    }

	subhandler = NULL;
	// search action CLI entry
	for(i=0; i < RGSM_CLI_PARAM_COUNT(rgsm_cli_device_acts); i++){
		// get actions by name
		if(!strcmp(a->argv[2], rgsm_cli_device_acts[i].name)){
			subhandler =rgsm_cli_device_acts[i].handler;
			break;
        }
    }
	if (subhandler) return subhandler(e, cmd, a);

	// if command not handled
	return CLI_FAILURE;
}

static char *device_action_show(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct gateway *gw;
    ggw8_info_t    *ggw8_info;
    uint16_t       sys_id;

	//
	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "action subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
    }

	if(a->argc != 3){
		sprintf(device_actions_usage, "Usage: rgsm device show\n");
		return CLI_SHOWUSAGE;
    }

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);

	if (!gateways.first) {
        ast_cli(a->fd, "  There are no GGW-8 cards installed\n");
	} else {
		ast_cli(a->fd, "| %-3.3s | %-7.7s | %-20.20s | %-20.20s | %-8.8s | %-4.4s |\n",
				"UID",
				"SYSID",
				"HW Rev",
				"FW Rev",
				"AvChnls",
				"Mode");


        AST_LIST_TRAVERSE (&gateways, gw, link)
        {
            ggw8_info = ggw8_get_device_info(gw->ggw8_device);
            sys_id = ggw8_get_device_sysid(gw->ggw8_device);

            ast_cli(a->fd, "| %3d | %.3u:%.3u | %-20.20s | %-20.20s | %s%s%s%s%s%s%s%s | %-4.4s |\n",
                    gw->uid,
                    (sys_id >> 8),
                    (sys_id & 0xff),
                    ggw8_info->hw_version,
                    ggw8_info->fw_version,
                    ((ggw8_info->gsm_presence & 0x80) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x40) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x20) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x10) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x08) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x04) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x02) ? "Y" : "-"),
                    ((ggw8_info->gsm_presence & 0x01) ? "Y" : "-"),
                    ((ggw8_get_device_mode(gw->ggw8_device) == DFU) ? "DFU" : "GSM"));

        }
	}
		// lock rgsm subsystem
	ast_mutex_unlock(&rgsm_lock);
    return CLI_SUCCESS;
}

static char *device_action_dfu(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct gateway *gw;
    //ggw8_info_t    *ggw8_info;
    int             gw_uid;


	//
	switch(cmd){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_INIT:
			ast_cli(a->fd, "action subhandler -- CLI_INIT unsupported in this context\n");
			return CLI_FAILURE;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_GENERATE:
			return NULL;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CLI_HANDLER:
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_cli(a->fd, "unknown CLI command = %d\n", cmd);
			return CLI_FAILURE;
    }

	if(a->argc != 5 || !is_str_digit(a->argv[3])){
		sprintf(device_actions_usage, "Usage: rgsm device dfu <UID> <pathto-ggw8-dfu.manifest>\n");
		return CLI_SHOWUSAGE;
    }

    gw_uid = atoi(a->argv[3]);

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);

	if (!gateways.first) {
        ast_cli(a->fd, "There are no GGW-8 cards installed\n");
        ast_mutex_unlock(&rgsm_lock);
        return CLI_SUCCESS;
	}

    AST_LIST_TRAVERSE (&gateways, gw, link)
    {
        if (gw->uid == gw_uid) {
            ast_mutex_unlock(&rgsm_lock);
            goto _gw_found;
        }
	}

    ast_cli(a->fd, "GGW-8 for given UID=%d not found\n", gw_uid);
    ast_mutex_unlock(&rgsm_lock);
    return CLI_SUCCESS;

_gw_found:

    rgsm_gw_dfu(&gw, a->argv[4], a->fd);

    return CLI_SUCCESS;
}

