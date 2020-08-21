#ifndef RGSM_AT_PROCESSOR_H_INCLUDED
#define RGSM_AT_PROCESSOR_H_INCLUDED

#include "chan_rgsm.h"

#define ATP_CLEAR_RECV_BUF(pvt) \
do { \
    pvt->recv_buf[0] = '\0'; \
    pvt->recv_ptr = pvt->recv_buf; \
    pvt->recv_len = 0; \
    pvt->recv_buf_valid = 0; \
    pvt->recv_en = 0; \
} while(0)

void atp_handle_pdu_prompt(struct gsm_pvt* pvt);
int atp_check_cmd_expired(struct gsm_pvt* pvt);
void atp_handle_response(struct gsm_pvt* pvt);
void atp_handle_unsolicited_result(struct gsm_pvt* pvt);

#endif // RGSM_AT_PROCESSOR_H_INCLUDED
