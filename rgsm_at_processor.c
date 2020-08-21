#include "asterisk.h"
#include <asterisk/channel.h>
#include <asterisk/causes.h>
#include <asterisk/pbx.h>
#include <asterisk/indications.h>
#include "rgsm_at_processor.h"
#include "at.h"
#include "rgsm_sms.h"
#include "rgsm_call_sm.h"
#include "rgsm_utilities.h"
#include "rgsm_sim900.h"
#include "rgsm_sim5320.h"
#include "rgsm_uc15.h"
#include "rgsm_manager.h"

//! start of static staff
static const char* cw_arrived_tone = "425/200,0/200,425/200,0/4400";
static const char* cw_gone_tone = "480+620/500,0/500";
static const char* cw_accepted_tone = "440+480/2000,0/4000";

static const char preambule[] = {0x49, 0x49, 0x49, 0x49, 0xfe, 0xfe, 0xfe, 0xfe, 0};

static struct ast_channel *fake_owner;

//!forwards
static void ussd_handler(struct gsm_pvt *pvt);



//! implementation
void atp_handle_pdu_prompt(struct gsm_pvt* pvt)
{
    ast_debug(2, "<%s>: AT SEND PDU - [%s]\n", pvt->name, pvt->now_send_pdu_buf);
    // Send SMS PDU + ^Z
    if (write(pvt->at_fd, pvt->now_send_pdu_buf, pvt->now_send_pdu_len) < 0
        || write(pvt->at_fd, (char *)&ctrlz, 1) < 0) {
        //log error
        ast_log(LOG_ERROR, "<%s>: write: %s\n", pvt->name, strerror(errno));
    }

    //
    pvt->recv_len = 0;
    pvt->recv_ptr = pvt->recv_buf;
    pvt->recv_buf[0] = '\0';
    pvt->recv_buf_valid = 0;
}

//! check for command timeout expired
int atp_check_cmd_expired(struct gsm_pvt* pvt)
{
    struct rgsm_atcmd *rm_cmd;

    if (pvt->cmd_queue.first) {
        // check command timeout
        if (is_rgsm_timer_enable(pvt->cmd_queue.first->timer)) {
            // time is out
            if (is_rgsm_timer_fired(pvt->cmd_queue.first->timer)) {
                pvt->cmd_done = 1;
                rgsm_timer_stop(pvt->timers.trysend);

                ast_verbose("<%s>: command [%.*s] timeout expired!!!\n",
                        pvt->name, pvt->cmd_queue.first->cmd_len - 1, pvt->cmd_queue.first->cmd_buf);

                DEBUG_AT("<%s>: command [%.*s] timeout expired!!!\n",
                        pvt->name, pvt->cmd_queue.first->cmd_len - 1, pvt->cmd_queue.first->cmd_buf);

                // remove from queue
                rm_cmd = AST_LIST_REMOVE_HEAD(&pvt->cmd_queue, entry);

                if (rm_cmd->id == AT_CUSD) {
                    if(rm_cmd->sub_cmd == SUBCMD_CUSD_GET_BALANCE || rm_cmd->sub_cmd == SUBCMD_CUSD_MANAGER) {
                        //transit state from CALL_STATE_ONSERVICE to CALL_STATE_NULL
                        pvt->call_state = CALL_STATE_NULL;
                    } else { // SUBCMD_CUSD_USER
                        pvt->ussd_done = 1;
                    }
                }

                rgsm_atcmd_queue_free(pvt, rm_cmd);

                if (pvt->mdm_state == MDM_STATE_AUTO_BAUDING) {
                    pvt->mdm_state = MDM_STATE_PWR_DOWN;
                    pvt->flags.adjust_baudrate = 0;
                    ast_verbose("<%s>: Modem does not respond on any baud-rate, channel disabling!!!\n", pvt->name);
                    return -1;
                }
                // init at command buffer
                ATP_CLEAR_RECV_BUF(pvt);

                //May 31, 2013: fix for Bz1940 - Recover a channel when gsm module hangup
                pvt->exp_cmd_counter++;
                if (pvt->exp_cmd_counter >= MAX_EXPIRE_CMD_COUNTER) {
                    return -2;
                }
            } // end of timer fired
        } // end of timer run

        //Sept 10, 2014: count erroneous at commands
        if (pvt->err_cmd_counter >= MAX_ERROR_CMD_COUNTER) {
            return -2;
        }
    }

    return 0;
}

//!module type must be known
static void query_module_data(struct gsm_pvt* pvt)
{
    if(pvt->module_type != MODULE_TYPE_UNKNOWN)
    {
	// get imei
	if (!strlen(pvt->imei)) {
	    rgsm_atcmd_queue_append(pvt, AT_GSN, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
	}
	// get model
	if (!strlen(pvt->model)) {
	    rgsm_atcmd_queue_append(pvt, AT_GMM, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
	}
	// get firmware
	if (!strlen(pvt->firmware)) {
	    rgsm_atcmd_queue_append(pvt, AT_GMR, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
	}
    }
}

static void set_sim_poll(struct gsm_pvt* pvt) {
    if(pvt->module_type != MODULE_TYPE_UNKNOWN)
    {
	if(pvt->functions.set_sim_poll != NULL)
	{
	    pvt->functions.set_sim_poll(pvt);
	}
	else
	{
	    ast_log(LOG_DEBUG, "<%s>: set_sim_poll function not defined for that module type\n", pvt->name);
	}
    }
}

void atp_handle_response(struct gsm_pvt* pvt)
{
    parser_ptrs_t parser_ptrs;
    char *tp;
    struct ast_channel *tmp_ast_pvt;
    int rc;
    char *ip;
	int ilen;
	char *op;
	int olen;
	struct rgsm_atcmd *rm_cmd;


    if (!pvt->recv_buf_valid) return;

    if (!pvt->cmd_queue.first || !is_at_com_response(pvt->cmd_queue.first->at, pvt->recv_buf)) {
        ast_log(AST_LOG_DEBUG, "<%s>: AT UNSOLICITED RESPONSE - [%s]\n", pvt->name, pvt->recv_buf);
        return;
    }

    if (strstr(pvt->recv_buf, "+CME ERROR:")) {
        pvt->ussd_done = 1;
//        return;
    }

    // is response from module for first command in queue
    if(pvt->cmd_queue.first->show) {
        ast_verbose("<%s>: AT recv [%.*s] - [%s] - \n",
                        pvt->name,
                        pvt->cmd_queue.first->cmd_len - 1,
                        pvt->cmd_queue.first->cmd_buf,
                        pvt->recv_buf);
    }

    // test for command done
    if(is_at_com_done(pvt->recv_buf)){
        DEBUG_AT("<%s>: AT recv [%.*s] - [%s] - done\n",
                        pvt->name,
                        pvt->cmd_queue.first->cmd_len - 1,
                        pvt->cmd_queue.first->cmd_buf,
                        pvt->recv_buf);

        if(pvt->cmd_queue.first->show) {
            ast_verbose("done\n");
        }
        pvt->cmd_done = 1;
        //May 31, 2013: fix Bz1940 - Recover a channel when gsm module hangup
        //reset exp_cmd_counter if command run successfully
        pvt->exp_cmd_counter = 0;

        //Sept 10, 2014: count erroneous at commands
        if (strstr(pvt->recv_buf, "ERROR")) {
            pvt->err_cmd_counter++;
        } else {
            pvt->err_cmd_counter = 0;
        }
    }
    else{
        //
        DEBUG_AT("<%s>: AT recv [%.*s] - [%s] - in progress\n",
                pvt->name,
                pvt->cmd_queue.first->cmd_len - 1,
                pvt->cmd_queue.first->cmd_buf,
                pvt->recv_buf);

        if(pvt->cmd_queue.first->show)
            ast_verbose("in progress\n");
    }

    // general AT commands
    // select by operation
    if(pvt->cmd_queue.first->oper == AT_OPER_EXEC) {
        // EXEC operations
        switch(pvt->cmd_queue.first->id) {
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_GMM:
            case AT_CGMM:
                if(strcmp(pvt->recv_buf, "OK") && !strstr(pvt->recv_buf, "ERROR"))
                    ast_copy_string(pvt->model, pvt->recv_buf, sizeof(pvt->model));
                break;
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_GMR:
            case AT_CGMR:
                if(strcmp(pvt->recv_buf, "OK") && !strstr(pvt->recv_buf, "ERROR"))
                    ast_copy_string(pvt->firmware, pvt->recv_buf, sizeof(pvt->firmware));
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_GSN:
            case AT_CGSN:
                if(is_str_digit(pvt->recv_buf)) {
                    ast_copy_string(pvt->imei, pvt->recv_buf, sizeof(pvt->imei));
                    ast_verbose("rgsm: <%s>: IMEI \"%s\"\n", pvt->name, pvt->imei);

                    //Sept 2, 2014: moved from MDM_STATE_INIT handler to change IMEI at the first opportunity and don't wait sim card
                    if (pvt->init_settings.imei_change) {
                        if (pvt->module_type == MODULE_TYPE_SIM900 || pvt->module_type == MODULE_TYPE_SIM5320 || pvt->module_type == MODULE_TYPE_UC15) {
                            if (strlen(pvt->new_imei)) {
                        	if(pvt->functions.change_imei != NULL)
                        	{
                        	    pvt->functions.change_imei(pvt);
                            	    ast_verbose("rgsm: <%s>: change IMEI to \"%s\"\n", pvt->name, pvt->new_imei);
                            	    //May 21, 2013: fix for Bz1932 - New IMEI is not shown immediately after IMEI change complete
                            	    //re-query IMEI
                            	    rgsm_atcmd_queue_append(pvt, AT_GSN, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                            	    pvt->init_settings.imei_cmp = 1;
                            	    //May 21, 2013: fix for Bz1932 - New IMEI is not shown immediately after IMEI change complete
                            	    //clear old imei
                            	    pvt->imei[0] = '\0';
                            	}
                            	else
                            	{
                            	    ast_log(LOG_WARNING, "rgsm: <%s>: Rejected IMEI change request: driver OR device not supported\n", pvt->name);
                            	}

                            } else {
                                ast_log(LOG_WARNING, "rgsm: <%s>: Rejected IMEI change request: new IMEI code empty\n", pvt->name);
                            }
                        } else {
                            ast_log(LOG_WARNING, "rgsm: <%s>: Rejected IMEI change request: feature not implemented for this module\n", pvt->name);
                        }
                        pvt->init_settings.imei_change = 0;
                        break;
                    }

                    if (pvt->init_settings.imei_cmp) {
                        if (strcmp(pvt->imei, pvt->new_imei)) {
                            ast_verbose("rgsm: <%s>: IMEI change FAILED: IMEIs do not match\n", pvt->name);
                            rgsm_man_event_imei_change_complete(pvt->name, 1, "IMEIs do not match");

                        } else {
                            ast_verbose("rgsm: <%s>: IMEI change SUCCEED\n", pvt->name);
                            rgsm_man_event_imei_change_complete(pvt->name, 0, pvt->imei);
                        }
                        pvt->new_imei[0] = '\0';
                        //Jul 29, 2014: clear imei_cmp flag
                        pvt->init_settings.imei_cmp = 0;
                        if(pvt->last_mdm_state == 0) rgsm_pvt_power_off(NULL, pvt);
                        //TODO: !!! after modem reset the default imei becomes instead of new one !!!
                        //Sept 12, 2012
                        //ast_verbose("rgsm: <%s>: restart channel to avoid detection \"imei change on the fly\"\n", pvt->name);
                        //RST_CHANNEL(pvt, 0);
                    }
                }
                else if(strstr(pvt->recv_buf, "ERROR")){
                    ast_verbose("rgsm: <%s>: IMEI read error\n", pvt->name);
                    if (pvt->init_settings.imei_cmp) {
                        ast_verbose("rgsm: <%s>: IMEI change FAILED: Couldn't read module's IMEI\n", pvt->name);
                        rgsm_man_event_imei_change_complete(pvt->name, 1, "Couldn't read module's IMEI");
                        pvt->new_imei[0] = '\0';
                        //Jul 29, 2014: clear imei_cmp flag
                        pvt->init_settings.imei_cmp = 0;
                        //TODO: !!! after modem reset the default imei becomes instead of new one !!!
                        //Sept 12, 2012
                        //ast_verbose("rgsm: <%s>: restart channel to avoid detection \"imei change on the fly\"\n", pvt->name);
                        //RST_CHANNEL(pvt, 0);
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CIMI:
                if(is_str_digit(pvt->recv_buf))
                    ast_copy_string(pvt->imsi, pvt->recv_buf, sizeof(pvt->imsi));
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CLCC:
                if (strstr(pvt->recv_buf, "+CLCC:")) {
                    parser_ptrs.clcc_ex = (struct at_gen_clcc_exec *)&pvt->parser_buf;
                    if (at_gen_clcc_exec_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.clcc_ex) < 0) {
                        ast_log(LOG_ERROR, "<%s>: at_gen_clcc_exec_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    } else {
                        // get active line id
                        if (pvt->owner && !pvt->active_line) {
                            // check for clcc stat waiting
                            if (parser_ptrs.clcc_ex->dir) {
                                // incoming call - mobile terminated
                                if ((parser_ptrs.clcc_ex->stat == 0) || (parser_ptrs.clcc_ex->stat == 4)) {
                                    pvt->active_line = parser_ptrs.clcc_ex->id;
                                    ast_verb(4, "<%s>: incoming active line id=%d\n", pvt->name, parser_ptrs.clcc_ex->id);
                                    // processing OVERLAP_RECEIVING
                                    if (pvt->call_state == CALL_STATE_OVERLAP_RECEIVING) {
                                        // get calling number
                                        if (parser_ptrs.clcc_ex->number_len > 0) {
                                            ast_copy_string(pvt->calling_number.value, parser_ptrs.clcc_ex->number, MAX_ADDRESS_LENGTH);
                                            pvt->calling_number.length = parser_ptrs.clcc_ex->number_len;
                                            pvt->calling_number.type.full = parser_ptrs.clcc_ex->type;
                                            address_normalize(&pvt->calling_number);
                                        }
                                        else
                                            address_classify("unknown", &pvt->calling_number);
                                        // run call state machine
                                        call_sm(pvt, CALL_MSG_INFO_IND, 0);
                                    }
                                }
                            } else {
                                // outgoing call - mobile originated
                                if ((parser_ptrs.clcc_ex->stat == 0) || (parser_ptrs.clcc_ex->stat == 2) || (parser_ptrs.clcc_ex->stat == 3)) {
                                    pvt->active_line = parser_ptrs.clcc_ex->id;
                                    ast_verb(4, "<%s>: outgoing active line id=%d\n", pvt->name, parser_ptrs.clcc_ex->id);
                                }
                            }
                        }
                        // check for change call state on active line
                        if (pvt->active_line == parser_ptrs.clcc_ex->id) {
                            if (pvt->active_line_stat != parser_ptrs.clcc_ex->stat) {
                                pvt->active_line_stat = parser_ptrs.clcc_ex->stat;
                                if(parser_ptrs.clcc_ex->dir == 0){
                                    // outgoing call - mobile originated
                                    switch(pvt->active_line_stat){
                                        case 0: // active
                                            if((pvt->call_state == CALL_STATE_OUT_CALL_PROC) || (pvt->call_state == CALL_STATE_CALL_DELIVERED)){
                                                // user response - setup confirm
                                                call_sm(pvt, CALL_MSG_SETUP_CONFIRM, 0);
                                                // stop dial timer
                                                rgsm_timer_stop(pvt->timers.dial);
                                            }
                                        case 2: // dialing
                                            if(pvt->call_state == CALL_STATE_OUT_CALL_PROC)
                                                call_sm(pvt, CALL_MSG_PROCEEDING_IND, 0);
                                            break;
                                        case 3: // alerting
                                            if(pvt->call_state == CALL_STATE_OUT_CALL_PROC)
                                                call_sm(pvt, CALL_MSG_ALERTING_IND, 0);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                        }

                        // get wait line id
                        if(pvt->owner && pvt->active_line && !pvt->wait_line){
                            // check for clcc stat waiting
                            if(parser_ptrs.clcc_ex->stat == 5){
                                // get wait line id
                                pvt->wait_line = parser_ptrs.clcc_ex->id;
                                // get wait line number
                                if(parser_ptrs.clcc_ex->number_len > 0)
                                    ast_copy_string(pvt->wait_line_num, parser_ptrs.clcc_ex->number, sizeof(pvt->wait_line_num));
                                else
                                    ast_copy_string(pvt->wait_line_num, "unknown", sizeof(pvt->wait_line_num));
                                ast_verb(2, "<%s>: Call \"%s\" Waiting...\n", pvt->name, pvt->wait_line_num);
                                ast_verb(4, "<%s>: wait line id=%d\n", pvt->name, parser_ptrs.clcc_ex->id);
                                // play callwait arrived tone
                                // tmp_ast_pvt = ast_channel_internal_bridged_channel(pvt->owner);
                                if(tmp_ast_pvt){
                                    ast_playtones_start(tmp_ast_pvt, 0, cw_arrived_tone, 0);
                                    tmp_ast_pvt = NULL;
                                    pvt->is_play_tone = 1;
                                }
                            }
                        } /* get wait line id */
                        // perform active line contest
                        if((pvt->active_line) && (!pvt->active_line_contest)){
                            if(pvt->active_line == parser_ptrs.clcc_ex->id)
                                pvt->active_line_contest = 1;
                        }
                        // perform wait line contest
                        if((pvt->wait_line) && (!pvt->wait_line_contest)){
                            if(pvt->wait_line == parser_ptrs.clcc_ex->id)
                                pvt->wait_line_contest = 1;
                        }
                    } /* parsing success */
                }
                else if(strstr(pvt->recv_buf, "OK")){
                    // perform active line contest
                    if(pvt->active_line && pvt->wait_line){
                        // check for contest is bad end
                        if(!pvt->active_line_contest){
                            ast_verb(4, "<%s>: active line id=%d is gone\n", pvt->name, pvt->active_line);
                            pvt->active_line = 0;
                            pvt->wait_line = 0;
                            // get waiting call
                            rgsm_atcmd_queue_append(pvt, AT_CHLD, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 2);
                            // play callwait arrived tone
                            // tmp_ast_pvt = ast_channel_internal_bridged_channel(pvt->owner);
                            if(tmp_ast_pvt){
                                ast_playtones_start(tmp_ast_pvt, 0, cw_accepted_tone, 0);
                                tmp_ast_pvt = NULL;
                                pvt->is_play_tone = 1;
                            }
                        }
                    }
                    // perform wait line contest
                    if(pvt->wait_line){
                        // check for contest is bad end
                        if(!pvt->wait_line_contest){
                            ast_verb(4, "<%s>: wait line id=%d is gone\n", pvt->name, pvt->wait_line);
                            pvt->wait_line = 0;
                            pvt->wait_line_num[0] = '\0';
                            // play callwait gone tone
                            // tmp_ast_pvt = ast_channel_internal_bridged_channel(pvt->owner);
                            if(tmp_ast_pvt){
                                ast_playtones_start(tmp_ast_pvt, 0, cw_gone_tone, 0);
                                tmp_ast_pvt = NULL;
                                pvt->is_play_tone = 1;
                            }
                        }
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CPAS:
                if(strstr(pvt->recv_buf, "+CPAS:")){
                    if((tp = strchr(pvt->recv_buf, SP))){
                        tp++;
                        if(is_str_digit(tp)){
                            // perform action if cpas changed
                            switch(atoi(tp)){
                                //++++++++++++++++++++++++++++++++++
                                case 0:
                                    if(pvt->call_state == CALL_STATE_DISABLE) {
                                        pvt->call_state = CALL_STATE_NULL;
                                    }
                                case 2:
                                case 3:
                                case 4:
                                    break;
                                //++++++++++++++++++++++++++++++++++
                                default:
                                    ast_log(LOG_WARNING, "<%s>: unknown cpas value = %d\n", pvt->name, atoi(tp));
                                    break;
                            }
                        }
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CSQ:
                if(strstr(pvt->recv_buf, "+CSQ:")){
                    parser_ptrs.csq_ex = (struct at_gen_csq_exec *)&pvt->parser_buf;
                    if(at_gen_csq_exec_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.csq_ex) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_gen_csq_exec_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    } else {
                        pvt->rssi = parser_ptrs.csq_ex->rssi;
                        pvt->ber = parser_ptrs.csq_ex->ber;
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CNUM:
                if(strstr(pvt->recv_buf, "+CNUM:")){
                    parser_ptrs.cnum_ex = (struct at_gen_cnum_exec *)&pvt->parser_buf;
                    if(at_gen_cnum_exec_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.cnum_ex) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_gen_cnum_exec_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    } else{
                        ast_copy_string(pvt->subscriber_number.value, parser_ptrs.cnum_ex->number, MAX_ADDRESS_LENGTH);
                        pvt->subscriber_number.length = parser_ptrs.cnum_ex->number_len;
                        pvt->subscriber_number.type.full = parser_ptrs.cnum_ex->type;
                        address_normalize(&pvt->subscriber_number);
                    }
                    ast_debug(2, "<%s>: subscriber_number=\"%s\", type=%d\n", pvt->name, pvt->subscriber_number.value, pvt->subscriber_number.type.full);
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_I:
                if(strcmp(pvt->recv_buf, "OK") && !strstr(pvt->recv_buf, "ERROR")) {
            	    if(pvt->module_type == MODULE_TYPE_UNKNOWN)
            	    {
                	if (strstr(pvt->recv_buf, "SIM900")) {
                    	    pvt->module_type = MODULE_TYPE_SIM900;
                    	    // tune AT commands send rate
                    	    if (sim900_config.at_send_rate == 1) {
                    		trysend_timeout.tv_sec = 1;
                        	trysend_timeout.tv_usec = 0;
                    	    } else {
                        	trysend_timeout.tv_sec = 0;
                        	trysend_timeout.tv_usec = 1000000/sim900_config.at_send_rate;
                    	    }
                    	    sim900_init(pvt);
                    	    ast_verbose("rgsm: <%s>: gsm module is [%s], at_send_rate=%d commands per second\n",
                                    pvt->name, pvt->recv_buf, sim900_config.at_send_rate);
                	} else if (strstr(pvt->recv_buf, "SIM5320E")) {
                	    pvt->module_type = MODULE_TYPE_SIM5320;
                	    // tune AT commands send rate
                    	    if (sim5320_config.at_send_rate == 1) {
                    		trysend_timeout.tv_sec = 1;
                        	trysend_timeout.tv_usec = 0;
                    	    } else {
                        	trysend_timeout.tv_sec = 0;
                        	trysend_timeout.tv_usec = 1000000/sim5320_config.at_send_rate;
                    	    }
                    	    sim5320_init(pvt);
                    	    ast_verbose("rgsm: <%s>: gsm module is [%s], at_send_rate=%d commands per second\n",
                                    pvt->name, pvt->recv_buf, sim5320_config.at_send_rate);
                	} else if (strstr(pvt->recv_buf, "UC15")) {
                	    pvt->module_type = MODULE_TYPE_UC15;
                	    // tune AT commands send rate
                    	    if (uc15_config.at_send_rate == 1) {
                    		trysend_timeout.tv_sec = 1;
                        	trysend_timeout.tv_usec = 0;
                    	    } else {
                        	trysend_timeout.tv_sec = 0;
                        	trysend_timeout.tv_usec = 1000000/uc15_config.at_send_rate;
                    	    }
                    	    uc15_init(pvt);
                    	    ast_verbose("rgsm: <%s>: gsm module is [%s], at_send_rate=%d commands per second\n",
                                    pvt->name, pvt->recv_buf, uc15_config.at_send_rate);
                	}
                	 else {
                    	    ast_verbose("rgsm: <%s>: gsm module type unknown [%s]\n", pvt->name, pvt->recv_buf);
                	}

                	if(pvt->module_type != MODULE_TYPE_UNKNOWN)
                	{
                	    pvt->flags.module_type_discovered = 1;
                	    if (pvt->flags.sim_present)
                		gsm_query_sim_data(pvt);
                	    // processing response
        		    query_module_data(pvt);
        		    //
        		    //set_sim_poll(pvt);

        		    //Start sim poll
        		    //if(!pvt->flags.sim_present)
        		    //	gsm_start_simpoll(pvt);

        		}
            	    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_andW:
                if(strstr(pvt->recv_buf, "OK")){
                    if (pvt->mdm_state == MDM_STATE_AUTO_BAUDING) {
                        //3sec slee
                        rgsm_usleep(pvt, 3000000);
                        ast_verbose("rgsm: <%s>: Auto-bauding complete, new serial port rate %s saved. Restarting channel...\n",
                                    pvt->name, baudrate_str(pvt->baudrate));
                        //pvt->mdm_state = MDM_STATE_PWR_DOWN;
                        //pvt->mdm_state = MDM_STATE_DISABLE;
                        pvt->flags.adjust_baudrate = 0;
                        //Aug 31,  2014: do hard reset
                        RST_CHANNEL(pvt, 0);
                        pvt->flags.hard_reset = 1;
                    }
                }
                break; //AT&W
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    } // end of EXEC operations
    else if(pvt->cmd_queue.first->oper == AT_OPER_TEST){
        // TEST operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    } // end of TEST operations
    else if(pvt->cmd_queue.first->oper == AT_OPER_READ) {
        // READ operations
        switch(pvt->cmd_queue.first->id) {
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CLIR:
                if(strstr(pvt->recv_buf, "+CLIR:")){
                    parser_ptrs.clir_rd = (struct at_gen_clir_read *)&pvt->parser_buf;
                    if(at_gen_clir_read_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.clir_rd) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_gen_clir_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                        // check for query
                        if(pvt->querysig.hidenum){
                            ast_verbose("rgsm: <%s>: qwery(hidenum): error\n", pvt->name);
                            pvt->querysig.hidenum = 0;
                        }
                    } else {
                        pvt->hidenum_set = parser_ptrs.clir_rd->n;
                        pvt->hidenum_stat = parser_ptrs.clir_rd->m;
                        // check for query
                        if(pvt->querysig.hidenum){
                            ast_verbose("rgsm: <%s>: qwery(hidenum): %s\n",
                                pvt->name,
                                hidenum_settings_str(pvt->hidenum_set));
                            pvt->querysig.hidenum = 0;
                        }
                    }
                }
                else if(strstr(pvt->recv_buf, "ERROR")){
                    pvt->hidenum_set = HIDENUM_UNKNOWN;
                    pvt->hidenum_stat = HIDENUM_STATUS_UNKNOWN;
                    // check for query
                    if(pvt->querysig.hidenum){
                        ast_verbose("rgsm: <%s>: qwery(hidenum): error\n", pvt->name);
                        pvt->querysig.hidenum = 0;
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_COPS:
                if(strstr(pvt->recv_buf, "+COPS:")){
                    parser_ptrs.cops_rd = (struct at_gen_cops_read *)&pvt->parser_buf;
                    if(at_gen_cops_read_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.cops_rd) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_gen_cops_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    } else{
                        if((parser_ptrs.cops_rd->format == 0) && (parser_ptrs.cops_rd->oper_len >= 0))
                            ast_copy_string(pvt->operator_name, parser_ptrs.cops_rd->oper, sizeof(pvt->operator_name));
                        else if((parser_ptrs.cops_rd->format == 2) && (parser_ptrs.cops_rd->oper_len >= 0))
                            ast_copy_string(pvt->operator_code, parser_ptrs.cops_rd->oper, sizeof(pvt->operator_code));
                    }
                }
                break; //+COPS
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CREG:
                if(strstr(pvt->recv_buf, "+CREG:")){
                    //
                    reg_state_t reg_stat_old = pvt->reg_state;
                    parser_ptrs.creg_rd = (struct at_gen_creg_read *)&pvt->parser_buf;
                    //
                    if(at_gen_creg_read_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.creg_rd) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_gen_parse creg_read(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    } else{
                        pvt->reg_state = parser_ptrs.creg_rd->stat;
                        if(reg_stat_old != pvt->reg_state) {
                            //Sept 2, 2014: out iccid for all reg statuses
							//if((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)) {
							//	ast_verbose("rgsm: <%s>: iccid %s registration status - %s\n", pvt->name, pvt->iccid, reg_state_print(pvt->reg_state));
							//} else {
							//	ast_verbose("rgsm: <%s>: registration status - %s\n", pvt->name, reg_state_print(pvt->reg_state));
							//}
							ast_verbose("rgsm: <%s>: iccid %s registration status - %s\n", pvt->name, pvt->iccid, reg_state_print(pvt->reg_state));

                            // processing registaration status
                            if (pvt->reg_state == REG_STATE_NOTREG_SEARCH) {
                                rgsm_man_event_channel(pvt, "SIM not registered searching operator", 1);
                            }
                            else if((pvt->reg_state == REG_STATE_NOTREG_NOSEARCH) || (pvt->reg_state == REG_STATE_REG_DENIED)){
                                // "0" no search, "3" denied
                                // stop runhalfsecond timer
/*! May 22, 2013: runhalfsecond retired
                                rgsm_timer_stop(pvt->timers.runhalfsecond);
*/
                                // stop runonesecond timer
                                rgsm_timer_stop(pvt->timers.runonesecond);
                                // stop runhalfminute timer
                                rgsm_timer_stop(pvt->timers.runhalfminute);
                                // stop runoneminute timer
                                rgsm_timer_stop(pvt->timers.runoneminute);
                                // stop smssend timer
                                rgsm_timer_stop(pvt->timers.smssend);
                                // stop runvifesecond timer
                                rgsm_timer_stop(pvt->timers.runfivesecond);
                                // hangup active call
                                if(pvt->owner){
                                    // hangup - GSM
                                    if(pvt->functions.hangup != NULL)
    				    {
    					pvt->functions.hangup(pvt);
    				    }
    				    else
    					rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                                    // hangup - asterisk core call
                                    call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_NORMAL_CLEARING);
                                }

                                if(pvt->flags.sim_present) {
                                    // decrement attempt counter
                                    if(pvt->reg_try_cnt_curr > 0)
                                        pvt->reg_try_cnt_curr--;
                                    // check rest of attempt count
                                    if(pvt->reg_try_cnt_curr != 0){
                                        ast_verbose("rgsm: <%s>: restart GSM module for next attempt registration on BTS\n", pvt->name);
                                        if (pvt->reg_state == REG_STATE_REG_DENIED) {
                                            rgsm_man_event_channel(pvt, "SIM registration denied", 1);
                                        } else if (pvt->reg_state == REG_STATE_NOTREG_NOSEARCH) {
                                            rgsm_man_event_channel(pvt, "SIM not registered no search", 1);
                                        }
                                        if(pvt->reg_try_cnt_curr > 0) {
                                            ast_verbose("rgsm: <%s>: remaining %d attempts\n", pvt->name, pvt->reg_try_cnt_curr);
                                            RST_CHANNEL(pvt, 0);
                                        }
                                    } else { // attempts count fired
                                        ast_verbose("rgsm: <%s>: exceed a registration attempts count\n", pvt->name);
                                        // set change sim signal
#if 0
                                        //obsolete Apr 22,2 013
                                        pvt->flags.changesim = 1;
                                        // mark baned sim id
                                        ast_copy_string(pvt->iccid_ban, pvt->iccid, sizeof(pvt->iccid_ban));
#endif
                                        gsm_abort_channel(pvt, MAN_CHSTATE_ABORTED_REGFAILURE);
                                        rgsm_man_event_channel(pvt, "All SIM registration attempts failed", 1);
                                    }
                                }
#if 0
                                //obsolete Apr 22,2 013
                                // disable GSM module functionality
                                rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
                                //
                                pvt->mdm_state = MDM_STATE_SUSPEND;
                                // start simpoll timer
                                rgsm_timer_set(pvt->timers.simpoll, simpoll_timeout);
#endif
                            }
                            else if((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)){
                                // "1" home network, "5" roaming
                                pvt->reg_try_cnt_curr = pvt->reg_try_cnt_conf;

                                if (pvt->man_chstate != MAN_CHSTATE_IDLE) {
                                    pvt->man_chstate = MAN_CHSTATE_IDLE;
                                    rgsm_man_event_channel_state(pvt);
                                }

                                rgsm_man_event_channel(pvt, "SIM registered", 1);

                            }
                        }
                    }
                }
                break; //CREG
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CFUN:
                if (pvt->func_test_run) {
                    if (strstr(pvt->recv_buf, "+CFUN:")) {
                        pvt->func_test_run = 0;
                        pvt->func_test_done = 1;
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CLVL:
                if(strstr(pvt->recv_buf, "+CLVL:")){
                    if((tp = strchr(pvt->recv_buf, SP))){
                        tp++;
                        if(is_str_digit(tp)){
                            rc = atoi(tp);
                            pvt->spk_gain_curr = rc;
                            // check for query
                            if(pvt->querysig.spk_gain){
                                ast_verbose("rgsm: <%s>: qwery(spk_gain): %d\n", pvt->name, rc);
                                pvt->querysig.spk_gain = 0;
                            }
                        } else {
                            // check for query
                            if(pvt->querysig.spk_gain){
                                ast_verbose("rgsm: <%s>: qwery(spk_gain): error\n", pvt->name);
                                pvt->querysig.spk_gain = 0;
                            }
                        }
                    }
                } else if(strstr(pvt->recv_buf, "ERROR")){
                    // check for query
                    if(pvt->querysig.spk_gain){
                        ast_verbose("rgsm: <%s>: qwery(spk_gain): error\n", pvt->name);
                        pvt->querysig.spk_gain = 0;
                    }
                }
                break; //CLVL
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CSCA:
                if(strstr(pvt->recv_buf, "+CSCA:")){
                    parser_ptrs.csca_rd = (struct at_gen_csca_read *)&pvt->parser_buf;
                    if(at_gen_csca_read_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.csca_rd) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_gen_csca_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    } else{
                        if(is_address_string(parser_ptrs.csca_rd->sca)){
                            ast_copy_string(pvt->smsc_number.value, parser_ptrs.csca_rd->sca, MAX_ADDRESS_LENGTH);
                            pvt->smsc_number.length = parser_ptrs.csca_rd->sca_len;
                            pvt->smsc_number.type.full = parser_ptrs.csca_rd->tosca;
                            address_normalize(&pvt->smsc_number);
                        } else if(is_str_xdigit(parser_ptrs.csca_rd->sca)){
                            ip = parser_ptrs.csca_rd->sca;
                            ilen = parser_ptrs.csca_rd->sca_len;
                            op = pvt->smsc_number.value;
                            olen = MAX_ADDRESS_LENGTH;
                            memset(pvt->smsc_number.value, 0 , MAX_ADDRESS_LENGTH);
                            //
                            if(!str_hex_to_bin(&ip, &ilen, &op, &olen)){
                                pvt->smsc_number.length = strlen(pvt->smsc_number.value);
                                pvt->smsc_number.type.full = parser_ptrs.csca_rd->tosca;
                                address_normalize(&pvt->smsc_number);
                            }
                        }
                    }
                    //ast_debug(2, "<%s>: smsc_number=\"%s\", type=%d\n", pvt->name, pvt->smsc_number.value, pvt->smsc_number.type.full);
                }
                else if(strstr(pvt->recv_buf, "ERROR")){
                    unknown_address(&pvt->smsc_number);
                }
                break; //CSCA
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_IPR:
                if(strstr(pvt->recv_buf, "+IPR:")){
                    //ast_log(LOG_DEBUG, "<%s>: pvt->mdm_state == %d\n", pvt->name, pvt->mdm_state);
                    if (pvt->mdm_state == MDM_STATE_AUTO_BAUDING) {
                        //TA and ME understand each other on selected baud rate:
                        //set max fixed rate
                        //and persist it via AT&W
                        //modem reset will be issued in AT_andW handler
                        rgsm_atcmd_queue_append(pvt, AT_IPR, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "115200");
                    }
                }
                break; //IPR
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    } // end of READ operations
    else if(pvt->cmd_queue.first->oper == AT_OPER_WRITE) {
        // WRITE operations
        switch(pvt->cmd_queue.first->id) {
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CCWA:
                if(strstr(pvt->recv_buf, "+CCWA:")){
                    parser_ptrs.ccwa_wr = (struct at_gen_ccwa_write *)&pvt->parser_buf;
                    if(at_gen_ccwa_write_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.ccwa_wr) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_gen_ccwa_write_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                        // check for query
                        if(pvt->querysig.callwait){
                            ast_verbose("rgsm: <%s>: qwery(callwait): error\n", pvt->name);
                            pvt->querysig.callwait = 0;
                        }
                    } else {
                        if(parser_ptrs.ccwa_wr->class & CALL_CLASS_VOICE){
                            //
                            pvt->callwait_status = parser_ptrs.ccwa_wr->status;
                            // check for query
                            if(pvt->querysig.callwait){
                                ast_verbose("rgsm: <%s>: qwery(callwait): %s\n",
                                    pvt->name,
                                    callwait_status_str(pvt->callwait_status));
                                pvt->querysig.callwait = 0;
                            }
                        }
                    }
                } else if(strstr(pvt->recv_buf, "ERROR")){
                    pvt->callwait_status = CALLWAIT_STATUS_UNKNOWN;
                    // check for query
                    if(pvt->querysig.callwait){
                        ast_verbose("rgsm: <%s>: qwery(callwait): error\n", pvt->name);
                        pvt->querysig.callwait = 0;
                    }
                }
                break; //CCWA
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CLIR:
                if(strstr(pvt->recv_buf, "OK"))
                    pvt->hidenum_set = pvt->hidenum_conf;
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CLVL:
                if(strstr(pvt->recv_buf, "OK"))
                    pvt->spk_gain_curr = pvt->spk_gain_conf;
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CUSD:
                ussd_handler(pvt);
                break; //CUSD
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CMGR:
                if(pvt->cmd_queue.first->sub_cmd == SUBCMD_CMGR_USER){
                    if(strstr(pvt->recv_buf, "+CMGR:")){
                        //
                        parser_ptrs.cmgr_wr = (struct at_gen_cmgr_write *)&pvt->parser_buf;
                        if(at_gen_cmgr_write_parse(pvt->recv_buf, pvt->recv_len, parser_ptrs.cmgr_wr) < 0){
                            ast_log(LOG_ERROR, "<%s>: at_gen_cmgr_write_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                            pvt->recv_buf[0] = '\0';
                            pvt->recv_buf_valid = 0;
                            pvt->recv_len = 0;
                            //
                            ast_copy_string(pvt->sms_user_pdubuf, "message read error", 1024);
                            pvt->sms_user_pdulen = strlen("message read error");
                            pvt->sms_user_done = 1;
                        } else {
                            pvt->sms_user_length = parser_ptrs.cmgr_wr->length;
                        }
                    }
                    else if(is_str_xdigit(pvt->recv_buf)){ // PDU data
                        //
                        if(pvt->recv_len < 1024){
                            memcpy(pvt->sms_user_pdubuf, pvt->recv_buf, pvt->recv_len);
                            pvt->sms_user_pdulen = pvt->recv_len;
                            pvt->sms_user_valid = 1;
                        } else {
                            ast_log(LOG_WARNING, "<%s>: pdu buffer too long=%d\n", pvt->name, pvt->recv_len);
                        }
                    }
                    else if(strstr(pvt->recv_buf, "OK")){
                        //
                        if(!pvt->sms_user_valid){
                            ast_copy_string(pvt->sms_user_pdubuf, "empty memory slot", 1024);
                            pvt->sms_user_pdulen = strlen("empty memory slot");
                        }
                        pvt->sms_user_done = 1;
                    }
                    else if(strstr(pvt->recv_buf, "ERROR")){
                        //
                        ast_copy_string(pvt->sms_user_pdubuf, "message read error", 1024);
                        pvt->sms_user_pdulen = strlen("message read error");
                        pvt->sms_user_done = 1;
                    }
                }
                break; //CMGR
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_CMGS:
                process_cmgs_response(pvt);
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_IPR:
                if(strstr(pvt->recv_buf, "OK")){
                    if (pvt->mdm_state == MDM_STATE_AUTO_BAUDING) {
                        //here is response for AT+IPR=115200, write profile settings
                        rgsm_atcmd_queue_append(pvt, AT_andW, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                    }
                }
                break; //CSCA
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_PSSTK:
                if (strstr(pvt->recv_buf, "*PSSTK:")) {
                    // parse psstrk
                    char *tp;
                    if (pvt->stk_capabilities) {
                        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: STK Notify from SIM: %s\n", pvt->name, pvt->recv_buf);
                        if((tp = strchr(pvt->recv_buf, ' '))) {
                            tp++;
                            rgsm_man_event_stk_notify(pvt, tp);
                        }
                    }
                } else if (strstr(pvt->recv_buf, "OK")) {
                    pvt->stk_cmd_done = 1;
                    strcpy(pvt->stk_status, "OK");
                } else if (strstr(pvt->recv_buf, "ERROR") || strstr(pvt->recv_buf, "+CME ERROR:")) {
                    pvt->stk_cmd_done = 1;
                    strncpy(pvt->stk_status, pvt->recv_buf, sizeof(pvt->stk_status));
                }

            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    } // end of WRITE operations
    else {
        ast_log(LOG_ERROR, "<%s>: general AT command [%.*s] with unknown [%0X] operation\n",
                pvt->name,
                pvt->cmd_queue.first->cmd_len - 1,
                pvt->cmd_queue.first->cmd_buf,
                pvt->cmd_queue.first->oper);
    } //end of general commands processing

    //19 Feb 2016 driver atp_handle_response

    if(pvt->module_type != MODULE_TYPE_UNKNOWN)
    {
	if (pvt->functions.atp_handle_response != NULL)
	{
	    pvt->functions.atp_handle_response(pvt);
	}
	else
	{
	    ast_log(LOG_WARNING, "<%s>: atp_handle_reponse function not defined for that module type\n", pvt->name);
	}
    }

    // SIM900 AT commands
    //if(pvt->module_type == MODULE_TYPE_SIM900) {
    //    sim900_atp_handle_response(pvt);
    //}

// 19 Feb 2016 Remove unecessary drivers
/*    // SIM300 AT commands
    else if(pvt->module_type == MODULE_TYPE_SIM300){
        sim300_atp_handle_response(pvt);
    } // end of SIM300 AT commands
    // M10 AT commands
    else if(pvt->module_type == MODULE_TYPE_M10){
        m10_atp_handle_response(pvt);
    } // end of M10 AT commands
*/
    // test for command done
    if (pvt->cmd_done) {
        // if done -> remove from queue
        rm_cmd = AST_LIST_REMOVE_HEAD(&pvt->cmd_queue, entry);
        rgsm_atcmd_queue_free(pvt, rm_cmd);
    }
    //
    pvt->recv_buf_valid = 0;
    pvt->recv_buf[0] = '\0';
    pvt->recv_len = 0;
} //handle response



void atp_handle_unsolicited_result(struct gsm_pvt* pvt)
{
    //char *tp;
    char *str0;

    if (!pvt->recv_buf_valid) return;

    // -----------------------------------------------------------------
    // UNSOLICITED RESULT CODE
    if (!strcmp(pvt->recv_buf, preambule)) {
        ast_log(AST_LOG_DEBUG, "Preambule processed\n");
    }
    // BUSY
    else if (strstr(pvt->recv_buf, "BUSY")) {
        // valid if call state OUT_CALL_PROC || CALL_DELIVERED
        if ((pvt->call_state == CALL_STATE_OUT_CALL_PROC) || (pvt->call_state == CALL_STATE_CALL_DELIVERED)) {
            call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_USER_BUSY);
        }
    }
    else if (strstr(pvt->recv_buf, "RING")) {
        // check if pvt channel has owner
        if ((pvt->owner) && (pvt->call_dir == CALL_DIR_OUT)) {
            // owner, release outgoing channel on concurent incoming call
            call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_CHANNEL_UNACCEPTABLE);
        } else {
            // not owned
            if (pvt->call_state == CALL_STATE_NULL) {
                //
                fake_owner = ast_dummy_channel_alloc();
                pvt->owner = fake_owner;
                // set call direction
                pvt->call_dir = CALL_DIR_IN;
                // start incoming call state machine
                call_sm(pvt, CALL_MSG_SETUP_IND, 0);
            } else if (pvt->call_state == CALL_STATE_OVERLAP_RECEIVING) {
                // if calling party number not present
                // set calling party as unknown
                address_classify("unknown", &pvt->calling_number);
                // run call state machine
                call_sm(pvt, CALL_MSG_INFO_IND, 0);
            } else if (pvt->call_state == CALL_STATE_DISABLE) {
                //
                ast_verb(3, "<%s>: incoming call - channel disabled\n", pvt->name);
                // hangup
                if(pvt->functions.hangup != NULL)
    		{
    		    pvt->functions.hangup(pvt);
    		}
    		else
    		    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
            }
        }
    }
    else if (strstr(pvt->recv_buf, "+CLIP:")) {
        // parse clip
        struct at_gen_clip_unsol *clip_unsol = (struct at_gen_clip_unsol *)&pvt->parser_buf;
        if (at_gen_clip_unsol_parse(pvt->recv_buf, pvt->recv_len, clip_unsol) < 0) {
            ast_log(LOG_ERROR, "<%s>: at_gen_clip_unsol_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
            pvt->recv_buf[0] = '\0';
            pvt->recv_buf_valid = 0;
            pvt->recv_len = 0;
        } else {
            // valid if call state OVERLAP_RECEIVING
            if (pvt->call_state == CALL_STATE_OVERLAP_RECEIVING) {
                // get calling number
                if (clip_unsol->number_len > 0) {
                    ast_copy_string(pvt->calling_number.value, clip_unsol->number, MAX_ADDRESS_LENGTH);
                    pvt->calling_number.length = clip_unsol->number_len;
                    pvt->calling_number.type.full = clip_unsol->type;

                    //ast_log(LOG_DEBUG, "<%s>: CLIP parsed: clip.number=%s, clip.number_len=%d, calling_number=%s, type=%d\n",
                    //        pvt->name, clip_unsol->number, clip_unsol->number_len, pvt->calling_number.value, pvt->calling_number.type.full);


                    address_normalize(&pvt->calling_number);

                    //ast_log(LOG_DEBUG, "<%s>: number normalized: calling_number=%s\n",
                    //        pvt->name, pvt->calling_number.value);
                } else {
                    address_classify("unknown", &pvt->calling_number);
                }
                // run call state machine
                call_sm(pvt, CALL_MSG_INFO_IND, 0);
            }
        }
    }
    else if (strstr(pvt->recv_buf, "NO CARRIER")) {
        // if call state
        if (pvt->call_state == CALL_STATE_CALL_RECEIVED) {
            //
            call_sm(pvt, CALL_MSG_RELEASE_IND, 0);
        } else if (pvt->call_state == CALL_STATE_CALL_DELIVERED) {
            //
            call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_NO_ANSWER);
        } else if ((!pvt->wait_line) && (pvt->call_state == CALL_STATE_ACTIVE)) {
            //
            call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_NORMAL_CLEARING);
        }
    }
    else if (strstr(pvt->recv_buf, "NO ANSWER")) {
        // NO ANSWER (M10)
        // valid if call state OUT_CALL_PROC || CALL_DELIVERED
        if ((pvt->call_state == CALL_STATE_OUT_CALL_PROC) || (pvt->call_state == CALL_STATE_CALL_DELIVERED)) {
            call_sm(pvt, CALL_MSG_RELEASE_IND, AST_CAUSE_NO_ANSWER);
        }
    }
    else if (strstr(pvt->recv_buf, "+CUSD:")) {
        ussd_handler(pvt);
    }
    else if (strstr(pvt->recv_buf, "+CME ERROR:")) {
        pvt->ussd_done = 1;
    }
    else if (!strcasecmp(pvt->recv_buf, "RDY") || !strcasecmp(pvt->recv_buf, "START")) {
        // stop waitrdy timer
        rgsm_timer_stop(pvt->timers.waitrdy);
        pvt->flags.adjust_baudrate = 0;
        // May 12, 2017: allow to send AT commands after then a RDY/START received and until power off or reset
        pvt->allow_cli_at = 1;

        // valid if mgmt state WAIT_RDY
        if (pvt->mdm_state == MDM_STATE_WAIT_RDY) {
            pvt->mdm_state = MDM_STATE_WAIT_CFUN;
        }

        //pvt->config.baudrate = pvt->func_test_rate;

        ast_verbose("rgsm: <%s>: serial port working at %s baud\n", pvt->name, baudrate_str(pvt->baudrate));
        rgsm_atcmd_queue_append(pvt, AT_E, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, "%d", 0);
        rgsm_atcmd_queue_append(pvt, AT_I, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

        //Aug 30, 2014: query module data ASAP
        //query_module_data(pvt);
    }
    else if (!strncasecmp(pvt->recv_buf, "+CFUN:", 6)) {
        // valid if mgmt state WAIT_CFUN
        if (pvt->mdm_state == MDM_STATE_WAIT_CFUN) {
            if (!strcasecmp(pvt->recv_buf, "+CFUN: 0")) {
                // minimum functionality - try to enable GSM module full functionality
                rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "1");
                // next mgmt state -- check for SIM is READY
                pvt->mdm_state = MDM_STATE_CHECK_PIN;
                // start pinwait timer
                rgsm_timer_set(pvt->timers.pinwait, pinwait_timeout);
                // end of  CFUN: 0
            }
            else if (!strcasecmp(pvt->recv_buf, "+CFUN: 1")) {
                //
                if (pvt->flags.cpin_checked) {
                    //
                    query_module_data(pvt);
                    //
                    set_sim_poll(pvt);

                    if (pvt->flags.sim_present) {
                        gsm_query_sim_data(pvt);
                        //
// Fix for SIM5320 Modem module
//SIM5320 never sending phrase "Call Ready"
                        if(pvt->module_type == MODULE_TYPE_SIM5320){
	                        // try to set initial settings
	                        pvt->mdm_state = MDM_STATE_INIT;
	                        // get imsi
        		        rgsm_atcmd_queue_append(pvt, AT_CIMI, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
			        ast_log(AST_LOG_NOTICE, "rgsm: <%s>: %s MDM_STATE=%d\n", pvt->name, pvt->recv_buf, pvt->mdm_state);
			}
// End of Fix			
            	    	else{
        		    pvt->mdm_state = MDM_STATE_WAIT_CALL_READY;
                            // start callready timer
		                rgsm_timer_set(pvt->timers.callready, callready_timeout);
                        }
                    } else {
                        gsm_start_simpoll(pvt);
                    }
                } else {
                    //
                    pvt->mdm_state = MDM_STATE_CHECK_PIN;
                    // start pinwait timer
                    rgsm_timer_set(pvt->timers.pinwait, pinwait_timeout);
                }
            } // end of  CFUN: 1
        }
        // end of CFUN
    }
    else if (!strncasecmp(pvt->recv_buf, "+CPIN:", 6)) {
        // stop pinwait timer
        rgsm_timer_stop(pvt->timers.pinwait);
        //
        pvt->flags.cpin_checked = 1;
        //

        ast_log(AST_LOG_NOTICE, "rgsm: <%s>: %s MDM_STATE=%d\n", pvt->name, pvt->recv_buf, pvt->mdm_state);

        if (pvt->mdm_state == MDM_STATE_CHECK_PIN) {
            // 20 Feb 2016 Move to ATI process command.
            // On current state we don't know module type
            // processing response
            //query_module_data(pvt);
            //
            //set_sim_poll(pvt);

            if (!strcasecmp(pvt->recv_buf, "+CPIN: NOT INSERTED")) {
                // - SIM card not inserted
                if (pvt->flags.sim_present) {
                    gsm_reset_sim_data(pvt);
                    ast_verbose("rgsm: <%s>: SIM REMOVED\n", pvt->name);
                    rgsm_man_event_channel(pvt, "SIM removed", 0);
                } else if (!pvt->flags.sim_startup) {
                    ast_verbose("rgsm: <%s>: SIM NOT INSERTED\n", pvt->name);
                    rgsm_man_event_channel(pvt, "SIM not inserted", 0);
                }
                //
                pvt->flags.sim_startup = 1;
                pvt->flags.sim_present = 0;
                //
                if (pvt->flags.changesim)
                    pvt->flags.testsim = 1;

		//20 Feb 2016 MODULE_TYPE_UNKNOWN moved to ATI handle
                //gsm_start_simpoll(pvt);
                // end of NOT INSERTED
            }
            else if (!strcasecmp(pvt->recv_buf, "+CPIN: READY")) {
                // - PIN is not checked
                if (!pvt->flags.sim_present) {
                    ast_verbose("rgsm: <%s>: SIM INSERTED\n", pvt->name);
                    rgsm_man_event_channel(pvt, "SIM inserted", 0);
                }
                if (!pvt->flags.changesim) {
                    ast_verbose("rgsm: <%s>: PIN is not checked!!!\n", pvt->name);
                }
                // set SIM present flag
                pvt->flags.sim_startup = 1;
                pvt->flags.sim_present = 1;
                pvt->sim_try_cnt_curr = pvt->sim_try_cnt_conf;

                gsm_query_sim_data(pvt);

                //May 21, 2014
                //enable sim toolkit notifications as it was configured in chan_rgsm.conf and asap after RDY
                if (pvt->chnl_config.sim_toolkit) {
                    pvt->flags.stk_capabilities_req = 0;

                    //pvt->stk_capabilities equals the number of menu item
                    ast_verbose("rgsm: <%s>: Enable SIM Toolkit capabilities\n", pvt->name);
                    rgsm_atcmd_queue_append(pvt, AT_PSSTKI, AT_OPER_WRITE, 0, 30, 0, "1");
                }

                //
                if (pvt->flags.suspend_now) {
                    // stop all timers
                    memset(&pvt->timers, 0, sizeof(pvt->timers));
                    //
                    pvt->flags.suspend_now = 0;
                    //
                    rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
                    //
                    pvt->mdm_state = MDM_STATE_WAIT_SUSPEND;
                    // start waitsuspend timer
                    rgsm_timer_set(pvt->timers.waitsuspend, waitsuspend_timeout);
                } else if (pvt->flags.changesim) {
                    gsm_start_simpoll(pvt);
                } else {
                    //
                    pvt->mdm_state = MDM_STATE_WAIT_CALL_READY;
                    // start callready timer
                    rgsm_timer_set(pvt->timers.callready, callready_timeout);
                }
                // end of READY
            }
            else if (!strcasecmp(pvt->recv_buf, "+CPIN: SIM ERROR")) {
                gsm_start_simpoll(pvt);
                // end of SIM ERROR
            }
            // end of if (pvt->mdm_state == MDM_STATE_CHECK_PIN)
        }
        else if (pvt->mdm_state == MDM_STATE_WAIT_CFUN) {
            // processing response
            if (!strcasecmp(pvt->recv_buf, "+CPIN: NOT INSERTED")) {
                // - SIM card not inserted
                if (pvt->flags.sim_present) {
                    gsm_reset_sim_data(pvt);
                    ast_verbose("rgsm: <%s>: SIM REMOVED\n", pvt->name);
                    rgsm_man_event_channel(pvt, "SIM removed", 0);
                } else if (!pvt->flags.sim_startup) {
                    ast_verbose("rgsm: <%s>: SIM NOT INSERTED\n", pvt->name);
                    rgsm_man_event_channel(pvt, "SIM not inserted", 0);
                }

                //gsm_next_sim_search(pvt);
                pvt->mdm_state = MDM_STATE_WAIT_CFUN;
                // end of NOT INSERTED
            }
            else if (!strcasecmp(pvt->recv_buf, "+CPIN: READY")) {
                // - PIN is not checked
                if (!pvt->flags.sim_present) {
                    ast_verbose("rgsm: <%s>: SIM INSERTED\n", pvt->name);
                    rgsm_man_event_channel(pvt, "SIM inserted", 0);
                }
                if (!pvt->flags.changesim) {
                    ast_verbose("rgsm: <%s>: PIN is not checked!!!\n", pvt->name);
                }
                // set SIM present flag
                pvt->flags.sim_startup = 1;
                pvt->flags.sim_present = 1;
                pvt->sim_try_cnt_curr = pvt->sim_try_cnt_conf;

                //Aug 19, 2014
                gsm_query_sim_data(pvt);

                pvt->mdm_state = MDM_STATE_WAIT_CFUN;
                // end of READY
            }
            else if (!strcasecmp(pvt->recv_buf, "+CPIN: SIM ERROR")) {
                // - SIM ERROR
                //
                pvt->mdm_state = MDM_STATE_WAIT_CFUN;
                // end of SIM ERROR
            }
            // end of (pvt->mdm_state == MDM_STATE_WAIT_CFUN)
        }
        else if (pvt->mdm_state == MDM_STATE_WAIT_SUSPEND) {
            //
            if (!strcasecmp(pvt->recv_buf, "+CPIN: NOT READY")) {
                // stop waitsuspend timer
                rgsm_timer_stop(pvt->timers.waitsuspend);
                //
                ast_verbose("<%s>: module switched to suspend state\n", pvt->name);
                //
                pvt->mdm_state = MDM_STATE_SUSPEND;
                //
                pvt->reg_state = REG_STATE_NOTREG_NOSEARCH;
            }
            // end of (pvt->mdm_state == MDM_STATE_WAIT_SUSPEND)
        }
        else if (pvt->mdm_state == MDM_STATE_RUN) {
            if (!strcasecmp(pvt->recv_buf, "+CPIN: NOT READY")) {
                ast_verbose("rgsm: <%s>: SIM NOT READY\n", pvt->name);
                ast_verbose("rgsm: <%s>: restarting channel for next SIM search\n", pvt->name);
                RST_CHANNEL(pvt, RST_DELAY_SIMSEARCH);
                //gsm_next_sim_search(pvt);
            }
        }
        // end of CPIN
    }
    else if (!strcasecmp(pvt->recv_buf, "Call Ready")) {
        ast_verbose("rgsm: <%s>: Call Ready on MDM_STATE=%d\n", pvt->name, pvt->mdm_state);
        // valid if mgmt state WAIT_CALL_READY
        if ((pvt->mdm_state == MDM_STATE_WAIT_CALL_READY) || (pvt->mdm_state == MDM_STATE_CHECK_PIN)) {
            //
            if (pvt->flags.suspend_now) {
                //
                pvt->flags.suspend_now = 0;
                // try to set suspend state
                rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
                //
                pvt->mdm_state = MDM_STATE_WAIT_SUSPEND;
                // start waitsuspend timer
                rgsm_timer_set(pvt->timers.waitsuspend, waitsuspend_timeout);
            } else {
                if (pvt->flags.init ) {
                    // try to set initial settings
                    pvt->mdm_state = MDM_STATE_INIT;
                    // get imsi
                    rgsm_atcmd_queue_append(pvt, AT_CIMI, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                    //Sept 10, 2014
                    // check SIM status to make sure gsm_query_sim_data() called to know iccid
                    if(pvt->module_type == MODULE_TYPE_SIM900) {
                        rgsm_atcmd_queue_append(pvt, AT_SIM900_CSMINS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
                    }

                } else {
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
                }
            }
            // stop callready timer
            rgsm_timer_stop(pvt->timers.callready);
            // stop pinwait timer
            rgsm_timer_stop(pvt->timers.pinwait);
        }
    }
    else if (strstr(pvt->recv_buf, "+CMT:")) {
        if ((str0 = strrchr(pvt->recv_buf, ','))) {
            str0++;
            if (is_str_digit(str0)) {
                pvt->pdu_len = atoi(str0);
                pvt->cmt_pdu_wait = 1;
            }
        }
    }
    else if (strstr(pvt->recv_buf, "*PSSTK:")) {
        //unsolicited STK notifications, asyncronous
        // parse psstrk
        int rc;
        char *tp;
        struct at_psstk_unsol *psstk_unsol;
        if (pvt->stk_capabilities) {
            //
            ast_log(AST_LOG_DEBUG, "rgsm: <%s>: STK Notify from SIM: %s\n", pvt->name, pvt->recv_buf);
            if((tp = strchr(pvt->recv_buf, ' '))) {
                tp++;
                rgsm_man_event_stk_notify(pvt, tp);
            }
        } else {
            psstk_unsol = (struct at_psstk_unsol *)&pvt->parser_buf;
            if ((rc = at_psstk_unsol_parse(pvt->recv_buf, pvt->recv_len, psstk_unsol)) < 0) {
                ast_log(LOG_ERROR, "rgsm: <%s>: at_psstk_unsol_parse() error=%d\n", pvt->name, rc);
            } else {
                ast_verbose("rgsm: <%s>: SIM Toolkit capable: \"%s\", items=%d\n", pvt->name, psstk_unsol->alpha_id, psstk_unsol->number_of_item);
                pvt->stk_capabilities = psstk_unsol->number_of_item;
            }
        }
    }
    else if (strstr(pvt->recv_buf, "+DTMF:")) {
        if ((str0 = strrchr(pvt->recv_buf, ':'))) {
            str0++;
            if (pvt->dtmf_sym) {
                DEBUG_VOICE("rgsm: <%s>: overlap DTMF [%c] skipped\n", pvt->name, *str0);
            } else {
                DEBUG_VOICE("rgsm: <%s>: DTMF [%c] receive\n", pvt->name, *str0);
                pvt->dtmf_sym = *str0;
            }
        }
    }
    else {
        sms_pdu_response_handler(pvt);
    }

    ATP_CLEAR_RECV_BUF(pvt);
}
//end of atp_handle_unsolicited_result()

static const char USSD_RESP_PARSE_ERR[] = "ERROR: USSD response parsing error";
static const char USSD_RESP_EMPTY[] = "ERROR: empty response";
static const char USSD_RESP_NOT_PRESENT[] = "ERROR: response can't be presented";
static const char USSD_RESP_BAD[] = "ERROR: bad response";
static const char USSD_REQ_BAD[] = "ERROR: bad request";

//!static functions impl
void ussd_handler(struct gsm_pvt *pvt)
{
    char *tp;
    struct at_gen_cusd_write *cusd_wr = (struct at_gen_cusd_write *)&pvt->parser_buf;

    if(!strstr(pvt->recv_buf, "+CUSD: ")){
        if(strstr(pvt->recv_buf, "ERROR")){
	    pvt->ussd_done = 1;
    	    if (pvt->ussd_subcmd == SUBCMD_CUSD_MANAGER || pvt->ussd_subcmd == SUBCMD_CUSD_GET_BALANCE) {
                pvt->call_state = CALL_STATE_NULL;
	    }
    	    return;
        } else if (strstr(pvt->recv_buf, "OK")) {
            //do nothing, the rest of uusd response will be processed later as unsolicite response
            return;
	}
	return;
    }

    //assume pvt->recv_buf contains "+CUSD"

    pvt->ussd_done = 1;
    memset(pvt->ussd_databuf, 0, sizeof(pvt->ussd_databuf));

    if (at_gen_cusd_write_parse(pvt->recv_buf, pvt->recv_len, cusd_wr) < 0) {
        ast_log(LOG_ERROR, "<%s>: at_gen_cusd_write_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
        pvt->recv_buf[0] = '\0';
        pvt->recv_buf_valid = 0;
        pvt->recv_len = 0;
        //
        //pvt->balance_str[0] = '\0';
        //
        ast_copy_string(pvt->ussd_databuf, USSD_RESP_PARSE_ERR, sizeof(pvt->ussd_databuf));
        pvt->ussd_datalen = strlen(pvt->ussd_databuf);
    } else {
        if (cusd_wr->str_len > 0) {
            //
            if ((tp = get_ussd_decoded(cusd_wr->str, cusd_wr->str_len, cusd_wr->dcs))) {
                //ast_copy_string(pvt->balance_str, tp, sizeof(pvt->balance_str));
                ast_copy_string(pvt->ussd_databuf, tp, sizeof(pvt->ussd_databuf));
                free(tp);
                if((cusd_wr->n == 1) && (pvt_config.autoclose_ussd_menu != 0))
                    	rgsm_atcmd_queue_append(pvt, AT_CUSD, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "2");
            } else {
                ast_copy_string(pvt->ussd_databuf, USSD_RESP_BAD, sizeof(pvt->ussd_databuf));
            }

            pvt->ussd_dcs = (cusd_wr->dcs >= 0)?(cusd_wr->dcs):(0);
            pvt->ussd_valid = 1;
        } else {
            if (cusd_wr->n == 0) {
                ast_copy_string(pvt->ussd_databuf, USSD_RESP_EMPTY, sizeof(pvt->ussd_databuf));
            } else if (cusd_wr->n == 1) {
                ast_copy_string(pvt->ussd_databuf, USSD_RESP_NOT_PRESENT, sizeof(pvt->ussd_databuf));
            }
            else if (cusd_wr->n == 2) {
                ast_copy_string(pvt->ussd_databuf, USSD_REQ_BAD, sizeof(pvt->ussd_databuf));
            }
        }
    }

    pvt->ussd_datalen = strlen(pvt->ussd_databuf);

    switch (pvt->ussd_subcmd) {
        case SUBCMD_CUSD_GET_BALANCE:
            if (pvt->ussd_valid) {
                ast_copy_string(pvt->balance_str, pvt->ussd_databuf, sizeof(pvt->balance_str));
            } else {
                pvt->balance_str[0] = '\0';
            }
            pvt->call_state = CALL_STATE_NULL;
            break;
        case SUBCMD_CUSD_MANAGER:
            rgsm_man_event_new_ussd(pvt->name, pvt->ussd_databuf);
            pvt->call_state = CALL_STATE_NULL;
            break;
        case SUBCMD_CUSD_USER:
            break;
        default:
            break;
    }
}





