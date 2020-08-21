#include <stdint.h>
#include "asterisk/md5.h"
#include "rgsm_defs.h"
#include "chan_rgsm.h"
#include "rgsm_utilities.h"
#include "rgsm_manager.h"
#include "rgsm_dao.h"

//!forwards for manager action handlers
static int _show_channels_impl(struct mansession* s, const struct message* m);
static int _send_ussd_impl(struct mansession* s, const struct message* m);
static int _send_sms_impl(struct mansession* s, const struct message* m);
static int _send_stk_response_impl(struct mansession* s, const struct message* m);

static char * espace_newlines(const char * text);

static const struct rgsm_man_action {
    const char *name;
    int authority;
    int (*handler)(struct mansession *s, const struct message *m);
    const char *brief;
    const char *desc;
} rgsm_man_actions[] = {
    {
		.name = "RGSMShowChannels",
		.authority = EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING,
		.handler = _show_channels_impl,
		.brief = "List RGSM channels",
		.desc = "Description: List RGSM channels (slots) info in text format with details on current status.\n\n"
                "RGSMShowDevicesComplete.\n"
                "Variables:\n"
                "   ActionID: <id>	  Action ID for this transaction. Will be returned.\n"
                "   Device: <name>    Optional name of device (slot).\n"
    },
    {
		.name = "RGSMSendUSSD",
		.authority = EVENT_FLAG_CALL,
		.handler = _send_ussd_impl,
		.brief = "Send a ussd message to a RGSM device.",
		.desc = "Description: Send a ussd message to a RGSM device.\n\n"
                "Variables: (Names marked with * are required)\n"
                "	ActionID:  <id>		Action ID for this transaction. Will be returned.\n"
                "	*Device: <device>	The RGSM device (slot) to which the ussd code will be send.\n"
                "	*USSD:   <code>	    The ussd code that will be send to the device.\n"
    },
    {
		.name = "RGSMSendSMS",
		.authority = EVENT_FLAG_CALL,
		.handler = _send_sms_impl,
		.brief = "Send a SMS message.",
		.desc = "Description: Send a sms message from a RGSM device.\n\n"
                "Variables: (Names marked with * are required)\n"
                "	ActionID:   <id>	Action ID for this transaction. Will be returned.\n"
                "	*Device:  <device>	The RGSM device (slot) to which the SMS be send.\n"
                "	*Number:  <number>	The phone number to which the sms will be send.\n"
                "	*Message: <message>	The SMS message that will be send.\n"
    },
    {
		.name = "RGSMSendSTKResponse",
		.authority = EVENT_FLAG_CALL,
		.handler = _send_stk_response_impl,
		.brief = "Send SIM Toolkit response to SIM card.",
		.desc = "Description: Send SIM Toolkit response to SIM card.\n\n"
                "Variables: (Names marked with * are required)\n"
                "	ActionID:   <id>   Action ID for this transaction. Will be returned.\n"
                "	*Device:  <device> The RGSM device (slot) to which the SMS be send.\n"
                "	*Type:    <type>   The response type without quotes to take an action.\n"
                "	*Params:  <params> The response's comma delimited list of parameters.\n"
    },
};

//! global functions
void rgsm_man_register()
{
    int i;
    for (i = 0; i < ITEMS_OF(rgsm_man_actions); i++) {
        ast_manager_register2(rgsm_man_actions[i].name,
                              rgsm_man_actions[i].authority,
                              rgsm_man_actions[i].handler,
                              NULL, // this is written by Roman
                              rgsm_man_actions[i].brief,
                              rgsm_man_actions[i].desc);
    }
}

void rgsm_man_unregister()
{
    int i;
    for (i = ITEMS_OF(rgsm_man_actions)-1; i >= 0; i--) {
        ast_manager_unregister((char*)rgsm_man_actions[i].name);
    }
}

void rgsm_man_event_message_raw(const char * event, const char * devname, const char * message)
{
    manager_event (EVENT_FLAG_CALL, event,
        "Device: %s\r\n"
        "Message: %s\r\n",
        devname,
        message
    );
}

void rgsm_man_event_message(const char * event, const char * devname, const char * message)
{
    char * escaped = espace_newlines(message);
    if(escaped) {
        rgsm_man_event_message_raw(event, devname, (const char *)escaped);
        ast_free(escaped);
    }
}

void rgsm_man_event_imei_change_complete(const char * devname, int error, const char* message)
{
    manager_event (EVENT_FLAG_CALL, "RGSMImeiChangeComplete",
        "Device: %s\r\n"
        "Status: %s\r\n"
        "Message: %s\r\n",
        devname,
        error ? "ERROR" : "OK",
        message ? message : ""
    );
}

void rgsm_man_event_channel_state(struct gsm_pvt *pvt)
{
    manager_event (EVENT_FLAG_CALL, "RGSMChannelState",
        "Device: %s\r\n"
        "Status: %d\r\n"
        "Message: %s\r\n",
        pvt->name,
        pvt->man_chstate,
        man_chstate_str( pvt->man_chstate)
    );
}

void rgsm_man_event_channel(struct gsm_pvt *pvt, const char* msg, int iccid_aware)
{
    char iccid_field[100];
    if (iccid_aware) {
        sprintf(iccid_field, "ICCID: %s\r\n", pvt->iccid);
    } else {
        iccid_field[0] = 0;
    }
    manager_event (EVENT_FLAG_CALL, "GSMChannelState", "Device: %s\r\nMessage: %s\r\n%s", pvt->name, msg, iccid_field);
}


void rgsm_man_event_new_ussd(const char * devname, char* message)
{
    struct ast_str*    buf;
    char*        s = message;
    char*        sl;
    size_t        linecount = 0;

    buf = ast_str_create (256);

    while ((sl = strsep (&s, "\r\n")))
    {
        if (*sl != '\0')
        {
            ast_str_append (&buf, 0, "MessageLine%zu: %s\r\n", linecount, sl);
            linecount++;
        }
    }

    manager_event (EVENT_FLAG_CALL, "RGSMNewUSSD",
        "Device: %s\r\n"
        "LineCount: %zu\r\n"
/* FIXME: empty lines inserted */
//        "%s\r\n",
        "%s",
        devname, linecount, ast_str_buffer (buf)
    );

    ast_free (buf);
}

// TODO: use espace_newlines() and join with manager_event_new_sms_base64()
void rgsm_man_event_new_sms (const char * devname, char* number, char* message)
{
    struct ast_str* buf;
    size_t    linecount = 0;
    char*    s = message;
    char*    sl;

    buf = ast_str_create (256);

    while ((sl = strsep (&s, "\r\n")))
    {
        if (*sl != '\0')
        {
            ast_str_append (&buf, 0, "MessageLine%zu: %s\r\n", linecount, sl);
            linecount++;
        }
    }

    manager_event (EVENT_FLAG_CALL, "RGSMNewSMS",
        "Device: %s\r\n"
        "From: %s\r\n"
        "LineCount: %zu\r\n"
        "%s\r\n",
        devname, number, linecount, ast_str_buffer (buf)
    );
    ast_free (buf);
}

/*!
 * \brief Send a RGSMNewSMSBase64 event to the manager
 * \param pvt a pvt structure
 * \param number a null terminated buffer containing the from number
 * \param message_base64 a null terminated buffer containing the base64 encoded message
 */
/*
void rgsm_man_event_new_sms_base64 (const char * devname, char * number, char * message_base64)
{
    manager_event (EVENT_FLAG_CALL, "RGSMNewSMSBase64",
        "Device: %s\r\n"
        "From: %s\r\n"
        "Message: %s\r\n",
        devname, number, message_base64
    );
}
*/

//! static functions
static char * espace_newlines(const char * text)
{
    char * escaped;
    int i, j;
    for(j = i = 0; text[i]; ++i, ++j) {
        if(text[i] == '\r' || text[i] == '\n')
            j++;
    }
    escaped = ast_malloc(j + 1);
    if(escaped) {
        for(j = i = 0; text[i]; ++i) {
            if(text[i] == '\r') {
                escaped[j++] = '\\';
                escaped[j++] = 'r';
            }
            else if(text[i] == '\n') {
                escaped[j++] = '\\';
                escaped[j++] = 'n';
            } else {
                escaped[j++] = text[i];
            }
        }
        escaped[j] = 0;
    }

    return escaped;
}

//!manager action's handlers
static int _show_channels_impl (struct mansession* s, const struct message* m)
{
	const char*	id = astman_get_header (m, "ActionID");
	struct      gsm_pvt *pvt;
	size_t		count = 0;
	struct      gateway* gw;
	int         i;
	char        buf[256];
    //ggw8_info_t *ggw8_info;
    //uint16_t    sys_id;

	astman_send_listack (s, m, "Channels status list will follow", "start");
    ast_mutex_lock(&rgsm_lock);
	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
        //ggw8_info = ggw8_get_device_info(gw->ggw8_device);
        //sys_id = ggw8_get_device_sysid(gw->ggw8_device);

        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            pvt = gw->gsm_pvts[i];
            if (pvt) {
                ast_mutex_lock (&pvt->lock);
                astman_append (s, "Event: RGSMChannelEntry\r\n");
				if(!ast_strlen_zero (id)) {
					astman_append (s, "ActionID: %s\r\n", id);
				}

                astman_append (s, "Name: %s\r\n", pvt->name);
                astman_append (s, "Power: %s\r\n", pvt->power_man_disable ? "N/A" : onoff_str(pvt->flags.enable));
                astman_append (s, "MdmState: %s\r\n", mdm_state_str(pvt->mdm_state));
                astman_append (s, "GsmModule: %s\r\n", pvt->model);
                //astman_append (s, "Manufacturer: %s\r\n", pvt->manufacturer);
                astman_append (s, "Firmware: %s\r\n", pvt->firmware);
                astman_append (s, "IMEI: %s\r\n", pvt->imei);
                astman_append (s, "RSSI: %s\r\n", rssi_print(buf, pvt->rssi));
                astman_append (s, "BER: %s\r\n", ber_print(pvt->ber));

                astman_append (s, "SimStatus: %s\r\n", (gw->gsm_pvts[i]->flags.sim_present)?("inserted"):(""));
                astman_append (s, "IMSI: %s\r\n", pvt->imsi);
                astman_append (s, "ICCID: %s\r\n", pvt->iccid);
                astman_append (s, "Operator: %s (%s)\r\n", pvt->operator_name, pvt->operator_code);
                astman_append (s, "RegStatus: %s\r\n", reg_state_print(pvt->reg_state));
                astman_append (s, "SubscriberNumber: %s\r\n", pvt->subscriber_number.value);

                astman_append (s, "Context: %s\r\n", pvt->chnl_config.context);
                astman_append (s, "Incoming: %s\r\n", incoming_type_str(pvt->chnl_config.incoming_type));
                if(pvt->chnl_config.incoming_type == INC_TYPE_SPEC) {
                    astman_append (s, "Incomingto: %s\r\n", pvt->chnl_config.incomingto);
                }
                astman_append (s, "Outgoing: %s\r\n", outgoing_type_str(pvt->chnl_config.outgoing_type));
                astman_append (s, "SimToolkit: %s,%d\r\n",
                               pvt->chnl_config.sim_toolkit ? "enable" : "disable",
                               pvt->stk_capabilities);

                //statistics
                astman_append (s, "LastIncomingCallTime: %s (%ld sec)\r\n",
                               second_to_dhms(buf, pvt->last_time_incoming), pvt->last_time_incoming);
                astman_append (s, "LastOutgoingCallTime: %s (%ld sec)\r\n",
                               second_to_dhms(buf, pvt->last_time_outgoing), pvt->last_time_outgoing);
                astman_append (s, "TotalIncomingCallTime: %s (%ld sec)\r\n",
                               second_to_dhms(buf, pvt->call_time_incoming), pvt->call_time_incoming);
                astman_append (s, "TotalOutgoingCallTime: %s (%ld sec)\r\n",
                               second_to_dhms(buf, pvt->call_time_outgoing), pvt->call_time_outgoing);
                astman_append (s, "TotalCallTime: %s (%ld sec)\r\n",
                               second_to_dhms(buf, (pvt->call_time_incoming + pvt->call_time_outgoing)),
                               (pvt->call_time_incoming + pvt->call_time_outgoing));
                astman_append (s, "\r\n");

                ast_mutex_unlock (&pvt->lock);
                count++;
            }
        }
	}
    ast_mutex_unlock(&rgsm_lock);

	astman_append (s, "Event: RGSMShowChannelsComplete\r\n");

	if(!ast_strlen_zero (id)) {
		astman_append (s, "ActionID: %s\r\n", id);
	}

	astman_append (s,
		"EventList: Complete\r\n"
		"ListItems: %d\r\n"
		"\r\n",
		count
	);

	return 0;
}

static int _send_ussd_impl (struct mansession* s, const struct message* m)
{
    const char* device    = astman_get_header (m, "Device");
    const char* ussd    = astman_get_header (m, "USSD");

    char buf[256];
    const char* msg;
    int is_ussd_send;
    struct gsm_pvt *pvt;


    if (ast_strlen_zero (device))
    {
        astman_send_error (s, m, "Device not specified");
        return 0;
    }

    if (ast_strlen_zero (ussd))
    {
        astman_send_error (s, m, "USSD not specified");
        return 0;
    }

	//
	is_ussd_send = 0;
	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);
	// get channel by name
	pvt = find_ch_by_name(device);
	if (pvt) {
        ast_mutex_lock(&pvt->lock);
        msg = send_ussd(pvt, ussd, &is_ussd_send, SUBCMD_CUSD_MANAGER);
        ast_mutex_unlock(&pvt->lock);
	} else {
        msg = "channel not found";
	}
	ast_mutex_unlock(&rgsm_lock);

    snprintf(buf, sizeof (buf), "<%s> %s\r\n", device, msg);

    if(is_ussd_send)
    {
        astman_send_ack(s, m, buf);
    }
    else
    {
        astman_send_error(s, m, buf);
    }

    return 0;
}

static int _send_sms_impl (struct mansession* s, const struct message* m)
{
    const char*    device    = astman_get_header (m, "Device");
    const char*    number    = astman_get_header (m, "Number");
    const char*    message   = astman_get_header (m, "Message");

    char buf[256];
    int msgid;

	char *str;
	int res;
	int row;

	struct timeval tv;

	char hsec[24];
	char husec[8];
	char hash[32];
	struct MD5Context Md5Ctx;
	unsigned char hashbin[16];

	struct gsm_pvt *pvt;


    if (ast_strlen_zero (device))
    {
        astman_send_error (s, m, "Device not specified");
        return 0;
    }

	ast_mutex_lock(&rgsm_lock);
	pvt = find_ch_by_name(device);
    ast_mutex_unlock(&rgsm_lock);

    if (!pvt) {
        astman_send_error (s, m, "Device not found");
        return 0;
    }

    if (ast_strlen_zero (number))
    {
        astman_send_error (s, m, "Number not specified");
        return 0;
    }

    if (ast_strlen_zero (message))
    {
        astman_send_error (s, m, "Message not specified");
        return 0;
    }

 	// get current time
	gettimeofday(&tv, NULL);
	sprintf(hsec, "%ld", tv.tv_sec);
	sprintf(husec, "%ld", tv.tv_usec);

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, (unsigned char *)device, strlen(device));
	MD5Update(&Md5Ctx, (unsigned char *)number, strlen(number));
	MD5Update(&Md5Ctx, (unsigned char *)message, strlen(message));
	MD5Update(&Md5Ctx, (unsigned char *)hsec, strlen(hsec));
	MD5Update(&Md5Ctx, (unsigned char *)husec, strlen(husec));
	MD5Final(hashbin, &Md5Ctx);

	res = 0;
	for(row=0; row<16; row++)
		res += sprintf(hash+res, "%02x", (unsigned char)hashbin[row]);

	// add new message to outbox
	str = sqlite3_mprintf("INSERT INTO '%q-outbox' ("
								"destination, " // TEXT
								"content, " // TEXT
								"flash, " // INTEGER
								"enqueued, " // INTEGER
								"hash" // VARCHAR(32) UNIQUE
								") VALUES ("
								"'%q', " // destination TEXT
								"'%q', " // content TEXT
								"%d, " // flash INTEGER
								"%ld, " // enqueued INTEGER
								"'%q');", // hash  VARCHAR(32) UNIQUE
								pvt->chname,
								number, // destination TEXT
								message, // content TEXT
								0, // flash INTEGER
								tv.tv_sec, // enqueued INTEGER
								hash); // hash  VARCHAR(32) UNIQUE

	dao_exec_stmt(str, 1, NULL);

	str = sqlite3_mprintf("SELECT msgno FROM '%q-outbox' WHERE hash='%q'", pvt->chname, hash);
	dao_query_int(str, 1, NULL, &msgid);

    snprintf (buf, sizeof (buf), "[%s] SMS sent to outbox\r\nID: %d\nTO: %s\nMSG: %s", device, msgid, number, message);
    astman_send_ack (s, m, buf);

    return 0;
}

#define STK_NOTIFY_MSGS_BUFSIZE 2048

//called by at_processor for each *PSSTK unsolicited response
extern void rgsm_man_event_stk_notify(struct gsm_pvt *pvt, const char *notification)
{
    struct ast_str* buf;
    size_t linecount = 0;
    char* s = (char*)notification;
    char* sl;

    if (pvt->stk_mansession) {
        //synchronous notification => add it to response
        astman_append (pvt->stk_mansession, "Line-%zu: %s\r\n",
                       pvt->stk_sync_notification_count++,
                        notification);
    } else {
        //async notification => send event
        //we dont't know apriori the size of line
        buf = ast_str_create (STK_NOTIFY_MSGS_BUFSIZE);

        while ((sl = strsep (&s, "\r\n")))
        {
            if (*sl != '\0')
            {
                ast_str_append (&buf, 0, "Line-%zu: %s\r\n", linecount, sl);
                linecount++;
            }
        }

		if(!ast_strlen_zero(pvt->stk_action_id)) {
			ast_str_append (&buf, 0, "ActionID: %s\r\n", pvt->stk_action_id);
		}
        
		manager_event (EVENT_FLAG_CALL, "RGSMNotifySTK",
            "Device: %s\r\n"
            "LineCount: %zu\r\n"
            "%s\r\n",
            pvt->name, linecount, ast_str_buffer (buf)
        );

        ast_free (buf);
    }
}

static int _send_stk_response_impl(struct mansession* s, const struct message* m)
{
    const char*	id = astman_get_header (m, "ActionID");
	const char* device = astman_get_header (m, "Device");
    const char* resp_type = astman_get_header (m, "Type");
    const char* resp_params = astman_get_header (m, "Params");
    int queued, timeout, session_set;
    const char *status_msg;

	struct gsm_pvt *pvt;

    if (ast_strlen_zero (device))
    {
        astman_send_error (s, m, "Device not specified");
        return 0;
    }

	ast_mutex_lock(&rgsm_lock);
	pvt = find_ch_by_name(device);

    if (!pvt) {
        astman_send_error (s, m, "Device not found");
        ast_mutex_unlock(&rgsm_lock);
        return 0;
    }

    if (ast_strlen_zero (resp_type))
    {
        astman_send_error (s, m, "Response type not specified");
        ast_mutex_unlock(&rgsm_lock);
        return 0;
    }

    if (ast_strlen_zero (resp_params))
    {
        astman_send_error (s, m, "Response parameters not specified");
        ast_mutex_unlock(&rgsm_lock);
        return 0;
    }

    ast_mutex_lock(&pvt->lock);

    session_set = 0;
    timeout = 30*100;   //30*100*10ms = 30sec
    while (timeout-- > 0) {
        //
        if(pvt->stk_mansession == NULL){
            pvt->stk_mansession = s;
            pvt->stk_sync_notification_count = 0;
            pvt->stk_cmd_done = 0;
            *pvt->stk_status = '\0';
			if(!ast_strlen_zero(id)) {
				//will get used for async events
				strcpy(pvt->stk_action_id, id);
			} else {
				*pvt->stk_action_id = '\0'; 
			}

            session_set = 1;
            break;
        }
        rgsm_usleep(pvt, 10000);
    }

    if (!session_set) {
        astman_send_error(s, m, "Outstanding STK operation in progress");
        goto _cleanup;
    }

    status_msg = send_stk_response_str(pvt, resp_type, resp_params, &queued, 30);

    if (!queued) {
        astman_send_error(s, m, (char*)status_msg);
        goto _cleanup;
    }

    //write header
	astman_append (s, "Response: Success\r\n");

	if(!ast_strlen_zero(id)) {
		astman_append (s, "ActionID: %s\r\n", id);
	}

    astman_append (s,
        "Device: %s\r\n"
        "ResponseType: %s\r\n",
        device, resp_type);

    //wait until command complete,
    //syncronouse messages will be appended to man session in rgsm_man_event_stk_notify()
    timeout = 30*200;   //30*200*5ms = 30sec
    while (timeout-- > 0) {
        rgsm_usleep(pvt, 5000);
        //
        if(pvt->stk_cmd_done){
            break;
        }
    }

    //write footer
    astman_append (s,
        "LineCount: %d\r\n"
        "Status: %s\r\n"
        "\r\n",
        pvt->stk_sync_notification_count,
        (pvt->stk_cmd_done ? pvt->stk_status : "Timed out"));

_cleanup:
    if (session_set) {
        pvt->stk_mansession = NULL;
    }
    ast_mutex_unlock(&pvt->lock);
    ast_mutex_unlock(&rgsm_lock);

    return 0;
}

