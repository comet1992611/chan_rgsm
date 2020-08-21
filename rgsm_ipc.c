//Jul 6, 2014: IPC interface between rgsm and simsimulator

//#include <asterisk/logger.h>
#include "rgsm_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int ipc_exchange_msg(char *request, char **reply_buf, int reply_buf_len) {
    int sfd;
	size_t reply_len;
    struct sockaddr_un remote;
	int ret = -1;

    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return -1;
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, IPC_PIPE_NAME);
    //int len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(sfd, (struct sockaddr *)&remote, sizeof(struct sockaddr_un)) == -1) {
        goto cleanup_;
    }

    if (send(sfd, request, strlen(request), MSG_NOSIGNAL) == -1) {
        goto cleanup_;
    }

	ret = 0;

	if (*reply_buf != NULL && reply_buf_len) {
		//recv will block, be careful by suppling non null *reply_buf for protocol request that does not expect a response
		reply_len=recv(sfd, *reply_buf, reply_buf_len-1, 0);
		if (reply_len > 0) {
			*(*reply_buf + reply_len) = '\0';
		} else {
			ret = -1;
		}
	}

cleanup_:
    close(sfd);
	return ret;
}

void ipc_propagate_gateway_uid(struct gateway *gw, int log_on_fail) {
	unsigned char bus;
	char msg[60];
	char *resp = msg;
	int succ = 0;

	bus = ggw8_get_device_sysid(gw->ggw8_device) >> 8;
	//request="PING GWUID=xxx BUS=yyy"
	sprintf(msg, "%s %s=%d %s=%u", IPC_CMD_PING, IPC_ARG_GWUID, gw->uid, IPC_ARG_BUS, bus);

	//exchange message with simulator, if simulator down then following call will fail
	if (ipc_exchange_msg(msg, &resp, sizeof(msg)) == 0) {
		//simulator ipc is up but usb fuction may be un-enumerated, in this case reponse will be NODEVICE
		//we are interesting for "OK" or "NOTCHANGE"
		succ = !strcmp(resp, IPC_RET_OK) || !strcmp(resp, IPC_RET_NOTCHANGE);
		if (!strcmp(resp, IPC_RET_OK)) {
			//"OK"
			ast_verbose("rgsm: gateway's UID=%d on BUS=%.3u propagated to simsimulator\n", gw->uid, (bus));
		}
	}

	gw->uid_propagated = succ;

	if (!succ && log_on_fail) {
		ast_verbose("rgsm: gateway's UID=%d on BUS=%.3u not propagated to simsimulator, will try later\n", gw->uid, bus);
	}
}
