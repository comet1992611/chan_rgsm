#include "rgsm_call_sm.h"
#include "rgsm_utilities.h"
#include "rgsm_sim900.h"
#include <asterisk/causes.h>
#include <asterisk/pbx.h>
#include "rgsm_manager.h"

extern gen_config_t     gen_config;

#define LOCK_AST_CHNL() do { \
	while(pvt->owner && ast_channel_trylock(pvt->owner)) { \
        ast_mutex_unlock(&pvt->lock); \
        ast_mutex_unlock(&rgsm_lock); \
        usleep(1); \
        ast_mutex_lock(&rgsm_lock); \
        ast_mutex_lock(&pvt->lock); \
	} \
} while (0)

//queues a control_frame_type and marks pvt's hangupcause
#define AST_QUEUE_CONTROL_CAUSE(ast_cft) do { \
    ast_mutex_unlock(&pvt->lock); \
    ast_mutex_unlock(&rgsm_lock); \
    ast_queue_control(pvt->owner, ast_cft); \
    ast_channel_hangupcause_set(pvt->owner, cause); \
    ast_mutex_lock(&rgsm_lock); \
    ast_mutex_lock(&pvt->lock); \
    ast_channel_unlock(pvt->owner); \
} while (0)

//queues a control_frame_type and marks pvt's hangupcause
#define AST_QUEUE_CONTROL(ast_cft) do { \
    ast_mutex_unlock(&pvt->lock); \
    ast_mutex_unlock(&rgsm_lock); \
    ast_queue_control(pvt->owner, ast_cft); \
    ast_mutex_lock(&rgsm_lock); \
    ast_mutex_lock(&pvt->lock); \
    ast_channel_unlock(pvt->owner); \
} while (0)

#define CALL_ST_NULL(arc) do { \
    pvt->owner = NULL; \
    pvt->call_state = CALL_STATE_NULL; \
    rc = arc; \
} while (0)

//allocates new ast_channel, rgsm_lock and pvt_lock mutexes already locked
#if ASTERISK_VERSION_NUM < 10800
struct ast_channel *init_call(call_dir_t dir, struct gsm_pvt *pvt, int *format, int *cause)
#else
struct ast_channel *init_call(call_dir_t dir, struct gsm_pvt *pvt, format_t *format, int *cause)
#endif
{
    struct ast_channel *ast_chnl = NULL;
    int native;
   	char calling[MAX_ADDRESS_LENGTH];
	char called[MAX_ADDRESS_LENGTH];

    if (cause) *cause = 0;

	// check cahnnel private data
	if(!pvt){
		ast_log(LOG_ERROR, "invalid channel pointer\n");
		return NULL;
    }

    if (dir == CALL_DIR_NONE) {
        ast_log(LOG_ERROR, "<%s>: call has wrong direction\n", pvt->name);
        return NULL;
    }

	// check for context
	if( dir == CALL_DIR_IN && !strlen(pvt->chnl_config.context)){
		ast_log(LOG_ERROR, "<%s>: call without context\n", pvt->name);
		return NULL;
    }

	pvt->event_is_now_recv_begin = 0;
	pvt->event_is_now_recv_end = 0;
	//pvt->event_recv_seq_num = 0;
	pvt->event_is_now_send = 0;

	// init session statistic counter
	pvt->send_sid_curr = 0;
	pvt->send_drop_curr = 0;
	pvt->send_frame_curr = 0;
	pvt->recv_frame_curr = 0;

	// init start call time mark
	pvt->start_call_time_mark.tv_sec = 0;
	//pvt->config.reg_try_cnt = DEFAULT_REG_TRY_CNT;

    //!use supported codes read from device instead of hardcoded in driver
	native = pvt_config.voice_codec == 0 ? ggw8_get_device_info(pvt->ggw8_device)->supported_codecs : pvt_config.voice_codec;

    if (format) {
        if(native & *format) {
            native &= *format;
        } else {
            ast_log(AST_LOG_ERROR, "<%s>: natively supported codecs=0x%.4x but requested 0x%.4x\n",
                    pvt->name,
                    native,
                    (uint16_t)(*format));

            if (cause) *cause = AST_CAUSE_BEARERCAPABILITY_NOTIMPL;
            pvt->owner = NULL;
            ast_mutex_unlock(&pvt->lock);
            ast_mutex_unlock(&rgsm_lock);
            return NULL;
        }
    }

	// get audio payload type for requested frame format
	// pvt->frame_format = ast_best_codec(native);
	pvt->frame_format = VC_G729A;

	pvt->voice_fd = ggw8_open_voice(pvt->ggw8_device, pvt->modem_id, (uint16_t)pvt->frame_format, gen_config.dev_jbsize);
    if (pvt->voice_fd == -1) {
        ast_log(AST_LOG_ERROR, "<%s>: can't open voice channel\n", pvt->name);
        *cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
        pvt->owner = NULL;
        ast_mutex_unlock(&pvt->lock);
        ast_mutex_unlock(&rgsm_lock);
        return NULL;
    } else {
        ast_verb(3, "<%s>: open voice channel voice_fd=%d\n", pvt->name, pvt->voice_fd);
    }

	// prevent deadlock while asterisk channel is allocating
	ast_mutex_unlock(&pvt->lock);
	ast_mutex_unlock(&rgsm_lock);

	if (dir == CALL_DIR_IN) {
        sprintf(calling, "%s%s", (pvt->calling_number.type.full == 145)?("+"):(""), pvt->calling_number.value);
        sprintf(called, "%s%s", (pvt->called_number.type.full == 145)?("+"):(""), pvt->called_number.value);
	} else {
	    calling[0] = '\0';
	    called[0] = '\0';
	}

	// allocation channel in pbx spool
	ast_chnl = ast_channel_alloc(1,						                                /* int needqueue */
								dir == CALL_DIR_IN ? AST_STATE_RING : AST_STATE_DOWN,	/* int state */
								dir == CALL_DIR_IN ? calling : "",	    		        /* const char *cid_num */
								dir == CALL_DIR_IN ? calling : "",				        /* const char *cid_name */
								NULL,					                                /* const char *acctcode */
								dir == CALL_DIR_IN ? called : "",					    /* const char *exten */

								dir == CALL_DIR_IN ? pvt->chnl_config.context : "",	    /* const char *context */
#if ASTERISK_VERSION_NUM >= 10800
								NULL,				                                    /* const char *linkedid */
#endif
								pvt->owner,
								0,						                                /* const int amaflag */
								"RGSM/%s-%08x",		                                    /* const char *name_fmt, ... */
								pvt->name, channel_id);

	// lock rgsm subsystem
	ast_mutex_lock(&rgsm_lock);
	// lock pvt channel
	ast_mutex_lock(&pvt->lock);

	// increment channel ID
	channel_id++;

	// fail allocation channel
	if(!ast_chnl){
		ast_log(LOG_ERROR, "can't allocate channel structure\n");
        *cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
        if (pvt->voice_fd != -1) {
            ast_verb(3, "<%s>: close voice channel\n", pvt->name);
            ggw8_close_voice(pvt->ggw8_device, pvt->modem_id);
            pvt->voice_fd = -1;
        }

        pvt->owner = NULL;
        ast_mutex_unlock(&pvt->lock);
        ast_mutex_unlock(&rgsm_lock);
        return NULL;
    }

	// this is written by Roman
	ast_channel_tech_set(ast_chnl, &rgsm_tech);
	ast_channel_nativeformats_set(ast_chnl, pvt->frame_format);
	// ast_channel_rawreadformats_set(ast_chnl, pvt->frame_format);
	// ast_channel_rawwriteformats_set(ast_chnl, pvt->frame_format);
	// ast_channel_writeformats_set(ast_chnl, pvt->frame_format);
	// ast_channel_readformats_set(ast_chnl, pvt->frame_format);
	ast_channel_tech_pvt_set(ast_chnl, pvt);

	// init asterisk channel tag's
	// ast_chnl->tech = &rgsm_tech;

	// ast_chnl->nativeformats = pvt->frame_format; //native;
	// ast_chnl->rawreadformat = pvt->frame_format;
	// ast_chnl->rawwriteformat = pvt->frame_format;
	// ast_chnl->writeformat = pvt->frame_format;
	// ast_chnl->readformat = pvt->frame_format;

	// ast_chnl->tech_pvt = pvt;

    //moved to rgsm_devicestate
	//ast_channel_set_fd(ast_chnl, 0, pvt->voice_fd);

	// set channel language (written by comet)
	// ast_string_field_set(ast_chnl, language, "en");

	// set owner -- for busy indication
	pvt->owner = ast_chnl;
    pvt->call_dir = dir;
	pvt->active_line = 0;
	pvt->wait_line = 0;
	pvt->wait_line_num[0] = '\0';
	pvt->is_play_tone = 0;

	ast_jb_configure(ast_chnl, &gen_config.jbconf);

    ast_channel_set_fd(pvt->owner, 0, pvt->voice_fd);


//	pvt->dtmfptr = pvt->dtmfbuf;
//	*pvt->dtmfptr = '\0';

    if (dir == CALL_DIR_IN) {
        //for outgoing calls the calling number is unknown now
        ast_verb(2, "rgsm: <%s> incoming call \"%s\" -> \"%s\" using codec \"%ld\"\n",
              pvt->name, calling, called, pvt->frame_format);
    }

    pvt->man_chstate = (dir == CALL_DIR_IN) ? MAN_CHSTATE_INCOMING_CALL : MAN_CHSTATE_OUTGOING_CALL;
    rgsm_man_event_channel_state(pvt);

    return ast_chnl;
}


int call_sm(struct gsm_pvt* pvt, call_msg_t msg, int cause)
{
	struct timeval time_mark;
	//
	int rc = -1;

	if(!pvt){
		ast_log(LOG_ERROR, "<pvt->name>: fail chnl\n");
		return rc;
    }

	switch(pvt->call_state){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CALL_STATE_NULL:
            DEBUG_CALL_SM("<%s>: state NULL\n", pvt->name);
			switch(msg){
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_SETUP_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg > SETUP_REQ\n", pvt->name);
					/*if(pvt->module_type == MODULE_TYPE_SIM300)
						rgsm_atcmd_queue_append(pvt, AT_SIM300_CHFA, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 0);
					else*/
					if(pvt->functions.setup_audio_channel != NULL)
					    pvt->functions.setup_audio_channel(pvt);

					// Send ATD
					if(rgsm_atcmd_queue_append(pvt, AT_D, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, "%s%s;",
                                (pvt->called_number.type.full == 145)?("+"):(""), pvt->called_number.value) < 0){
                        LOCK_AST_CHNL();
						if(pvt->owner){
						    AST_QUEUE_CONTROL_CAUSE(AST_CONTROL_CONGESTION);
                        }
						rc = -1;
						break;
                    }
					pvt->call_state = CALL_STATE_OUT_CALL_PROC;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_SETUP_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg < SETUP_IND\n", pvt->name);
					//
					pvt->call_state = CALL_STATE_OVERLAP_RECEIVING;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg RELEASE_REQ >\n", pvt->name);
					//
					if(pvt->functions.hangup != NULL) 
    					    pvt->functions.hangup(pvt);
    					else
    					    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
					//
					pvt->call_state = CALL_STATE_NULL;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg RELEASE_IND <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL_CAUSE(AST_CONTROL_HANGUP);
					} else {
					    CALL_ST_NULL(-1);
						break;
                    }

					pvt->call_state = CALL_STATE_RELEASE_IND;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				default:
					ast_log(LOG_WARNING, "<%s>: state NULL unrecognized msg <%d>\n", pvt->name, msg);
					rc = -1;
					break;
            }
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CALL_STATE_OUT_CALL_PROC:
			DEBUG_CALL_SM("<%s>: state OUT_CALL_PROC\n", pvt->name);
			switch(msg){
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_PROCEEDING_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg PROCEEDINGING_IND <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
                    if(pvt->owner){
                        AST_QUEUE_CONTROL(AST_CONTROL_PROCEEDING);
					} else {
					    if(pvt->functions.hangup != NULL) 
						pvt->functions.hangup(pvt);
					    else
						rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

                        CALL_ST_NULL(-1);
						break;
                    }
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_ALERTING_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg ALERTING_IND <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
						// unlock rgsm subsystem
						ast_mutex_unlock(&pvt->lock);
						// unlock rgsm subsystem
						ast_mutex_unlock(&rgsm_lock);
						// send alerting indiacation
						/*if(!pvt->alertmedia){
							ast_queue_control(pvt->owner, AST_CONTROL_RINGING);
							if(pvt->owner->_state != AST_STATE_UP)
								ast_setstate(pvt->owner, AST_STATE_RINGING);
							}
						else*/
							ast_queue_control(pvt->owner, AST_CONTROL_PROGRESS);

						// lock rgsm subsystem
						ast_mutex_lock(&rgsm_lock);
						// lock rgsm subsystem
						ast_mutex_lock(&pvt->lock);

						ast_channel_unlock(pvt->owner);
                    }
					else{
						if(pvt->functions.hangup != NULL) 
						    pvt->functions.hangup(pvt);
						else
						    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

                        CALL_ST_NULL(-1);
						break;
                    }
					//
					pvt->call_state = CALL_STATE_CALL_DELIVERED;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_SETUP_CONFIRM: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM( "<%s>: msg SETUP_CONFIRM <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL(AST_CONTROL_ANSWER);
					} else {
						if(pvt->functions.hangup != NULL) 
						    pvt->functions.hangup(pvt);
						else
						    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

                        CALL_ST_NULL( -1);
						break;
                    }
					// get mark of time at call started
					gettimeofday(&pvt->start_call_time_mark, NULL);
					//
					pvt->call_state = CALL_STATE_ACTIVE;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg RELEASE_REQ >\n", pvt->name);
					//
					if(pvt->functions.hangup != NULL) 
					    pvt->functions.hangup(pvt);
					else
					    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
					//
					pvt->call_state = CALL_STATE_NULL;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg RELEASE_IND <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL_CAUSE(AST_CONTROL_HANGUP);
					} else {
                        CALL_ST_NULL(-1);
						break;
                    }

					pvt->call_state = CALL_STATE_RELEASE_IND;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				default:
					ast_log(LOG_WARNING, "<%s>: state OUT_CALL_PROC unrecognized msg <%d>\n", pvt->name, msg);
					rc = -1;
					break;
            }
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CALL_STATE_CALL_DELIVERED:
			DEBUG_CALL_SM( "<%s>: state CALL_DELIVERED\n", pvt->name);
			switch(msg){
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_SETUP_CONFIRM: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg SETUP_CONFIRM <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL(AST_CONTROL_ANSWER);
					} else {
						if(pvt->functions.hangup != NULL) 
						    pvt->functions.hangup(pvt);
						else
						    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

                        CALL_ST_NULL(-1);
						break;
                    }
					// get mark of time at call started
					gettimeofday(&pvt->start_call_time_mark, NULL);
					//
					pvt->call_state = CALL_STATE_ACTIVE;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg RELEASE_REQ >\n", pvt->name);
					//
					if(pvt->functions.hangup != NULL) 
					    pvt->functions.hangup(pvt);
					else
					    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
					//
					pvt->call_state = CALL_STATE_NULL;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg RELEASE_IND <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL_CAUSE(AST_CONTROL_HANGUP);
					} else {
                        CALL_ST_NULL(-1);
						break;
                    }
					pvt->call_state = CALL_STATE_RELEASE_IND;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				default:
					ast_log(LOG_WARNING, "<%s>: state CALL_DELIVERED unrecognized msg <%d>\n", pvt->name, msg);
					rc = -1;
					break;
            }
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CALL_STATE_CALL_RECEIVED:
			DEBUG_CALL_SM("<%s>: state CALL_RECEIVED\n", pvt->name);
			switch(msg){
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_SETUP_RESPONSE: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg SETUP_RESPONSE >\n", pvt->name);
					// get mark of time at call started
					gettimeofday(&pvt->start_call_time_mark, NULL);
					// send ATA
					rgsm_atcmd_queue_append(pvt, AT_A, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
					//
					pvt->call_state = CALL_STATE_ACTIVE;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg RELEASE_REQ >\n", pvt->name);
					//
					if(pvt->functions.hangup != NULL) 
					    pvt->functions.hangup(pvt);
					else
					    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
					//
					pvt->call_state = CALL_STATE_NULL;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg RELEASE_IND <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL_CAUSE(AST_CONTROL_HANGUP);
                    } else {
                        CALL_ST_NULL(-1);
						break;
                    }

					pvt->call_state = CALL_STATE_RELEASE_IND;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				default:
					ast_log(LOG_WARNING, "<%s>: state CALL_RECEIVED unrecognized msg <%d>\n", pvt->name, msg);
					rc = -1;
					break;
            }
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CALL_STATE_ACTIVE:
			DEBUG_CALL_SM( "<%s>: state ACTIVE\n", pvt->name);
			switch(msg){
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg RELEASE_REQ >\n", pvt->name);
					//
					if(pvt->functions.hangup != NULL) 
					    pvt->functions.hangup(pvt);
					else
					    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
					//
					if(pvt->start_call_time_mark.tv_sec){
						gettimeofday(&time_mark, NULL);
						//
						if(pvt->call_dir == CALL_DIR_OUT){
							//
							pvt->last_time_outgoing = time_mark.tv_sec - pvt->start_call_time_mark.tv_sec;
							pvt->call_time_outgoing += pvt->last_time_outgoing;
							//
                        } else if(pvt->call_dir == CALL_DIR_IN){
							//
							pvt->last_time_incoming = time_mark.tv_sec - pvt->start_call_time_mark.tv_sec;
							pvt->call_time_incoming += pvt->last_time_incoming;
							//
						} else {
							ast_log(LOG_WARNING, "<%s>: call has't unknown direction\n", pvt->name);
						}
                    } else {
						ast_log(LOG_WARNING, "<%s>: call has't start time mark\n", pvt->name);
                    }

					pvt->call_dir = CALL_DIR_NONE;

					pvt->call_state = CALL_STATE_NULL;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg RELEASE_IND <\n", pvt->name);
					//
					if(pvt->start_call_time_mark.tv_sec){
						gettimeofday(&time_mark, NULL);
						//
						if(pvt->call_dir == CALL_DIR_OUT){
							//
							pvt->last_time_outgoing = time_mark.tv_sec - pvt->start_call_time_mark.tv_sec;
							pvt->call_time_outgoing += pvt->last_time_outgoing;
							//
                        } else if(pvt->call_dir == CALL_DIR_IN){
							//
							pvt->last_time_incoming = time_mark.tv_sec - pvt->start_call_time_mark.tv_sec;
							pvt->call_time_incoming += pvt->last_time_incoming;
							//
                        } else{
							ast_log(LOG_WARNING, "<%s>: call has't start time mark\n", pvt->name);
							}
                    } else {
						ast_log(LOG_WARNING, "<%s>: call has't unknown direction\n", pvt->name);
                    }
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL_CAUSE(AST_CONTROL_HANGUP);
					} else {
                        CALL_ST_NULL(-1);
						break;
                    }

					pvt->call_state = CALL_STATE_RELEASE_IND;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				default:
					ast_log(LOG_WARNING, "<%s>: state ACTIVE unrecognized msg <%d>\n", pvt->name, msg);
					rc = -1;
					break;
            }
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CALL_STATE_RELEASE_IND:
			DEBUG_CALL_SM("<%s>: state RELEASE_IND\n", pvt->name);
			switch(msg){
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg RELEASE_REQ >\n", pvt->name);
					//
                    CALL_ST_NULL(0);
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				default:
					ast_log(LOG_WARNING, "<%s>: state RELEASE_IND unrecognized msg <%d>\n", pvt->name, msg);
					rc = -1;
					break;
				}
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case CALL_STATE_OVERLAP_RECEIVING:
			DEBUG_CALL_SM("<%s>: state CALL_STATE_OVERLAP_RECEIVING\n", pvt->name);
			switch(msg){
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_INFO_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg INFO_IND <\n", pvt->name);
					// get called name
					// set default extension
					address_classify("s", &pvt->called_number);
					// check incoming type
					if(pvt->chnl_config.incoming_type == INC_TYPE_SPEC){
						// route incoming call to specified extension
						address_classify(pvt->chnl_config.incomingto, &pvt->called_number);
                    }
                    else if(pvt->chnl_config.incoming_type == INC_TYPE_DYN){
						// incoming call dynamic routed
						gettimeofday(&time_mark, NULL);
                    } // end of dyn section
					else if(pvt->chnl_config.incoming_type == INC_TYPE_DENY){
						// incoming call denied
						ast_verb(2, "rgsm: <%s>: call from \"%s%s\" denied\n", pvt->name,
                            (pvt->calling_number.type.full == 145)?("+"):(""), pvt->calling_number.value);
						// hangup
						if(pvt->functions.hangup != NULL) 
						    pvt->functions.hangup(pvt);
						else
						    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

                        CALL_ST_NULL( 0);
						break;
                    }
					else {
						// incoming call unknown type
						ast_verbose("rgsm: <%s>: unknown type of incoming call\n", pvt->name);
						// hangup
						if(pvt->functions.hangup != NULL) 
						    pvt->functions.hangup(pvt);
						else
						    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

                        CALL_ST_NULL(0);
						break;
                    }

					// alloc channel in pbx spool
					if (!init_call(CALL_DIR_IN, pvt, NULL, NULL)) {
						// error - hangup
						if(pvt->functions.hangup != NULL) 
						    pvt->functions.hangup(pvt);
						else
						    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);

                        CALL_ST_NULL(-1);
						break;
					}
					// start pbx
					if(ast_pbx_start(pvt->owner)){
						ast_log(LOG_ERROR, "<%s>: unable to start pbx on incoming call\n", pvt->name);
						ast_hangup(pvt->owner);
						rc = -1;
						break;
                    }

					// prevent leak real audio channel
					/*if(pvt->module_type == MODULE_TYPE_SIM300)
						rgsm_atcmd_queue_append(pvt, AT_SIM300_CHFA, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 0);
					else*/
					if(pvt->functions.setup_audio_channel != NULL)
					    pvt->functions.setup_audio_channel(pvt);

					pvt->call_state = CALL_STATE_CALL_RECEIVED;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_REQ: // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
					DEBUG_CALL_SM("<%s>: msg RELEASE_REQ >\n", pvt->name);
					//
					if(pvt->functions.hangup != NULL) 
					    pvt->functions.hangup(pvt);
					else
					    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
					//
					pvt->call_state = CALL_STATE_NULL;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				case CALL_MSG_RELEASE_IND: // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
					DEBUG_CALL_SM("<%s>: msg RELEASE_IND <\n", pvt->name);
					//
                    LOCK_AST_CHNL();
					if(pvt->owner){
					    AST_QUEUE_CONTROL_CAUSE(AST_CONTROL_HANGUP);
					} else {
                        CALL_ST_NULL(-1);
						break;
                    }
					pvt->call_state = CALL_STATE_RELEASE_IND;
					rc = 0;
					break;
				//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				default:
					ast_log(LOG_WARNING, "<%s>: state CALL_STATE_OVERLAP_RECEIVING unrecognized msg <%d>\n", pvt->name, msg);
					rc = -1;
					break;
            }
			break;
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default:
			ast_log(LOG_WARNING, "<%s>: unknown state=<%d> message=<%d> \n", pvt->name, pvt->call_state, msg);
			rc = -1;
			break;
    }
	//
	return rc;
}
