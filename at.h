#ifndef AT_H_INCLUDED
#define AT_H_INCLUDED

#include <sys/types.h>
//------------------------------------------------------------------------------

#define AT_BASIC_MAXNUM 18

// AT command enumerator
enum{
	AT_UNKNOWN = 0,

	// V.25TER
	AT_A_SLASH,		// A/ - Re-issues last AT command given
	AT_A,			// ATA - Answer an incoming call
	AT_D,			// ATD - Mobile originated call to dial a number
	AT_D_MEM,		// ATD><MEM><N> - Originate call to phone number in memory <MEM>
	AT_D_CURMEM,	// ATD><N> - Originate call to phone number in current memory
	AT_D_PHBOOK,	// ATD><STR> - Originate call to phone number in memory which corresponds to field <STR>
	AT_DL,			// ATDL - Redial last telephone number used
	AT_E,			// ATE - Set command echo mode
	AT_H,			// ATH - Disconnect existing connection
	AT_I,			// ATI - Display product identification information
	AT_L,			// ATL - Set monitor speaker loudness
	AT_M,			// ATM - Set monitor speaker mode
	AT_3PLUS,		// +++ - Switch from data mode or PPP online mode to command mode
	AT_O,			// ATO - Switch from command mode to data mode
	AT_P,			// ATP - Select pulse dialling
	AT_Q,			// ATQ - Set result code presentation mode
	AT_andW,		// AT&W - Store current parameter to user defined profile
	AT_IPR,			// AT+IPR - Set TE-TA fixed local rate
	
	AT_S0,			// ATS0 - Set number of rings before automatically answering the call
	AT_S3,			// ATS3 - Set command line termination character
	AT_S4,			// ATS4 - Set response formatting character
	AT_S5,			// ATS5 - Set command line editing character
	AT_S6,			// ATS6 - Set pause before blind dialling
	AT_S7,			// ATS7 - Set number of seconds to wait for connection completion
	AT_S8,			// ATS8 - Set number of seconds to wait when comma dial modifier used
	AT_S10,			// ATS10 - Set disconnect delay after indicating the absence of data carrier
	AT_T,			// ATT - Select tone dialling
	AT_V,			// ATV - Set result code format mode
	AT_X,			// ATX - Set connect result code format and monitor call progress
	AT_Z,			// ATZ - Set all current parameters to user defined profile
	AT_andC,		// AT&C - Set DCD function mode
	AT_andD,		// AT&D - Set DTR function mode
	AT_andF,		// AT&F - Set all current parameters to manufacturer defaults
	AT_andV,		// AT&V - Display current configuration
	AT_DR,			// AT+DR - V.42bis data compression reporting control
	AT_DS,			// AT+DS - V.42bis data compression control
	AT_GCAP,		// AT+GCAP - Request complete ta capabilities list
	AT_GMI,			// AT+GMI - Request manufacturer identification
	AT_GMM,			// AT+GMM - Request ta model identification
	AT_GMR,			// AT+GMR - Request ta revision indentification of software release
	AT_GOI,			// AT+GOI - Request global object identification
	AT_GSN,			// AT+GSN - Request ta serial number identification (IMEI)
	AT_ICF,			// AT+ICF - Set TE-TA control character framing
	AT_IFC,			// AT+IFC - Set TE-TA local data flow control
	AT_ILRR,		// AT+ILRR - Set TE-TA local rate reporting mode

	// GSM07.07
	AT_CACM,		// AT+CACM - Accumulated call meter(ACM) reset or query
	AT_CAMM,		// AT+CAMM - Accumulated call meter maximum(ACMMAX) set or query
	AT_CAOC,		// AT+CAOC - Advice of charge
	AT_CBST,		// AT+CBST - Select bearer service type
	AT_CCFC,		// AT+CCFC - Call forwarding number and conditions control
	AT_CCUG,		// AT+CCUG - Closed user group control
	AT_CCWA,		// AT+CCWA - Call waiting control
	AT_CERR,		// AT+CEER - Extended error report
	AT_CGMI,		// AT+CGMI - Request manufacturer identification
	AT_CGMM,		// AT+CGMM - Request model identification
	AT_CGMR,		// AT+CGMR - Request ta revision identification of software release
	AT_CGSN,		// AT+CGSN - Request product serial number identification (identical with +GSN)
	AT_CSCS,		// AT+CSCS - Select TE character set
	AT_CSTA,		// AT+CSTA - Select type of address
	AT_CHLD,		// AT+CHLD - Call hold and multiparty
	AT_CIMI,		// AT+CIMI - Request international mobile subscriber identity
	AT_CKPD,		// AT+CKPD - Keypad control
	AT_CLCC,		// AT+CLCC - List current calls of ME
	AT_CLCK,		// AT+CLCK - Facility lock
	AT_CLIP,		// AT+CLIP - Calling line identification presentation
	AT_CLIR,		// AT+CLIR - Calling line identification restriction
	AT_CMEE,		// AT+CMEE - Report mobile equipment error
	AT_COLP,		// AT+COLP - Connected line identification presentation
	AT_COPS,		// AT+COPS - Operator selection
	AT_CPAS,		// AT+CPAS - Mobile equipment activity status
	AT_CPBF,		// AT+CPBF - Find phonebook entries
	AT_CPBR,		// AT+CPBR - Read current phonebook entries
	AT_CPBS,		// AT+CPBS - Select phonebook memory storage
	AT_CPBW,		// AT+CPBW - Write phonebook entry
	AT_CPIN,		// AT+CPIN - Enter PIN
	AT_CPWD,		// AT+CPWD - Change password
	AT_CR,			// AT+CR - Service reporting control
	AT_CRC,			// AT+CRC - Set cellular result codes for incoming call indication
	AT_CREG,		// AT+CREG - Network registration
	AT_CRLP,		// AT+CRLP - Select radio link protocol parameter
	AT_CRSM,		// AT+CRSM - Restricted SIM access
	AT_CSQ,			// AT+CSQ - Signal quality report
	AT_FCLASS,		// AT+FCLASS - Fax: select, read or test service class
	AT_FMI,			// AT+FMI - Fax: report manufactured ID (SIM300)
	AT_FMM,			// AT+FMM - Fax: report model ID (SIM300)
	AT_FMR,			// AT+FMR - Fax: report revision ID (SIM300)
	AT_VTD,			// AT+VTD - Tone duration
	AT_VTS,			// AT+VTS - DTMF and tone generation
	AT_CMUX,		// AT+CMUX - Multiplexer control
	AT_CNUM,		// AT+CNUM - Subscriber number
	AT_CPOL,		// AT+CPOL - Preferred operator list
	AT_COPN,		// AT+COPN - Read operator names
	AT_CFUN,		// AT+CFUN - Set phone functionality
	AT_CCLK,		// AT+CCLK - Clock
	AT_CSIM,		// AT+CSIM - Generic SIM access
	AT_CALM,		// AT+CALM - Alert sound mode
	AT_CRSL,		// AT+CRSL - Ringer sound level
	AT_CLVL,		// AT+CLVL - Loud speaker volume level
	AT_CMUT,		// AT+CMUT - Mute control
	AT_CPUC,		// AT+CPUC - Price per unit currency table
	AT_CCWE,		// AT+CCWE - Call meter maximum event
	AT_CBC,			// AT+CBC - Battery charge
	AT_CUSD,		// AT+CUSD - Unstructured supplementary service data
	AT_CSSN,		// AT+CSSN - Supplementary services notification
	AT_CSNS,		// AT+CSNS - Single numbering scheme (M10)
	AT_CMOD,		// AT+CMOD - Configure alternating mode calls (M10)

	// GSM07.05
	AT_CMGD,		// AT+CMGD - Delete SMS message
	AT_CMGF,		// AT+CMGF - Select SMS message format
	AT_CMGL,		// AT+CMGL - List SMS messages FROM preferred store
	AT_CMGR,		// AT+CMGR - Read SMS message
	AT_CMGS,		// AT+CMGS - Send SMS message
	AT_CMGW,		// AT+CMGW - Write SMS message to memory
	AT_CMSS,		// AT+CMSS - Send SMS message from storage
	AT_CMGC,		// AT+CMGC - Send SMS command
	AT_CNMI,		// AT+CNMI - New SMS message indications
	AT_CPMS,		// AT+CPMS - Preferred SMS message storage
	AT_CRES,		// AT+CRES - Restore SMS settings
	AT_CSAS,		// AT+CSAS - Save SMS settings
	AT_CSCA,		// AT+CSCA - SMS service center address
	AT_CSCB,		// AT+CSCB - Select cell broadcast SMS messages
	AT_CSDH,		// AT+CSDH - Show SMS text mode parameters
	AT_CSMP,		// AT+CSMP - Set SMS text mode parameters
	AT_CSMS,		// AT+CSMS - Select message service

	//STK
	AT_PSSTKI,      //AT*PSSTKI - SIM Toolkit interface configuration
	AT_PSSTK,       //AT*PSSTK  - SIM Toolkit control
};

//------------------------------------------------------------------------------
// CMS ERROR CODE
#define CMS_ERR_ME_FAILURE				300
#define CMS_ERR_SMS_ME_RESERVED			301
#define CMS_ERR_OPER_NOT_ALLOWED		302
#define CMS_ERR_OPER_NOT_SUPPORTED		303
#define CMS_ERR_INVALID_PDU_MODE		304
#define CMS_ERR_INVALID_TEXT_MODE		305
#define CMS_ERR_SIM_NOT_INSERTED		310
#define CMS_ERR_SIM_PIN_NECESSARY		311
#define CMS_ERR_PH_SIM_PIN_NECESSARY	312
#define CMS_ERR_SIM_FAILURE				313
#define CMS_ERR_SIM_BUSY				314
#define CMS_ERR_SIM_WRONG				315
#define CMS_ERR_SIM_PUK_REQUIRED		316
#define CMS_ERR_SIM_PIN2_REQUIRED		317
#define CMS_ERR_SIM_PUK2_REQUIRED		318
#define CMS_ERR_MEMORY_FAILURE			320
#define CMS_ERR_INVALID_MEMORY_INDEX	321
#define CMS_ERR_MEMORY_FULL				322
#define CMS_ERR_SMSC_ADDRESS_UNKNOWN	330
#define CMS_ERR_NO_NETWORK				331
#define CMS_ERR_NETWORK_TIMEOUT			332
#define CMS_ERR_UNKNOWN					500
#define CMS_ERR_SIM_NOT_READY			512
#define CMS_ERR_UNREAD_SIM_RECORDS		513
#define CMS_ERR_CB_ERROR_UNKNOWN		514
#define CMS_ERR_PS_BUSY					515
#define CMS_ERR_SM_BL_NOT_READY			517
#define CMS_ERR_INVAL_CHARS_IN_PDU		528
#define CMS_ERR_INCORRECT_PDU_LENGTH	529
#define CMS_ERR_INVALID_MTI				530
#define CMS_ERR_INVAL_CHARS_IN_ADDR		531
#define CMS_ERR_INVALID_ADDRESS			532
#define CMS_ERR_INCORRECT_PDU_UDL_L		533
#define CMS_ERR_INCORRECT_SCA_LENGTH	534
#define CMS_ERR_INVALID_FIRST_OCTET		536
#define CMS_ERR_INVALID_COMMAND_TYPE	537
#define CMS_ERR_SRR_BIT_NOT_SET			538
#define CMS_ERR_SRR_BIT_SET				539
#define CMS_ERR_INVALID_UDH_IE			540

//------------------------------------------------------------------------------
// call class
enum{
	CALL_CLASS_VOICE = (1 << 0),
	CALL_CLASS_DATA = (1 << 1),
	CALL_CLASS_FAX = (1 << 2),
};
//------------------------------------------------------------------------------
// parsing param type
enum{
	PRM_TYPE_UNKNOWN = 0,
	PRM_TYPE_STRING = 1,
	PRM_TYPE_INTEGER = 2,
};
//------------------------------------------------------------------------------
// AT command operation
enum{
	AT_OPER_EXEC = (1 << 0),
	AT_OPER_TEST = (1 << 1),
	AT_OPER_READ = (1 << 2),
	AT_OPER_WRITE = (1 << 3),
	AT_OPER_COUNT,
};
//------------------------------------------------------------------------------
// at command
#define MAX_AT_CMD_RESP 2
typedef int add_check_at_resp_fun_t(const char*);

struct at_command{
	int id;
	u_int32_t operations;
	char name[16];
	char response[MAX_AT_CMD_RESP][16];
	char description[256];
	add_check_at_resp_fun_t *check_fun;
};
//------------------------------------------------------------------------------
// at command operation
struct at_command_operation{
	u_int32_t id;
	char str[4];
};
//------------------------------------------------------------------------------
// parsing param
struct parsing_param{
	int type;
	char *buf;
	int len;
};
//------------------------------------------------------------------------------
// general AT command parameters
// exec
// clcc exec
struct at_gen_clcc_exec{
	// integer (mandatory)
	int id;		// line id
	// integer (mandatory)
	int dir;	// call direction
	// integer (mandatory)
	int stat;	// call state
	// integer (mandatory)
	int mode;	// call mode
	// integer (mandatory)
	int mpty;	// call multiparty
	// string (mandatory)
	char *number;	// phone number
	int number_len;
	// integer (mandatory)
	int type;	// number type
};

// csq exec
struct at_gen_csq_exec{
	// integer (mandatory)
	int rssi;	// rssi level
	// integer (mandatory)
	int ber;		// ber level
};

// cnum exec
struct at_gen_cnum_exec{
	// string
	char *alpha;	// optional name
	int alpha_len;
	// string (mandatory)
	char *number;	// phone number
	int number_len;
	// integer (mandatory)
	int type;		// type of address
	// integer
	int speed;		// speed
	// integer
	int service;	// service related to phone number
	// integer
	int itc;		// information transfer capability
};

// read
// clir read
struct at_gen_clir_read{
	// integer (mandatory)
	int n;	// CLIR setting
	// integer (mandatory)
	int m;	// CLIR status
};

// cops read
struct at_gen_cops_read{
	// integer (mandatory)
	int mode;	// mode of registration
	// integer
	int format;	// information transfer capability
	// string
	char *oper;	// operator presented in format above
	int oper_len;
};

// creg read
struct at_gen_creg_read{
	// integer (mandatory)
	int n;			// unsolicited result enable
	// integer (mandatory)
	int stat;		// registration status
	// string
	char *lac;		// location area code
	int lac_len;
	// string
	char *ci;		// cell ID
	int ci_len;
};

// csca read
struct at_gen_csca_read{
	// string (mandatory)
	char *sca;		// service center address
	int sca_len;
	// integer (mandatory)
	int tosca;		// type of service center address
};

// write
// ccwa write
struct at_gen_ccwa_write{
	// integer (mandatory)
	int status;	// call wait status
	// integer (mandatory)
	int class;	// call class
};

// cusd write
struct at_gen_cusd_write{
	// integer (mandatory)
	int n;	// control of registration
	// string (optional)
	char *str;	// ussd response
	int str_len;
	// integer (optional)
	int dcs;	// Cell Broadcast Data Coding Scheme
};

// cmgr write
struct at_gen_cmgr_write{
	// integer (mandatory)
	int stat;
	// string (optional)
	char *alpha;
	int alpha_len;
	// integer (mandatory)
	int length;
};

// unsolicited
// clip unsolicited
struct at_gen_clip_unsol{
	// string (mandatory)
	char *number;	// phone number
	int number_len;
	// integer (mandatory)
	int type;	// type of address
	// string (mandatory)
	char *alphaid;	// phone book number representation
	int alphaid_len;
	// integer (mandatory)
	int cli_validity;	// information of CLI validity
};

struct at_psstk_unsol {
    char *response_type;
    int response_type_len;
    int alpha_id_presence;
    int alphabet;
    char *alpha_id;
    int alpha_id_len;
    int icon_id;
    int icon_qualifier;
    int command_number;
    int default_item_id;
    int help_info;
    int number_of_item;
};

typedef union parser_ptrs {
		// exec
		struct at_gen_clcc_exec *clcc_ex;
		struct at_gen_csq_exec *csq_ex;
		struct at_gen_cnum_exec *cnum_ex;
		// test
		// read
		struct at_gen_clir_read *clir_rd;
		struct at_gen_cops_read *cops_rd;
		struct at_gen_creg_read *creg_rd;
		struct at_gen_clvl_read *clvl_rd;
		struct at_gen_csca_read *csca_rd;
		//18 Feb 2016 Remove unnecessary drivers
		//struct at_sim300_cmic_read *sim300_cmic_rd;
		// 18 Feb 2016 Added mic_rd for any driver
		//struct at_sim900_cmic_read *sim900_cmic_rd;
		//struct at_m10_qmic_read *m10_qmic_rd;
		// 18 Feb 2016 Added mic_rd for any driver
		void *mic_rd;
		//TODO: Reimplement that for use void type
		//struct at_sim300_csmins_read *sim300_csmins_rd;
		// 18 Feb 2016 Added mic_rd for any driver
		//struct at_sim900_csmins_read *sim900_csmins_rd;
		//struct at_m10_qsimstat_read *m10_qsimstat_rd;
		// 18 Feb 2016 Added mic_rd for any driver
		void *simstat_rd;
		// write
		struct at_gen_ccwa_write *ccwa_wr;
		struct at_gen_cusd_write *cusd_wr;
		struct at_gen_cmgr_write *cmgr_wr;
		// unsolicited
		struct at_gen_clip_unsol *clip_un;
		struct at_psstk_unsol *psstk_un;
} parser_ptrs_t;

//------------------------------------------------------------------------------
// extern
extern const char ctrlz;

extern const struct at_command basic_at_com_list[/*AT_BASIC_MAXNUM*/];

// prototype
extern struct at_command *get_at_com_by_id(int id, const struct at_command *list, int maxnum);
extern int is_at_com_done(const char *response);
extern int is_at_com_response(struct at_command *at, const char *response);
extern char *get_at_com_oper_by_id(u_int32_t oper);


// prototype parse function
// exec
int at_gen_clcc_exec_parse(const char *fld, int fld_len, struct at_gen_clcc_exec *clcc);
int at_gen_csq_exec_parse(const char *fld, int fld_len, struct at_gen_csq_exec *csq);
int at_gen_cnum_exec_parse(const char *fld, int fld_len, struct at_gen_cnum_exec *cnum);
// read
int at_gen_clir_read_parse(const char *fld, int fld_len, struct at_gen_clir_read *clir);
int at_gen_cops_read_parse(const char *fld, int fld_len, struct at_gen_cops_read *cops);
int at_gen_creg_read_parse(const char *fld, int fld_len, struct at_gen_creg_read *creg);
int at_gen_csca_read_parse(const char *fld, int fld_len, struct at_gen_csca_read *csca);
// write
int at_gen_ccwa_write_parse(const char *fld, int fld_len, struct at_gen_ccwa_write *ccwa);
int at_gen_cusd_write_parse(const char *fld, int fld_len, struct at_gen_cusd_write *cusd);
int at_gen_cmgr_write_parse(const char *fld, int fld_len, struct at_gen_cmgr_write *cmgr);
// unsolicited
int at_gen_clip_unsol_parse(const char *fld, int fld_len, struct at_gen_clip_unsol *clip);
int at_psstk_unsol_parse(const char *fld, int fld_len, struct at_psstk_unsol *psstk);



#endif // AT_H_INCLUDED
