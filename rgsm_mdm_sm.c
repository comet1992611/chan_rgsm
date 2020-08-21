#include "rgsm_mdm_sm.h"
#include "at.h"
#include "rgsm_sim900.h"
#include "rgsm_uc15.h"
#include "rgsm_sim5320.h"
#include "rgsm_utilities.h"
#include "rgsm_manager.h"

int mdm_sm(struct gsm_pvt* pvt)
{
    struct ast_tm *tm_ptr, tm_buf;
	char tmpbuf[256];
	char *ip;
	int ilen;
	char *op;
	int olen;
	int restart_req;
	int hard_reset;
	ggw8_baudrate_t br;

    switch (pvt->mdm_state)
    {
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_DISABLE:
            //
            restart_req = pvt->flags.restart_now;
            hard_reset = pvt->flags.hard_reset;

            rgsm_init_pvt_state(pvt);

            if (!restart_req) {
                //channel powerings
                //if fixed rate or auto-bauding requested start with it otherwise start with 115200
                if (pvt->baudrate == BR_UNKNOWN) {
                    pvt->baudrate = BR_115200;
                }
            } else {
                //channel restartings
                //ast_verbose("rgsm: <%s>: sleeping %d seconds before restart\n", pvt->name, pvt->rst_delay_sec);
                if (pvt->rst_delay_sec > 0) {
                    rgsm_usleep(pvt, pvt->rst_delay_sec*1000000);
                    pvt->rst_delay_sec = 0;
                }
            }

            if (pvt->flags.adjust_baudrate) {
                //try fixed rates 115200 or 57600 or 38400 before switch to auto-bauding
                switch (pvt->baudrate) {
                    case BR_115200: pvt->baudrate = BR_57600; break;
                    case BR_57600: pvt->baudrate = BR_38400; break;
                    default: pvt->baudrate = BR_AUTO; break;
                }
            }

            if (pvt->baudrate == BR_UNKNOWN) {
                ast_log(AST_LOG_ERROR, "rgsm: <%s>: ME does not respond at any baud rate\n", pvt->name);
                //this will force exitting a channel thead
                return -1;
            }
            else if ((pvt->baudrate == BR_AUTO) || (pvt->baudrate == BR_AUTO_POWER_ON)) {
                //
                br = pvt->baudrate;
                pvt->baudrate = BR_115200;
                ast_verbose("rgsm: <%s>: try auto-baunding for serial port at 115200\n", pvt->name);
                if (ggw8_baudrate_ctl(pvt->ggw8_device, pvt->modem_id, BR_115200))
                {
                    ast_log(AST_LOG_ERROR, "<%s>: couldn't set serial port rate\n", pvt->name);
                    //this will force exitting a channel thead
                    return -1;
                }

                pvt->mdm_state = MDM_STATE_AUTO_BAUDING;
                // run abaudwait timer to send AT+IPR?
                rgsm_timer_set(pvt->timers.abaudwait, abaudwait_timeout);
                if (br == BR_AUTO_POWER_ON) {
                    //module still not powered
                    //send power control command - enable GSM module
                    ast_verbose("rgsm: <%s>: modem power on\n", pvt->name);
                    if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_POWERON))
                    {
                        ast_log(AST_LOG_ERROR, "<%s>: couldn't supply modem power\n", pvt->name);
                        //this will force exitting a channel thead
                        return -1;
                    }
                }

                //no sense to wait "RDY"
                break;
            }
            else {
                ast_verbose("rgsm: <%s>: probe serial port at %s\n", pvt->name, baudrate_str(pvt->baudrate));
                if (ggw8_baudrate_ctl(pvt->ggw8_device, pvt->modem_id, pvt->baudrate))
                {
                    ast_log(AST_LOG_ERROR, "<%s>: couldn't set serial port rate\n", pvt->name);
                    //this will force exitting a channel thead
                    return -1;
                }
            }

            if (!restart_req) {
                // send power control command - enable GSM module
                ast_verbose("rgsm: <%s>: modem power on - continue channel init\n", pvt->name);
                if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_POWERON))
                {
                    ast_log(AST_LOG_ERROR, "<%s>: couldn't supply modem power\n", pvt->name);
                    //this will force exitting a channel thead
                    return -1;
                }
            } else {
                //May 31, 2013: fix Bz1940 - Recover a channel when gsm module hangup
                if (hard_reset) {
                    ast_verbose("rgsm: <%s>: modem hard reset...\n", pvt->name);
                    ast_verbose("rgsm: <%s>: modem power off\n", pvt->name);
                    //send MODEM_POWEROFF/MODEM_POWERON in case of hard reset insterad of MODEM_RESET
                    if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_POWEROFF))
                    {
                        ast_log(AST_LOG_ERROR, "<%s>: couldn't hard reset modem\n", pvt->name);
                        //this will force exitting a channel thead
                        return -1;
                    }
                    rgsm_usleep(pvt, 100000);
                    ast_verbose("rgsm: <%s>: modem power on - continue channel init\n", pvt->name);
                    if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_POWERON))
                    {
                        ast_log(AST_LOG_ERROR, "<%s>: couldn't hard reset modem\n", pvt->name);
                        //this will force exitting a channel thead
                        return -1;
                    }
                } else {
                    ast_verbose("rgsm: <%s>: modem reset - continue channel init\n", pvt->name);
                    if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_RESET))
                    {
                        ast_log(AST_LOG_ERROR, "<%s>: couldn't reset modem\n", pvt->name);
                        //this will force exitting a channel thead
                        return -1;
                    }
                }
            }

            // run waitrdy timer
            rgsm_timer_set(pvt->timers.waitrdy, waitrdy_timeout);
            //
            pvt->mdm_state = MDM_STATE_WAIT_RDY;
            break;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_RESET:
            if (pvt->man_chstate != MAN_CHSTATE_STARTING) {
                pvt->man_chstate = MAN_CHSTATE_STARTING;
                rgsm_man_event_channel_state(pvt);
            }
            //just transit to DISABLE state
            pvt->mdm_state = MDM_STATE_DISABLE;
            //will not exit channel thread
            return 0;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_AUTO_BAUDING:
            break;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_TEST_FUN:
            // check if test functionality is done
            if (!pvt->func_test_done) break;
            // stop testfun timer
            rgsm_timer_stop(pvt->timers.testfun);
            // stop testfunsend timer
            rgsm_timer_stop(pvt->timers.testfunsend);
#if 0
            pvt->config.baudrate = pvt->func_test_rate;
            ast_verb(3, "<%s>: serial port work at speed = %d baud\n", pvt->name, pvt->config.baudrate);
#endif
            rgsm_atcmd_queue_append(pvt, AT_E, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, "%d", 0);

#if 0
            // set GSM module baud rate
            pvt->config.baudrate = pvt->func_test_rate;
            rgsm_atcmd_queue_append(pvt, AT_UNKNOWN, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, "AT+IPR=%d;&W", pvt->config.baudrate);
#endif
            // disable GSM module functionality
            rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");

            // try to run normal functionality
            pvt->mdm_state = MDM_STATE_SUSPEND;
            // start simpoll timer
            rgsm_timer_set(pvt->timers.simpoll, simpoll_timeout);
            break;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_INIT:
            if(!pvt->cmd_done) break;

            if(pvt->init_settings.cscs){
                rgsm_atcmd_queue_append(pvt, AT_CSCS, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%s", "\"HEX\"");
                pvt->init_settings.cscs = 0;
                break;
            }
            if(pvt->init_settings.clip){
                rgsm_atcmd_queue_append(pvt, AT_CLIP, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 1);
                pvt->init_settings.clip = 0;
                break;
            }
            if(pvt->init_settings.chfa){
                if(pvt->functions.setup_audio_channel != NULL)
			pvt->functions.setup_audio_channel(pvt);
                pvt->init_settings.chfa = 0;
                break;
            }
/* Sept 2, 2014: moved in RDY handler to change IMEI at the first opportunity and don't wait sim card

            if (pvt->init_settings.imei_change) {
                if (pvt->module_type == MODULE_TYPE_SIM900) {
                    if (strlen(pvt->new_imei)) {
                        rgsm_atcmd_queue_append(pvt, AT_SIM900_SIMEI, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0,
                            "\"%s\"",
                            pvt->new_imei);
                        ast_verbose("rgsm: <%s>: set IMEI to \"%s\"\n", pvt->name, pvt->new_imei);
                        //May 21, 2013: fix for Bz1932 - New IMEI is not shown immediately after IMEI change complete
                        //requery IMEI
                        rgsm_atcmd_queue_append(pvt, AT_GSN, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                        pvt->init_settings.imei_cmp = 1;
                    } else {
                        ast_log(LOG_WARNING, "rgsm: <%s>: Rejected IMEI change request: new IMEI code empty\n", pvt->name);
                    }
                } else {
                    ast_log(LOG_WARNING, "rgsm: <%s>: Rejected IMEI change request: feature not implemented for this module\n", pvt->name);
                }
                pvt->init_settings.imei_change = 0;
                //May 21, 2013: fix for Bz1932 - New IMEI is not shown immediately after IMEI change complete
                //clear old imei
                pvt->imei[0] = '\0';
                break;
            }
*/
            if(pvt->init_settings.clvl){
                if(pvt->module_type == MODULE_TYPE_UC15) {
                    ast_verbose("rgsm: <%s>: set spk_gain=%d\n", pvt->name, pvt->spk_gain_conf);
	            rgsm_atcmd_queue_append(pvt, AT_CLVL, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", pvt->spk_gain_conf);
//                    ast_verbose("rgsm: <%s>: set spk_audg=%d\n", pvt->name, pvt->spk_audg_conf);
//                    rgsm_atcmd_queue_append(pvt, AT_SIM900_AUDG, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0,1,%d", pvt->spk_audg_conf);
                    ast_verbose("rgsm: <%s>: set SIDET to 0\n", pvt->name);
                    rgsm_atcmd_queue_append(pvt, AT_UC15_QSIDET, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0", 0);
                }
                if(pvt->module_type == MODULE_TYPE_SIM900) {
                    ast_verbose("rgsm: <%s>: set spk_gain=%d\n", pvt->name, pvt->spk_gain_conf);
	            rgsm_atcmd_queue_append(pvt, AT_CLVL, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", pvt->spk_gain_conf);
                    ast_verbose("rgsm: <%s>: set spk_audg=%d\n", pvt->name, pvt->spk_audg_conf);
                    rgsm_atcmd_queue_append(pvt, AT_SIM900_AUDG, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0,1,%d", pvt->spk_audg_conf);
                    ast_verbose("rgsm: <%s>: set SIDET to 0\n", pvt->name);
                    rgsm_atcmd_queue_append(pvt, AT_SIM900_SIDET, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0,0", 0);
                }
                if(pvt->module_type == MODULE_TYPE_SIM5320) {
                    ast_verbose("rgsm: <%s>: set rx_vol=%d\n", pvt->name, pvt->rx_lvl_conf);
                    rgsm_atcmd_queue_append(pvt, AT_SIM5320_CRXVOL, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", pvt->rx_lvl_conf);
                    ast_verbose("rgsm: <%s>: set rx_gain=%d\n", pvt->name, pvt->rx_gain_conf);
                    rgsm_atcmd_queue_append(pvt, AT_SIM5320_CRXGAIN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", pvt->rx_gain_conf);
                    ast_verbose("rgsm: <%s>: set SIDET to 0\n", pvt->name);
                    rgsm_atcmd_queue_append(pvt, AT_SIM5320_SIDET, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0", 0);
                }
                pvt->init_settings.clvl = 0;
                break;
            }
            if(pvt->init_settings.cmic){
                /*if(pvt->module_type == EGGSM_MODULE_TYPE_SIM300)
                    rgsm_atcmd_queue_append(pvt, AT_SIM300_CMIC, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0,%d", pvt->gainout_conf);
                else*/
                if(pvt->module_type == MODULE_TYPE_UC15)
                {
                    rgsm_atcmd_queue_append(pvt, AT_UC15_CMIC, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0,%d", pvt->mic_gain_conf);
                    ast_verbose("rgsm: <%s>: set mic_gain=%d\n", pvt->name, pvt->mic_gain_conf);
		}
                if(pvt->module_type == MODULE_TYPE_SIM900)
                {
                    rgsm_atcmd_queue_append(pvt, AT_SIM900_CMIC, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0,%d", pvt->mic_gain_conf);
                /*else if(pvt->module_type == EGGSM_MODULE_TYPE_M10)
                    rgsm_atcmd_queue_append(pvt, AT_M10_QMIC, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "1,%d", pvt->gainout_conf);*/
                    ast_verbose("rgsm: <%s>: set mic_gain=%d\n", pvt->name, pvt->mic_gain_conf);
		}
                if(pvt->module_type == MODULE_TYPE_SIM5320) {
                    ast_verbose("rgsm: <%s>: set mic_amp1=%d\n", pvt->name, pvt->mic_amp1_conf);
                    rgsm_atcmd_queue_append(pvt, AT_SIM5320_CMICAMP1, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", pvt->mic_amp1_conf);
                    ast_verbose("rgsm: <%s>: set tx_vol=%d\n", pvt->name, pvt->tx_lvl_conf);
                    rgsm_atcmd_queue_append(pvt, AT_SIM5320_CTXVOL, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", pvt->tx_lvl_conf);
                    ast_verbose("rgsm: <%s>: set tx_gain=%d\n", pvt->name, pvt->tx_gain_conf);
                    rgsm_atcmd_queue_append(pvt, AT_SIM5320_CTXGAIN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", pvt->tx_gain_conf);
                }
                pvt->init_settings.cmic = 0;
                break;
            }
            if(pvt->init_settings.cmgf){
                rgsm_atcmd_queue_append(pvt, AT_CMGF, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
                pvt->init_settings.cmgf = 0;
                break;
            }
            if(pvt->init_settings.cnmi){
                if(pvt->module_type == MODULE_TYPE_SIM900)
                {
                    rgsm_atcmd_queue_append(pvt, AT_CNMI, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d,%d,%d,%d,%d", 2, 2, 0, 1, 0);
		} else
		    if(pvt->module_type == MODULE_TYPE_UC15)
            	    {
                	rgsm_atcmd_queue_append(pvt, AT_CNMI, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d,%d,%d,%d,%d", 1, 2, 0, 1, 0);
		    } else 
                	if(pvt->module_type == MODULE_TYPE_SIM5320)
	        	{
    	        	    rgsm_atcmd_queue_append(pvt, AT_CNMI, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d,%d,%d,%d", 2, 2, 0, 0);
			}
                pvt->init_settings.cnmi = 0;
                break;
            }
            if(pvt->init_settings.cmee){
                rgsm_atcmd_queue_append(pvt, AT_CMEE, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 1);
                pvt->init_settings.cmee = 0;
                break;
            }
            if(pvt->init_settings.cclk){
                if((tm_ptr = ast_localtime(&pvt->curr_tv, &tm_buf, NULL)))
                    rgsm_atcmd_queue_append(pvt, AT_CCLK, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0,
                        "\"%02d/%02d/%02d,%02d:%02d:%02d%c%02d\"",
                            tm_ptr->tm_year%100,
                            tm_ptr->tm_mon+1,
                            tm_ptr->tm_mday,
                            tm_ptr->tm_hour,
                            tm_ptr->tm_min,
                            tm_ptr->tm_sec,
                            (pvt->curr_tz.tz_minuteswest > 0)?('-'):('+'),
                            abs(pvt->curr_tz.tz_minuteswest)/15);
                else
                    ast_log(LOG_WARNING, "<%s>: can't set module clock\n", pvt->name);
                pvt->init_settings.cclk = 0;
                break;
            }
            pvt->flags.init = 0;
/*! May 22, 2013: runhalfsecond retired
            // start runhalfsecond timer
            rgsm_timer_set(pvt->timers.runhalfsecond, runhalfsecond_timeout);
*/
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
            pvt->mdm_state = MDM_STATE_RUN;
            break;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_RUN:
            // get subscriber number
            if(((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)))
            {
                if (pvt->flags.subscriber_number_req) {
                    rgsm_atcmd_queue_append(pvt, AT_CNUM, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                    pvt->flags.subscriber_number_req = 0;
                }
            }

            //May 21, 2014
            //disable sim toolkit notifications as it was configured in chan_rgsm.conf
            if (pvt->flags.stk_capabilities_req && !pvt->chnl_config.sim_toolkit) {

                pvt->flags.stk_capabilities_req = 0;
                pvt->stk_capabilities = 0;

                //pvt->stk_capabilities equals the number of menu item
                ast_verbose("rgsm: <%s>: Disable SIM Toolkit capabilities\n", pvt->name);
                rgsm_atcmd_queue_append(pvt, AT_PSSTKI, AT_OPER_WRITE, 0, 30, 0, "0", pvt->chnl_config.sim_toolkit);
            }

            // manage call wait status option
            if(pvt->reg_state == REG_STATE_REG_HOME_NET){
                // check callwait status
                if((pvt->callwait_status == CALLWAIT_STATUS_UNKNOWN) && (CALL_STATE_NULL == pvt->call_state)){
                    // request actual call wait status if not known
                    pvt->callwait_status = CALLWAIT_STATUS_QUERY;
                    pvt->call_state = CALL_STATE_DISABLE;
                    rgsm_atcmd_queue_append(pvt, AT_CCWA, AT_OPER_WRITE, 0, 30, 0, "%d,%d", 0, 2);
                }
                else if((pvt->callwait_status != CALLWAIT_STATUS_QUERY) && (CALL_STATE_NULL == pvt->call_state)){
                    if(pvt->callwait_status != pvt->callwait_conf){
                        // set call wait status
                        pvt->callwait_status = CALLWAIT_STATUS_QUERY;
                        pvt->call_state = CALL_STATE_DISABLE;
                        rgsm_atcmd_queue_append(pvt, AT_CCWA, AT_OPER_WRITE, 0, 30, 0, "%d,%d", 0, pvt->callwait_conf);
                    }
                }
                // check hidenum state
                if((pvt->hidenum_set == HIDENUM_UNKNOWN) && (CALL_STATE_NULL == pvt->call_state)){
                    // request actual call wait status if not known
                    pvt->hidenum_set = HIDENUM_QUERY;
                    pvt->call_state = CALL_STATE_DISABLE;
                    rgsm_atcmd_queue_append(pvt, AT_CLIR, AT_OPER_READ, 0, 30, 0, NULL);
                }
                else if((pvt->hidenum_set != HIDENUM_QUERY) && (CALL_STATE_NULL == pvt->call_state)){
                    if(pvt->hidenum_set != pvt->hidenum_conf){
                        // set line presentation restriction mode
                        pvt->hidenum_set = HIDENUM_QUERY;
                        pvt->call_state = CALL_STATE_DISABLE;
                        rgsm_atcmd_queue_append(pvt, AT_CLIR, AT_OPER_WRITE, 0, 30, 0, "%d", pvt->hidenum_conf);
                    }
                }
            }
#if 1
            // get balance
            if((pvt->reg_state == REG_STATE_REG_HOME_NET) &&
                (pvt->call_state == CALL_STATE_NULL) &&
                (pvt->flags.balance_req) && (strlen(pvt->balance_req_str))){
                //
                memset(tmpbuf, 0, sizeof(tmpbuf));
                ip = pvt->balance_req_str;
                ilen = strlen(pvt->balance_req_str);
                op = tmpbuf;
                olen = 256;
                if(!str_bin_to_hex(&ip, &ilen, &op, &olen)){
                    //
                    pvt->call_state = CALL_STATE_ONSERVICE;
                    //
                    if(pvt->functions.send_ussd == NULL)rgsm_atcmd_queue_append(pvt, AT_CUSD, AT_OPER_WRITE, SUBCMD_CUSD_GET_BALANCE, 30, 0, "%d,\"%s\"", 1, ip);
                      else pvt->functions.send_ussd(pvt, SUBCMD_CUSD_GET_BALANCE, (char*)pvt->balance_req_str);
                }
                else
                    ast_log(LOG_ERROR, "<%s>: fail convert USSD \"%s\" to hex\n", pvt->name, pvt->balance_req_str);
                pvt->flags.balance_req = 0;
            }
#endif
            break;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_SUSPEND:
            break;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case MDM_STATE_PWR_DOWN:
            // send power control command - disable GSM module
            ast_verbose("rgsm: <%s>: channel power supply turn off\n", pvt->name);
            if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_POWEROFF))
            {
                ast_log(AST_LOG_ERROR, "<%s>: couldn't turn off channel power\n", pvt->name);
            }
            pvt->mdm_state = MDM_STATE_DISABLE;
            //this will force exitting a channel thead if restart not requested
            return -1;
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        default:
            break;
    }
    return 0;
}
