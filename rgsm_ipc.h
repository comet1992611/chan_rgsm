#ifndef RGSM_IPC_H_INCLUDED
#define RGSM_IPC_H_INCLUDED

#include "chan_rgsm.h"

#define IPC_PIPE_NAME "/tmp/rgsm-ipc-simulator"

//"PING GWUID=xxx BUS=yyy" -> "[OK]|[NOTCHANGE]|[NODEVICE]|[ERROR]" 

#define IPC_CMD_PING "PING"

#define IPC_ARG_BUS "BUS"
#define IPC_ARG_GWUID "GWUID"

#define IPC_RET_ERROR "ERROR"
#define IPC_RET_OK "OK"
#define IPC_RET_NOTCHANGE "NOTCHANGE"
#define IPC_RET_NODEVICE "NODEVICE"


/**
 * Exchange messages with simsimulator
 * msg - the message to send
 * reply_buf - optional buffer to receive response
 * reply_buf_len - length of reply_buf
 * returns -1 on error or 0 on success
 *
 * To avoid infinite blocks the reply_buf must be null for protocol request that does not expect a response
 */
int ipc_exchange_msg(char *request, char **reply_buf, int reply_buf_len);

void ipc_propagate_gateway_uid(struct gateway *gw, int log_on_fail);

#endif //RGSM_IPC_H_INCLUDED
