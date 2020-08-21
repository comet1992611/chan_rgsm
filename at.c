#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "at.h"
#include "rgsm_utilities.h"
//------------------------------------------------------------------------------
const char ctrlz = 0x1a;
//------------------------------------------------------------------------------
const struct at_command_operation at_com_oper_list[AT_OPER_COUNT] = {
	{AT_OPER_EXEC, ""},
	{AT_OPER_TEST, "=?"},
	{AT_OPER_READ, "?"},
	{AT_OPER_WRITE, "="},
};
//------------------------------------------------------------------------------


const struct at_command basic_at_com_list[/*AT_BASIC_MAXNUM*/] = {
	// int id; u_int32_t operations; char name[16]; char response[MAX_AT_CMD_RESP][16]; char description[256]; add_check_at_resp_fun_t *check_fun;
	{AT_UNKNOWN, AT_OPER_EXEC, "", {"", ""}, "", is_str_printable},
	{AT_A_SLASH, AT_OPER_EXEC, "A/", {"", ""}, "Re-issues last AT command given", NULL},
	{AT_A, AT_OPER_EXEC, "ATA", {"", ""}, "Answer an incoming call", NULL},
	{AT_D, AT_OPER_EXEC, "ATD", {"", ""}, "Mobile originated call to dial a number", NULL},
	{AT_D_CURMEM, AT_OPER_EXEC, "ATD", {"", ""}, "Originate call to phone number in current memory", NULL},
	{AT_D_PHBOOK, AT_OPER_EXEC, "ATD", {"", ""}, "Originate call to phone number in memory which corresponds to field <STR>", NULL},
	{AT_DL, AT_OPER_EXEC, "ATDL",  {"", ""}, "Redial last telephone number used", NULL},
	{AT_E, AT_OPER_EXEC, "ATE",  {"ATE", ""}, "Set command echo mode", NULL},
	{AT_H, AT_OPER_EXEC, "ATH",  {"", ""}, "Disconnect existing connection", NULL},
	{AT_I, AT_OPER_EXEC, "ATI",  {"", ""}, "Display product identification information", is_str_non_unsolicited},
	{AT_L, AT_OPER_EXEC, "ATL",  {"", ""}, "Set monitor speaker loudness", NULL},
	{AT_M, AT_OPER_EXEC, "ATM",  {"", ""}, "Set monitor speaker mode", NULL},
	{AT_3PLUS, AT_OPER_EXEC, "+++",  {"", ""}, "Switch from data mode or PPP online mode to command mode", NULL},
	{AT_O, AT_OPER_EXEC, "ATO",  {"", ""}, "Switch from command mode to data mode", NULL},
	{AT_P, AT_OPER_EXEC, "ATP",  {"", ""}, "Select pulse dialling", NULL},
	{AT_Q, AT_OPER_EXEC, "ATQ",  {"", ""}, "Set result code presentation mode", NULL},
	{AT_IPR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+IPR",  {"+IPR:", ""}, "Set TE-TA fixed local rate", NULL},
	{AT_andW, AT_OPER_EXEC, "AT&W",  {"", ""}, "Store current parameter to user defined profile", NULL},
};

struct at_command *get_at_com_by_id(int id, const struct at_command *list, int maxnum){

	int i;
	struct at_command *cmd = (struct at_command *)list;
	//
	for(i=0; i<maxnum; i++){
		if(cmd){
			if(cmd->id == id) return cmd;
			cmd++;
        }
    }
	return NULL;
}

int is_at_com_done(const char *response)
{
	 return ((response) && (strlen(response)) && (!strcmp(response, "OK") || strstr(response, "ERROR"))) ? 1 : 0;
}

int is_at_com_response(struct at_command *at, const char *response)
{
	int i;
	//
	if(!at) return 0;
	if(!response) return 0;
	// ok
	if(strstr(response, "OK")) return 1;
	// error
	if(strstr(response, "ERROR")) return 1;
	// specific
	for(i=0;i<MAX_AT_CMD_RESP;i++){
		if (strlen(at->response[i])){
			if (strstr(response, at->response[i])) return 1;
        }
    }
	if((at->check_fun) && (at->check_fun(response))) return 1;
	// is not AT command response
	return 0;
}

char *get_at_com_oper_by_id(u_int32_t oper)
{
	int i;
	//
	for(i=0; i<AT_OPER_COUNT; i++) {
		if(at_com_oper_list[i].id == oper) return (char *)at_com_oper_list[i].str;
	}

	return NULL;
}




//------------------------------------------------------------------------------
// at_gen_ccwa_write_parse()
//------------------------------------------------------------------------------
int at_gen_ccwa_write_parse(const char *fld, int fld_len, struct at_gen_ccwa_write *ccwa){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CCWA_WRITE_PARAM 2
	struct parsing_param params[MAX_CCWA_WRITE_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!ccwa) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;
	// init params
	for(param_cnt=0; param_cnt<MAX_CCWA_WRITE_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}
	// init at_gen_ccwa_write
	ccwa->status = -1;
	ccwa->class = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CCWA_WRITE_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// processing integer params
	// status (mandatory)
	if(params[0].len > 0){
		tp = params[0].buf;
		while(params[0].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		ccwa->status = atoi(params[0].buf);
		}
	else
		return -1;

	// call class (mandatory)
	if(params[1].len > 0){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		ccwa->class = atoi(params[1].buf);
		}
	else
		return -1;

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_ccwa_write_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_cops_read_parse()
//------------------------------------------------------------------------------
int at_gen_cops_read_parse(const char *fld, int fld_len, struct at_gen_cops_read *cops){

	char *sp;
	char *tp;
	char *ep;

#define MAX_COPS_READ_PARAM 3
	struct parsing_param params[MAX_COPS_READ_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!cops) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_COPS_READ_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_cops_read
	cops->mode = -1;
	cops->format = -1;
	cops->oper = NULL;
	cops->oper_len = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_COPS_READ_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// mode (mandatory)
	if((param_cnt >= 1) && (params[0].type == PRM_TYPE_INTEGER)){
		// check for digit
		if(params[0].len > 0){
			tp = params[0].buf;
			while(params[0].len--){
				if(!isdigit(*tp++))
					return -1;
				}
			}
		cops->mode = atoi(params[0].buf);
		}
	else
		return -1;

	// format (optional)
	if(param_cnt >= 2){
		if(params[1].type == PRM_TYPE_INTEGER){
			// check for digit
			if(params[1].len > 0){
				tp = params[1].buf;
				while(params[1].len--){
					if(!isdigit(*tp++))
						return -1;
					}
				}
			cops->format = atoi(params[1].buf);
			}
		else
			return -1;
		}
	// oper (optional)
	if(param_cnt >= 3){
		if(params[2].type == PRM_TYPE_STRING){
			//
			cops->oper = params[2].buf;
			cops->oper_len = params[2].len;
			}
		else
			return -1;
		}

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_cops_read_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_cusd_write_parse()
//------------------------------------------------------------------------------
int at_gen_cusd_write_parse(const char *fld, int fld_len, struct at_gen_cusd_write *cusd){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CUSD_WRITE_PARAM 3
	struct parsing_param params[MAX_CUSD_WRITE_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 512)) return -1;

	if(!cusd) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CUSD_WRITE_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_cusd_write
	cusd->n = -1;
	cusd->str = NULL;
	cusd->str_len = -1;
	cusd->dcs = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CUSD_WRITE_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
		// search delimiter and put terminated null-symbol
			if(tp = strstr(sp, "\",")){
				*tp = '\0';
				params[param_cnt].len = tp - sp;
				tp++;
			} else tp = ep;
		}
		else{
		// search delimiter and put terminated null-symbol
			if(tp = strchr(sp, ',')){
				*tp = '\0';
				params[param_cnt].len = tp - sp;
			} else tp = ep;
		}
		//
		param_cnt++;
		tp++;
	}

	// n (mandatory)
	if((param_cnt >= 1) && (params[0].type == PRM_TYPE_INTEGER)){
		// check for digit
		if(params[0].len > 0){
			tp = params[0].buf;
			while(params[0].len--){
				if(!isdigit(*tp++))
					return -1;
				}
			}
		cusd->n = atoi(params[0].buf);
		}
	else
		return -1;

	// str (optional)
	if(param_cnt >= 2){
		if(params[1].type == PRM_TYPE_STRING){
			//
			cusd->str = params[1].buf;
			cusd->str_len = params[1].len;
			}
		else
			return -1;
		}

	// dcs (optional)
	if(param_cnt >= 3){
		if(params[2].type == PRM_TYPE_INTEGER){
			// check for digit
			if(params[2].len > 0){
				tp = params[2].buf;
				while(params[2].len--){
					if(!isdigit(*tp++))
						return -1;
					}
				}
			cusd->dcs = atoi(params[2].buf);
			}
		else
			return -1;
		}

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_cusd_write_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_cmgr_write_parse()
//------------------------------------------------------------------------------
int at_gen_cmgr_write_parse(const char *fld, int fld_len, struct at_gen_cmgr_write *cmgr){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CMGR_WRITE_PARAM 3
	struct parsing_param params[MAX_CMGR_WRITE_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 512)) return -1;

	if(!cmgr) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CMGR_WRITE_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_cmgr_write
	cmgr->stat = -1;
	cmgr->alpha = NULL;
	cmgr->alpha_len = -1;
	cmgr->length = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CMGR_WRITE_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';

		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// stat (mandatory)
	if((param_cnt >= 1) && (params[0].type == PRM_TYPE_INTEGER)){
		// check for digit
		if(params[0].len > 0){
			tp = params[0].buf;
			while(params[0].len--){
				if(!isdigit(*tp++))
					return -1;
				}
			}
		cmgr->stat = atoi(params[0].buf);
		}
	else
		return -1;

	// alpha
	if(param_cnt >= 2){
		cmgr->alpha = params[1].buf;
		cmgr->alpha_len = params[1].len;
		}
	else
		return -1;

	// length (mandatory)
	if(param_cnt >= 3){
		if(params[2].type == PRM_TYPE_INTEGER){
			// check for digit
			if(params[2].len > 0){
				tp = params[2].buf;
				while(params[2].len--){
					if(!isdigit(*tp++))
						return -1;
					}
				}
			cmgr->length = atoi(params[2].buf);
			}
		else
			return -1;
		}
	else
		return -1;

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_cmgr_write_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_csq_exec_parse()
//------------------------------------------------------------------------------
int at_gen_csq_exec_parse(const char *fld, int fld_len, struct at_gen_csq_exec *csq){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CSQ_EXEC_PARAM 2
	struct parsing_param params[MAX_CSQ_EXEC_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!csq) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CSQ_EXEC_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_csq_exec
	csq->rssi = -1;
	csq->ber = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CSQ_EXEC_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// processing integer params
	// rssi
	if(params[0].len > 0){
		tp = params[0].buf;
		while(params[0].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		csq->rssi = atoi(params[0].buf);
		}
	else
		return -1;

	// ber
	if(params[1].len > 0){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		csq->ber = atoi(params[1].buf);
		}
	else
		return -1;

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_csq_exec_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_csca_read_parse()
//------------------------------------------------------------------------------
int at_gen_csca_read_parse(const char *fld, int fld_len, struct at_gen_csca_read *csca){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CSCA_READ_PARAM 2
	struct parsing_param params[MAX_CSCA_READ_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!csca) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CSCA_READ_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_csca_read
	csca->sca = NULL;
	csca->sca_len = -1;
	csca->tosca = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CSCA_READ_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// processing params
	// sca
	if((param_cnt >= 1) && (params[0].type == PRM_TYPE_STRING)){
		csca->sca = params[0].buf;
		csca->sca_len = params[0].len;
		}
	else
		return -1;

	// tosca
	if((param_cnt >= 2) && (params[1].len > 0)){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		csca->tosca = atoi(params[1].buf);
		}
	else
		return -1;

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_csca_read_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Calling Line Identification Presentation
//------------------------------------------------------------------------------
int at_gen_clip_unsol_parse(const char *fld, int fld_len, struct at_gen_clip_unsol *clip){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CLIP_UNSOL_PARAM 6
	struct parsing_param params[MAX_CLIP_UNSOL_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!clip) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CLIP_UNSOL_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
    }

	// init at_gen_clip_unsol
	clip->number = NULL;
	clip->number_len = -1;
	clip->type = -1;
	clip->alphaid = NULL;
	clip->alphaid_len = -1;
	clip->cli_validity = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CLIP_UNSOL_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
        }
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
        }
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
        }
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
        }
		else{
			params[param_cnt].len = tp - sp;
        }
		//
		param_cnt++;
		tp++;
    }

	// processing params
	// number
	if((param_cnt >= 1) && (params[0].type == PRM_TYPE_STRING)){
		clip->number = params[0].buf;
		clip->number_len = params[0].len;
    }
	else
		return -1;

	// type
	if((param_cnt >= 2) && (params[1].len > 0)){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
        }
		clip->type = atoi(params[1].buf);
    }

	// alphaid
	if((param_cnt >= 5) && (params[4].type == PRM_TYPE_STRING)){
		clip->alphaid = params[4].buf;
		clip->alphaid_len = params[4].len;
    }

	// CLI validity
	if((param_cnt >= 6) && (params[5].len > 0)){
		tp = params[5].buf;
		while(params[5].len--){
			if(!isdigit(*tp++))
				return -1;
        }
		clip->cli_validity = atoi(params[5].buf);
    }

	return param_cnt;
}
//------------------------------------------------------------------------------
// end of at_gen_clip_unsol_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_cnum_exec_parse()
//------------------------------------------------------------------------------
int at_gen_cnum_exec_parse(const char *fld, int fld_len, struct at_gen_cnum_exec *cnum){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CNUM_EXEC_PARAM 6
	struct parsing_param params[MAX_CNUM_EXEC_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!cnum) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CNUM_EXEC_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_cnum_exec
	cnum->alpha = NULL;
	cnum->alpha_len = -1;
	cnum->number = NULL;
	cnum->number_len = -1;
	cnum->type = -1;
	cnum->speed = -1;
	cnum->service = -1;
	cnum->itc = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CNUM_EXEC_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	//
	if((param_cnt >= 2) &&
			(params[0].type == PRM_TYPE_STRING) &&
				(params[1].type == PRM_TYPE_INTEGER)){
		// check if alpha (optional) not present
		// get number
		cnum->number = params[0].buf;
		cnum->number_len = params[0].len;
		// get type
		if(params[1].len > 0){
			tp = params[1].buf;
			while(params[1].len--){
				if(!isdigit(*tp++))
					return -1;
				}
			cnum->type = atoi(params[1].buf);
			}
		// check for speed service pair
		if(param_cnt == 3){
			return -1;
			}
		// check for speed service
		if((param_cnt >= 4) &&
			(params[2].type == PRM_TYPE_INTEGER) &&
				(params[3].type == PRM_TYPE_INTEGER)){
			// get speed
			if(params[2].len > 0){
				tp = params[2].buf;
				while(params[2].len--){
					if(!isdigit(*tp++))
						return -1;
					}
				cnum->speed = atoi(params[2].buf);
				}
			// get service
			if(params[3].len > 0){
				tp = params[3].buf;
				while(params[3].len--){
					if(!isdigit(*tp++))
						return -1;
					}
				cnum->service = atoi(params[3].buf);
				}
			// check for itc
			if((param_cnt == 5) &&
				(params[4].type == PRM_TYPE_INTEGER)){
				// get itc
				if(params[4].len > 0){
					tp = params[4].buf;
					while(params[4].len--){
						if(!isdigit(*tp++))
							return -1;
						}
					cnum->itc = atoi(params[4].buf);
					}
				}
			}
		}
	else if((param_cnt >= 3) &&
				(params[0].type == PRM_TYPE_STRING) &&
					(params[1].type == PRM_TYPE_STRING) &&
						(params[2].type == PRM_TYPE_INTEGER)){
		// check if alpha (optional) present
		// get alpha
		cnum->alpha = params[0].buf;
		cnum->alpha_len = params[0].len;
		// get number
		cnum->number = params[1].buf;
		cnum->number_len = params[1].len;
		// get type
		if(params[2].len > 0){
			tp = params[2].buf;
			while(params[2].len--){
				if(!isdigit(*tp++))
					return -1;
				}
			cnum->type = atoi(params[2].buf);
			}
		// check for speed service pair
		if(param_cnt == 4){
			return -1;
			}
		// check for speed service
		if((param_cnt >= 5) &&
			(params[3].type == PRM_TYPE_INTEGER) &&
				(params[4].type == PRM_TYPE_INTEGER)){
			// get speed
			if(params[3].len > 0){
				tp = params[3].buf;
				while(params[3].len--){
					if(!isdigit(*tp++))
						return -1;
					}
				cnum->speed = atoi(params[3].buf);
				}
			// get service
			if(params[4].len > 0){
				tp = params[4].buf;
				while(params[4].len--){
					if(!isdigit(*tp++))
						return -1;
					}
				cnum->service = atoi(params[4].buf);
				}
			// check for itc
			if((param_cnt == 6) &&
				(params[5].type == PRM_TYPE_INTEGER)){
				// get itc
				if(params[5].len > 0){
					tp = params[5].buf;
					while(params[5].len--){
						if(!isdigit(*tp++))
							return -1;
						}
					cnum->itc = atoi(params[5].buf);
					}
				}
			}
		}
	else
		return -1;

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_cnum_exec_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_clcc_exec_parse()
//------------------------------------------------------------------------------
int at_gen_clcc_exec_parse(const char *fld, int fld_len, struct at_gen_clcc_exec *clcc){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CLCC_EXEC_PARAM 7
	struct parsing_param params[MAX_CLCC_EXEC_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!clcc) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CLCC_EXEC_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init sim300_clcc_exec
	clcc->id = -1;
	clcc->dir = -1;
	clcc->stat = -1;
	clcc->mode = -1;
	clcc->mpty = -1;
	clcc->number = NULL;
	clcc->number_len = -1;
	clcc->type = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt <MAX_CLCC_EXEC_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// processing integer params
	// id
	if((param_cnt >= 1) && (params[0].len > 0)){
		tp = params[0].buf;
		while(params[0].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clcc->id = atoi(params[0].buf);
		}
	else
		return -1;

	// dir
	if((param_cnt >= 2) && (params[1].len > 0)){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clcc->dir = atoi(params[1].buf);
		}
	else
		return -1;

	// stat
	if((param_cnt >= 3) && (params[2].len > 0)){
		tp = params[2].buf;
		while(params[2].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clcc->stat = atoi(params[2].buf);
		}
	else
		return -1;

	// mode
	if((param_cnt >= 4) && (params[3].len > 0)){
		tp = params[3].buf;
		while(params[3].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clcc->mode = atoi(params[3].buf);
		}
	else
		return -1;

	// mpty
	if((param_cnt >= 5) && (params[4].len > 0)){
		tp = params[4].buf;
		while(params[4].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clcc->mpty = atoi(params[4].buf);
		}
	else
		return -1;

	// number
	if(param_cnt >= 6){
	  if(params[5].type == PRM_TYPE_STRING){
			clcc->number = params[5].buf;
			clcc->number_len = params[5].len;
			}
		else
			return -1;
		}

	// type
	if((param_cnt >= 7) && (params[6].len > 0)){
		tp = params[6].buf;
		while(params[6].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clcc->type = atoi(params[6].buf);
		}

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_clcc_exec_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_creg_read_parse()
//------------------------------------------------------------------------------
int at_gen_creg_read_parse(const char *fld, int fld_len, struct at_gen_creg_read *creg){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CREG_READ_PARAM 4
	struct parsing_param params[MAX_CREG_READ_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!creg) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CREG_READ_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_creg_read
	creg->n = -1;
	creg->stat = -1;
	creg->lac = NULL;
	creg->lac_len = -1;
	creg->ci = NULL;
	creg->ci_len = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt <MAX_CREG_READ_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// processing params
	// n (mandatory)
	if((param_cnt >= 1) && (params[0].len > 0)){
		tp = params[0].buf;
		while(params[0].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		creg->n = atoi(params[0].buf);
		}
	else
		return -1;

	// stat (mandatory)
	if((param_cnt >= 2) && (params[1].len > 0)){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		creg->stat = atoi(params[1].buf);
		}
	else
		return -1;

	// lac
	if(param_cnt >= 3){
//	  if(params[2].type == PRM_TYPE_STRING){
			creg->lac = params[2].buf;
			creg->lac_len = params[2].len;
//			}
//		else
//			return -1;
		}

	// ci
	if(param_cnt >= 4){
//	  if(params[3].type == PRM_TYPE_STRING){
			creg->ci = params[3].buf;
			creg->ci_len = params[3].len;
//			}
//		else
//			return -1;
		}

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_creg_read_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_gen_clir_read_parse()
//------------------------------------------------------------------------------
int at_gen_clir_read_parse(const char *fld, int fld_len, struct at_gen_clir_read *clir){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CLIR_READ_PARAM 2
	struct parsing_param params[MAX_CLIR_READ_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!clir) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CLIR_READ_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_gen_clir_read
	clir->n = -1;
	clir->m = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CLIR_READ_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
			}
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
			}
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
			}
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ',')))
			tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
			}
		else{
			params[param_cnt].len = tp - sp;
			}
		//
		param_cnt++;
		tp++;
		}

	// processing integer params
	// n
	if(params[0].len > 0){
		tp = params[0].buf;
		while(params[0].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clir->n = atoi(params[0].buf);
		}
	else
		return -1;

	// m
	if(params[1].len > 0){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		clir->m = atoi(params[1].buf);
		}
	else
		return -1;

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_gen_clir_read_parse()
//------------------------------------------------------------------------------

int at_psstk_unsol_parse(const char *fld, int fld_len, struct at_psstk_unsol *psstk)
{
	char *sp;
	char *tp;
	char *ep;

#define MAX_PSSTK_UNSOL_PARAM 10
	struct parsing_param params[MAX_PSSTK_UNSOL_PARAM];
	int param_cnt;
	int param_indx;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!psstk) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' '))) return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_PSSTK_UNSOL_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
    }

	// init at_psstk_unsol
    psstk->response_type = NULL;
    psstk->response_type_len = -1;
    psstk->alpha_id_presence = -1;
    psstk->alphabet = -1;
    psstk->alpha_id = NULL;
    psstk->alpha_id_len = -1;
    psstk->icon_id = -1;
    psstk->icon_qualifier = -1;
    psstk->command_number = -1;
    psstk->default_item_id = -1;
    psstk->help_info = -1;
    psstk->number_of_item = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_PSSTK_UNSOL_PARAM)){
		// get param type
		if(*tp == '"'){
			params[param_cnt].type = PRM_TYPE_STRING;
			params[param_cnt].buf = ++tp;
        }
		else if(isdigit(*tp)){
			params[param_cnt].type = PRM_TYPE_INTEGER;
			params[param_cnt].buf = tp;
        }
		else{
			params[param_cnt].type = PRM_TYPE_UNKNOWN;
			params[param_cnt].buf = tp;
        }
		sp = tp;
		// search delimiter and put terminated null-symbol
		if(!(tp = strchr(sp, ','))) tp = ep;
		*tp = '\0';
		// set param len
		if(params[param_cnt].type == PRM_TYPE_STRING){
			params[param_cnt].len = tp - sp - 1;
			*(tp-1) = '\0';
        }
		else{
			params[param_cnt].len = tp - sp;
        }
		//
		param_cnt++;
		tp++;
    }

	// processing params
	// response type text
    param_indx = 0;
	if((param_cnt >= 1) && (params[param_indx].type == PRM_TYPE_STRING)){
		psstk->response_type = params[param_indx].buf;
		psstk->response_type_len = params[param_indx].len;
    }
	else {
		return -2;
	}

    param_indx++;
	// alpha_id_presence - int
	if((param_cnt >= 2) && (params[param_indx].len > 0)){
		if (!is_str_digit(params[param_indx].buf)) return -3;
		psstk->alpha_id_presence = atoi(params[param_indx].buf);
    }


    if (psstk->alpha_id_presence) {
        param_indx++;
        // alphabet - int
        if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_INTEGER)){
            if (!is_str_digit(params[param_indx].buf)) return -4;
            psstk->alphabet = atoi(params[param_indx].buf);
        } else {
            return -5;
        }

        //alpha_id - text
        param_indx++;
        if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_STRING)){
            psstk->alpha_id = params[param_indx].buf;
            psstk->alpha_id_len = params[param_indx].len;
        }
        else {
            ast_log(AST_LOG_DEBUG, "param_cnt=%d, param_type=%d\n", param_cnt, params[param_indx].type);
            return -6;
        }
    }

    // icon_id - int
    param_indx++;
    if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_INTEGER)){
        if (!is_str_digit(params[param_indx].buf)) return -7;
        psstk->icon_id = atoi(params[param_indx].buf);
    } else {
        return -8;
    }

    // icon qualifier - int
    param_indx++;
    if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_INTEGER)){
        if (!is_str_digit(params[param_indx].buf)) return -9;
        psstk->icon_qualifier = atoi(params[param_indx].buf);
    } else {
        return -10;
    }

    // command_number
    param_indx++;
    if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_INTEGER)){
        if (!is_str_digit(params[param_indx].buf)) return -11;
        psstk->command_number = atoi(params[param_indx].buf);
    } else {
        return -12;
    }

    // default_item_id
    param_indx++;
    if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_INTEGER)){
        if (!is_str_digit(params[param_indx].buf)) return -13;
        psstk->default_item_id = atoi(params[param_indx].buf);
    } else {
        return -14;
    }


    // help_info
    param_indx++;
    if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_INTEGER)){
        if (!is_str_digit(params[param_indx].buf)) return -15;
        psstk->help_info = atoi(params[param_indx].buf);
    } else {
        return -16;
    }

    // number_of_item
    param_indx++;
    if((param_cnt >= (param_indx+1)) && (params[param_indx].type == PRM_TYPE_INTEGER)){
        if (!is_str_digit(params[param_indx].buf)) return -17;
        psstk->number_of_item = atoi(params[param_indx].buf);
    } else {
        return -18;
    }

	return param_cnt;
}
