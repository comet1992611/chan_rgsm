#ifndef RGSM_MANAGER_H_INCLUDED
#define RGSM_MANAGER_H_INCLUDED

#include "rgsm_defs.h"

#ifdef __MANAGER__

extern void rgsm_man_register();
extern void rgsm_man_unregister();

extern void rgsm_man_event_message(const char * event, const char * devname, const char * message);
extern void rgsm_man_event_message_raw(const char * event, const char * devname, const char * message);

extern void rgsm_man_event_new_ussd(const char * devname, char * message);
extern void rgsm_man_event_new_sms(const char * devname, char * number, char * message);

//STK notifications
extern void rgsm_man_event_stk_notify(struct gsm_pvt *pvt, const char *notification);

//IMEI change complete event
extern void rgsm_man_event_imei_change_complete(const char * devname, int error, const char* message);

//Channel state change event
extern void rgsm_man_event_channel_state(struct gsm_pvt *pvt);

//Aug 20, 2013, Bz2009 Add missing events and use Unification in names for Events
extern void rgsm_man_event_channel(struct gsm_pvt *pvt, const char* msg, int iccid_aware);


//extern void rgsm_man_event_new_sms_base64 (const char * devname, char * number, char * message_base64);
//extern void manager_event_cend(const char * devname, int call_index, int duration, int end_status, int cc_cause);
//extern void manager_event_call_state_change(const char * devname, int call_index, const char * newstate);
//extern void manager_event_device_status(const char * devname, const char * newstatus);
//extern void manager_event_sent_notify(const char * devname, const char * type, const void * id, const char * result);

#else  // __MANAGER__

#define void rgsm_man_register()
#define void rgsm_man_unregister()

#define rgsm_man_event_message(event, devname, message)
#define rgsm_man_event_message_raw(event, devname, message)

#define rgsm_man_event_new_ussd(devname, message)
#define rgsm_man_event_new_sms(devname, number, message)
#define rgsm_man_event_new_sms_base64(devname, number, message_base64)
//#define manager_event_cend(devname, call_index, duration, end_status, cc_cause)
//#define manager_event_call_state_change(devname, call_index, newstate)
//#define manager_event_device_status(devname, newstatus)
//#define manager_event_sent_notify(devname, type, id, result)

#define rgsm_man_event_stk_notify(struct gsm_pvt *pvt, const char *notification)
#define rgsm_man_event_imei_change_complete(const char * devname, int error, char* message)
#define void rgsm_man_event_channel_state(const char * devname, man_chstate_t state)

#endif //__MANAGER__

#endif // RGSM_MANAGER_H_INCLUDED
