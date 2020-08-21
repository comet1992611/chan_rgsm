#include "rgsm_sim300.h"

void sim300_atp_handle_response(struct gsm_pvt* pvt)
{
    if (!pvt->cmd_queue.first) return;

    ast_verbose("rgsm: SIM300 module not supported now\n");

/*
    // select by operation
    if(pvt->cmd_queue.first->oper == AT_OPER_EXEC){
        // EXEC operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_SIM300_CCID:
                if(is_str_xdigit(pvt->recv_buf)){
                    ast_copy_string(pvt->iccid, pvt->recv_buf, sizeof(pvt->iccid));
                    //
                    if(pvt->flags.changesim){
                        if(strcmp(pvt->iccid, pvt->iccid_ban)){
                            // new SIM
                            pvt->flags.changesim = 0;
// 											pvt->flags.sim_present = 0;
                            pvt->flags.testsim = 0;
                            }
                        else{
                            // old SIM
                            if(pvt->flags.testsim){
                                ast_verbose("eggsm: <%s>: this SIM card used all registration attempts and already inserted\n", pvt->name);
                                pvt->flags.testsim = 0;
                                }
                            }
                        }
                    }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
            }
        }
    else if(pvt->cmd_queue.first->oper == AT_OPER_TEST){
        // TEST operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
            }
        }
    else if(pvt->cmd_queue.first->oper == AT_OPER_READ){
        // READ operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_SIM300_CMIC:
                if(strstr(pvt->recv_buf, "+CMIC:")){
                    pvt->parser_ptrs.sim300_cmic_rd = (struct at_sim300_cmic_read *)&pvt->parser_buf;
                    if(at_sim300_cmic_read_parse(pvt->recv_buf, pvt->recv_len, pvt->parser_ptrs.sim300_cmic_rd) < 0){
                        // parsing error
                        ast_log(LOG_ERROR, "<%s>: at_sim300_cmic_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                        // check for query
                        if(pvt->querysig.gainout){
                            ast_verbose("eggsm: <%s>: qwery(gainout): error\n", pvt->name);
                            pvt->querysig.gainout = 0;
                            }
                        }
                    else{
                        pvt->gainout_curr = pvt->parser_ptrs.sim300_cmic_rd->main_mic;
                        // check for query
                        if(pvt->querysig.gainout){
                            ast_verbose("eggsm: <%s>: qwery(gainout): %d\n", pvt->name, pvt->gainout_curr);
                            pvt->querysig.gainout = 0;
                            }
                        }
                    }
                else if(strstr(pvt->recv_buf, "ERROR")){
                    // check for query
                    if(pvt->querysig.gainout){
                        ast_verbose("eggsm: <%s>: qwery(gainout): error\n", pvt->name);
                        pvt->querysig.gainout = 0;
                        }
                    }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_SIM300_CSMINS:
                if(strstr(pvt->recv_buf, "+CSMINS:")){
                    // parse csmins
                    pvt->parser_ptrs.sim300_csmins_rd = (struct at_sim300_csmins_read *)&pvt->parser_buf;
                    if(at_sim300_csmins_read_parse(pvt->recv_buf, pvt->recv_len, pvt->parser_ptrs.sim300_csmins_rd) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_sim300_csmins_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                        }
                    else{
                        if(pvt->parser_ptrs.sim300_csmins_rd->sim_inserted != pvt->flags.sim_present){
                            ast_verbose("eggsm: <%s>: SIM %s\n", pvt->name, (pvt->parser_ptrs.sim300_csmins_rd->sim_inserted)?("inserted"):("removed"));
                            if(!pvt->parser_ptrs.sim300_csmins_rd->sim_inserted){
                                // reset channel phone number
                                ast_copy_string(pvt->subscriber_number.value, "unknown", MAX_ADDRESS_LENGTH);
                                pvt->subscriber_number.length = strlen(pvt->subscriber_number.value);
                                pvt->subscriber_number.type.bits.reserved = 1;
                                pvt->subscriber_number.type.bits.numbplan = NUMBERING_PLAN_UNKNOWN;
                                pvt->subscriber_number.type.bits.typenumb = TYPE_OF_NUMBER_SUBSCRIBER;
                                // reset SMS center number
                                ast_copy_string(pvt->smsc_number.value, "unknown", MAX_ADDRESS_LENGTH);
                                pvt->smsc_number.length = strlen(pvt->smsc_number.value);
                                pvt->smsc_number.type.bits.reserved = 1;
                                pvt->smsc_number.type.bits.numbplan = NUMBERING_PLAN_UNKNOWN;
                                pvt->smsc_number.type.bits.typenumb = TYPE_OF_NUMBER_SUBSCRIBER;
                                //
                                ast_copy_string(pvt->operator_name, "unknown", sizeof(pvt->operator_name));
                                ast_copy_string(pvt->operator_code, "unknown", sizeof(pvt->operator_code));
                                ast_copy_string(pvt->imsi, "unknown", sizeof(pvt->imsi));
                                ast_copy_string(pvt->iccid, "unknown", sizeof(pvt->iccid));
                                // reset attempts count
                                pvt->reg_try_cnt_curr = pvt->reg_try_cnt_conf;
                                if(pvt->flags.changesim)
                                    pvt->flags.testsim = 1;
                                // start simpoll timer
                                if(pvt->mgmt_state != CH_MGMT_ST_SUSPEND)
                                    rgsm_timer_set(pvt->timers.simpoll, simpoll_timeout);
                                }
                            }
                        pvt->flags.sim_present = pvt->parser_ptrs.sim300_csmins_rd->sim_inserted;
                        }
                    }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
            }
        }
    else if(pvt->cmd_queue.first->oper == AT_OPER_WRITE){
        // WRITE operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_SIM300_CMIC:
                if(strstr(pvt->recv_buf, "OK"))
                    pvt->gainout_curr = pvt->gainout_conf;
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
            }
        }
*/
}
