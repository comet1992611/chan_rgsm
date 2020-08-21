#ifndef RGSM_APP_H_INCLUDED
#define RGSM_APP_H_INCLUDED

#ifdef __APP__

#include <asterisk.h>

extern char* app_status;
extern char* app_status_synopsis;
extern char* app_status_desc;

extern char* app_send_sms;
extern char* app_send_sms_synopsis;
extern char* app_send_sms_desc;

#if ASTERISK_VERSION_NUM < 10800
int app_status_exec(struct ast_channel *channel, void *data);
#else
int app_status_exec(struct ast_channel *channel, const char *data);
#endif

#if ASTERISK_VERSION_NUM < 10800
int app_send_sms_exec(struct ast_channel *channel, void *data)
#else
int app_send_sms_exec(struct ast_channel *channel, const char *data);
#endif

#endif //__APP__

#endif // RGSM_APP_H_INCLUDED
