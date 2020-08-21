/*! \file
 *
 * \brief Right and Above GSM Gateway driver
 *
 * \author Right and Above LLC
 *
 * \ingroup channel_drivers
 */

#include <asterisk.h>

ASTERISK_FILE_VERSION(__FILE__, "$Rev: 79 $")

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <signal.h>

#include <asterisk/lock.h>
#include <asterisk/channel.h>
#include <asterisk/config.h>
#include <asterisk/logger.h>
#include <asterisk/module.h>
#include <asterisk/pbx.h>
#include <asterisk/options.h>
#include <asterisk/utils.h>
#include <asterisk/linkedlists.h>
#include <asterisk/cli.h>
#include <asterisk/devicestate.h>
#include <asterisk/causes.h>
#include <asterisk/dsp.h>
#include <asterisk/app.h>
#include <asterisk/manager.h>
#include <asterisk/io.h>
#include <asterisk/musiconhold.h>
#include <asterisk/paths.h>
#include <asterisk/select.h>
#include <asterisk/indications.h>
#include "asterisk/md5.h"
#include "asterisk/format_compatibility.h"
#include <asterisk/frame.h>

#include "rgsm_defs.h"
#include "chan_rgsm.h"
#include "char_conv.h"
#include "at.h"
#include "rgsm_sim900.h"
#include "rgsm_sim5320.h"
#include "rgsm_uc15.h"
#include "ggw8_hal.h"
#include "rgsm_cli.h"
#include "rgsm_app.h"
#include "rgsm_manager.h"
#include "rgsm_utilities.h"
#include "rgsm_at_processor.h"
#include "rgsm_call_sm.h"
#include "rgsm_mdm_sm.h"
#include "atomics.h"
#include "rgsm_dao.h"
#include "rgsm_sms.h"
#include "rgsm_ipc.h"

#include <string.h>

#define INIT_CMD_QUEUE(pvt)          \
    do                               \
    {                                \
        pvt->cmd_queue.first = NULL; \
        pvt->cmd_queue.last = NULL;  \
        pvt->cmd_queue_length = 0;   \
        pvt->cmd_done = 1;           \
    } while (0)

//globals
//List of gateways discovered and initialized during module_load
gateways_t gateways = AST_LIST_HEAD_NOLOCK_INIT_VALUE;

ast_mutex_t rgsm_lock = AST_MUTEX_INIT_VALUE;
struct timeval rgsm_start_time = {0, 0};
uint32_t channel_id = 0;

gen_config_t gen_config = {
    .timeout_at_response = DEFAULT_TIMEOUT_AT_RESPONSE,
    .debug_ctl = {
        .at = 0,
        .callsm = 0,
        .rcvr = 0,
        .hal = 0,
        .voice = 0,
    },
    .jbconf = {
        .flags = 0,
        .max_size = -1,
        .resync_threshold = -1,
        .impl = "",
        .target_extra = -1,
    },
    .dev_jbsize = 100,
};

hw_config_t hw_config = {
    .vid = 0,
    .pid = 0,
    .product_string = "",
};

me_config_t uc15_config = {
    .mic_gain = UC15_MIC_GAIN_DEFAULT,
    .spk_gain = UC15_SPK_GAIN_DEFAULT,
    .spk_audg = UC15_SPK_AUDG_DEFAULT,
    .hex_download = "",
    .fw_image = "",
    .at_send_rate = 10,
};

me_config_t sim900_config = {
    .mic_gain = SIM900_MIC_GAIN_DEFAULT,
    .spk_gain = SIM900_SPK_GAIN_DEFAULT,
    .spk_audg = SIM900_SPK_AUDG_DEFAULT,
    .hex_download = "sim900_hex_download.hex",
    .fw_image = "sim900_fw_image.bin",
    .at_send_rate = 10,
};

me_config_t sim5320_config = {
    .mic_amp1 = SIM5320_MIC_AMP1_DEFAULT,
    .tx_lvl = SIM5320_TX_LVL_DEFAULT,
    .rx_lvl = SIM5320_RX_LVL_DEFAULT,
    .tx_gain = SIM5320_TX_GAIN_DEFAULT,
    .rx_gain = SIM5320_RX_GAIN_DEFAULT,
    .hex_download = "",
    .fw_image = "",
    .at_send_rate = 10,
    .ussd_dcs = 15,
    .init_delay = 10};

pvt_config_t pvt_config = {
    .context = "default",
    .incoming_type = INC_TYPE_DENY,
    .incomingto = "s",
    .outgoing_type = OUT_CALL_ALLOW,
    .balance_req_str = "",
    .sms_sendinterval = 20,
    .sms_sendattempt = 2,
    .sms_maxpartcount = 2,
    .sms_autodelete = 0, //!TDB: implement
    .voice_codec = 0,    //defaults use codecs supported by device
    .reg_try_cnt = 5,
    .sim_try_cnt = 5,
    .sim_toolkit = 0, //May 21, 2014, disabled by default
    .auto_start = 0,  //Oct 20, 2014, power on channel on module load
    .autoclose_ussd_menu = 0,
};

static struct ast_channel *fake_owner;
static struct gsm_pvt *last_used_pvt = NULL;

/*! Global jitterbuffer configuration - by default, jb is disabled */
static struct ast_jb_conf default_jbconf = {
    .flags = 0,
    .max_size = -1,
    .resync_threshold = -1,
    .impl = "",
    .target_extra = -1,
};

static struct ast_jb_conf global_jbconf;

//static int prefformat = DEVICE_FRAME_FORMAT;

//AST_MUTEX_DEFINE_STATIC(unload_mutex);

//static int unloading_flag = 0;

static atomic_t unloading_flag = ATOMIC_INIT(0);

//static inline int check_unloading();
//static inline void set_unloading();

#if ASTERISK_VERSION_NUM < 10800
static struct ast_channel *rgsm_requester(const char *type, int format, void *data, int *cause);
#else
static struct ast_channel *rgsm_requester(const char *type, format_t format, const struct ast_channel *requester, void *data, int *cause);
#endif
static int rgsm_call(struct ast_channel *ast, char *dest, int timeout);
static int rgsm_hangup(struct ast_channel *ast);
static int rgsm_answer(struct ast_channel *ast);
static int rgsm_voice_write(struct ast_channel *ast, struct ast_frame *frame);
static int rgsm_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static int rgsm_devicestate(void *data);
static int rgsm_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen);
static int rgsm_digit_start(struct ast_channel *ast, char digit);
static int rgsm_digit_end(struct ast_channel *ast, char digit, unsigned int duration);

static struct ast_frame *rgsm_voice_read(struct ast_channel *ast);

static void *channel_thread_func(void *data);

/*
 * channel stuff
 */

const struct ast_channel_tech rgsm_tech = {
    .type = "RGSM",
    .description = "Right&Above GSM gateway",
    //supported codecs below are redundent, the device capabilities will get used: see rgsm_call_sm.c::init_call()
    .capabilities = /*AST_FORMAT_SLINEAR | AST_FORMAT_SPEEX |*/ AST_FORMAT_GSM | AST_FORMAT_ULAW | AST_FORMAT_G729 | AST_FORMAT_ALAW | AST_FORMAT_SPEEX, //more to be added lated, when blackfin code will be ready
    .requester = rgsm_requester,
    .call = rgsm_call,
    .hangup = rgsm_hangup,
    .answer = rgsm_answer,
    .read = rgsm_voice_read,
    .write = rgsm_voice_write,
    .fixup = rgsm_fixup,
    .devicestate = rgsm_devicestate,
    .indicate = rgsm_indicate,
    // DTMF
    .send_digit_begin = rgsm_digit_start,
    .send_digit_end = rgsm_digit_end,

};

void rgsm_usleep(struct gsm_pvt *pvt, int us)
{
    //unlock pvt
    ast_mutex_unlock(&pvt->lock);
    //unlock rgsm subsystem
    ast_mutex_unlock(&rgsm_lock);

    us_sleep(us);

    //lock rgsm subsystem
    ast_mutex_lock(&rgsm_lock);
    //lock pvt
    ast_mutex_lock(&pvt->lock);
}

static void rgsm_reset_pvt_state(struct gsm_pvt *pvt, char flush_queue)
{

    // reset management state
    pvt->mdm_state = MDM_STATE_DISABLE;
    // reset call state
    pvt->call_state = CALL_STATE_DISABLE;
    // reset callwait status
    pvt->callwait_status = CALLWAIT_STATUS_UNKNOWN;
    // reset registaration status
    pvt->reg_state = REG_STATE_NOTREG_NOSEARCH;
    // reset rssi value for disabled state
    pvt->rssi = -1;
    // set ber value for disabled state
    pvt->ber = -1;
    // reset hidenum settings
    pvt->hidenum_set = HIDENUM_UNKNOWN;
    // reset hidenum status
    pvt->hidenum_stat = HIDENUM_STATUS_UNKNOWN;

    pvt->dtmf_sym = 0;
    pvt->exp_cmd_counter = 0;
    pvt->err_cmd_counter = 0;
    pvt->ussd_text_param = 0;

    gsm_reset_sim_data(pvt);
    gsm_reset_modem_data(pvt);

    // flush at command queue
    if (flush_queue)
    {
        rgsm_atcmd_queue_flush(pvt);
    }
}

void rgsm_pvt_power_on(struct ast_cli_args *a, struct gsm_pvt *pvt, int pwr_delay_sec, ggw8_baudrate_t baudrate)
{
    ast_mutex_lock(&pvt->lock);
    //
    if (pvt->power_man_disable)
    {
        if (a)
            ast_cli(a->fd, "rgsm: <%s>: manual power management disabled\n", pvt->name);
    }
    else if (pvt->flags.enable)
    {
        if (a)
            ast_cli(a->fd, "rgsm: <%s>: already enabled\n", pvt->name);
    }
    else
    {
        // set enable flag
        pvt->flags.enable = 1;
        // set power delay
        pvt->pwr_delay_sec = pwr_delay_sec;
        // run channel monitor thread
        pvt->channel_thread = AST_PTHREADT_NULL;
        pvt->baudrate = baudrate;

        // open AT channel
        pvt->at_fd = ggw8_open_at(pvt->ggw8_device, pvt->modem_id);
        if (pvt->at_fd == -1)
        {
            ast_log(AST_LOG_ERROR, "rgsm: <%s>: can't open AT channel\n", pvt->name);
            pvt->flags.enable = 0;
        }

        // run gsm channel loop
        if (pvt->flags.enable)
        {
            if (ast_pthread_create_detached(&pvt->channel_thread, NULL, channel_thread_func, pvt) < 0)
            {
                //if (pthread_create(&pvt->channel_thread, NULL, channel_thread_func, pvt) < 0) {
                pvt->channel_thread = AST_PTHREADT_NULL;
                ast_log(AST_LOG_ERROR, "rgsm: <%s>: unable creating channel monitor thread\n", pvt->name);
                pvt->flags.enable = 0;
                //cleanup on failure
                ggw8_close_at(pvt->ggw8_device, pvt->modem_id);
            }
        }

        if (pvt->flags.enable)
        {
            if (a)
                ast_cli(a->fd, "rgsm: <%s>: channel starting...\n", pvt->name);
        }
        else
        {
            if (a)
                ast_cli(a->fd, "rgsm: <%s>: can't enable\n", pvt->name);
        }
    }
    ast_mutex_unlock(&pvt->lock);
}

void rgsm_pvt_power_off(struct ast_cli_args *a, struct gsm_pvt *pvt)
{
    ast_mutex_lock(&pvt->lock);

    // check current power state
    if (pvt->power_man_disable)
    {
        if (a)
            ast_cli(a->fd, "rgsm: <%s>: manual power management disabled\n", pvt->name);
    }
    else if (!pvt->flags.enable)
    {
        if (a)
            ast_cli(a->fd, "rgsm:<%s>: already disabled\n", pvt->name);
    }
    else
    {
        // check shutdown flag
        if (!pvt->flags.shutdown_now)
        {
            pvt->flags.shutdown = 1;
            pvt->flags.shutdown_now = 1;
            pvt->man_chstate = MAN_CHSTATE_DISABLED;
            if (a)
                ast_cli(a->fd, "rgsm: <%s>: send shutdown signal\n", pvt->name);
        }
        else
        {
            if (a)
                ast_cli(a->fd, "rgsm: <%s>: shutdown signal already sent\n", pvt->name);
        }
    }

    ast_mutex_unlock(&pvt->lock);
}

void rgsm_pvt_power_reset(struct ast_cli_args *a, struct gsm_pvt *pvt)
{
    ast_mutex_lock(&pvt->lock);
    // check current power state
    if (pvt->power_man_disable)
    {
        if (a)
            ast_cli(a->fd, "rgsm: <%s>: manual power management disabled\n", pvt->name);
    }
    else if (!pvt->flags.enable)
    {
        if (a)
            ast_cli(a->fd, "rgsm: <%s>: already disabled\n", pvt->name);
    }
    else
    {
        // check power flags
        if (!pvt->flags.shutdown_now && !pvt->flags.restart_now)
        {
            RST_CHANNEL(pvt, 0);
            if (a)
                ast_cli(a->fd, "rgsm: <%s>: send restart signal\n", pvt->name);
        }
        else if (pvt->flags.shutdown_now)
        {
            if (a)
                ast_cli(a->fd, "rgsm: <%s>: an attempt to restart a channel which is under shutdown\n", pvt->name);
        }
        else
        {
            if (a)
                ast_cli(a->fd, "rgsm: <%s>: restart signal already sent\n", pvt->name);
        }
    }

    ast_mutex_unlock(&pvt->lock);
}

void gsm_reset_sim_data(struct gsm_pvt *pvt)
{
    // reset channel phone number
    unknown_address(&pvt->subscriber_number);
    // reset SMS center number
    unknown_address(&pvt->smsc_number);
    //reset rest of fields
    ast_copy_string(pvt->operator_name, "unknown", sizeof(pvt->operator_name));
    ast_copy_string(pvt->operator_code, "unknown", sizeof(pvt->operator_code));
    ast_copy_string(pvt->imsi, "unknown", sizeof(pvt->imsi));
    ast_copy_string(pvt->iccid, "unknown", sizeof(pvt->iccid));
}

void gsm_reset_modem_data(struct gsm_pvt *pvt)
{
    *pvt->firmware = '\0';
    *pvt->imei = '\0';
    *pvt->model = '\0';
    *pvt->manufacturer = '\0';
}

void gsm_abort_channel(struct gsm_pvt *pvt, man_chstate_t reason)
{
    ast_verbose("rgsm: <%s>: abort channel, reason \"%s\"\n", pvt->name, man_chstate_str(reason));
    pvt->man_chstate = reason;
    if (!pvt->flags.shutdown_now)
    {
        pvt->flags.shutdown = 1;
        pvt->flags.shutdown_now = 1;
    }
}

void gsm_shutdown_channel(struct gsm_pvt *pvt)
{
    if (!pvt->flags.shutdown_now)
    {
        ast_verbose("rgsm: <%s>: shutdown channel\n", pvt->name);
        pvt->man_chstate = MAN_CHSTATE_DISABLED;
        pvt->flags.shutdown = 1;
        pvt->flags.shutdown_now = 1;
    }
}
void gsm_next_sim_search(struct gsm_pvt *pvt)
{
    pvt->sim_try_cnt_curr--;
    if (pvt->sim_try_cnt_curr)
    {
        ast_verbose("rgsm: <%s>: restart channel for next SIM search, remaining %d attempts\n", pvt->name, pvt->sim_try_cnt_curr);
        RST_CHANNEL(pvt, RST_DELAY_SIMSEARCH);
    }
    else
    {
        gsm_abort_channel(pvt, MAN_CHSTATE_ABORTED_NOSIM);
    }
}

//!module type must be known
void gsm_query_sim_data(struct gsm_pvt *pvt)
{
    //! do not query IMSI here

    if (pvt->module_type != MODULE_TYPE_UNKNOWN)
    {
        if (pvt->functions.gsm_query_sim_data != NULL)
        {
            pvt->functions.gsm_query_sim_data(pvt);
        }
        else
        {
            ast_log(AST_LOG_DEBUG, "<%s>: gsm_query_sim_data function not defined for that module type\n", pvt->name);
        }
    }

    /*    if(pvt->module_type == MODULE_TYPE_SIM900) {
        rgsm_atcmd_queue_append(pvt, AT_SIM900_CCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
        //enable DTMF detection
        //Aug 19, 2014: moved here from query_module_data() according to Application Notes
        rgsm_atcmd_queue_append(pvt, AT_SIM900_DDET, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "1");

    }
*/
    /*
    else if(pvt->module_type == MODULE_TYPE_SIM300) {
        rgsm_atcmd_queue_append(pvt, AT_SIM300_CCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
    }
    else if(pvt->module_type == MODULE_TYPE_M10) {
        rgsm_atcmd_queue_append(pvt, AT_M10_QCCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
    }
    */
}

void gsm_start_simpoll(struct gsm_pvt *pvt)
{
    if (pvt->module_type != MODULE_TYPE_UNKNOWN)
    {
        ast_verbose("rgsm: <%s>: start SIM polling\n", pvt->name);
        //
        rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
        //
        pvt->mdm_state = MDM_STATE_SUSPEND;
        // start simpoll timer
        rgsm_timer_set(pvt->timers.simpoll, simpoll_timeout);
    }
}

/*

	Channel Driver callbacks

*/

#if ASTERISK_VERSION_NUM < 10800
static struct ast_channel *rgsm_requester(const char *type, int format, void *data, int *cause)
#else
static struct ast_channel *rgsm_requester(const char *type, format_t format, const struct ast_channel *requester, void *data, int *cause)
#endif
{
    char *device_name;
    char *called_name;

    char *sp;
    char *ep;

    struct gsm_pvt *pvt;
    struct gsm_pvt *pvt_start;
    struct ast_channel *ast_chnl;
    struct gateway *gw;

    struct gsm_pvt *pvt_list[256];
    int pvt_count, start_from, i;

    *cause = AST_CAUSE_NOTDEFINED;

    //
    if (!data)
    {
        ast_log(LOG_WARNING, "requester data not present\n");
        *cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
        return NULL;
    }

    device_name = ast_strdupa((char *)data);

    ast_verb(3, "rgsm: request data=\"%s\"\n", device_name);

    // search '/' before called name
    called_name = strchr(device_name, '/');
    if (called_name)
    {
        *called_name++ = '\0';
        if (strncasecmp(device_name, "CH[", 3))
        {
            ast_log(LOG_WARNING, "invalid device identifier=\"%.*s\" must be \"CH\"(channel)\n", 2, device_name);
            *cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
            return NULL;
        }
    }
    else
    {
        called_name = device_name;
        device_name = NULL;
    }

    // check for valid called name
    if (!is_address_string(called_name))
    {
        ast_log(LOG_WARNING, "invalid called name=\"%s\"\n", called_name);
        *cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
        return NULL;
    }

    // lock rgsm subsystem
    ast_mutex_lock(&rgsm_lock);

    pvt = NULL;
    pvt_start = NULL;

    // get channel
    if (device_name)
    {
        if (!strncasecmp(device_name, "CH", 2))
        {
            // get specified channel
            if (!(sp = strchr(device_name, '[')))
            {
                ast_log(LOG_WARNING, "can't get requested channel name\n");
                *cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
                // unlock rgsm subsystem
                ast_mutex_unlock(&rgsm_lock);
                return NULL;
            }
            sp++;
            if (!(ep = strchr(sp, ']')))
            {
                ast_log(LOG_WARNING, "requested channel has unbalanced name\n");
                *cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
                // unlock rgsm subsystem
                ast_mutex_unlock(&rgsm_lock);
                return NULL;
            }
            *ep = '\0';
            ast_verb(3, "rgsm: request channel name=\"%s\"\n", sp);
            //
            pvt_start = find_ch_by_name(sp);
            if (!pvt_start)
            {
                ast_log(LOG_WARNING, "requested channel=\"%s\" not found\n", sp);
                *cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
                // unlock rgsm subsystem
                ast_mutex_unlock(&rgsm_lock);
                return NULL;
            }

            ast_log(LOG_DEBUG, "requested channel=\"%s\" found: call_state=%d, owner=%p, reg_state=%d, out_type=%d\n",
                    sp, pvt_start->call_state, pvt_start->owner, pvt_start->reg_state, pvt_start->chnl_config.outgoing_type);

            // lock pvt channel
            ast_mutex_lock(&pvt_start->lock);
            //
            if ((pvt_start->call_state == CALL_STATE_NULL) &&
                (!pvt_start->owner) &&
                ((pvt_start->reg_state == REG_STATE_REG_HOME_NET) || (pvt_start->reg_state == REG_STATE_REG_ROAMING)) &&
                (pvt_start->chnl_config.outgoing_type == OUT_CALL_ALLOW))
            {
                // set owner for prevent missing ownership
                fake_owner = ast_dummy_channel_alloc();
                pvt_start->owner = fake_owner;
                pvt = pvt_start;
                ast_verb(3, "rgsm: request got channel=\"%s\"\n", pvt_start->name);
            }
            // unlock pvt channel
            ast_mutex_unlock(&pvt_start->lock);
        }
    }
    else
    {
        pvt_count = 0;

        //Generate a list of all availible devices
        AST_LIST_TRAVERSE(&gateways, gw, link)
        {
            for (i = 0; i < MAX_MODEMS_ON_BOARD; i++)
            {
                if (!gw->gsm_pvts[i])
                    continue;
                pvt_list[pvt_count] = gw->gsm_pvts[i];
                pvt_count++;
            }
        }

        start_from = 0;
        if (last_used_pvt)
        {
            // Find last used device
            for (i = 0; i < pvt_count; i++)
            {
                if (pvt_list[i]->unique_id == last_used_pvt->unique_id)
                {
                    start_from = i + 1;
                    break;
                }
            }
        }

        // Search for a availible device starting at the last used device
        for (i = 0; i < pvt_count; i++)
        {
            if (start_from == pvt_count)
            {
                start_from = 0;
            }

            pvt_start = pvt_list[start_from];

            // lock pvt channel
            ast_mutex_lock(&pvt_start->lock);
            //
            if ((pvt_start->call_state == CALL_STATE_NULL) &&
                (!pvt_start->owner) &&
                ((pvt_start->reg_state == REG_STATE_REG_HOME_NET) || (pvt_start->reg_state == REG_STATE_REG_ROAMING)) &&
                (pvt_start->chnl_config.outgoing_type == OUT_CALL_ALLOW))
            {
                // set owner for prevent missing ownership
                fake_owner = ast_dummy_channel_alloc();
                pvt_start->owner = fake_owner;
                pvt = pvt_start;
                ast_verb(3, "rgsm: request got channel=\"%s\"\n", pvt->name);
            }
            // unlock pvt channel
            ast_mutex_unlock(&pvt_start->lock);
            if (pvt)
                break;
            start_from++;
        }
    }

    // congestion -- no requested or free channel
    if (!pvt)
    {
        ast_verb(3, "rgsm: request no free channels\n");
        *cause = AST_CAUSE_NORMAL_CIRCUIT_CONGESTION;
        // unlock rgsm subsystem
        ast_mutex_unlock(&rgsm_lock);
        return NULL;
    }

    //save it as last_used
    last_used_pvt = pvt;

    ast_mutex_lock(&pvt->lock);

    ast_chnl = init_call(CALL_DIR_OUT, pvt, &format, cause);

    ast_mutex_unlock(&pvt->lock);
    ast_mutex_unlock(&rgsm_lock);

    if (!ast_chnl)
    {
        ast_log(LOG_WARNING, "rgsm: request unable to allocate channel structure.\n");
        if (*cause == AST_CAUSE_NOTDEFINED)
        {
            *cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
        }
        return NULL;
    }

    return ast_chnl;
}

static int rgsm_call(struct ast_channel *ast_chnl, char *dest, int timeout)
{
    char *device_name;
    char *called_name;
    struct gsm_pvt *pvt;
    struct timeval tv, dial_timeout;

    if (!dest)
    {
        ast_log(LOG_WARNING, "ast channel=\"%s\" call has't data\n", ast_channel_name(ast_chnl));
        return -1;
    }
    device_name = ast_strdupa((char *)dest);
    //
    if (!(pvt = ast_channel_tech_pvt(ast_chnl)))
    {
        ast_log(LOG_WARNING, "ast channel=\"%s\" has't tech pvt\n", ast_channel_name(ast_chnl));
        return -1;
    }
    // search '/' before called name
    called_name = strchr(device_name, '/');
    if (called_name)
    {
        *called_name++ = '\0';
    }
    else
    {
        called_name = device_name;
        device_name = NULL;
    }
    // check for valid called name
    if (!is_address_string(called_name))
    {
        ast_log(LOG_WARNING, "ast channel=\"%s\" has invalid called name=\"%s\"\n", ast_channel_name(ast_chnl), called_name);
        return -1;
    }

    // lock rgsm subsystem
    ast_mutex_lock(&rgsm_lock);
    // lock pvt channel
    ast_mutex_lock(&pvt->lock);
    // check for is asterisk channel in idle state

    if ((ast_channel_state(ast_chnl) != AST_STATE_DOWN) && (ast_channel_state(ast_chnl) != AST_STATE_RESERVED))
    {
        ast_log(LOG_WARNING, "ast channel=\"%s\" has wrong state\n", ast_channel_name(ast_chnl));
        // unlock pvt channel
        ast_mutex_unlock(&pvt->lock);
        // unlock rgsm subsystem
        ast_mutex_unlock(&rgsm_lock);
        return -1;
    }
    //
    gettimeofday(&tv, NULL);
    // get called name
    address_classify(called_name, &pvt->called_number);
    // get calling name
#if ASTERISK_VERSION_NUM < 10800
    if (ast_chnl->cid.cid_num)
    {
        address_classify(ast_ch->cid.cid_num, &pvt->calling_name);
    }
#else
    if (ast_channel_caller(ast_chnl)->id.number.str)
    {
        address_classify(ast_channel_caller(ast_chnl)->id.number.str, &pvt->calling_number);
    }
#endif
    else
    {
        address_classify("s", &pvt->calling_number);
    }
    //
    ast_verb(3, "rgsm: <%s> outgoing call \"%s%s\" -> \"%s%s\" using codec \"%ld\"\n",
             pvt->name,
             (pvt->calling_number.type.full == 145) ? ("+") : (""),
             pvt->calling_number.value,
             (pvt->called_number.type.full == 145) ? ("+") : (""),
             pvt->called_number.value,
             pvt->frame_format);

    //
    if (call_sm(pvt, CALL_MSG_SETUP_REQ, 0) < 0)
    {
        // unlock pvt channel
        ast_mutex_unlock(&pvt->lock);
        // unlock rgsm subsystem
        ast_mutex_unlock(&rgsm_lock);
        return -1;
    }

    // set dialing timeout
    if (timeout > 0)
    {
        dial_timeout.tv_sec = timeout;
        dial_timeout.tv_usec = 0;
    }
    else
    {
        dial_timeout.tv_sec = 180;
        dial_timeout.tv_usec = 0;
    }
    // start dial timer
    rgsm_timer_set(pvt->timers.dial, dial_timeout);

    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    // unlock rgsm subsystem
    ast_mutex_unlock(&rgsm_lock);
    return 0;
}

static int rgsm_hangup(struct ast_channel *ast_chnl)
{
    struct gsm_pvt *pvt;

    //struct timeval tvstart;

    //char *str;
    //sqlite3_stmt *sql;
    //int res;
    //int row;

    if (!ast_chnl)
    {
        ast_log(LOG_WARNING, "invalid ast_ch channel\n");
        return 0;
    }

    if (!(pvt = ast_channel_tech_pvt(ast_chnl)))
        return 0;

    ast_verb(3, "rgsm: <%s> hangup\n", pvt->name);

    //tv_set(tvstart, 0, 0);

    // lock rgsm subsystem
    ast_mutex_lock(&rgsm_lock);
    // lock pvt channel
    ast_mutex_lock(&pvt->lock);

    pvt->active_line = 0;
    pvt->active_line_stat = -1;
    pvt->wait_line = 0;
    pvt->wait_line_num[0] = '\0';

    // request balance
    pvt->flags.balance_req = 1;

    if (pvt->voice_fd != -1)
    {
        ast_verb(3, "<%s>: close voice channel\n", pvt->name);
        ggw8_close_voice(pvt->ggw8_device, pvt->modem_id);
        pvt->voice_fd = -1;
    }

    // stop dial timer
    rgsm_timer_stop(pvt->timers.dial);

    // clean
    pvt->called_number.value[0] = '\0';
    pvt->calling_number.value[0] = '\0';
    ast_channel_set_fd(ast_chnl, 0, -1);
    ast_setstate(ast_chnl, AST_STATE_DOWN);
    call_sm(pvt, CALL_MSG_RELEASE_REQ, 0);

    ast_channel_tech_pvt_set(ast_chnl, NULL);
    pvt->owner = NULL;
    pvt->call_dir = CALL_DIR_NONE;

    pvt->man_chstate = MAN_CHSTATE_IDLE;
    rgsm_man_event_channel_state(pvt);

    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    // unlock rgsm subsystem
    ast_mutex_unlock(&rgsm_lock);
    return 0;
}

static int rgsm_answer(struct ast_channel *ast_chnl)
{
    struct gsm_pvt *pvt;

    // get channel pvt data
    if (!(pvt = ast_channel_tech_pvt(ast_chnl)))
    {
        ast_log(LOG_ERROR, "ast channel=\"%s\" has't tech pvt\n", ast_channel_name(ast_chnl));
        return -1;
    }

    ast_verb(3, "rgsm: <%s> answer\n", pvt->name);

    // lock pvt channel
    ast_mutex_lock(&pvt->lock);

    if (call_sm(pvt, CALL_MSG_SETUP_RESPONSE, 0) < 0)
    {
        ast_log(LOG_ERROR, "<%s>: state machine error\n", pvt->name);
        // unlock pvt channel
        ast_mutex_unlock(&pvt->lock);
        return -1;
    }

    ast_setstate(ast_chnl, AST_STATE_UP);
    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    return 0;
}

static int rgsm_digit_start(struct ast_channel *ast_ch, char digit)
{

    struct gsm_pvt *pvt;

    // check for ast channel has tech channel
    if (!(pvt = ast_channel_tech_pvt(ast_ch)))
    {
        ast_log(LOG_ERROR, "ast channel=\"%s\" has't tech pvt\n", ast_channel_name(ast_ch));
        return -1;
    }

    // lock pvt channel
    ast_mutex_lock(&pvt->lock);

    if (pvt->event_is_now_send)
    {
        ast_log(LOG_ERROR, "<%s>: dtmf send just in progress\n", pvt->name);
        // unlock pvt channel
        ast_mutex_unlock(&pvt->lock);
        return -1;
    }

    // check for valid dtmf symbol
    switch (digit)
    {
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '*':
    case '#':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
        break;
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    default:
        ast_log(LOG_NOTICE, "<%s>: unsupported dtmf symbol=%d\n", pvt->name, digit);
        // unlock pvt channel
        ast_mutex_unlock(&pvt->lock);
        return -1;
        break;
    }

    ast_verb(4, "<%s>: sending DTMF [%c] begin\n", pvt->name, digit);

    if (rgsm_atcmd_queue_append(pvt, AT_VTS, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "\"%c\"", digit) < 0)
    {
        ast_log(LOG_ERROR, "<%s>: DTMF [%c] sending fail\n", pvt->name, digit);
        // unlock pvt channel
        ast_mutex_unlock(&pvt->lock);
        return -1;
    }

    pvt->event_is_now_send = 1;
    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    return 0;
}

static int rgsm_digit_end(struct ast_channel *ast_ch, char digit, unsigned int duration)
{

    struct gsm_pvt *pvt;

    // check for ast channel has tech channel
    if (!(pvt = ast_channel_tech_pvt(ast_ch)))
    {
        ast_log(LOG_ERROR, "ast channel=\"%s\" has't tech pvt\n", ast_channel_name(ast_ch));
        return -1;
    }

    // lock pvt channel
    ast_mutex_lock(&pvt->lock);

    if (!pvt->event_is_now_send)
    {
        ast_log(LOG_ERROR, "<%s>: DTMF sending is not started\n", pvt->name);
        pvt->event_is_now_send = 0;
        // unlock pvt channel
        ast_mutex_unlock(&pvt->lock);
        return -1;
    }

    ast_verb(4, "<%s>: sending DTMF [%c] end\n", pvt->name, digit);

    pvt->event_is_now_send = 0;
    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    return 0;
}

static struct ast_frame *rgsm_voice_read(struct ast_channel *ast_ch)
{
    struct gsm_pvt *pvt;

    int data_len;
    char *data_ptr;
    //rgsm_voice_frame_header_t *hdr_ptr;
    int samples;
    struct timeval dtmf_timeout;

    //unsigned short seq_num;
    //unsigned int timestamp;
    //unsigned int ssrc;

    //
    if (!ast_ch)
    {
        ast_log(LOG_ERROR, "Bad ast_channel param\n");
        return &ast_null_frame;
    }

    if (!(pvt = ast_channel_tech_pvt(ast_ch)))
    {
        ast_log(LOG_ERROR, "Channel [%s] has't tech pvt\n", ast_channel_name(ast_ch));
        return &ast_null_frame;
    }

    // lock pvt channel
    ast_mutex_lock(&pvt->lock);

    if (!pvt->owner)
    {
        ast_log(LOG_ERROR, "rgsm channel has't owner\n");
        goto exit_null_frame_;
    }

    if (pvt->voice_fd == -1)
    {
        ast_log(LOG_ERROR, "rgsm channel's voice not opened\n");
        goto exit_null_frame_;
    }

    // read data from voice channel
    data_ptr = (pvt->voice_recv_buf + AST_FRIENDLY_OFFSET);
    if ((data_len = read(pvt->voice_fd, data_ptr, RGSM_VOICE_BUF_LEN)) < 0)
    {
        if (errno != EAGAIN)
        {
            ast_log(LOG_ERROR, "read error=%d(%s)\n", errno, strerror(errno));
        }
        goto exit_null_frame_;
    }

    if (pvt->dtmf_sym)
    {

        if (!pvt->event_is_now_recv_begin)
        {
            //send DTMF frame begin
            pvt->event_is_now_recv_begin = 1;
            memset(&pvt->frame, 0, sizeof(struct ast_frame));
            pvt->frame.frametype = AST_FRAME_DTMF;
#if ASTERISK_VERSION_NUM < 10800
            pvt->frame.subclass = pvt->dtmf_sym;
#else
            pvt->frame.subclass.integer = pvt->dtmf_sym;
#endif
            pvt->frame.datalen = 0;
            pvt->frame.samples = 0;
            pvt->frame.mallocd = 0;
            pvt->frame.src = "RGSM";
            pvt->frame.len = ast_tvdiff_ms(ast_samp2tv(50, 1000), ast_tv(0, 0));

            dtmf_timeout.tv_sec = 0;
            dtmf_timeout.tv_usec = 50 * 1000;

            rgsm_timer_set(pvt->timers.dtmf, dtmf_timeout);

            DEBUG_VOICE("rgsm: <%s>: DTMF [%c] read begin\n", pvt->name, pvt->dtmf_sym);
            if (data_len)
            {
                DEBUG_VOICE("rgsm: <%s>: skipped voice read %d bytes due to DTMF\n", pvt->name, data_len);
            }
            goto exit_pvt_frame_;
            //
        }
        else if (pvt->event_is_now_recv_end)
        {
            // now event end
            pvt->event_is_now_recv_begin = 0;
            pvt->event_is_now_recv_end = 0;
            pvt->dtmf_sym = 0;

            DEBUG_VOICE("rgsm, <%s>: DTMF [%c] read end\n", pvt->name, pvt->dtmf_sym);
        }

        if (data_len)
        {
            DEBUG_VOICE("rgsm: <%s>: skipped voice read %d bytes due to DTMF\n", pvt->name, data_len);
        }
        goto exit_null_frame_;
    }

    if (data_len == 0)
    {
        goto exit_null_frame_;
    }

    //TODO: calculate samples count
    unsigned short voice_pck_len;
    switch (pvt->frame_format)
    {
    case AST_FORMAT_GSM:
        voice_pck_len = 33;
        break;
    case AST_FORMAT_G729:
        voice_pck_len = 20;
        break;
    case AST_FORMAT_ULAW:
        voice_pck_len = 160;
        break;
    case AST_FORMAT_ALAW:
        voice_pck_len = 160;
        break;
    case AST_FORMAT_SPEEX:
        voice_pck_len = 38;
        break; // TO DO: Right calculate Speex packet lengthg. Now value 38 is one particular case
    default:
        voice_pck_len = data_len; //Not Supportrd codec. We do not know real packet length
    }
    samples = data_len / voice_pck_len * 160;

    DEBUG_VOICE("<%s>: voice read %d bytes, samples %d\n", pvt->name, data_len, samples);
    DEBUG_VOICE("<%s>: first 10 bytes: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", pvt->name,
                (unsigned char)data_ptr[0], (unsigned char)data_ptr[1], (unsigned char)data_ptr[2], (unsigned char)data_ptr[3], (unsigned char)data_ptr[4], (unsigned char)data_ptr[5], (unsigned char)data_ptr[6], (unsigned char)data_ptr[7], (unsigned char)data_ptr[8], (unsigned char)data_ptr[9]);

    if (data_len > voice_pck_len)
    {
        DEBUG_VOICE("<%s>: first 10 bytes: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", pvt->name,
                    (unsigned char)data_ptr[33], (unsigned char)data_ptr[34], (unsigned char)data_ptr[35], (unsigned char)data_ptr[36], (unsigned char)data_ptr[37], (unsigned char)data_ptr[38], (unsigned char)data_ptr[39], (unsigned char)data_ptr[40], (unsigned char)data_ptr[41], (unsigned char)data_ptr[42]);
    }

    // 	memset(&pvt->frame, 0x00, sizeof(struct ast_frame));
    pvt->frame.frametype = AST_FRAME_VOICE;
#if ASTERISK_VERSION_NUM < 10800
    pvt->frame.subclass = pvt->frame_format;
#else
    // pvt->frame.subclass.codec = pvt->frame_format;
    pvt->frame.subclass.format = ast_channel_readformat(ast_ch);    // this is written by Roman
#endif
    pvt->frame.datalen = data_len;
    pvt->frame.samples = samples;
    pvt->frame.src = "RGSM";
    pvt->frame.offset = AST_FRIENDLY_OFFSET; // + hdr_len;
    pvt->frame.mallocd = 0;
    pvt->frame.delivery.tv_sec = 0;
    pvt->frame.delivery.tv_usec = 0;
#if ASTERISK_VERSION_NUM == 10600
    pvt->frame.data = data_ptr;
#else
    pvt->frame.data.ptr = data_ptr;
#endif

    // increment statistic counter
    pvt->recv_frame_curr++;
    pvt->recv_frame_total++;

exit_pvt_frame_:
    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    return &pvt->frame;

exit_null_frame_:
    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    return &ast_null_frame;
}

static int rgsm_voice_write(struct ast_channel *ast_ch, struct ast_frame *frame)
{
    struct gsm_pvt *pvt;

    //struct rtp_hdr *rtp_hdr_ptr;
    //rgsm_voice_frame_header_t   *hdr_ptr;

    char *data_ptr;
    int rc;
    int send_len;

    //
    if (!ast_ch)
    {
        ast_log(LOG_WARNING, "invalid ast channel\n");
        return 0;
    }

    if (!(pvt = ast_channel_tech_pvt(ast_ch)))
    {
        ast_log(LOG_ERROR, "ast channel=\"%s\" has't tech pvt\n", ast_channel_name(ast_ch));
        return 0;
    }

    // lock pvt channel
    ast_mutex_lock(&pvt->lock);
    //
    if (!pvt->owner)
    {
        ast_log(LOG_DEBUG, "rgsm channel has't owner\n");
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

    if (pvt->voice_fd == -1)
    {
        ast_log(LOG_DEBUG, "rgsm channel's voice not opened\n");
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

    // check for frame present
    if (!frame)
    {
        ast_log(LOG_ERROR, "<%s>: frame expected\n", pvt->name);
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

    // check for frame type
    if (frame->frametype != AST_FRAME_VOICE)
    {
        ast_log(LOG_ERROR, "<%s>: unsupported frame type = [%d]\n", pvt->name, frame->frametype);
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

    // check for frame format
#if ASTERISK_VERSION_NUM < 10800
    if (frame->subclass != pvt->frame_format)
    {
        ast_log(LOG_ERROR, "<%s>: frame format=%d mismatch session frame format=%d\n", pvt->name, frame->subclass, pvt->frame_format);
#else
    // This is written by Roman
    if (ast_format_get_codec_id(frame->subclass.format) != pvt->frame_format)
    {
        // 	ast_log(LOG_ERROR, "<%s>: frame format=%ld mismatch session frame format=%ld\n", pvt->name, (unsigned long)frame->subclass.codec, (unsigned long)pvt->frame_format);
#endif
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

    /* no RTP
	// get buffer for rtp header in AST_FRIENDLY_OFFSET
	if(frame->offset < sizeof(struct rtp_hdr)){
		ast_log(LOG_ERROR, "<%s>: not free space=%d in frame data for RTP header\n", pvt->name, frame->offset);
		ast_mutex_unlock(&pvt->lock); 	return 0;
    }
*/

    // check for valid frame datalen - SID packet was droped
    if (frame->datalen <= 4)
    {
        pvt->send_sid_curr++;
        pvt->send_sid_total++;
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

#if ASTERISK_VERSION_NUM == 10600
    if (!frame->data)
    {
#else
    if (!frame->data.ptr)
    {
#endif
        ast_log(LOG_ERROR, "<%s>: frame without data\n", pvt->name);
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

#if ASTERISK_VERSION_NUM == 10600
    data_ptr = (char *)(frame->data) /* + frame->offset*/;
#else
    data_ptr = (char *)(frame->data.ptr) /* + frame->offset*/;
#endif

    /*
    hdr_ptr = (rgsm_voice_frame_header_t*)(data_ptr - sizeof(rgsm_voice_frame_header_t));
    hdr_ptr->modem_id = pvt->modem_id;
    hdr_ptr->data_len = (uint16_t)frame->datalen;

    send_len = frame->datalen + sizeof(rgsm_voice_frame_header_t);
*/
    send_len = frame->datalen;

    DEBUG_VOICE("<%s>: voice write %d bytes, samples=%d, seqNo=%d\n", pvt->name, send_len, frame->samples, frame->seqno);

    //	if((rc = write(pvt->voice_fd, hdr_ptr, send_len)) < 0){

    //send raw data only, rgsm_voice_frame_header will be included in hal
    if ((rc = write(pvt->voice_fd, data_ptr, send_len)) < 0)
    {
        pvt->send_drop_curr++;
        pvt->send_drop_total++;
        ast_log(LOG_ERROR, "<%s>: write error=%d(%s)\n", pvt->name, errno, strerror(errno));
        ast_mutex_unlock(&pvt->lock);
        return 0;
    }

    pvt->send_frame_curr++;
    pvt->send_frame_total++;

write_exit_:
    // unlock pvt channel
    ast_mutex_unlock(&pvt->lock);
    return 0;
}

static int rgsm_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
    struct gsm_pvt *pvt = ast_channel_tech_pvt(newchan);

    if (!pvt)
    {
        ast_debug(1, "fixup failed, no pvt on newchan\n");
        return -1;
    }

    ast_mutex_lock(&pvt->lock);
    if (pvt->owner == oldchan)
        pvt->owner = newchan;
    ast_mutex_unlock(&pvt->lock);

    return 0;
}

static int rgsm_devicestate(void *data)
{
    char *device;
    int res = AST_DEVICE_INVALID;
    struct gsm_pvt *pvt;

    device = ast_strdupa(data);

    if ((pvt = find_ch_by_name(device)))
    {
        ast_mutex_lock(&pvt->lock);
        if (pvt->flags.enable)
        {
            if (pvt->owner)
            {
                res = AST_DEVICE_INUSE;
                /*
                pvt->voice_fd = ggw8_open_voice(pvt->ggw8_device, pvt->modem_id, (uint16_t)pvt->frame_format);
                if (pvt->voice_fd == -1) {
                    ast_log(AST_LOG_ERROR, "<%s>: can't open voice channel\n", pvt->name);
                } else {
                    ast_verb(3, "<%s>: open voice channel voice_fd=%d\n", pvt->name, pvt->voice_fd);
                    ast_channel_set_fd(pvt->owner, 0, pvt->voice_fd);
                }
*/
            }
            else
            {
                res = AST_DEVICE_NOT_INUSE;
                /*
                if (pvt->voice_fd != -1) {
                    ast_verb(3, "<%s>: close voice channel\n", pvt->name);
                    ggw8_close_voice(pvt->ggw8_device, pvt->modem_id);
                    pvt->voice_fd = -1;
                }
*/
            }
        }
        ast_mutex_unlock(&pvt->lock);
    }

    ast_debug(1, "[%s] Checking state for device: state=%d(%s)\n", device, res, ast_devstate2str(res));

    return res;
}

static int rgsm_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen)
{
    int res = 0;
    struct gsm_pvt *pvt = ast_channel_tech_pvt(ast);

    ast_mutex_lock(&pvt->lock);

    ast_debug(1, "Requested indication %d on modem %s\n", condition, pvt->name);

    switch (condition)
    {
    case AST_CONTROL_BUSY:
        break;
    case AST_CONTROL_CONGESTION:
        break;
    case AST_CONTROL_RINGING:
        break;
    case -1:
        res = -1; /* Ask for inband indications */
        break;
    case AST_CONTROL_PROGRESS:
        break;
    case AST_CONTROL_PROCEEDING:
        break;
    case AST_CONTROL_VIDUPDATE:
        break;
    case AST_CONTROL_SRCUPDATE:
        break;
    case AST_CONTROL_HOLD:
        ast_moh_start(ast, data, NULL);
        break;
    case AST_CONTROL_UNHOLD:
        ast_moh_stop(ast);
        break;
    default:
        ast_log(LOG_WARNING, "Don't know how to indicate condition %d\n on %s", condition, pvt->name);
        res = -1;
        break;
    }

    ast_mutex_unlock(&pvt->lock);

    return res;
}

/*

	Module

*/

static const char hw_section[] = "rgsm-hw";

//loads settings for particular channel
//default values already pre-set using rgsm_pvt section
static int rgsm_load_channel_config(struct gsm_pvt *pvt, chnl_config_t *chnl_config)
{
    struct ast_config *cfg;
    struct ast_flags config_flags = {0};

    const char *cvar;

    if (!chnl_config)
        return -1;

    cfg = ast_config_load(CONFIG_FILE, config_flags);
    if (!cfg)
    {
        ast_log(AST_LOG_DEBUG, "Failed to load %s/%s\n", ast_config_AST_CONFIG_DIR, CONFIG_FILE);
        return -1;
    }

    // this is written by Roman
    if (!ast_category_exist(cfg, pvt->name, ""))
        return -1;

    //May 17, 2013: Bz1927 - channel aliasing
    //get alias
    if ((cvar = ast_variable_retrieve(cfg, pvt->name, "alias")))
    {
        ast_copy_string(chnl_config->alias, cvar, sizeof(chnl_config->alias));
    }

    //get context
    if ((cvar = ast_variable_retrieve(cfg, pvt->name, "context")))
    {
        ast_copy_string(chnl_config->context, cvar, sizeof(chnl_config->context));
    }

    //get incoming type
    if ((cvar = ast_variable_retrieve(cfg, pvt->name, "incoming")))
    {
        if ((chnl_config->incoming_type = get_incoming_type(cvar)) == INC_TYPE_UNKNOWN)
        {
            ast_log(LOG_WARNING, "\"incoming\" has unknown param \"%s\" - set \"deny\"\n", cvar);
            chnl_config->incoming_type = INC_TYPE_DENY;
        }
    }

    // get specified name for incoming call
    if (chnl_config->incoming_type == INC_TYPE_SPEC)
    {
        if ((cvar = ast_variable_retrieve(cfg, pvt->name, "incomingto")))
        {
            // storing name
            ast_copy_string(chnl_config->incomingto, cvar, sizeof(chnl_config->incomingto));
        }
        else if (!chnl_config->incomingto || !strlen(chnl_config->incomingto))
        {
            ast_log(LOG_WARNING, "incoming type is \"spec\" but \"incomingto\" not present- set type \"deny\"\n");
            chnl_config->incoming_type = INC_TYPE_DENY;
        }
    }

    //get outgoing type
    if ((cvar = ast_variable_retrieve(cfg, pvt->name, "outgoing")))
    {
        if ((chnl_config->outgoing_type = get_outgoing_type(cvar)) == OUT_CALL_UNKNOWN)
        {
            ast_log(LOG_WARNING, "\"outgoing\" has unknown param \"%s\" - set \"deny\"\n", cvar);
            chnl_config->outgoing_type = OUT_CALL_DENY;
        }
    }

    if ((cvar = ast_variable_retrieve(cfg, pvt->name, "sim_toolkit")))
    {
        if (!strncmp("enable", cvar, 6))
        {
            chnl_config->sim_toolkit = 1;
        }
    }

    //Oct 20, 2014, power on channel on module load
    if ((cvar = ast_variable_retrieve(cfg, pvt->name, "auto_start")))
    {
        if (!strncmp("enable", cvar, 6))
        {
            chnl_config->auto_start = 1;
        }
    }

    ast_config_destroy(cfg);
    return 0;
}

//! instantiates gsm_pvt for modem
static struct gsm_pvt *gsm_pvt_new(hw_config_t *cfg,
                                   struct gateway *gateway,
                                   uint8_t modem_id)
{
    struct gsm_pvt *pvt;
    chnl_config_t chnl_config;

    ast_log(AST_LOG_DEBUG, "Instantiating gsm_pvt: gateway=%p, modem_id=%d\n", gateway, modem_id);

    /* create and initialize our pvt structure */
    if (!(pvt = ast_calloc(1, sizeof(*pvt))))
    {
        ast_log(AST_LOG_ERROR, "Skipping gsm_pvt instantiation. Error allocating memory.\n");
        return NULL;
    }

    memset(&chnl_config, 0, sizeof(chnl_config_t));

    ast_mutex_init(&pvt->lock);

    INIT_CMD_QUEUE(pvt);

    /* set some defaults */
    rgsm_init_pvt_state(pvt);

    //these flags should not change between channel restart
    pvt->power_man_disable = 0;
    pvt->init_settings.imei_change = 0;

    /* populate the pvt structure */
    pvt->ggw8_device = gateway->ggw8_device;
    pvt->unique_id = 8 * gateway->uid + modem_id;
    //May 17, 2013: fix Bz1926 - channel names start from 1
    sprintf(pvt->name, "%s%d", GSMPVT_NAME_PREFIX, pvt->unique_id + 1);
    //May 27, 2013: fix Bz1939 - Refactore rgsm-sms.db to make channel name insensitive
    sprintf(pvt->chname, "%s%d", GSMPVT_CHNAME_PREFIX, pvt->unique_id);
    pvt->modem_id = modem_id;
    //pvt->timeout = 10000;
    pvt->channel_thread = AST_PTHREADT_NULL;

    //init modem's fds
    pvt->at_fd = -1;
    pvt->voice_fd = -1;

    //assign debug flags
    memcpy(&pvt->debug_ctl, &gen_config.debug_ctl, sizeof(debug_ctl_t));

    //populate a chan_config with values of some pvt_config properies
    ast_copy_string(chnl_config.context, pvt_config.context, sizeof(pvt_config.context));
    chnl_config.incoming_type = pvt_config.incoming_type;
    ast_copy_string(chnl_config.incomingto, pvt_config.incomingto, sizeof(pvt_config.incomingto));
    chnl_config.outgoing_type = pvt_config.outgoing_type;
    //May 21, 2014
    chnl_config.sim_toolkit = pvt_config.sim_toolkit;

    //Oct 20, 2014, power on channel on module load
    chnl_config.auto_start = pvt_config.auto_start;

    //load overwritten properties, optional
    rgsm_load_channel_config(pvt, &chnl_config);

    //May 17, 2013: Bz1927 - channel aliasing
    //replace channel name with alias if alias non blank
    if (strlen(chnl_config.alias))
    {
        ast_copy_string(pvt->name, chnl_config.alias, sizeof(pvt->name));
    }

    memcpy(&pvt->chnl_config, &chnl_config, sizeof(chnl_config));

    pvt->reg_try_cnt_conf = pvt_config.reg_try_cnt;
    pvt->sim_try_cnt_conf = pvt_config.sim_try_cnt;
    ast_copy_string(pvt->balance_req_str, pvt_config.balance_req_str, sizeof(pvt->balance_req_str));
    //sms params
    pvt->sms_sendinterval.tv_sec = pvt_config.sms_sendinterval;
    pvt->sms_sendattempt = pvt_config.sms_sendattempt;
    pvt->sms_maxpartcount = pvt_config.sms_maxpartcount;
    pvt->sms_autodelete = pvt_config.sms_autodelete;

    //module type unknown for unpowered modem, it will be queried via ATI command at power on
    //use sim900 as default
    //19 Feb 2016, MODULE_TYPE_UNKNOWN
    pvt->module_type = MODULE_TYPE_UNKNOWN;
    //	pvt->module_type = MODULE_TYPE_SIM900;

    //	pvt->mic_gain_conf = sim900_config.mic_gain;
    //	pvt->spk_gain_conf = sim900_config.spk_gain;
    //	pvt->spk_audg_conf = sim900_config.spk_audg;

    pvt->functions.atp_handle_response = NULL;
    pvt->functions.set_sim_poll = NULL;
    pvt->functions.gsm_query_sim_data = NULL;
    pvt->functions.check_sim_status = NULL;
    pvt->functions.hangup = NULL;
    pvt->functions.change_imei = NULL;
    pvt->functions.setup_audio_channel = NULL;

    unknown_address(&pvt->subscriber_number);

    /* setup the smoother */
    /*
	if (!(pvt->smoother = ast_smoother_new(DEVICE_FRAME_SIZE))) {
		ast_log(AST_LOG_ERROR, "Skipping device %s. Error setting up frame smoother.\n", pvt->name);
		goto e_free_pvt;
	}
*/

    /* setup the dsp */
    /*
	if (!(pvt->dsp = ast_dsp_new())) {
		ast_log(AST_LOG_ERROR, "Skipping device %s. Error setting up dsp for dtmf detection.\n", pvt->name);
		goto e_free_smoother;
	}
*/

    /*! TODO: do we need a software DSP ???

	ast_dsp_set_features(pvt->dsp, DSP_FEATURE_DIGIT_DETECT);
	ast_dsp_set_digitmode(pvt->dsp, DSP_DIGITMODE_DTMF | DSP_DIGITMODE_RELAXDTMF);
*/
    ast_log(AST_LOG_DEBUG, "Instantiated gsm_pvt: gateway=%p, modem_id=%d, pvt=%p, name=\"%s\", context=\"%s\", "
                           "incoming=\"%s\", incomingto=\"%s\", outgoing=\"%s\", sim_toolkit=%d, auto_start=%d\n",
            gateway, modem_id, pvt, pvt->name,
            pvt->chnl_config.context,
            incoming_type_str(pvt->chnl_config.incoming_type),
            pvt->chnl_config.incomingto,
            outgoing_type_str(pvt->chnl_config.outgoing_type),
            pvt->chnl_config.sim_toolkit,
            pvt->chnl_config.auto_start);
    return pvt;
}

static void gsm_pvt_release(struct gsm_pvt *pvt, int hw_failure)
{
    int modem_id = pvt->modem_id;

    ast_log(AST_LOG_DEBUG, "Destroying gsm_pvt: modem_id=%d\n", modem_id);

    if (pvt->channel_thread != AST_PTHREADT_NULL)
    {
        //make attemp to shutdown the channel gracefully
        if (hw_failure)
        {
            gsm_abort_channel(pvt, MAN_CHSTATE_ABORTED_HWFAILURE);
        }
        else
        {
            gsm_shutdown_channel(pvt);
        }
        //pvt->flags.shutdown = 1;
        //wait until thead exit
        while (pvt->channel_thread != AST_PTHREADT_NULL)
        {
            us_sleep(1000);
        }
    }

    //ast_smoother_free(pvt->smoother);
    //ast_dsp_free(pvt->dsp);

    ast_mutex_destroy(&pvt->lock);
    ast_free(pvt);
    ast_log(AST_LOG_DEBUG, "Destroyed gsm_pvt: modem_id=%d\n", modem_id);
}

//!returns 0 on success, -1 on failure
static int rgsm_load_config(void)
{
    int ret = -1;
    struct ast_config *cfg;
    const char *cat;
    //struct ast_variable *v;
    struct ast_flags config_flags = {0};

    const char *cvar;
    struct ast_variable *v;

    cfg = ast_config_load(CONFIG_FILE, config_flags);
    if (!cfg)
    {
        ast_log(AST_LOG_ERROR, "Failed to load %s/%s\n", ast_config_AST_CONFIG_DIR, CONFIG_FILE);
        return -1;
    }

    ast_log(AST_LOG_DEBUG, "ast_config_load(%s/%s) - Ok\n", ast_config_AST_CONFIG_DIR, CONFIG_FILE);

    // lock rgsm subsystem
    ast_mutex_lock(&rgsm_lock);
    cat = ast_category_browse(cfg, NULL);

    while (cat)
    {
        if (!strcasecmp(cat, "general"))
        {
            // get at response timeout
            if ((cvar = ast_variable_retrieve(cfg, cat, "timeout.at.response")))
            {
                if (is_str_digit((char *)cvar))
                {
                    gen_config.timeout_at_response = atoi(cvar);
                }
                if (!gen_config.timeout_at_response)
                    gen_config.timeout_at_response = DEFAULT_TIMEOUT_AT_RESPONSE;
            }
            if ((cvar = ast_variable_retrieve(cfg, cat, "debug.at")))
            {
                if (is_str_digit((char *)cvar))
                {
                    gen_config.debug_ctl.at = atoi(cvar);
                }
            }
            if ((cvar = ast_variable_retrieve(cfg, cat, "debug.callsm")))
            {
                if (is_str_digit((char *)cvar))
                {
                    gen_config.debug_ctl.callsm = atoi(cvar);
                }
            }
            if ((cvar = ast_variable_retrieve(cfg, cat, "debug.hal")))
            {
                if (is_str_digit((char *)cvar))
                {
                    gen_config.debug_ctl.hal = atoi(cvar);
                }
            }
            if ((cvar = ast_variable_retrieve(cfg, cat, "debug.voice")))
            {
                if (is_str_digit((char *)cvar))
                {
                    gen_config.debug_ctl.voice = atoi(cvar);
                }
            }

            for (v = ast_variable_browse(cfg, cat); v; v = v->next)
            {
                ast_jb_read_conf(&gen_config.jbconf, v->name, v->value);
            }

            //get device jbsize
            gen_config.dev_jbsize = 100;
            if ((cvar = ast_variable_retrieve(cfg, cat, "dev_jbsize")))
            {
                if (is_str_digit((char *)cvar))
                {
                    gen_config.dev_jbsize = atoi(cvar);
                    if (gen_config.dev_jbsize < 0)
                    {
                        gen_config.dev_jbsize = 100;
                    }
                }
            }

            ast_log(AST_LOG_NOTICE, "Read general settings: timeout.at.response=%d, debug={at=%d, callsm=%d, hal=%d, voice=%d},"
                                    "jb_conf={flags=%u, max_size=%ld, resync_threshold=%ld, impl=%s, target_extra=%ld}, dev_jbsize=%d\n",
                    gen_config.timeout_at_response,
                    gen_config.debug_ctl.at, gen_config.debug_ctl.callsm, gen_config.debug_ctl.hal, gen_config.debug_ctl.voice,
                    gen_config.jbconf.flags, gen_config.jbconf.max_size, gen_config.jbconf.resync_threshold,
                    gen_config.jbconf.impl, gen_config.jbconf.target_extra,
                    gen_config.dev_jbsize);
        }
        else if (!strcasecmp(cat, "rgsm-hw"))
        {
            if (!(cvar = ast_variable_retrieve(cfg, cat, "vid")))
            {
                ast_log(AST_LOG_ERROR, "Missing or blank %s.vid option in config file\n", cat);
                goto cleanup_;
            }
            else
            {
                hw_config.vid = strtoul(cvar, NULL, 16);
                if (hw_config.vid == 0 || hw_config.vid > (unsigned int)0xffff)
                {
                    ast_log(AST_LOG_ERROR, "Invalid value of %s.vid=%s\n", cat, cvar);
                    goto cleanup_;
                }
            }

            if (!(cvar = ast_variable_retrieve(cfg, cat, "pid")))
            {
                ast_log(AST_LOG_ERROR, "Missing or blank %s.pid option in config file\n", cat);
                goto cleanup_;
            }
            else
            {
                hw_config.pid = strtoul(cvar, NULL, 16);
                if (hw_config.pid == 0 || hw_config.pid > (unsigned int)0xffff)
                {
                    ast_log(AST_LOG_ERROR, "Invalid value of %s.pid=%s\n", cat, cvar);
                    goto cleanup_;
                }
            }

            //product_string is optional param
            if ((cvar = ast_variable_retrieve(cfg, hw_section, "product_string")))
            {
                ast_copy_string(hw_config.product_string, cvar, sizeof(hw_config.product_string));
            }

            ast_log(AST_LOG_NOTICE, "Read RGSM-HW settings: vid=0x%.4x, pid=0x%.4x, product_string='%s'\n",
                    hw_config.vid, hw_config.pid, hw_config.product_string);
        }
        else if (!strcasecmp(cat, "sim900"))
        {
            //assume me_config has been initialized with correct default values
            if ((cvar = ast_variable_retrieve(cfg, cat, "mic_gain")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim900_config.mic_gain = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "spk_gain")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim900_config.spk_gain = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "spk_audg")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim900_config.spk_audg = atoi(cvar);
                }
            }

            cvar = ast_variable_retrieve(cfg, cat, "hex_download");
            if (cvar)
            {
                ast_copy_string(sim900_config.hex_download, cvar, sizeof(sim900_config.hex_download));
            }

            cvar = ast_variable_retrieve(cfg, cat, "fw_image");
            if (cvar)
            {
                ast_copy_string(sim900_config.fw_image, cvar, sizeof(sim900_config.fw_image));
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "at_send_rate")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim900_config.at_send_rate = atoi(cvar);
                }
            }

            ast_log(AST_LOG_NOTICE, "Read SIM900 settings: mic_gain=%d, spk_gain=%d, audg=%d, at_send_rate=%d, hex_download=\"%s\", fw_image=\"%s\"\n",
                    sim900_config.mic_gain,
                    sim900_config.spk_gain,
                    sim900_config.spk_audg,
                    sim900_config.at_send_rate,
                    sim900_config.hex_download,
                    sim900_config.fw_image);
        }
        else if (!strcasecmp(cat, "rgsm-pvt"))
        {
            //setting vcommon for all channels, TODO: channel may owerride some settings
            //get context
            if ((cvar = ast_variable_retrieve(cfg, cat, "context")))
            {
                ast_copy_string(pvt_config.context, cvar, sizeof(pvt_config.context));
            }

            //get incoming type
            pvt_config.incoming_type = INC_TYPE_DENY;
            if ((cvar = ast_variable_retrieve(cfg, cat, "incoming")))
            {
                if ((pvt_config.incoming_type = get_incoming_type(cvar)) == INC_TYPE_UNKNOWN)
                {
                    ast_log(LOG_WARNING, "\"incoming\" has unknown param \"%s\" - set \"deny\"\n", cvar);
                    pvt_config.incoming_type = INC_TYPE_DENY;
                }
            }

            // get specified name for incoming call
            pvt_config.incomingto[0] = '\0';
            if (pvt_config.incoming_type == INC_TYPE_SPEC)
            {
                if ((cvar = ast_variable_retrieve(cfg, cat, "incomingto")))
                {
                    // storing name
                    ast_copy_string(pvt_config.incomingto, cvar, sizeof(pvt_config.incomingto));
                }
                else
                {
                    ast_log(LOG_WARNING, "incoming type is \"spec\" but \"incomingto\" not present- set type \"deny\"\n");
                    pvt_config.incoming_type = INC_TYPE_DENY;
                }
            }

            //get outgoing type
            pvt_config.outgoing_type = OUT_CALL_DENY;
            if ((cvar = ast_variable_retrieve(cfg, cat, "outgoing")))
            {
                if ((pvt_config.outgoing_type = get_outgoing_type(cvar)) == OUT_CALL_UNKNOWN)
                {
                    ast_log(LOG_WARNING, "\"outgoing\" has unknown param \"%s\" - set \"deny\"\n", cvar);
                    pvt_config.outgoing_type = OUT_CALL_DENY;
                }
            }

            //get reg_try_cnt
            pvt_config.reg_try_cnt = 5;
            if ((cvar = ast_variable_retrieve(cfg, cat, "reg_try_cnt")))
            {
                if (is_str_digit((char *)cvar))
                {
                    pvt_config.reg_try_cnt = atoi(cvar);
                }
            }

            //get sim_try_cnt
            pvt_config.sim_try_cnt = 5;
            if ((cvar = ast_variable_retrieve(cfg, cat, "sim_try_cnt")))
            {
                if (is_str_digit((char *)cvar))
                {
                    pvt_config.sim_try_cnt = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "balance_req_str")))
            {
                // storing name
                ast_copy_string(pvt_config.balance_req_str, cvar, sizeof(pvt_config.balance_req_str));
            }

            // SMS
            // sms_sendinterval
            pvt_config.sms_sendinterval = 20;
            if ((cvar = ast_variable_retrieve(cfg, cat, "sms_sendinterval")))
            {
                /* check is digit */
                if (is_str_digit((char *)cvar))
                    pvt_config.sms_sendinterval = atoi(cvar);
            }
            // sms_sendattempt
            pvt_config.sms_sendattempt = 2;
            if ((cvar = ast_variable_retrieve(cfg, cat, "sms_sendattempt")))
            {
                /* check is digit */
                if (is_str_digit((char *)cvar))
                    pvt_config.sms_sendattempt = atoi(cvar);
            }
            // sms_maxpartcount
            pvt_config.sms_maxpartcount = 2;
            if ((cvar = ast_variable_retrieve(cfg, cat, "sms_maxpartcount")))
            {
                /* check is digit */
                if (is_str_digit((char *)cvar))
                    pvt_config.sms_maxpartcount = atoi(cvar);
            }

            // sms_autodelete
            pvt_config.sms_autodelete = 0;
            if ((cvar = ast_variable_retrieve(cfg, cat, "sms_autodelete")))
            {
                /* check is digit */
                if (is_str_digit((char *)cvar))
                    pvt_config.sms_autodelete = atoi(cvar);
            }

            //get voice_codec
            pvt_config.voice_codec = 0;
            if ((cvar = ast_variable_retrieve(cfg, cat, "voice_codec")))
            {
                if (is_str_digit((char *)cvar))
                {
                    pvt_config.voice_codec = atoi(cvar);
                }
            }

            //get autoclose_ussd_menu
            pvt_config.autoclose_ussd_menu = 0;
            if ((cvar = ast_variable_retrieve(cfg, cat, "autoclose_ussd_menu")))
            {
                if (is_str_digit((char *)cvar))
                {
                    pvt_config.autoclose_ussd_menu = atoi(cvar);
                }
            }

            //May 22, 2014
            //get sim_toolkit
            pvt_config.sim_toolkit = 0;
            if ((cvar = ast_variable_retrieve(cfg, cat, "sim_toolkit")))
            {
                if (!strncmp("enable", cvar, 6))
                {
                    pvt_config.sim_toolkit = 1;
                }
            }

            //Oct 20, 2014: "power on" channel on module load
            pvt_config.auto_start = 0;
            if ((cvar = ast_variable_retrieve(cfg, cat, "auto_start")))
            {
                if (!strncmp("enable", cvar, 6))
                {
                    pvt_config.auto_start = 1;
                }
            }

            ast_log(AST_LOG_NOTICE, "Read RGSM-PVT settings: context=%s, incoming_type=%s, incomingto=%s, outgoing=%s, reg_try_cnt=%d, "
                                    "sim_try_cnt=%d, sms_sendinterval=%d, sms_sendattempt=%d, sms_maxpartcount=%d, voice_codec=%d, sim_toolkit=%d, auto_start=%d, autoclose_ussd_menu=%d\n",
                    pvt_config.context,
                    incoming_type_str(pvt_config.incoming_type),
                    pvt_config.incomingto,
                    outgoing_type_str(pvt_config.outgoing_type),
                    pvt_config.reg_try_cnt,
                    pvt_config.sim_try_cnt,
                    pvt_config.sms_sendinterval,
                    pvt_config.sms_sendattempt,
                    pvt_config.sms_maxpartcount,
                    pvt_config.voice_codec,
                    pvt_config.sim_toolkit,
                    pvt_config.auto_start,
                    pvt_config.autoclose_ussd_menu);
        }
        else if (!strcasecmp(cat, "sim5320"))
        {
            //assume me_config has been initialized with correct default values
            if ((cvar = ast_variable_retrieve(cfg, cat, "mic_amp1")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.mic_amp1 = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "tx_vol")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.tx_lvl = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "rx_vol")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.rx_lvl = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "tx_gain")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.tx_gain = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "rx_gain")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.rx_gain = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "at_send_rate")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.at_send_rate = atoi(cvar);
                }
            }

            if ((cvar = ast_variable_retrieve(cfg, cat, "ussd_dcs")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.ussd_dcs = atoi(cvar);
                }
            }
            if ((cvar = ast_variable_retrieve(cfg, cat, "init_delay")))
            {
                if (is_str_digit((char *)cvar))
                {
                    sim5320_config.init_delay = atoi(cvar);
                }
            }

            ast_log(AST_LOG_NOTICE, "Read SIM5320 settings: mic_amp1=%d, tx_vol=%d, rx_vol=%d, at_send_rate=%d\n",
                    sim5320_config.mic_amp1,
                    sim5320_config.tx_lvl,
                    sim5320_config.rx_lvl,
                    sim5320_config.at_send_rate);
        }
        else
        {
            //TODO: more sections
        }
        //next category aka section
        cat = ast_category_browse(cfg, cat);
    }

    /* parse [general] section */
    /*
	for (v = ast_variable_browse(cfg, "general"); v; v = v->next) {
		// handle jb conf
		if (!ast_jb_read_conf(&global_jbconf, v->name, v->value))
			continue;
	}
*/

    ret = 0;
cleanup_:
    ast_config_destroy(cfg);
    ast_mutex_unlock(&rgsm_lock);
    return ret;
}

static void release_gateway(struct gateway *gw, int hw_failure)
{
    struct gsm_pvt *pvt;
    int i;

    //release gsm_pvts
    for (i = 0; i < MAX_MODEMS_ON_BOARD; i++)
    {
        pvt = gw->gsm_pvts[i];
        if (!pvt)
            continue;
        gsm_pvt_release(pvt, hw_failure);
        gw->gsm_pvts[i] = NULL;
    }

    ggw8_close_device(gw->ggw8_device);

    ast_verbose("rgsm: unregistered gateway=%p\n", gw);

    ast_free(gw);
}

static int ggw8_isnew_device_cb(ggw8_device_sysid_t sysid)
{
    struct gateway *gw;

    ast_mutex_lock(&rgsm_lock);
    gw = find_gateway_by_sysid(sysid);
    ast_mutex_unlock(&rgsm_lock);
    return gw == NULL;
}

//! It's a callback invoked by HAL. Should return 0 on success, -1 on failure
static int ggw8_add_device_cb(hw_config_t *cfg, struct ggw8_device *dev)
{
    struct gateway *gw = NULL;
    int modem_indx;
    int i;
    ggw8_device_sysid_t sysid;
    ggw8_mode_t mode;

    gw = ast_calloc(1, sizeof(struct gateway));
    gw->ggw8_device = dev;

    ast_mutex_lock(&rgsm_lock);
    i = 0;
    while (find_gateway_by_uid(i))
    {
        i++;
    }
    gw->uid = i;
    ast_mutex_unlock(&rgsm_lock);

    mode = ggw8_get_device_mode(dev);

    // create channels if non DFU
    if (mode == GSM)
    {
        i = 0;
        for (modem_indx = 0; modem_indx < MAX_MODEMS_ON_BOARD; modem_indx++)
        {
            if (ggw8_get_device_info(dev)->gsm_presence & (1 << modem_indx))
            {
                gw->gsm_pvts[modem_indx] = gsm_pvt_new(cfg, gw, modem_indx);
                if (gw->gsm_pvts[modem_indx])
                {
                    i++;
                }
            }
            else
            {
                gw->gsm_pvts[modem_indx] = NULL;
            }
        }
    }

    ast_mutex_lock(&rgsm_lock);

    AST_LIST_INSERT_TAIL(&gateways, gw, link);

    sysid = ggw8_get_device_sysid(gw->ggw8_device);

    ast_verbose("rgsm: registered gateway=%p, UID=%d, dev=%p, SYSID=%.3u:%.3u, available_chnls=%d, codecs=0x%.4x, mode=\"%s\"\n",
                gw,
                gw->uid,
                dev,
                (sysid >> 8),
                (sysid & 0xff),
                i,
                ggw8_get_device_info(gw->ggw8_device)->supported_codecs,
                ggw8_get_device_mode(gw->ggw8_device) == DFU ? "DFU" : "GSM");

    ast_mutex_unlock(&rgsm_lock);

    //Jul 6, 2014: make consistent the channel's ids by propagating the gateway's uid to simsimulator
    if (ggw8_get_device_mode(gw->ggw8_device) == GSM)
    {
        ipc_propagate_gateway_uid(gw, 1);
    }

    //<<<Oct 20, 2014: auto-start the channels if need
    if (mode == GSM)
    {
        for (modem_indx = 0, i = 1; modem_indx < MAX_MODEMS_ON_BOARD; modem_indx++)
        {
            if (gw->gsm_pvts[modem_indx] && gw->gsm_pvts[modem_indx]->chnl_config.auto_start)
            {
                ast_verbose("rgsm: <%s>: defer auto start within %d sec...\n", gw->gsm_pvts[modem_indx]->name, i);
                //start at fixed baud rate
                rgsm_pvt_power_on(NULL, gw->gsm_pvts[modem_indx], i, BR_115200);
                i++;
            }
        }
    }
    //>>>

    return 0;
}

//device monitor members
static struct timeval t2_timeout = {GGW8_DISCOVERY_PERIOD, 0};
static struct rgsm_timer t2;
static struct timeval t3_timeout = {SIMULATOR_PING_PERIOD, 0};
static struct rgsm_timer t3;
static pthread_t device_monitor_thread = AST_PTHREADT_NULL;
static atomic_t device_monitor_running = ATOMIC_INIT(0);

static void *device_monitor_func(void *data)
{
    //
    struct gateway *gw;
    struct gateway *next_gw;
    int release_it;

    ast_log(AST_LOG_DEBUG, "rgsm: devices monitor thread started\n");
    atomic_set(&device_monitor_running, 1);
    rgsm_timer_set(t2, t2_timeout);
    //Aug 30, 2014:
    rgsm_timer_set(t3, t3_timeout);

    while (!atomic_read(&unloading_flag))
    {
        //check device presence every 50ms
        us_sleep(50000);

        ast_mutex_lock(&rgsm_lock);
        gw = AST_LIST_FIRST(&gateways);
        ast_mutex_unlock(&rgsm_lock);

    next_:
        release_it = 0;

        if (gw && (!ggw8_is_alive(gw->ggw8_device) || ggw8_is_restart(gw->ggw8_device)))
        {
            ast_mutex_lock(&rgsm_lock);
            next_gw = AST_LIST_NEXT(gw, link);
            AST_LIST_REMOVE(&gateways, gw, link);
            ast_mutex_unlock(&rgsm_lock);
            release_it = 1;
        }

        if (gw)
        {
            if (release_it)
            {
                //May 23, 2012: Bz1935 DFU with enabled channels fails
                //release gateway and its channels, the status HW_FAILURE only when device in not alive
                release_gateway(gw, (!ggw8_is_alive(gw->ggw8_device) ? 1 : 0));
                gw = next_gw;
            }
            else
            {
                ast_mutex_lock(&rgsm_lock);
                gw = AST_LIST_NEXT(gw, link);
                ast_mutex_unlock(&rgsm_lock);
            }
            goto next_;
        }

        if (is_rgsm_timer_fired(t2))
        {
            //rediscover devices
            rgsm_timer_stop(t2);
            ggw8_discover_devices(&hw_config, &ggw8_isnew_device_cb, &ggw8_add_device_cb);
            rgsm_timer_set(t2, t2_timeout);
        }

        //Jul 6, 2014: make consistent the channel's ids by propagating the gateway's uid to simsimulator
        //simulator may reboot, so ping the simulator periodically
        if (is_rgsm_timer_fired(t3))
        {
            rgsm_timer_stop(t3);

            ast_mutex_lock(&rgsm_lock);
            AST_LIST_TRAVERSE(&gateways, gw, link)
            {
                ipc_propagate_gateway_uid(gw, 0);
            }
            ast_mutex_unlock(&rgsm_lock);

            rgsm_timer_set(t3, t3_timeout);
        }
    }

    device_monitor_thread = AST_PTHREADT_NULL;
    atomic_set(&device_monitor_running, 0);

    ast_log(AST_LOG_DEBUG, "rgsm: devices monitor thread stopped\n");

    return NULL;
}

static void rgsm_atexit()
{
    struct gateway *gw;
    ast_log(AST_LOG_DEBUG, "Cleanup at exit\n");
    /* signal everyone we are unloading */
    //set_unloading();
    atomic_set(&unloading_flag, 1);

    //wait until device monitor stop
    while (atomic_read(&device_monitor_running))
    {
        us_sleep(1000);
    }

    //destroy gateways -> devices will be closed as well
next_:
    ast_mutex_lock(&rgsm_lock);
    gw = AST_LIST_REMOVE_HEAD(&gateways, link);
    ast_mutex_unlock(&rgsm_lock);

    if (gw)
    {
        release_gateway(gw, 0);
        goto next_;
    }

    //close hal
    ggw8_close_context();
    //close smsm db
    close_sms_db();
}

static int module_unload(void)
{
    ast_log(AST_LOG_DEBUG, "Unloading chan_rgsm.so....\n");

    rgsm_atexit();

    ast_unregister_atexit(rgsm_atexit);

    /* Unregister the CLI & APP & MANAGER */
#ifdef __APP__
    ast_unregister_application(app_status);
    ast_unregister_application(app_send_sms);
#endif
    rgsm_man_unregister();
    ast_cli_unregister_multiple(chan_rgsm_cli, sizeof(chan_rgsm_cli) / sizeof(chan_rgsm_cli[0]));

    ast_channel_unregister(&rgsm_tech);
    ast_log(AST_LOG_DEBUG, "Unloaded chan_rgsm.so\n");
    return 0;
}

static int module_load(void)
{
    ast_log(AST_LOG_DEBUG, "Loading chan_rgsm.so ....\n");

    //always start in non dfu
    atomic_set(&is_dfu, 0);

    /* Copy the default jb config over global_jbconf */
    memcpy(&global_jbconf, &default_jbconf, sizeof(struct ast_jb_conf));

    if (rgsm_load_config())
    {
        ast_log(AST_LOG_ERROR, "Failed to process config file: %s. Module not loaded\n", CONFIG_FILE);
        return AST_MODULE_LOAD_FAILURE;
    }

    //int rc = system("/home/sogurtsov/prj/raa.rsb/server-sw/simsimulator/bin/Debug/simsimulator -l1 -c simsimulator-10.10.0.118.conf -ad");
    //ast_log(AST_LOG_DEBUG, "SimSimulator running: %d\n", rc);

    /* register our channel type */
    if (ast_channel_register(&rgsm_tech))
    {
        ast_log(AST_LOG_ERROR, "Unable to register RGSM channel driver class\n");
        return AST_MODULE_LOAD_FAILURE;
    }

    if (ast_cli_register_multiple(chan_rgsm_cli, sizeof(chan_rgsm_cli) / sizeof(chan_rgsm_cli[0])))
    {
        ast_log(AST_LOG_ERROR, "Unable to register CLI staff\n");
        ast_channel_unregister(&rgsm_tech);
        return AST_MODULE_LOAD_FAILURE;
    }

    //populate start_time
    gettimeofday(&rgsm_start_time, NULL);

#ifdef __APP__
    ast_register_application(app_status, app_status_exec, app_status_synopsis, app_status_desc);
    ast_register_application(app_send_sms, app_send_sms_exec, app_send_sms_synopsis, app_send_sms_desc);
#endif

    rgsm_man_register();

    // register atexit function
    if (ast_register_atexit(rgsm_atexit) < 0)
    {
        ast_log(LOG_ERROR, "Unable to register atexit function\n");
        rgsm_man_unregister();
        ast_cli_unregister_multiple(chan_rgsm_cli, sizeof(chan_rgsm_cli) / sizeof(chan_rgsm_cli[0]));
        ast_channel_unregister(&rgsm_tech);
        return AST_MODULE_LOAD_FAILURE;
    }

    if (ggw8_init_context())
    {
        ast_log(AST_LOG_ERROR, "Failed to open HAL context. Module not loaded\n");
        module_unload();
        return AST_MODULE_LOAD_FAILURE;
    }

    //discover devices, the ggw8_add_device callback will be invoked for each device found for instantiating a
    //device channels
    ast_log(AST_LOG_DEBUG, "Device discovering at startup\n");
    int dev_cnt = ggw8_discover_devices(&hw_config, &ggw8_isnew_device_cb, &ggw8_add_device_cb);
    if (dev_cnt == -1)
    {
        module_unload();
        return AST_MODULE_LOAD_FAILURE;
    }
    else
    {
        ast_log(AST_LOG_DEBUG, "Total ggw8 devices discovered at startup: count=%d\n", dev_cnt);
    }

    if (ast_pthread_create_background(&device_monitor_thread, NULL, device_monitor_func, NULL) < 0)
    {
        ast_log(AST_LOG_ERROR, "Unable to create device_monitor_thread\n");
        module_unload();
        return AST_MODULE_LOAD_FAILURE;
    }

    open_sms_db(ast_config_AST_LOG_DIR);
    ast_log(AST_LOG_DEBUG, "Loaded chan_rgsm.so\n");
    return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "RGSM Channel Driver",
                .load = module_load,
                .unload = module_unload, );

/*

	Thread routines

*/

//helper function to handle pvt signals
//note both rgsm_lock and pvt->lock must be locked before this call
static void handle_pvt_signals(struct gsm_pvt *pvt)
{
    int restart_request = 0;

    // shutdown
    if (pvt->flags.shutdown)
    {
        //
        pvt->flags.shutdown = 0;
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: received signal to shutdown channel\n", pvt->name);
        // reset restart flag
        pvt->flags.restart = 0;
        pvt->flags.restart_now = 0;
    }
    else if (pvt->flags.restart && !pvt->flags.shutdown_now)
    {
        pvt->flags.restart = 0;
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: received signal to restart channel within %d seconds\n", pvt->name, pvt->rst_delay_sec);
        restart_request = 1;
    }
    else
    {
        return;
    }

    //modem power will be switched off
    if (pvt->owner)
    {
        // hangup - GSM
        if (pvt->functions.hangup != NULL)
        {
            pvt->functions.hangup(pvt);
        }
        else
            rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

        // hangup - asterisk core call
        call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_NORMAL_CLEARING);
        //
        while (pvt->owner)
        {
            rgsm_usleep(pvt, 1);
        }
    }

    // stop all timers
    memset(&pvt->timers, 0, sizeof(rgsm_timers_t));

    if (restart_request)
    {
        // set init signal
        pvt->flags.init = 1;
        pvt->flags.cpin_checked = 0;
        pvt->mdm_state = MDM_STATE_RESET;
    }
    else
    {
        pvt->mdm_state = MDM_STATE_PWR_DOWN;
    }

    // set channel call state as disabled
    pvt->call_state = CALL_STATE_DISABLE;
}

static void modem_status_request(struct gsm_pvt *pvt)
{
    //will be invoked by one second timer
    // request registration status
    rgsm_atcmd_queue_append(pvt, AT_CREG, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
    // get signal quality
    rgsm_atcmd_queue_append(pvt, AT_CSQ, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
    // get ME status
    if (pvt->call_state == CALL_STATE_DISABLE)
    {
        rgsm_atcmd_queue_append(pvt, AT_CPAS, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
    }
    // get current call list
    rgsm_atcmd_queue_append(pvt, AT_CLCC, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
}

static void handle_pvt_timers(struct gsm_pvt *pvt)
{
    struct ast_channel *tmp_ast_chnl = NULL;

    // timers processing
    // waitrdy
    if (is_rgsm_timer_enable(pvt->timers.waitrdy))
    {
        if (is_rgsm_timer_fired(pvt->timers.waitrdy))
        {
            // stop waitrdy timer
            rgsm_timer_stop(pvt->timers.waitrdy);
            // waitrdy timer fired
            ast_verbose("<%s>: waitrdy timer fired, try serial port at next baud rate\n", pvt->name);

            // set restart flag
            if (!pvt->flags.shutdown_now && !pvt->flags.restart_now)
            {
                RST_CHANNEL(pvt, 0);
                pvt->flags.adjust_baudrate = 1;
            }
        }
    }
    //abaudwait
    if (is_rgsm_timer_enable(pvt->timers.abaudwait))
    {
        if (is_rgsm_timer_fired(pvt->timers.abaudwait))
        {
            // stop abaudwait timer
            rgsm_timer_stop(pvt->timers.abaudwait);
            // waitrdy timer fired
            ast_log(AST_LOG_DEBUG, "<%s>: abaudwait timer fired, start sync procedure\n", pvt->name);
            //May 23, 2013: clear the trash in receive buffer
            ATP_CLEAR_RECV_BUF(pvt);
            // send AT_IPR read command to sync TE and ME
            rgsm_atcmd_queue_append(pvt, AT_SIM900_IPR, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
        }
    }
    //dtmf
    if (is_rgsm_timer_enable(pvt->timers.dtmf))
    {
        if (is_rgsm_timer_fired(pvt->timers.dtmf))
        {
            // stop dtmf timer
            rgsm_timer_stop(pvt->timers.dtmf);
            ast_debug(4, "<%s>: dtmf end timer fired\n", pvt->name);
            pvt->event_is_now_recv_end = 1;
        }
    }
    // callready
    if (is_rgsm_timer_enable(pvt->timers.callready))
    {
        if (is_rgsm_timer_fired(pvt->timers.callready))
        {
            // callready timer fired
            ast_verbose("<%s>: callready timer fired, restart channel\n", pvt->name);
            // set restart flag
            if (!pvt->flags.shutdown_now && !pvt->flags.restart_now)
            {
                RST_CHANNEL(pvt, 0);
            }
            // stop callready timer
            rgsm_timer_stop(pvt->timers.callready);
        }
    }
    // testfun
    if (is_rgsm_timer_enable(pvt->timers.testfun))
    {
        if (is_rgsm_timer_fired(pvt->timers.testfun))
        {

            // stop testfun timer
            rgsm_timer_stop(pvt->timers.testfun);
            // stop testfunsend timer
            rgsm_timer_stop(pvt->timers.testfunsend);
#if 0
            if (pvt->func_test_rate == 0)
                pvt->func_test_rate = pvt->config.baudrate;
            else if (pvt->func_test_rate == 115200)
                pvt->func_test_rate = 9600;
            else
                pvt->func_test_rate = 115200;

            ast_verb(3, "<%s>: probe serial port speed = %d baud\n", pvt->name, pvt->func_test_rate);
#endif
            // set restart flag
            if (!pvt->flags.shutdown_now && !pvt->flags.restart_now)
            {
                RST_CHANNEL(pvt, 0);
            }
        }
    }
    // testfunsend
    if (is_rgsm_timer_enable(pvt->timers.testfunsend))
    {
        if (is_rgsm_timer_fired(pvt->timers.testfunsend))
        {
            // testfunsend timer fired - send cfun read status command
            rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
            // restart testfunsend timer
            rgsm_timer_set(pvt->timers.testfunsend, testfunsend_timeout);
        }
    }
    /*! May 22, 2013: runhalfsecond retired
    // runhalfsecond
    if (is_rgsm_timer_enable(pvt->timers.runhalfsecond)) {
        if (is_rgsm_timer_fired(pvt->timers.runhalfsecond)) {
            //moved to one second timer
            //modem_status_request(pvt);
            rgsm_timer_set(pvt->timers.runhalfsecond, runhalfsecond_timeout);
        }
    }
*/
    // runonesecond
    if (is_rgsm_timer_enable(pvt->timers.runonesecond))
    {
        if (is_rgsm_timer_fired(pvt->timers.runonesecond))
        {
            // runonesecond timer fired
            if (pvt->active_line)
                pvt->active_line_contest = 0;
            if (pvt->wait_line)
                pvt->wait_line_contest = 0;
            // stop callwait tone
            if (pvt->owner && pvt->is_play_tone)
            {
//                tmp_ast_chnl = ast_channel_internal_bridged_channel(pvt->owner);

                if (tmp_ast_chnl)
                {
                    ast_playtones_stop(tmp_ast_chnl);
                    tmp_ast_chnl = NULL;
                }
                pvt->is_play_tone = 0;
            }

            //originally this handler executed twice per second
            modem_status_request(pvt);

            // restart runonesecond timer
            rgsm_timer_set(pvt->timers.runonesecond, runonesecond_timeout);
        }
    }
    // runfivesecond
    if (is_rgsm_timer_enable(pvt->timers.runfivesecond))
    {
        if (is_rgsm_timer_fired(pvt->timers.runfivesecond))
        {
            // runfivesecond timer fired
            if (pvt->call_state <= CALL_STATE_NULL)
            {
                // test functionality status
                rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
                // check SIM status
                if (pvt->module_type != MODULE_TYPE_UNKNOWN && pvt->functions.check_sim_status != NULL)
                {
                    pvt->functions.check_sim_status(pvt);
                }

                rgsm_atcmd_queue_append(pvt, AT_CSCA, AT_OPER_READ, 0, 5, 0, NULL);

                //! get operator, query both properties while they unknown
                //name
                if (!strncmp(pvt->operator_name, "unknown", sizeof(pvt->operator_name)) && ((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)))
                {
                    rgsm_atcmd_queue_append(pvt, AT_COPS, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "3,0");
                    rgsm_atcmd_queue_append(pvt, AT_COPS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
                }
                // code
                if (!strncmp(pvt->operator_code, "unknown", sizeof(pvt->operator_code)) && ((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)))
                {
                    rgsm_atcmd_queue_append(pvt, AT_COPS, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "3,2");
                    rgsm_atcmd_queue_append(pvt, AT_COPS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
                }
            }

            // restart runfivesecond timer
            rgsm_timer_set(pvt->timers.runfivesecond, runfivesecond_timeout);
        }
    }
    // runhalfminute
    if (is_rgsm_timer_enable(pvt->timers.runhalfminute))
    {
        if (is_rgsm_timer_fired(pvt->timers.runhalfminute))
        {
            // runhalfminute timer fired
            if (pvt->call_state <= CALL_STATE_NULL)
            {

                //May 28, 2015: query periodically iccid and imsi to workaround a simcard hot swap
                //if(pvt->module_type == MODULE_TYPE_SIM900) {
                //    rgsm_atcmd_queue_append(pvt, AT_SIM900_CCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                //}
                /*
                else if(pvt->module_type == MODULE_TYPE_SIM300) {
                    rgsm_atcmd_queue_append(pvt, AT_SIM300_CCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                }
                else if(pvt->module_type == MODULE_TYPE_M10) {
                    rgsm_atcmd_queue_append(pvt, AT_M10_QCCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                }
                */
                //rgsm_atcmd_queue_append(pvt, AT_CIMI, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
            }
            // restart runhalfminute timer
            rgsm_timer_set(pvt->timers.runhalfminute, halfminute_timeout);
        }
    }
    // runoneminute
    if (is_rgsm_timer_enable(pvt->timers.runoneminute))
    {
        if (is_rgsm_timer_fired(pvt->timers.runoneminute))
        {
            // runoneminute timer fired
            if (pvt->call_state <= CALL_STATE_NULL)
            {
                // get SMS center address
                //!moved to 5 seconds timer
                //rgsm_atcmd_queue_append(pvt, AT_CSCA, AT_OPER_READ, 0, 10, 0, NULL);
            }
            // restart runoneminute timer
            rgsm_timer_set(pvt->timers.runoneminute, runoneminute_timeout);
        }
    }
    // waitsuspend
    if (is_rgsm_timer_enable(pvt->timers.waitsuspend))
    {
        if (is_rgsm_timer_fired(pvt->timers.waitsuspend))
        {
            // waitsuspend timer fired
            ast_log(LOG_ERROR, "<%s>: can't switch in suspend mode\n", pvt->name);
            // stop waitsuspend timer
            rgsm_timer_stop(pvt->timers.waitsuspend);
            //
            if (pvt->flags.sim_present) // start pinwait timer
                rgsm_timer_set(pvt->timers.pinwait, pinwait_timeout);
            else // start simpoll timer
                rgsm_timer_set(pvt->timers.simpoll, simpoll_timeout);
        }
    }
    // dial
    if (is_rgsm_timer_enable(pvt->timers.dial))
    {
        if (is_rgsm_timer_fired(pvt->timers.dial))
        {
            // dial timer fired
            ast_verb(4, "<%s>: dialing timeout=%ld.%06ld expired\n",
                     pvt->name,
                     pvt->timers.dial.timeout.tv_sec,
                     pvt->timers.dial.timeout.tv_usec);
            // hangup channel
            if (pvt->owner)
                call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_NO_ANSWER);
            // hangup gsm channel
            if (pvt->functions.hangup != NULL)
            {
                pvt->functions.hangup(pvt);
            }
            else
                rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
            // stop dial timer
            rgsm_timer_stop(pvt->timers.dial);
        }
    }
    // simpoll
    if (is_rgsm_timer_enable(pvt->timers.simpoll))
    {
        if (is_rgsm_timer_fired(pvt->timers.simpoll))
        {
            // simpoll timer fired
            ast_verbose("<%s>: simpoll timer fired\n", pvt->name);
            // stop simpoll timer
            rgsm_timer_stop(pvt->timers.simpoll);
            //
            if (pvt->flags.resume_now)
            {
                ast_verbose("<%s>: module returned from suspend state\n", pvt->name);
                pvt->flags.resume_now = 0;
            }
            // check is sim present
            /*if(pvt->module_type == MODULE_TYPE_SIM300)
                rgsm_atcmd_queue_append(pvt, AT_SIM300_CSMINS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
            else*/
            if (pvt->module_type != MODULE_TYPE_UNKNOWN && pvt->functions.check_sim_status != NULL)
            {
                pvt->functions.check_sim_status(pvt);
            }
            // try to enable GSM module full functionality
            rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "1");
            // next mgmt state -- check for SIM is READY
            pvt->mdm_state = MDM_STATE_CHECK_PIN;
            // start pinwait timer
            rgsm_timer_set(pvt->timers.pinwait, pinwait_timeout);
        }
    }
    // pinwait
    if (is_rgsm_timer_enable(pvt->timers.pinwait))
    {
        if (is_rgsm_timer_fired(pvt->timers.pinwait))
        {
            //Sept 1, 2014
            pvt->sim_try_cnt_curr--;
            if (!pvt->sim_try_cnt_curr)
            {
                gsm_abort_channel(pvt, MAN_CHSTATE_ABORTED_NOSIM);
                return;
            }

            // pinwait timer fired
            ast_verbose("<%s>: pinwait timer fired (remaining attempts %d)\n", pvt->name, pvt->sim_try_cnt_curr);
            // stop pinwait timer
            rgsm_timer_stop(pvt->timers.pinwait);

            // try to disable GSM module full functionality
            rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");

            // check is sim present
            if (pvt->module_type != MODULE_TYPE_UNKNOWN && pvt->functions.check_sim_status != NULL)
            {
                pvt->functions.check_sim_status(pvt);
            }
            //
            if (pvt->flags.suspend_now)
            {
                // stop all timers
                memset(&pvt->timers, 0, sizeof(rgsm_timers_t));
                ast_verbose("<%s>: module switched to suspend state\n", pvt->name);
                pvt->mdm_state = MDM_STATE_SUSPEND;
                pvt->flags.suspend_now = 0;
            }
            // start simpoll timer
            if (pvt->mdm_state != MDM_STATE_SUSPEND)
                rgsm_timer_set(pvt->timers.simpoll, simpoll_timeout);
        }
    }
    // smssend
    if (is_rgsm_timer_enable(pvt->timers.smssend))
    {
        if (is_rgsm_timer_fired(pvt->timers.smssend))
        {
            // smssend timer fired
            ast_debug(2, "<%s>: sms send timer fired %ld.%ld\n", pvt->name, pvt->curr_tv.tv_sec, pvt->curr_tv.tv_usec);
            smssend_timer_handler(pvt);
            // restart smssend timer
            rgsm_timer_set(pvt->timers.smssend, pvt->sms_sendinterval);
        }
    }
    // initready
    if (is_rgsm_timer_enable(pvt->timers.initready))
    {
        if (is_rgsm_timer_fired(pvt->timers.initready))
        {
            // initready timer fired
            rgsm_timer_stop(pvt->timers.initready);
            ast_debug(2, "<%s>: initready timer fired %ld.%ld\n", pvt->name, pvt->curr_tv.tv_sec, pvt->curr_tv.tv_usec);
            if (pvt->mdm_state == MDM_STATE_WAIT_CFUN)
                rgsm_atcmd_queue_append(pvt, AT_SIM5320_CFUN, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
            // start runonesecond timer
            rgsm_timer_set(pvt->timers.runonesecond, runonesecond_timeout);
            // start runhalfminute timer
            rgsm_timer_set(pvt->timers.runhalfminute, runonesecond_timeout);
            // start runoneminute timer
            rgsm_timer_set(pvt->timers.runoneminute, runonesecond_timeout);
            // start smssend timer
            rgsm_timer_set(pvt->timers.smssend, pvt->sms_sendinterval);
            // start runvifesecond timer
            rgsm_timer_set(pvt->timers.runfivesecond, runfivesecond_timeout);
            //
        }
    }
    // runonesecond
    if (is_rgsm_timer_enable(pvt->timers.initstat))
    {
        if (is_rgsm_timer_fired(pvt->timers.initstat))
        {
            // Init status timer fired
            if (pvt->module_type != MODULE_TYPE_UNKNOWN)
            {
                if (pvt->functions.check_init_status != NULL)
                    pvt->functions.check_init_status(pvt);
            }
            // restart runoneminute timer
            rgsm_timer_set(pvt->timers.initstat, runonesecond_timeout);
        }
    }
}

void rgsm_init_pvt_state(struct gsm_pvt *pvt)
{
    ast_log(AST_LOG_NOTICE, "rgsm: <%s> init pvt state\n", pvt->name);

    rgsm_atcmd_queue_flush(pvt);

    // init command queue
    INIT_CMD_QUEUE(pvt);

    rgsm_timer_stop(pvt->timers.trysend);

    pvt->cmt_pdu_wait = 0;
    pvt->cds_pdu_wait = 0;
    pvt->now_send_pdu_id = 0;

    pvt->func_test_run = 0;
    pvt->func_test_done = 1;

    pvt->init_settings.ready = 0;
    pvt->init_settings.clip = 1;
    pvt->init_settings.chfa = 1;
    pvt->init_settings.colp = 0;
    pvt->init_settings.clvl = 1;
    pvt->init_settings.cmic = 1;
    pvt->init_settings.cscs = 1;
    pvt->init_settings.cmee = 1;
    pvt->init_settings.cclk = 1;
    pvt->init_settings.creg = 0;
    pvt->init_settings.cmgf = 1;
    pvt->init_settings.cnmi = 1;
    pvt->init_settings.fallback = 0;

    //! DO NOT CLEAR THE init_settings.imei_change bit here

    // startup signal
    pvt->flags.shutdown = 0;
    pvt->flags.shutdown_now = 0;
    pvt->flags.restart = 0;
    pvt->flags.restart_now = 0;
    pvt->flags.hard_reset = 0;
    pvt->flags.init = 1;
    pvt->flags.cpin_checked = 0;
    pvt->flags.changesim = 0;
    pvt->flags.testsim = 0;
    pvt->flags.sim_present = 0;
    pvt->flags.sim_startup = 0;
    pvt->flags.balance_req = 1;
    pvt->flags.subscriber_number_req = 1;
    pvt->flags.stk_capabilities_req = 1;
    //Aug 30, 2014
    pvt->flags.module_type_discovered = 0;

    // 20 Feb 2016 Need for hot change device on board
    // Clean module type and driver functions
    pvt->module_type = MODULE_TYPE_UNKNOWN;
    pvt->functions.atp_handle_response = NULL;
    pvt->functions.set_sim_poll = NULL;
    pvt->functions.gsm_query_sim_data = NULL;
    pvt->functions.check_sim_status = NULL;
    pvt->functions.hangup = NULL;
    pvt->functions.change_imei = NULL;
    pvt->functions.setup_audio_channel = NULL;

    //! DO NOT CLEAR THE pvt->power_man_disable bit here

    //SIM Toolkit state vars
    pvt->stk_capabilities = 0;
    pvt->stk_cmd_done = 0;
    pvt->stk_sync_notification_count = 0;
    pvt->stk_mansession = NULL;
    *pvt->stk_status = '\0';
    *pvt->stk_action_id = '\0';

    // query signal
    memset(&pvt->querysig, 0, sizeof(query_sig_t));

    pvt->sms_ref = ast_random();

    //
    ATP_CLEAR_RECV_BUF(pvt);

    //pvt->func_test_rate = pvt->config.baudrate;

    // stop all timers
    memset(&pvt->timers, 0, sizeof(pvt->timers));

    //don't flush at queue
    rgsm_reset_pvt_state(pvt, 0);

    // May 12, 2017: allow to send AT commands after then a RDY/START received and until power off or reset
    pvt->allow_cli_at = 0;
}

//! Channel thread function
static void *channel_thread_func(void *data)
{
    struct gsm_pvt *pvt = (struct gsm_pvt *)data;
    ast_fdset rfds;

    struct timeval timeout = {0, 0};
    int rc;
    char chr = 0;
    int running = 1;

    ast_verbose("rgsm: <%s>: started workthread, at_fd=%d\n", pvt->name, pvt->at_fd);

    rgsm_man_event_channel(pvt, "port enabled", 0);

    pvt->man_chstate = MAN_CHSTATE_STARTING;
    rgsm_man_event_channel_state(pvt);

    pvt->flags.adjust_baudrate = 0;
    pvt->sim_try_cnt_curr = pvt->sim_try_cnt_conf;
    //Aug 29,2014
    pvt->reg_try_cnt_curr = pvt->reg_try_cnt_conf;

    rgsm_init_pvt_state(pvt);

    if (pvt->pwr_delay_sec)
        us_sleep(pvt->pwr_delay_sec * 1000000);

    //zero timeout
    rgsm_timer_set(pvt->timers.trysend, timeout);

    //run while modem is powered
    while (running)
    {
        //
        //us_sleep(100);
        if (pvt->at_fd == -1)
        {
            //at channel closed unexpectedly
            //ast_log(AST_LOG_ERROR, "<%s>: at_fd not open, try exit channel thread", pvt->name);
            //break;

            // lock rgsm subsystem and pvt
            ast_mutex_lock(&rgsm_lock);
            ast_mutex_lock(&pvt->lock);

            goto process_signals_;
        }

        gettimeofday(&pvt->curr_tv, &pvt->curr_tz);

        FD_ZERO(&rfds);
        FD_SET(pvt->at_fd, &rfds);

        timeout.tv_sec = 0;
        timeout.tv_usec = 500;

        rc = ast_select(pvt->at_fd + 1, &rfds, NULL, NULL, &timeout);

        if (rc < 0)
        {
            ast_log(AST_LOG_ERROR, "<%s>: at_fd select error=%d(%s)\n", pvt->name, errno, strerror(errno));
            continue;
        }
        else if (rc > 0)
        {
            // read all awaylable data char by char
            while (1)
            {
                //data available, read single char
                rc = read(pvt->at_fd, &chr, 1);
                //
                if (rc < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        break;
                    }
                    ast_log(AST_LOG_ERROR, "<%s>: at_fd read error=%d(%s)\n", pvt->name, errno, strerror(errno));
                }
                else if (rc == 0)
                {
                    break;
                }
                else
                {
                    //process input char
                    if (chr == '\n' || chr == '\r')
                    {
                        if (pvt->recv_len >= 7)
                        {
                            if (strstr(pvt->recv_buf, "+CUSD: ") && pvt->ussd_text_param)
                            {
                                *pvt->recv_ptr++ = chr;
                                *pvt->recv_ptr = '\0';
                                pvt->recv_len++;
                            }
                            else
                            {
                                pvt->recv_buf_valid = (pvt->recv_len > 0) ? 1 : 0;
                                *pvt->recv_ptr = '\0';
                                pvt->recv_ptr = pvt->recv_buf;
                                if (pvt->recv_buf_valid)
                                {
                                    DEBUG_AT("<%s>: AT recv [%s]\n", pvt->name, pvt->recv_ptr);
                                    break;
                                }
                            }
                        }
                        else
                        {
                            pvt->recv_buf_valid = (pvt->recv_len > 0) ? 1 : 0;
                            *pvt->recv_ptr = '\0';
                            pvt->recv_ptr = pvt->recv_buf;
                            if (pvt->recv_buf_valid)
                            {
                                DEBUG_AT("<%s>: AT recv [%s]\n", pvt->name, pvt->recv_ptr);
                                break;
                            }
                        }
                    }
                    else if (chr == '>')
                    {
                        if (pvt->recv_len >= 7)
                        {
                            if (!strstr(pvt->recv_buf, "+CUSD: "))
                                atp_handle_pdu_prompt(pvt); //pdu prompt
                        }
                        else
                            atp_handle_pdu_prompt(pvt); //pdu prompt
                    }
                    else
                    {
                        if (pvt->recv_len >= 7)
                        {
                            if (strstr(pvt->recv_buf, "+CUSD: "))
                            {
                                if (*(pvt->recv_ptr - 1) == '\"')
                                {
                                    if (chr != ',')
                                    {
                                        pvt->ussd_text_param = 1;
                                    }
                                    else
                                    {
                                        pvt->ussd_text_param = (pvt->ussd_text_param == 1) ? 0 : 1;
                                    }
                                }
                            }
                            else
                                pvt->ussd_text_param = 0;
                        }
                        else
                            pvt->ussd_text_param = 0;
                        *pvt->recv_ptr++ = chr;
                        *pvt->recv_ptr = '\0';
                        pvt->recv_len++;
                        if (pvt->recv_len > (sizeof(pvt->recv_buf) - 4))
                        {
                            ast_log(AST_LOG_DEBUG, "<%s>: AT buffer full: recv_len=%d\n", pvt->name, pvt->recv_len);
                            ATP_CLEAR_RECV_BUF(pvt);
                            break;
                        }
                    }
                }
            } //while(1)
        }

        // lock rgsm subsystem and pvt
        ast_mutex_lock(&rgsm_lock);
        ast_mutex_lock(&pvt->lock);

        rc = atp_check_cmd_expired(pvt);

        if (rc == -1)
        {
            //auto-bauding failed, beed to power down a channel
            goto process_mdm_state_;
        }
        else if (rc == -2)
        {
            //May 31, 2013: fix Bz1940 - Recover a channel when gsm module hangup
            //Sept 10, 2014: count erroneous commands as well
            RST_CHANNEL(pvt, 0);
            pvt->flags.hard_reset = 1;
            ast_log(LOG_WARNING, "<%s>: exp_count=%d, err_count=%d: GSM module seems unresponsive, hard reset the channel\n",
                    pvt->name, pvt->exp_cmd_counter, pvt->err_cmd_counter);

            //clear error counters to avoid infinite loop
            pvt->exp_cmd_counter = 0;
            pvt->err_cmd_counter = 0;

            goto process_signals_;
        }

        atp_handle_response(pvt);
        atp_handle_unsolicited_result(pvt);

        // try to send at command
        if (rgsm_atcmd_trysend(pvt) < 0)
        {
            ast_log(LOG_ERROR, "<%s>: can't send AT command\n", pvt->name);
            // set restart flag
            if (!pvt->flags.shutdown_now && !pvt->flags.restart_now)
            {
                RST_CHANNEL(pvt, 0);
            }
        }

        //timers processing
        handle_pvt_timers(pvt);

    process_signals_:
        //handle signals
        handle_pvt_signals(pvt);
    process_mdm_state_:
        //!mdm_sm() must follow after handle_pvt_signals() to correct processing the power_off/restart
        running = mdm_sm(pvt) != -1;

        // unlock rgsm subsystem and pvt
        ast_mutex_unlock(&pvt->lock);
        ast_mutex_unlock(&rgsm_lock);
    }

    // close signalling channel device
    ggw8_close_at(pvt->ggw8_device, pvt->modem_id);
    pvt->at_fd = -1;
    // close voice channel device
    ggw8_close_voice(pvt->ggw8_device, pvt->modem_id);
    pvt->voice_fd = -1;

    //reset channel state variables
    rgsm_reset_pvt_state(pvt, 1);

    rgsm_man_event_channel_state(pvt);
    rgsm_man_event_channel(pvt, "port disabled", 0);

    memset(&pvt->flags, 0, sizeof(pvt_flags_t));

    // mark thread as NULL
    pvt->channel_thread = AST_PTHREADT_NULL;
    // reset channel flags
    //memset(&pvt->flags, 0, sizeof(struct eggsm_channel_flags));

    ast_verbose("rgsm: <%s>: stopped workthread\n", pvt->name);

    return NULL;
}

//! AT command queue impl
int rgsm_atcmd_queue_append(struct gsm_pvt *pvt,
                            int id,
                            u_int32_t oper,
                            int subcmd,
                            int timeout,
                            int show,
                            const char *fmt, ...)
{

    struct rgsm_atcmd *cmd;
    struct rgsm_atcmd *enqueued;
    struct at_command *at;
    char *opstr;

    va_list vargs;

    // check channel
    if (!pvt)
    {
        ast_log(LOG_ERROR, "can't get channel pvt for cmd id=%d\n", id);
        return -1;
    }
    // check command id
    if (id < 0)
    {
        ast_log(LOG_ERROR, "<%s>: invalid cmd id=[%d]\n", pvt->name, id);
        return -1;
    }
    // get at command data
    at = NULL;
    if (pvt->module_type == MODULE_TYPE_UNKNOWN)
        at = get_at_com_by_id(id, basic_at_com_list, AT_BASIC_MAXNUM);
    else if (pvt->module_type == MODULE_TYPE_SIM900)
        at = get_at_com_by_id(id, sim900_at_com_list, AT_SIM900_MAXNUM);
    else if (pvt->module_type == MODULE_TYPE_SIM5320)
        at = get_at_com_by_id(id, sim5320_at_com_list, AT_SIM5320_MAXNUM);
    else if (pvt->module_type == MODULE_TYPE_UC15)
        at = get_at_com_by_id(id, uc15_at_com_list, AT_UC15_MAXNUM);
    /*
	if(pvt->module_type == MODULE_TYPE_SIM300)
		at = get_at_com_by_id(id, sim300_at_com_list, AT_SIM300_MAXNUM);
	else if(pvt->module_type == MODULE_TYPE_SIM900)
		at = get_at_com_by_id(id, sim900_at_com_list, AT_SIM900_MAXNUM);
	else if(pvt->module_type == MODULE_TYPE_M10)
		at = get_at_com_by_id(id, m10_at_com_list, AT_M10_MAXNUM);
*/
    if (!at)
    {
        ast_log(LOG_WARNING, "<%s>: can't find at commmand id=[%d]\n", pvt->name, id);
        return -1;
    }
    // check for is one at command operation
    if (!(opstr = get_at_com_oper_by_id(oper)))
    {
        ast_log(LOG_WARNING, "<%s>: [%0X] is not known at commmand operation\n", pvt->name, oper);
        return -1;
    }
    // chech for operation is available for this AT command
    if (!(oper & at->operations))
    {
        ast_log(LOG_WARNING, "<%s>: operation [%0X] is not available for \"%s\"\n", pvt->name, oper, at->name);
        return -1;
    }
    // creating command container
    if (!(cmd = ast_calloc(1, sizeof(struct rgsm_atcmd))))
        return -1;

    // set command data
    cmd->timeout = (timeout > 0) ? timeout : gen_config.timeout_at_response;
    cmd->sub_cmd = subcmd;
    cmd->id = id;
    cmd->oper = oper;
    cmd->at = at;
    cmd->show = show;
    // build command
    if (fmt)
    {
        cmd->cmd_len = 0;
        cmd->cmd_len += sprintf(cmd->cmd_buf + cmd->cmd_len, "%s%s", cmd->at->name, opstr);
        va_start(vargs, fmt);
        cmd->cmd_len += vsprintf(cmd->cmd_buf + cmd->cmd_len, fmt, vargs);
        va_end(vargs);
        cmd->cmd_len += sprintf(cmd->cmd_buf + cmd->cmd_len, "\r");
    }
    else
    {
        cmd->cmd_len = sprintf(cmd->cmd_buf, "%s%s\r", cmd->at->name, opstr);
    }

    //disallow command duplicates in queue
    AST_LIST_TRAVERSE(&pvt->cmd_queue, enqueued, entry)
    {
        if (!strncmp(cmd->cmd_buf, enqueued->cmd_buf, cmd->cmd_len))
        {
            //Sept 12, 2014: command at the head is allowed to be re-queued
            if (pvt->cmd_queue.first == enqueued)
            {
                continue;
            }

            ast_log(LOG_DEBUG, "<%s>: [%s] already in queue, skip it\n", pvt->name, cmd->cmd_buf);
            return pvt->cmd_queue_length;
        }
    }

    // append to tail of queue
    AST_LIST_INSERT_TAIL(&pvt->cmd_queue, cmd, entry);
    pvt->cmd_queue_length++;

    return pvt->cmd_queue_length;
}

int rgsm_atcmd_trysend(struct gsm_pvt *pvt)
{

    //struct timeval time_data;
    struct timeval exp_timeout;
    int res;

    res = 0;
    if ((pvt->cmd_queue.first) && (pvt->cmd_done) && ((!is_rgsm_timer_enable(pvt->timers.trysend)) || (is_rgsm_timer_fired(pvt->timers.trysend))))
    {
        // check access to queue
        if (write(pvt->at_fd, pvt->cmd_queue.first->cmd_buf, pvt->cmd_queue.first->cmd_len) < 0)
        {
            if (errno != EAGAIN)
            {
                ast_log(LOG_ERROR, "<%s>: write error: %s\n", pvt->name, strerror(errno));
                pvt->cmd_done = 1;
                res = -1;
            }
        }
        else
        {
            //reset timer to disable shortly next AT cmd send
            rgsm_timer_set(pvt->timers.trysend, trysend_timeout);

            pvt->cmd_done = 0;
            if (pvt->cmd_queue.first->id == AT_PSSTK)
            {
                pvt->stk_cmd_done = 0;
                pvt->stk_sync_notification_count = 0;
                *pvt->stk_status = '\0';
            }

            //gettimeofday(&time_data, NULL);
            //
            tv_set(exp_timeout, pvt->cmd_queue.first->timeout, 0);
            rgsm_timer_set(pvt->cmd_queue.first->timer, exp_timeout);
            //
            if (pvt->cmd_queue.first->show)
                ast_verbose("<%s>: AT send [%.*s]\n",
                            pvt->name,
                            pvt->cmd_queue.first->cmd_len - 1,
                            pvt->cmd_queue.first->cmd_buf);

            DEBUG_AT("<%s>: AT send [%.*s]\n",
                     pvt->name,
                     pvt->cmd_queue.first->cmd_len - 1,
                     pvt->cmd_queue.first->cmd_buf);
        }
    } // end od data to send is ready
    return res;
}

int rgsm_atcmd_queue_free(struct gsm_pvt *pvt, struct rgsm_atcmd *cmd)
{
    if (!pvt)
    {
        ast_log(LOG_ERROR, "<pvt->name>: fail \n");
        return -1;
    }

    if (cmd)
    {
        ast_free(cmd);
        pvt->cmd_queue_length--;
    }

    return pvt->cmd_queue_length;
}

int rgsm_atcmd_queue_flush(struct gsm_pvt *pvt)
{
    struct rgsm_atcmd *cmd;

    while ((cmd = AST_LIST_REMOVE_HEAD(&pvt->cmd_queue, entry)))
    {
        rgsm_atcmd_queue_free(pvt, cmd);
    }

    pvt->cmd_done = 1;
    rgsm_timer_stop(pvt->timers.trysend);

    return pvt->cmd_queue_length;
}

//int main(void){}
