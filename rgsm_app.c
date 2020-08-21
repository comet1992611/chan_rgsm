#include "chan_rgsm.h"
#include "rgsm_defs.h"
#include "rgsm_utilities.h"

char* app_status			= "RGSMStatus";
char* app_status_synopsis	= "RGSMStatus(Device,Variable)";
char* app_status_desc		= "RGSMStatus(Device,Variable)\n"
                              "  Device - Id of device\n"
                              "  Variable - Variable to store status in will be 1-3.\n"
                              "             In order, Disconnected, Connected & Free, Connected & Busy.\n";

char* app_send_sms		    = "RGSMSendSMS";
char* app_send_sms_synopsis	= "RGSMSendSMS(Device,Dest,Message)";
char* app_send_sms_desc		= "RGSMSendSms(Device,Dest,Message)\n"
                              "  Device - Id of device\n"
                              "  Dest - destination\n"
                              "  Message - text of the message\n";


#if ASTERISK_VERSION_NUM < 10800
int app_status_exec(struct ast_channel *channel, void *data)
#else
int app_status_exec(struct ast_channel *channel, const char *data)
#endif
{
	struct gsm_pvt*	pvt;
	char* parse;
	int	stat;
	char status[2];

	AST_DECLARE_APP_ARGS (args,
		AST_APP_ARG (device);
		AST_APP_ARG (variable);
	);

	if (ast_strlen_zero (data))
	{
		return -1;
	}

	parse = ast_strdupa (data);

	AST_STANDARD_APP_ARGS (args, parse);

	if (ast_strlen_zero (args.device) || ast_strlen_zero (args.variable))
	{
		return -1;
	}

	stat = 1;

	pvt = find_ch_by_name(args.device);
	if (pvt) {
        ast_mutex_lock(&pvt->lock);
        if (pvt->flags.enable)
        {
            stat = 2;
        }
        if (pvt->owner)
        {
            stat = 3;
        }
        ast_mutex_unlock (&pvt->lock);
	}

	snprintf (status, sizeof (status), "%d", stat);
	pbx_builtin_setvar_helper (channel, args.variable, status);

	return 0;
}

#if ASTERISK_VERSION_NUM < 10800
int app_send_sms_exec(struct ast_channel *channel, void *data)
#else
int app_send_sms_exec(struct ast_channel *channel, const char *data)
#endif
{
	struct gsm_pvt*	pvt;
	char*	parse;
	char*	msg;

	AST_DECLARE_APP_ARGS (args,
		AST_APP_ARG (device);
		AST_APP_ARG (number);
		AST_APP_ARG (message);
	);

	if (ast_strlen_zero (data))
	{
		return -1;
	}

	parse = ast_strdupa (data);

	AST_STANDARD_APP_ARGS (args, parse);

	if (ast_strlen_zero (args.device))
	{
		ast_log (AST_LOG_ERROR, "NULL device for message -- SMS will not be sent\n");
		return -1;
	}

	if (ast_strlen_zero (args.number))
	{
		ast_log (AST_LOG_ERROR, "NULL destination for message -- SMS will not be sent\n");
		return -1;
	}

	if (ast_strlen_zero (args.message))
	{
		ast_log(AST_LOG_ERROR,"NULL Message to be sent -- SMS will not be sent\n");
		return -1;
	}

	pvt = find_ch_by_name(args.device);
	if (pvt)
	{
		ast_mutex_lock (&pvt->lock);
		if (pvt->flags.enable)
		{
            msg = ast_strdup (args.message);
            //if (dc_send_cmgs (pvt, args.number) || msg_queue_push_data (pvt, AT_SMS_PROMPT, AT_CMGS, msg))
            {
                ast_mutex_unlock (&pvt->lock);
                ast_free (msg);
                ast_log (AST_LOG_ERROR, "[%s] Error sending SMS message\n", pvt->name);

                return -1;
            }
		}
		else
		{
			ast_log (AST_LOG_ERROR, "<%s>  wasn't connected / initialized -- SMS will not be sent\n", args.device);
		}
		ast_mutex_unlock (&pvt->lock);

	}
	else
	{
		ast_log (AST_LOG_ERROR, "<%s> wasn't found -- SMS will not be sent\n", args.device);
		return -1;
	}

	return 0;
}
