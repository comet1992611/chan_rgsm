#ifndef RGSM_UTILITIES_H_INCLUDED
#define RGSM_UTILITIES_H_INCLUDED

#include "time.h"
#include "rgsm_defs.h"
#include "ggw8_hal.h"
#include <asterisk/cli.h>

/**
 * \brief
 * Searches a GSM device by name among all gateways
 * \param name the name in form "slot_X" to search
 * \return a pointer to gsm_pvt structure, may return NULL
 */
struct gsm_pvt* find_ch_by_name(const char* name);
struct gateway* find_gateway_by_sysid(ggw8_device_sysid_t sysid);
struct gateway* find_gateway_by_uid(int id);

//! rgsm_lock and pvt->lock mutexes must be locked befor this call
const char* send_ussd(struct gsm_pvt *pvt, const char *ussd, int *queued, subcmd_ussd_t ussd_subcmd);

//! rgsm_lock and pvt->lock mutexes must be locked befor this call
const char* send_stk_response(struct gsm_pvt *pvt, stk_response_t resp_type_str, const char *params, int *queued);
const char* send_stk_response_str(struct gsm_pvt *pvt, const char *resp_type, const char *params, int *queued, int timeout_sec);

const char *onoff_str(int val);
const char *yesno_str(int val);

char *second_to_dhms(char *buf, time_t sec);

const char *reg_state_print(reg_state_t state);
const char *reg_state_print_short(reg_state_t state);
const char *rgsm_call_state_str(call_state_t call_state);
const char *rgsm_call_dir_str(call_dir_t dir);
const char *callwait_status_str(int type);
const char *rssi_print(char *obuf, int rssi);
const char *rssi_print_short(char *obuf, int rssi);
const char *ber_print(int ber);
const char *ber_print_short(int ber);
const char *cms_error_print(int ec);
const char *mdm_state_str(mdm_state_t mgmt_state);
const char *hidenum_settings_str(int hideset);
const char *invalid_ch_state_str(struct gsm_pvt *pvt);
const char *man_chstate_str(man_chstate_t state);

int is_address_string(const char *buf);
void address_classify(const char *input, address_t *addr);
void unknown_address(address_t *addr);
void address_normalize(address_t *addr);

//char *address_show(char *buf, struct address *addr, int full);
int is_address_equal(address_t *a1, address_t *a2);

int is_str_nonblank(const char *buf);
int is_str_non_unsolicited(const char *buf);
int is_str_digit(const char *buf);
int is_str_xdigit(const char *buf);
int is_str_printable(const char *buf);
void str_digit_to_bcd(const char *instr, int inlen, char *out);
int str_bin_to_hex(char **instr, int *inlen, char **outstr, int *outlen);
int str_hex_to_bin(char **instr, int *inlen, char **outstr, int *outlen);
int from_ucs2_to_specset(char *specset, char **instr, int *inlen, char **outstr, int *outlen);

incoming_type_t get_incoming_type(const char *inc_type);
const char *incoming_type_str(incoming_type_t type);

outgoing_type_t get_outgoing_type(const char *out_type);
const char *outgoing_type_str(outgoing_type_t type);

const char *baudrate_str(ggw8_baudrate_t baudrate);

int imei_calc_check_digit(const char *imei);
int imei_change(const char *device, const char *imei, char *msgbuf, int msgbuf_len, int *completed, struct ast_cli_args *a);

/**
 * non-interrable version of usleep
 */
void us_sleep(int usec);


#endif // RGSM_UTILITIES_H_INCLUDED
