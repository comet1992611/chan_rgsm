#ifndef RGSM_CALL_SM_H_INCLUDED
#define RGSM_CALL_SM_H_INCLUDED

#include "chan_rgsm.h"
#include "rgsm_defs.h"

#if ASTERISK_VERSION_NUM < 10800
struct ast_channel *init_call(call_dir_t dir, struct gsm_pvt *pvt, int *format, int *cause);
#else
struct ast_channel *init_call(call_dir_t dir, struct gsm_pvt *pvt, format_t *format, int *cause);
#endif

int call_sm(struct gsm_pvt* pvt, call_msg_t msg, int cause);

#endif // RGSM_CALL_SM_H_INCLUDED
