#include <strings.h>
#include <iconv.h>
#include "rgsm_defs.h"
#include "rgsm_utilities.h"
#include "at.h"
#include "chan_rgsm.h"
#include "rgsm_sim900.h"
#include "rgsm_manager.h"

#define UCS2_UNKNOWN_SYMBOL 0x3F00

//!number of items must match with stk_response_t enum
static const char *resp_name[] = {
    "SETUP_MENU",
    "COMMAND_REJECTED",
    "NOTIFICATION",
    "DISPLAY_TEXT",
    "SELECT_ITEM",
    "MENU_SELECTION",
    "GET_ITEM_LIST",
};

//--------------------
// find gsm channel by its symbolic name ()
//--------------------
struct gsm_pvt* find_ch_by_name(const char* name)
{
	struct gsm_pvt*	pvt = NULL;
	struct gateway* gw;
	int i;

	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            if (gw->gsm_pvts[i] && !strcmp(gw->gsm_pvts[i]->name, name)) {
                pvt = gw->gsm_pvts[i];
                goto exit_;
            }
        }
	}
exit_:
	return pvt;
}

struct gateway* find_gateway_by_sysid(ggw8_device_sysid_t sysid)
{
	struct gateway* gw;

	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
	    if (ggw8_get_device_sysid(gw->ggw8_device) == sysid) return gw;
	}
	return NULL;
}

struct gateway* find_gateway_by_uid(int uid)
{
	struct gateway* gw;

	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
	    if (gw->uid == uid) return gw;
	}
	return NULL;
}

static const char RET_USSD_OK[] = "USSD queued";
static const char RET_USSD_NOT_QUEUED[] = "failed to queue USSD command";
static const char RET_USSD_CONVERSION_ERR[] = "failed convert to hex";

static const char CHAN_STATE_DISABLED[] = "channel disabled";
static const char CHAN_STATE_NOT_INIT[] = "channel not initialized";
static const char CHAN_STATE_NOT_REG[] = "channel not registered";
static const char CHAN_STATE_ONCALL[] = "channel has active call";
static const char CHAN_STATE_ONSERVICE[] = "channel on service";

const char *invalid_ch_state_str(struct gsm_pvt *pvt)
{
    if (pvt->call_state == CALL_STATE_DISABLE) {
        return CHAN_STATE_DISABLED;
    } else {
        if(pvt->mdm_state != MDM_STATE_RUN) {
            return CHAN_STATE_NOT_INIT;
        } else if((pvt->reg_state != REG_STATE_REG_HOME_NET) && (pvt->reg_state != REG_STATE_REG_ROAMING)) {
            return CHAN_STATE_NOT_REG;
        } else if(pvt->call_state > CALL_STATE_NULL) {
            return CHAN_STATE_ONCALL;
        } else if(pvt->call_state == CALL_STATE_ONSERVICE) {
            return CHAN_STATE_ONSERVICE;
        } else {
            return "";
        }
    }
}

const char *man_chstate_str(man_chstate_t state)
{
    switch (state) {
        case MAN_CHSTATE_DISABLED:              return "DISABLED";
        case MAN_CHSTATE_STARTING:              return "STARTING";
        case MAN_CHSTATE_IDLE:                  return "IDLE";
        case MAN_CHSTATE_INCOMING_CALL:         return "INCOMING CALL";
        case MAN_CHSTATE_OUTGOING_CALL:         return "OUTGOING CALL";
        case MAN_CHSTATE_ABORTED_REGFAILURE:    return "ABORTED_REG FAILURE";
        case MAN_CHSTATE_ABORTED_NOSIM:         return "ABORTED NO SIM";
        case MAN_CHSTATE_ABORTED_HWFAILURE:     return "ABORTED HW FAILURE";
        case MAN_CHSTATE_ONSERVICE:             return "ON SERVICE";
        default:
            return "UNSPECIFIED";
    }
}

const char* send_ussd(struct gsm_pvt *pvt, const char *ussd, int *queued, subcmd_ussd_t ussd_subcmd)
{
    char *in_ptr;
	int in_len;
	char buf[640];
	char *buf_ptr;
	int buf_len;


    *queued = 0;
    if((pvt->mdm_state == MDM_STATE_RUN) &&
        ((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)) &&
        (pvt->call_state == CALL_STATE_NULL)){
        // convert string to hex presentation
        in_len = strlen(ussd);
        in_ptr = (char *)ussd;
        memset(buf, 0, 640);
        buf_ptr = buf;
        buf_len = 640;
        //
        if(!str_bin_to_hex(&in_ptr, &in_len, &buf_ptr, &buf_len)){
            //
            if(pvt->functions.send_ussd == NULL)
            {
                if (rgsm_atcmd_queue_append(pvt, AT_CUSD,
                                    AT_OPER_WRITE,
                                    ussd_subcmd,
                                    60,
                                    0,
                                    "%d,\"%s\"",
                                    1, buf) != -1)
	        {
    		    *queued = 1;
	            pvt->ussd_subcmd = ussd_subcmd;
	            pvt->ussd_datalen = 0;
		    pvt->ussd_valid = 0;
	            pvt->ussd_done = 0;
	            //
                    pvt->call_state = CALL_STATE_ONSERVICE;
                    return RET_USSD_OK;
                } else {
                    return RET_USSD_NOT_QUEUED;
        	}
    	    }
            else 
            	{
	            pvt->functions.send_ussd(pvt, ussd_subcmd, (char*)ussd);
        	    *queued = 1;
	            pvt->ussd_subcmd = ussd_subcmd;
                    pvt->ussd_datalen = 0;
		    pvt->ussd_valid = 0;
	            pvt->ussd_done = 0;
	            pvt->call_state = CALL_STATE_ONSERVICE;
	            return RET_USSD_OK;
	        }
        } else {
            return RET_USSD_CONVERSION_ERR;
        }
    } else {
        // print expected condition
        return invalid_ch_state_str(pvt);
    }
}

//! rgsm_lock and pvt->lock mutexes must be locked befor this call
const char* send_stk_response(struct gsm_pvt *pvt, stk_response_t resp_type, const char *params, int *queued)
{
    *queued = 0;
    if((pvt->mdm_state == MDM_STATE_RUN) &&
        ((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)) &&
        (pvt->call_state == CALL_STATE_NULL)){
        //
        if (pvt->module_type == MODULE_TYPE_SIM900) {
            *queued = (rgsm_atcmd_queue_append(pvt, AT_SIM900_PSSTK, AT_OPER_WRITE, 0, 30, 0, "\"%s\",%s", resp_name[resp_type], params) != -1);
        } else {
            return "Not implemented for given module type";
        }

        if (*queued) {
            //TODO: set channel variables
            return "STK response queued";
        } else {
            return "STK response not queued";
        }


    } else {
        // print expected condition
        return invalid_ch_state_str(pvt);
    }
}

//! rgsm_lock and pvt->lock mutexes must be locked befor this call
//! the pvt->stk_mansession must be set before this call
const char* send_stk_response_str(struct gsm_pvt *pvt, const char *resp_type, const char *params, int *queued, int timeout_sec)
{
    *queued = 0;
     if((pvt->mdm_state == MDM_STATE_RUN) &&
        ((pvt->reg_state == REG_STATE_REG_HOME_NET) || (pvt->reg_state == REG_STATE_REG_ROAMING)) &&
        (pvt->call_state == CALL_STATE_NULL)){
        //
        if (!pvt->stk_capabilities) {
            return "Operator does not provide SIM Application Toolkit capabilities";
        }

        if (pvt->module_type == MODULE_TYPE_SIM900) {
            *queued = (rgsm_atcmd_queue_append(pvt, AT_SIM900_PSSTK, AT_OPER_WRITE, 0, timeout_sec, 0, "\"%s\",%s", resp_type, params) != -1);
        } else {
            return "Not implemented for given module type";
        }

        if (*queued) {
            //TODO: set channel variables
            return "STK response queued";
        } else {
            return "STK response not queued";
        }


    } else {
        // print expected condition
        return invalid_ch_state_str(pvt);
    }
}


const char *onoff_str(int val)
{
	return val ? "On" : "Off";
}

const char *yesno_str(int val)
{
	return val ? "Yes" : "No";
}

char* complete_device(const char* line, const char* word, int pos, int state, int flags)
{
    struct gateway* gw;
	struct gsm_pvt*	pvt;
	char*	res = NULL;
	int	which = 0;
	int i;
	int	wordlen = strlen (word);

	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            pvt = gw->gsm_pvts[i];
            if (pvt && !strncasecmp (pvt->name, word, wordlen) && ++which > state) {
                res = ast_strdup (pvt->name);
                goto exit_;
            }
        }
	}
exit_:
	return res;
}


char *second_to_dhms(char *buf, time_t sec)
{
	char *res = buf;
	int d,m,h,s;
	//
	d = 0;
	m = 0;
	h = 0;
	s = 0;

	// get days
	d = sec / (60*60*24);
	sec = sec % (60*60*24);
	// get hours
	h = sec / (60*60);
	sec = sec % (60*60);
	// get minutes
	m = sec / (60);
	sec = sec % (60);
	// get seconds
	s = sec;

	sprintf(buf, "%03d:%02d:%02d:%02d", d, h, m, s);

	return res;
}

static const char* reg_states_short[] = {
    "not search",
    "home net",
    "searching",
    "denied",
    "unknown",
    "roaming"
};

static const char* reg_states[] = {
    "not registered, ME is not searching operator to register",
    "registered, home network",
    "not registered, ME is searching operator to register",
    "registration denied",
    "unknown",
    "registered, roaming"
};

const char *reg_state_print_short(reg_state_t state)
{
    return (state >= REG_STATE_NOTREG_NOSEARCH && state <= REG_STATE_REG_ROAMING) ? reg_states_short[state] : "invalid";
}

const char *reg_state_print(reg_state_t state){
    return (state >= REG_STATE_NOTREG_NOSEARCH && state <= REG_STATE_REG_ROAMING) ? reg_states[state] : "invalid";
}


const char *rgsm_call_state_str(call_state_t call_state)
{
	switch(call_state){
		case CALL_STATE_INVAL: return "invalid";
		case CALL_STATE_DISABLE: return "disable";
		case CALL_STATE_ONSERVICE: return "on service";
		case CALL_STATE_NULL: return "null";
		case CALL_STATE_OUT_CALL_PROC: return "out call proc";
		case CALL_STATE_CALL_DELIVERED: return "call delivered";
		case CALL_STATE_CALL_PRESENT: return "call present";
		case CALL_STATE_CALL_RECEIVED: return "call received";
		case CALL_STATE_IN_CALL_PROC: return "in call proc";
		case CALL_STATE_ACTIVE: return "active";
		case CALL_STATE_RELEASE_IND: return "release ind";
		case CALL_STATE_OVERLAP_RECEIVING: return "overlap recv";
		default: return "unknown";
    }
}

const char *rgsm_call_dir_str(call_dir_t dir)
{
	switch(dir){
		case CALL_DIR_NONE: return "none";
		case CALL_DIR_IN: return "incoming";
		case CALL_DIR_OUT: return "outgoing";
		default: return "unknown";
    }
}


const char *rssi_print(char *obuf, int rssi){

	if(!obuf)
		return "obuf error";

	if(rssi == 0){
		sprintf(obuf, "-113 dBm");
    } else if ((rssi >= 1) && (rssi <= 30)){
		sprintf(obuf, "%d dBm", rssi*2 - 113);
    } else if (rssi == 31) {
		sprintf(obuf, "-51 dBm");
    } else if (rssi == 99){
		sprintf(obuf, "N/A");
    } else if (rssi < 0){
		sprintf(obuf, "N/A");
    } else {
		sprintf(obuf, "rssi error value");
    }
    return obuf;
}

const char *rssi_print_short(char *obuf, int rssi)
{
	if(!obuf)
		return "ERR";

	if(rssi == 0){
		sprintf(obuf, "-113");
    } else if ((rssi >= 1) && (rssi <= 30)){
		sprintf(obuf, "%d", rssi*2 - 113);
    } else if (rssi == 31) {
		sprintf(obuf, "-51");
    } else if (rssi == 99){
		sprintf(obuf, "N/A");
    } else if (rssi < 0){
		sprintf(obuf, "N/A");
    } else {
		sprintf(obuf, "ERR");
    }
    return obuf;
}

const char *ber_print(int ber){

	switch(ber){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case -1: return "N/A";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 0: return "0.14%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 1: return "0.28%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 2: return "0.57%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 3: return "1.13%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 4: return "2.26%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 5: return "4.53%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 6: return "9.05%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 7: return "18.10%";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 99: return "N/A";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default: return "ber error value";
    }
}

const char *ber_print_short(int ber){

	switch(ber){
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case -1: return "N/A";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 0: return "0.14";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 1: return "0.28";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 2: return "0.57";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 3: return "1.13";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 4: return "2.26";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 5: return "4.53";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 6: return "9.05";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 7: return "18.10";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		case 99: return "N/A";
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		default: return "ERR";
    }
}

const char *cms_error_print(int ec){
	switch(ec){
		case CMS_ERR_ME_FAILURE: return "ME failure";
		case CMS_ERR_SMS_ME_RESERVED: return "SMS ME reserved";
		case CMS_ERR_OPER_NOT_ALLOWED: return "operation not allowed";
		case CMS_ERR_OPER_NOT_SUPPORTED: return "operation not supported";
		case CMS_ERR_INVALID_PDU_MODE: return "invalid PDU mode";
		case CMS_ERR_INVALID_TEXT_MODE: return "invalid text mode";
		case CMS_ERR_SIM_NOT_INSERTED: return "SIM not inserted";
		case CMS_ERR_SIM_PIN_NECESSARY: return "SIM pin necessary";
		case CMS_ERR_PH_SIM_PIN_NECESSARY: return "PH SIM pin necessary";
		case CMS_ERR_SIM_FAILURE: return "SIM failure";
		case CMS_ERR_SIM_BUSY: return "SIM busy";
		case CMS_ERR_SIM_WRONG: return "SIM wrong";
		case CMS_ERR_SIM_PUK_REQUIRED: return "SIM PUK required";
		case CMS_ERR_SIM_PIN2_REQUIRED: return "SIM PIN2 required";
		case CMS_ERR_SIM_PUK2_REQUIRED: return "SIM PUK2 required";
		case CMS_ERR_MEMORY_FAILURE: return "memory failure";
		case CMS_ERR_INVALID_MEMORY_INDEX: return "invalid memory index";
		case CMS_ERR_MEMORY_FULL: return "memory full";
		case CMS_ERR_SMSC_ADDRESS_UNKNOWN: return "SMSC address unknown";
		case CMS_ERR_NO_NETWORK: return "no network";
		case CMS_ERR_NETWORK_TIMEOUT: return "network timeout";
		case CMS_ERR_UNKNOWN: return "unknown";
		case CMS_ERR_SIM_NOT_READY: return "SIM not ready";
		case CMS_ERR_UNREAD_SIM_RECORDS: return "unread records on SIM";
		case CMS_ERR_CB_ERROR_UNKNOWN: return "CB error unknown";
		case CMS_ERR_PS_BUSY: return "PS busy";
		case CMS_ERR_SM_BL_NOT_READY: return "SM BL not ready";
		case CMS_ERR_INVAL_CHARS_IN_PDU: return "Invalid (non-hex) chars in PDU";
		case CMS_ERR_INCORRECT_PDU_LENGTH: return "Incorrect PDU length";
		case CMS_ERR_INVALID_MTI: return "Invalid MTI";
		case CMS_ERR_INVAL_CHARS_IN_ADDR: return "Invalid (non-hex) chars in address";
		case CMS_ERR_INVALID_ADDRESS: return "Invalid address (no digits read)";
		case CMS_ERR_INCORRECT_PDU_UDL_L: return "Incorrect PDU length (UDL)";
		case CMS_ERR_INCORRECT_SCA_LENGTH: return "Incorrect SCA length";
		case CMS_ERR_INVALID_FIRST_OCTET: return "Invalid First Octet (should be 2 or 34)";
		case CMS_ERR_INVALID_COMMAND_TYPE: return "Invalid Command Type";
		case CMS_ERR_SRR_BIT_NOT_SET: return "SRR bit not set";
		case CMS_ERR_SRR_BIT_SET: return "SRR bit set";
		case CMS_ERR_INVALID_UDH_IE: return "Invalid User Data Header IE";
		default: return "unrecognized cme error";
    }
}


const char *mdm_state_str(mdm_state_t mgmt_state)
{

	switch(mgmt_state){
		case MDM_STATE_DISABLE: return "disable";
		case MDM_STATE_PWR_DOWN: return "turn power down";
		case MDM_STATE_WAIT_RDY: return "wait rdy";
		case MDM_STATE_WAIT_CFUN: return "wait cfun";
		case MDM_STATE_TEST_FUN: return "test AT functionality";
		case MDM_STATE_CHECK_PIN: return "check for PIN";
		case MDM_STATE_WAIT_CALL_READY: return "wait for call ready";
		case MDM_STATE_INIT: return "initialization";
		case MDM_STATE_RUN: return "run";
		case MDM_STATE_SUSPEND: return "suspend";
		case MDM_STATE_WAIT_SUSPEND: return "wait for suspend";
		//case MDM_STATE_WAIT_VIO_UP: return "wait for VIO is up";
		//case MDM_STATE_WAIT_VIO_DOWN: return "wait for VIO is down";
		case MDM_STATE_AUTO_BAUDING: return "serial port auto-bauding";
		default: return "unknown";
    }
}

int is_address_string(const char *buf)
{
 	//
	if(!buf || !strlen(buf)) return 0;
	// check first symbol
	if((!isdigit(*buf)) && (*buf != '+') && (*buf != '*') && (*buf != '#')) return 0;
	buf++;
	// check rest symbols
	while(*buf){
		if(!isdigit(*buf) && (*buf != '*')&& (*buf != '#'))
			return 0;
		buf++;
    }
	return 1;
}

void address_classify(const char *input, address_t *addr)
{
    // copy number
	memset(addr, 0, sizeof(struct address));
	strcpy(addr->value, input);
	addr->length = strlen(addr->value);
	addr->type.bits.reserved = 1;
	// get numbering plan
	if((is_address_string(addr->value)) && (addr->length > 7))
		addr->type.bits.numbplan = NUMBERING_PLAN_ISDN_E164;
	else
		addr->type.bits.numbplan = NUMBERING_PLAN_UNKNOWN;
	// get type of number
	if(((addr->value[0] == '0') && (addr->value[1] == '0')) ||
		((addr->value[0] == '0') && (addr->value[1] == '0') && (addr->value[2] == '0')) ||
			((addr->value[0] == '+'))){
		addr->type.bits.typenumb = TYPE_OF_NUMBER_INTERNATIONAL;
    }
	else if(addr->value[0] == '0'){
		addr->type.bits.typenumb = TYPE_OF_NUMBER_NATIONAL;
    }
	else{
		addr->type.bits.typenumb = TYPE_OF_NUMBER_UNKNOWN;
    }
	//
	address_normalize(addr);
}

void unknown_address(address_t *addr)
{
    address_classify("unknown", addr);
}

void address_normalize(address_t *addr)
{
	char buf[MAX_ADDRESS_LENGTH];
	int len;

	iconv_t tc;

	char *ib;
	char *ob;
	size_t incnt;
	size_t outcnt;
	size_t outres;

	//
	if((addr->type.bits.typenumb == TYPE_OF_NUMBER_INTERNATIONAL) &&
			(addr->type.bits.numbplan == NUMBERING_PLAN_ISDN_E164)){
		//
		memset(buf, 0, MAX_ADDRESS_LENGTH);
		//
		if((addr->value[0] == '0') && (addr->value[1] == '0') && (addr->value[2] == '0')){
			len = sprintf(buf, "%.*s", addr->length-3, &addr->value[3]);
			addr->length = len;
    		}
		else if((addr->value[0] == '0') && (addr->value[1] == '0')){
			len = sprintf(buf, "%.*s", addr->length-2, &addr->value[2]);
			addr->length = len;
		     }
			
		else if(addr->value[0] == '+'){
			len = sprintf(buf, "%.*s", addr->length-1, &addr->value[1]);
			addr->length = len;
    		     }
		else{
			len = sprintf(buf, "%.*s", addr->length, &addr->value[0]);
			addr->length = len;
    		}
		//
		strcpy(addr->value, buf);
	}
	else if(addr->type.bits.typenumb == TYPE_OF_NUMBER_ALPHANUMGSM7){
		//
		tc = iconv_open("UTF-8", "UCS-2BE");
		if(tc == (iconv_t)-1){
			// converter not created
			len = sprintf(buf, "unknown");
			addr->length = len;
    		}
		else{
			ib = addr->value;
			incnt = addr->length;
			ob = buf;
			outcnt = 256;
			outres = iconv(tc, &ib, &incnt, &ob, &outcnt);
			if(outres == (size_t)-1){
				// convertation failed
				len = sprintf(buf, "unknown");
				addr->length = len;
        	    }
			else{
				len = (ob - buf);
				addr->length = len;
				*(buf+len) = '\0';
        		}
			// close converter
			iconv_close(tc);
    		}
		//
		strcpy(addr->value, buf);
		unsigned short i;
		for(i=0;i<(addr->length);i++)if(addr->value[i]==0x0D)addr->value[i]=' ';
		
	}
}

//char *address_show(char *buf, struct address *addr, int full);

int is_address_equal(address_t *a1, address_t *a2)
{
	if(!a1 || !a2) return 0;
    return strcmp(a1->value, a2->value) == 0;
}

int is_str_nonblank(const char *buf)
{
    return buf && strlen(buf);
}

int is_str_non_unsolicited(const char *buf)
{
    return is_str_nonblank(buf)
        && strncasecmp(buf, "+CPIN:", 6)
        && strncasecmp(buf, "+CFUN:", 6)
        && strcasecmp(buf, "RDY")
        && strcasecmp(buf, "Call Ready")
        && !strstr(buf, "BUSY")
        && !strstr(buf, "RING")
        && !strstr(buf, "NO CARRIER");
}

//------------------------------------------------------------------------------
// is_str_digit()
//------------------------------------------------------------------------------
int is_str_digit(const char *buf){

	int len;
	char *test;
	//
	if(!(test = (char *)buf))
		return 0;
	if(!(len = strlen(test)))
		return 0;

	while(len--){
		if(!isdigit(*test++))
			return 0;
		}

	return 1;
}
//------------------------------------------------------------------------------
// end of is_str_digit()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// is_str_xdigit()
//------------------------------------------------------------------------------
int is_str_xdigit(const char *buf){

	int len;
	char *test;
	//
	if(!(test = (char *)buf))
		return 0;
	if(!(len = strlen(test)))
		return 0;

	while(len--){
		if(!isxdigit(*test++))
			return 0;
		}

	return 1;
	}
//------------------------------------------------------------------------------
// end of is_str_xdigit()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// is_str_printable()
//------------------------------------------------------------------------------
int is_str_printable(const char *buf){

	int len;
	char *test;
	//
	if(!(test = (char *)buf))
		return 0;
	if(!(len = strlen(test)))
		return 0;

	while(len--){
		if(!isprint(*test++))
			return 0;
		}

	return 1;
	}
//------------------------------------------------------------------------------
// end of is_str_printable()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// str_digit_to_bcd()
//------------------------------------------------------------------------------
void str_digit_to_bcd(const char *instr, int inlen, char *out){

	if(inlen%2) inlen--;
	do{
		if(inlen%2){
			*out ^= 0x0f;
			*out++ |= ((*instr) - 0x30);
			}
		else
			*out = (((*instr) - 0x30) << 4) + 0x0f;
		inlen--;
		instr++;
		}while(inlen > 0);
}

//------------------------------------------------------------------------------
// str_bin_to_hex()
// * convert byte by byte data into string HEX presentation *
// * outlen shall be twice bigger than inlen *
// char **instr - input binary buffer
// int *inlen - actual length in input buffer
// char **outstr - output string HEX buffer
// int *outlen - max size of output buffer
//
// * return 0 - success
// * return -1 - on fail
//------------------------------------------------------------------------------
int str_bin_to_hex(char **instr, int *inlen, char **outstr, int *outlen){

	int len;
	int rest;
	char *rdpos;
	char *wrpos;
	char chr;

	// check params
	if((!instr) || (!*instr) || (!inlen) || (!outstr) || (!*outstr) || (!outlen))
		return -1;

	len = *inlen;
	rdpos = *instr;
	rest = *outlen;
	wrpos = *outstr;

	memset(wrpos, 0, rest);

	while(len > 0){
// 		ast_verbose("in=[%s](%d)\n", rdpos, len);
// 		ast_verbose("out=[%s](%d)\n", wrpos, rest);
		if(rest > 0){
			chr = (*rdpos >> 4) & 0xf;
			if(chr <= 9)
				*wrpos = chr + '0';
			else
				*wrpos = chr - 10 + 'A';
			rest--;
			}
		else{
			*instr = rdpos;
			*inlen = len;
			*outstr = wrpos;
			*outlen = rest;
			return -1;
			}
		if(rest > 0){
			chr = (*rdpos) & 0xf;
			if(chr <= 9)
				*(wrpos+1) = chr + '0';
			else
				*(wrpos+1) = chr - 10 + 'A';
			wrpos += 2;
			rest--;
			}
		else{
			*instr = rdpos;
			*inlen = len;
			*outstr = wrpos;
			*outlen = rest;
			return -1;
			}
		len--;
		rdpos++;
		}

	*instr = rdpos;
	*inlen = len;
	*outstr = wrpos;
	*outlen = rest;

	return 0;
}

//------------------------------------------------------------------------------
// str_hex_to_bin()
// * convert string in ASCII-HEX presentation to binary data*
// * inlen shall be twice bigger than outlen *
// char **instr - input ASCII-HEX buffer
// int *inlen - actual length in input buffer
// char **outstr - output binary data buffer
// int *outlen - max size of output buffer
//
// * return 0 - success
// * return -1 - on fail
//------------------------------------------------------------------------------
int str_hex_to_bin(char **instr, int *inlen, char **outstr, int *outlen){

	int len;
	int rest;
	char *rdpos;
	char *wrpos;

	char high_nibble;
	char low_nibble;

	// check params
	if((!instr) || (!*instr) || (!inlen) || (!outstr) || (!*outstr) || (!outlen))
		return -1;

	len = *inlen;
	rdpos = *instr;
	rest = *outlen;
	wrpos = *outstr;

	// check for inbuf has even length
	if(len%2)
		return -1;

	memset(wrpos, 0, rest);

	while(len > 0){
		// check for free space in outbuf
		if(rest > 0){
			// check for valid input symbols
			if(isxdigit(*rdpos) && isxdigit(*(rdpos+1))){
				//
				*rdpos = (char)toupper((int)*rdpos);
				*(rdpos+1) = (char)toupper((int)*(rdpos+1));
				//
				if(isdigit(*rdpos))
					high_nibble = *rdpos - '0';
				else
					high_nibble = *rdpos - 'A' + 10;
				//
				if(isdigit(*(rdpos+1)))
					low_nibble = *(rdpos+1) - '0';
				else
					low_nibble = *(rdpos+1) - 'A' + 10;
				//
				*wrpos = (char)((high_nibble<<4) + (low_nibble));
				}
			else{
				*instr = rdpos;
				*inlen = len;
				*outstr = wrpos;
				*outlen = rest;
				return -1;
				}
			}
		len -= 2;
		rdpos += 2;
		wrpos++;
		rest--;
		}

	*instr = rdpos;
	*inlen = len;
	*outstr = wrpos;
	*outlen = rest;

	return 0;
	}
//------------------------------------------------------------------------------
// end of str_hex_to_bin()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// from_ucs2_to_specset()
//------------------------------------------------------------------------------
int from_ucs2_to_specset(char *specset, char **instr, int *inlen, char **outstr, int *outlen){

	int len;
	int rest;
	char *rdpos;
	char *wrpos;
	char *locbuf;

	char *ib;
	size_t incnt;
	char *ob;
	size_t outcnt;
	size_t outres;

	iconv_t tc;

	unsigned short *symptr;

	// check params
	if((!specset) || (!instr) || (!*instr) || (!inlen) || (!outstr) || (!*outstr) || (!outlen))
		return -1;

	len = *inlen;
	locbuf = malloc(len+2);
	if(!locbuf)
		return -1;
	//
	memcpy(locbuf, *instr, len);
	rdpos = locbuf;
	rest = *outlen;
	wrpos = *outstr;

	memset(wrpos, 0, rest);

	// prepare converter
	tc = iconv_open(specset, "UCS-2BE");
	if(tc == (iconv_t)-1){
		// converter not created
		free(locbuf);
		return -1;
		}
	//
	while(len > 2){
		//
		ib = rdpos;
		incnt = (size_t)len;
		ob = wrpos;
		outcnt = (size_t)rest;
		outres = iconv(tc, &ib, &incnt, &ob, &outcnt);
		if(outres == (size_t)-1){
			if(errno == EILSEQ){
				symptr = (unsigned short *)ib;
				*symptr = UCS2_UNKNOWN_SYMBOL;
				}
			else if(errno == EINVAL)
				break;
			}
		rdpos = ib;
		len = (int)incnt;
		wrpos = ob;
		rest = (int)outcnt;
		}

	// close converter
	iconv_close(tc);

	*instr = rdpos;
	*inlen = len;
	*outstr = wrpos;
	*outlen = rest;

	free(locbuf);
	return 0;
}

const char *hidenum_settings_str(int hideset) {

	switch(hideset){
		case HIDENUM_SUBSCRIPTION: return "subscription";
		case HIDENUM_INVOCATION: return "invocation";
		case HIDENUM_SUPPRESSION: return "suppression";
		case HIDENUM_QUERY: return "query";
		default: return "unknown";
    }
}

const char *callwait_status_str(int type) {

	switch(type){
		case CALLWAIT_STATUS_DISABLE: return "disable";
		case CALLWAIT_STATUS_ENABLE: return "enable";
		case CALLWAIT_STATUS_QUERY: return "query";
		default: return "unknown";
    }
}

incoming_type_t get_incoming_type(const char *inc_type)
{
	int res = INC_TYPE_UNKNOWN;
	if (!strncmp("deny", inc_type, 4)) {
        res = INC_TYPE_DENY;
	} else if (!strncmp("spec", inc_type, 4)) {
        res = INC_TYPE_SPEC;
	}
	return res;
}

const char *incoming_type_str(incoming_type_t type){

    switch (type)
    {
        case INC_TYPE_UNKNOWN:
            return "unknown";
        case INC_TYPE_SPEC:
            return "spec";
        case INC_TYPE_DENY:
            return "deny";
        default:
            return "unknown";
    }
}

outgoing_type_t get_outgoing_type(const char *out_type)
{
	int res = OUT_CALL_UNKNOWN;
	if (!strncmp("deny", out_type, 4)) {
        res = OUT_CALL_DENY;
	} else if (!strncmp("allow", out_type, 4)) {
        res = OUT_CALL_ALLOW;
	}
	return res;
}

const char *outgoing_type_str(outgoing_type_t type)
{
    switch (type)
    {
        case OUT_CALL_UNKNOWN:
            return "unknown";
        case OUT_CALL_ALLOW:
            return "allow";
        case OUT_CALL_DENY:
            return "deny";
        default:
            return "unknown";
    }
}

const char *baudrate_str(ggw8_baudrate_t baudrate)
{
    switch (baudrate) {
        case BR_UNKNOWN:    return "Unknown";
        case BR_AUTO_POWER_ON:
        case BR_AUTO:       return "Auto-bauding";
        case BR_1200:       return "1200";
        case BR_2400:       return "2400";
        case BR_4800:       return "4800";
        case BR_9600:       return "9600";
        case BR_19200:      return "19200";
        case BR_38400:      return "38400";
        case BR_57600:      return "57600";
        case BR_115200:      return "115200";
        default:            return "Invalid";
    }
}

int imei_calc_check_digit(const char *imei)
{

	int res;
	int i;
	int sum;
	char dgt[14];
	char *ch;

	// check for imei present
	if(!imei)
		return -1;
	ch = (char *)imei;
	//
	sum = 0;
	for(i=0; i<14; i++){
		// check input string for valid symbol
		if(*ch == '\0')	return -2;
		if(!isdigit(*ch)) return -3;
		// get value of digit
		dgt[i] = *ch++ - '0';
		// multiply by 2 every 2-th digit and sum
		if(i & 1){
			dgt[i] <<= 1;
			sum += dgt[i]/10 + dgt[i]%10;
        }
		else {
			sum += dgt[i];
		}
    }
	// calc complementary to mod 10
	res = sum%10;
	res = res ? 10 - res : 0;
	res += '0';
	//
	return (char)res;
}

int imei_change(const char *device, const char *imei, char *msgbuf, int msgbuf_len, int *completed, struct ast_cli_args *a)
{
    char imei_buf[16];
	char check_digit;
	int i, imei_len;
	struct gsm_pvt *pvt;
	int rc = -1;

    *completed = 1;

    // check valid IMEI
    if(!imei){
        strncpy(msgbuf, "blank IMEI provided", msgbuf_len);
        goto _exit;
    }

    imei_len = strlen(imei);
    if((imei_len != 14) && (imei_len != 15)){
        snprintf(msgbuf, msgbuf_len, "IMEI %s has wrong length=%d", imei, imei_len);
        goto _exit;
    }
    // copy imei data
    memset(imei_buf, 0, 16);
    memcpy(imei_buf, imei, (imei_len <= 15)?(imei_len):(15));
    // calc IMEI check digit
    if((i = imei_calc_check_digit(imei_buf)) < 0){
        if(i == -2) {
            strncpy(msgbuf, "IMEI is too short", msgbuf_len);
        }
        else if(i == -3) {
            strncpy(msgbuf, "IMEI has illegal character", msgbuf_len);
        }
        else {
            strncpy(msgbuf, "can't calc IMEI check digit", msgbuf_len);
        }
        goto _exit;
    }
    check_digit = (char)i;
    if(imei_len == 15){
        if(check_digit != imei_buf[14]){
            snprintf(msgbuf, msgbuf_len, "IMEI=%s has wrong check digit \"%c\" must be \"%c\"", imei, imei_buf[14], check_digit);
            goto _exit;
        }
    }

	// get channel by name
    // lock rgsm subsystem when find channel
	ast_mutex_lock(&rgsm_lock);
	pvt = find_ch_by_name(device);
	//
	if(!pvt){
        ast_mutex_unlock(&rgsm_lock);
	    snprintf(msgbuf, msgbuf_len, "Channel not found");
        // unlock rgsm subsystem
        goto _exit;
	}
    // lock pvt channel
    ast_mutex_lock(&pvt->lock);

    if (pvt->power_man_disable) {
        snprintf(msgbuf, msgbuf_len, "GSM module IMEI change already in progress");
        goto _cleanup_2;
    }

    //Aug 29, 2014
    //run modem in non gsm mode to know its model
    int was_enabled = pvt->flags.enable;
    pvt->last_mdm_state = pvt->flags.enable;
    if (!was_enabled) {
	    ast_log(AST_LOG_DEBUG, "rgsm: <%s> power on the channel to take module type\n", pvt->name);

	    //rgsm_pvt_power_on() requires pvt->lock unlocked
	    ast_mutex_unlock(&pvt->lock);
        rgsm_pvt_power_on(a, pvt, 0, BR_115200);
        //restore channel lock
        ast_mutex_lock(&pvt->lock);

        while (1) {
            if (!pvt->flags.enable) {
                //something wrong happens in rgsm_pvt_power_on()
                snprintf(msgbuf, msgbuf_len, "Could not take modem type");
                goto _cleanup_2;
            }
            //TODO: add some guard
            if (pvt->flags.module_type_discovered) {
            //if (strlen(pvt->imei)) {
                break;
            }
            rgsm_usleep(pvt, 50000);
        }
    }

/* Sept 2, 2014
    if (!strcmp(pvt->imei, imei_buf)) {
        snprintf(msgbuf, msgbuf_len, "Current and new IMEI are the same");
        if (!was_enabled) {
            gsm_shutdown_channel(pvt);
        }
        goto _cleanup_2;
    }
*/

    //disable manual power management
    pvt->power_man_disable = 1;

    //remove all locks
	ast_mutex_unlock(&pvt->lock);
    ast_mutex_unlock(&rgsm_lock);

    if (pvt->module_type == MODULE_TYPE_SIM900) {
        //
        *completed = 0;
        rc = sim900_fw_update(pvt, msgbuf, msgbuf_len);

        // lock rgsm subsystem before power on a channel
        ast_mutex_lock(&rgsm_lock);
        if (rc == 0) {
            pvt->init_settings.imei_change = 1;
            ast_copy_string(pvt->new_imei, imei_buf, sizeof(pvt->new_imei));
        }
        pvt->power_man_disable = 0;
        //restart channel to complete imei change
        //May 23, 2013: skip fixed baud rates and demand the auto-bauding
        rgsm_pvt_power_on(NULL, pvt, 0, BR_AUTO_POWER_ON);
        ast_mutex_unlock(&rgsm_lock);
    }
    else 
	    if (pvt->module_type == MODULE_TYPE_SIM5320) {
        //
	        *completed = 0;
	        rc = 0;
	        // lock rgsm subsystem before power on a channel
	        ast_mutex_lock(&rgsm_lock);
                pvt->init_settings.imei_change = 1;
                ast_copy_string(pvt->new_imei, imei_buf, sizeof(pvt->new_imei));
	        pvt->power_man_disable = 0;
	        rgsm_pvt_power_reset(NULL, pvt);
	        ast_mutex_unlock(&rgsm_lock);
	    }
	    else
	        if (pvt->module_type == MODULE_TYPE_UC15) {
        //	
	    	    *completed = 0;
		    rc = 0;
		        // lock rgsm subsystem before power on a channel
	            ast_mutex_lock(&rgsm_lock);
        	    pvt->init_settings.imei_change = 1;
                    ast_copy_string(pvt->new_imei, imei_buf, sizeof(pvt->new_imei));
		        pvt->power_man_disable = 0;
		            rgsm_pvt_power_reset(NULL, pvt);
	            ast_mutex_unlock(&rgsm_lock);
		}
		     else {
		        strncpy(msgbuf, "IMEI change not implemented for given module type", msgbuf_len);
			    //clear imei change flag set in this thread
			    pvt->power_man_disable = 0;
		    }

    goto _exit;

_cleanup_2:
	// unlock all locks
	ast_mutex_unlock(&pvt->lock);
	ast_mutex_unlock(&rgsm_lock);
_exit:
	return rc;
}

void us_sleep(int usec)
{
    struct timespec req;
    struct timespec rem;

    if (usec <= 0) return;

    req.tv_sec = usec/1000000;
    req.tv_nsec= (usec % 1000000) * 1000;

    while (1) {
        memset(&rem, 0, sizeof(req));
        if (nanosleep(&req, &rem) != EINTR) break;

        memcpy(&req, &rem, sizeof(req));
    }
}
