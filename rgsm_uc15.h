#ifndef __RGSM_UC15_H__
#define __RGSM_UC15_H__

#include <sys/types.h>
#include "at.h"

#define UC15_MIC_GAIN_DEFAULT     4
#define UC15_SPK_GAIN_DEFAULT     100
#define UC15_SPK_AUDG_DEFAULT     25000

extern const struct at_command uc15_at_com_list[];

//------------------------------------------------------------------------------
// at command id
enum{
	AT_UC15_UNKNOWN = AT_UNKNOWN,
	// UC15 V.25TER V1.04
	AT_UC15_A_SLASH = AT_A_SLASH,		// A/ - Re-issues last AT command given
	AT_UC15_A = AT_A,					// ATA - Answer an incoming call
	AT_UC15_D = AT_D,					// ATD - Mobile originated call to dial a number
	AT_UC15_D_CURMEM = AT_D_CURMEM,	// ATD><N> - Originate call to phone number in current memory
	AT_UC15_D_PHBOOK = AT_D_PHBOOK,	// ATD><STR> - Originate call to phone number in memory which corresponds to field <STR>
	AT_UC15_DL = AT_DL,				// ATDL - Redial last telephone number used
	AT_UC15_E = AT_E,					// ATE - Set command echo mode
	AT_UC15_H = AT_H,					// ATH - Disconnect existing connection
	AT_UC15_I = AT_I,					// ATI - Display product identification information
	AT_UC15_L = AT_L,					// ATL - Set monitor speaker loudness
	AT_UC15_M = AT_M,					// ATM - Set monitor speaker mode
	AT_UC15_3PLUS = AT_3PLUS,			// +++ - Switch from data mode or PPP online mode to command mode
	AT_UC15_O = AT_O,					// ATO - Switch from command mode to data mode
	AT_UC15_P = AT_P,					// ATP - Select pulse dialling
	AT_UC15_Q = AT_Q,					// ATQ - Set result code presentation mode
	AT_UC15_S0 = AT_S0,				// ATS0 - Set number of rings before automatically answering the call
	AT_UC15_S3 = AT_S3,				// ATS3 - Set command line termination character
	AT_UC15_S4 = AT_S4,				// ATS4 - Set response formatting character
	AT_UC15_S5 = AT_S5,				// ATS5 - Set command line editing character
	AT_UC15_S6 = AT_S6,				// ATS6 - Set pause before blind dialling
	AT_UC15_S7 = AT_S7,				// ATS7 - Set number of seconds to wait for connection completion
	AT_UC15_S8 = AT_S8,				// ATS8 - Set number of seconds to wait when comma dial modifier used
	AT_UC15_S10 = AT_S10,				// ATS10 - Set disconnect delay after indicating the absence of data carrier
	AT_UC15_T = AT_T,					// ATT - Select tone dialling
	AT_UC15_V = AT_V,					// ATV - Set result code format mode
	AT_UC15_X = AT_X,					// ATX - Set connect result code format and monitor call progress
	AT_UC15_Z = AT_Z,					// ATZ - Set all current parameters to user defined profile
	AT_UC15_andC = AT_andC,			// AT&C - Set DCD function mode
	AT_UC15_andD = AT_andD,			// AT&D - Set DTR function mode
	AT_UC15_andF = AT_andF,			// AT&F - Set all current parameters to manufacturer defaults
	AT_UC15_andV = AT_andV,			// AT&V - Display current configuration
	AT_UC15_andW = AT_andW,			// AT&W - Store current parameter to user defined profile
	AT_UC15_GCAP = AT_GCAP,			// AT+GCAP - Request complete TA capabilities list
	AT_UC15_GMI = AT_GMI,				// AT+GMI - Request manufacturer identification
	AT_UC15_GMM = AT_GMM,				// AT+GMM - Request TA model identification
	AT_UC15_GMR = AT_GMR,				// AT+GMR - Request TA revision indentification of software release
	AT_UC15_GOI = AT_GOI,				// AT+GOI - Request global object identification
	AT_UC15_GSN = AT_GSN,				// AT+GSN - Request ta serial number identification (IMEI)
	AT_UC15_ICF = AT_ICF,				// AT+ICF - Set TE-TA control character framing
	AT_UC15_IFC = AT_IFC,				// AT+IFC - Set TE-TA local data flow control
	AT_UC15_ILRR = AT_ILRR,			// AT+ILRR - Set TE-TA local rate reporting mode
	AT_UC15_IPR = AT_IPR,				// AT+IPR - Set TE-TA fixed local rate
	AT_UC15_HVOIC,					// AT+HVOIC - Disconnect voive call only

	// UC15 GSM07.07 V1.04
	AT_UC15_CACM = AT_CACM,		// AT+CACM - Accumulated call meter(ACM) reset or query
	AT_UC15_CAMM = AT_CAMM,		// AT+CAMM - Accumulated call meter maximum(ACMMAX) set or query
	AT_UC15_CAOC = AT_CAOC,		// AT+CAOC - Advice of charge
	AT_UC15_CBST = AT_CBST,		// AT+CBST - Select bearer service type
	AT_UC15_CCFC = AT_CCFC,		// AT+CCFC - Call forwarding number and conditions control
	AT_UC15_CCWA = AT_CCWA,		// AT+CCWA - Call waiting control
	AT_UC15_CEER = AT_CERR,		// AT+CEER - Extended error report
	AT_UC15_CGMI = AT_CGMI,		// AT+CGMI - Request manufacturer identification
	AT_UC15_CGMM = AT_CGMM,		// AT+CGMM - Request model identification
	AT_UC15_CGMR = AT_CGMR,		// AT+CGMR - Request TA revision identification of software release
	AT_UC15_CGSN = AT_CGSN,		// AT+CGSN - Request product serial number identification (identical with +GSN)
	AT_UC15_CSCS = AT_CSCS,		// AT+CSCS - Select TE character set
	AT_UC15_CSTA = AT_CSTA,		// AT+CSTA - Select type of address
	AT_UC15_CHLD = AT_CHLD,		// AT+CHLD - Call hold and multiparty
	AT_UC15_CIMI = AT_CIMI,		// AT+CIMI - Request international mobile subscriber identity
	AT_UC15_CKPD = AT_CKPD,		// AT+CKPD - Keypad control
	AT_UC15_CLCC = AT_CLCC,		// AT+CLCC - List current calls of ME
	AT_UC15_CLCK = AT_CLCK,		// AT+CLCK - Facility lock
	AT_UC15_CLIP = AT_CLIP,		// AT+CLIP - Calling line identification presentation
	AT_UC15_CLIR = AT_CLIR,		// AT+CLIR - Calling line identification restriction
	AT_UC15_CMEE = AT_CMEE,		// AT+CMEE - Report mobile equipment error
	AT_UC15_COLP = AT_COLP,		// AT+COLP - Connected line identification presentation
	AT_UC15_COPS = AT_COPS,		// AT+COPS - Operator selection
	AT_UC15_CPAS = AT_CPAS,		// AT+CPAS - Mobile equipment activity status
	AT_UC15_CPBF = AT_CPBF,		// AT+CPBF - Find phonebook entries
	AT_UC15_CPBR = AT_CPBR,		// AT+CPBR - Read current phonebook entries
	AT_UC15_CPBS = AT_CPBS,		// AT+CPBS - Select phonebook memory storage
	AT_UC15_CPBW = AT_CPBW,		// AT+CPBW - Write phonebook entry
	AT_UC15_CPIN = AT_CPIN,		// AT+CPIN - Enter PIN
	AT_UC15_CPWD = AT_CPWD,		// AT+CPWD - Change password
	AT_UC15_CR = AT_CR,			// AT+CR - Service reporting control
	AT_UC15_CRC = AT_CRC,			// AT+CRC - Set cellular result codes for incoming call indication
	AT_UC15_CREG = AT_CREG,		// AT+CREG - Network registration
	AT_UC15_CRLP = AT_CRLP,		// AT+CRLP - Select radio link protocol parameter
	AT_UC15_CRSM = AT_CRSM,		// AT+CRSM - Restricted SIM access
	AT_UC15_CSQ = AT_CSQ,			// AT+CSQ - Signal quality report
	AT_UC15_FCLASS = AT_FCLASS,	// AT+FCLASS - Fax: select, read or test service class
	AT_UC15_FMI = AT_FMI,			// AT+FMI - Fax: report manufactured ID (SIM300)
	AT_UC15_FMM = AT_FMM,			// AT+FMM - Fax: report model ID (SIM300)
	AT_UC15_FMR = AT_FMR,			// AT+FMR - Fax: report revision ID (SIM300)
	AT_UC15_VTD = AT_VTD,			// AT+VTD - Tone duration
	AT_UC15_VTS = AT_VTS,			// AT+VTS - DTMF and tone generation
	AT_UC15_CMUX = AT_CMUX,		// AT+CMUX - Multiplexer control
	AT_UC15_CNUM = AT_CNUM,		// AT+CNUM - Subscriber number
	AT_UC15_CPOL = AT_CPOL,		// AT+CPOL - Preferred operator list
	AT_UC15_COPN = AT_COPN,		// AT+COPN - Read operator names
	AT_UC15_CFUN = AT_CFUN,		// AT+CFUN - Set phone functionality
	AT_UC15_CCLK = AT_CCLK,		// AT+CCLK - Clock
	AT_UC15_CSIM = AT_CSIM,		// AT+CSIM - Generic SIM access
	AT_UC15_CALM = AT_CALM,		// AT+CALM - Alert sound mode
	AT_UC15_CALS,					// AT+CALS - Alert sound select
	AT_UC15_CRSL = AT_CRSL,		// AT+CRSL - Ringer sound level
	AT_UC15_CLVL = AT_CLVL,		// AT+CLVL - Loud speaker volume level
	AT_UC15_CMUT = AT_CMUT,		// AT+CMUT - Mute control
	AT_UC15_CPUC = AT_CPUC,		// AT+CPUC - Price per unit currency table
	AT_UC15_CCWE = AT_CCWE,		// AT+CCWE - Call meter maximum event
	AT_UC15_CBC = AT_CBC,			// AT+CBC - Battery charge
	AT_UC15_CUSD = AT_CUSD,		// AT+CUSD - Unstructured supplementary service data
	AT_UC15_CSSN = AT_CSSN,		// AT+CSSN - Supplementary services notification

	// UC15 GSM07.05 V1.04
	AT_UC15_CMGD = AT_CMGD,		// AT+CMGD - Delete SMS message
	AT_UC15_CMGF = AT_CMGF,		// AT+CMGF - Select SMS message format
	AT_UC15_CMGL = AT_CMGL,		// AT+CMGL - List SMS messages from preferred store
	AT_UC15_CMGR = AT_CMGR,		// AT+CMGR - Read SMS message
	AT_UC15_CMGS = AT_CMGS,		// AT+CMGS - Send SMS message
	AT_UC15_CMGW = AT_CMGW,		// AT+CMGW - Write SMS message to memory
	AT_UC15_CMSS = AT_CMSS,		// AT+CMSS - Send SMS message from storage
	AT_UC15_CNMI = AT_CNMI,		// AT+CNMI - New SMS message indications
	AT_UC15_CPMS = AT_CPMS,		// AT+CPMS - Preferred SMS message storage
	AT_UC15_CRES = AT_CRES,		// AT+CRES - Restore SMS settings
	AT_UC15_CSAS = AT_CSAS,		// AT+CSAS - Save SMS settings
	AT_UC15_CSCA = AT_CSCA,		// AT+CSCA - SMS service center address
	AT_UC15_CSCB = AT_CSCB,		// AT+CSCB - Select cell broadcast SMS messages
	AT_UC15_CSDH = AT_CSDH,		// AT+CSDH - Show SMS text mode parameters
	AT_UC15_CSMP = AT_CSMP,		// AT+CSMP - Set SMS text mode parameters
	AT_UC15_CSMS = AT_CSMS,		// AT+CSMS - Select message service

	//STK
	AT_UC15_PSSTKI = AT_PSSTKI,   //AT*PSSTKI - SIM Toolkit interface configuration
	AT_UC15_PSSTK = AT_PSSTK,     //AT*PSSTK  - SIM Toolkit control

	// UC15 SIMCOM V1.04
	AT_UC15_QSIDET,				// AT+QSIDET - Change the side tone gain level
	AT_UC15_CPOWD,				// AT+CPOWD - Power off
	AT_UC15_SPIC,					// AT+SPIC - Times remain to input SIM PIN/PUK
	AT_UC15_CMIC,					// AT+CMIC - Change the micophone gain level
	AT_UC15_CALA,					// AT+CALA - Set alarm time
	AT_UC15_CALD,					// AT+CALD - Delete alarm
	AT_UC15_CADC,					// AT+CADC - Read adc
	AT_UC15_CSNS,					// AT+CSNS - Single numbering scheme
	AT_UC15_CDSCB,				// AT+CDSCB - Reset cellbroadcast
	AT_UC15_CMOD,					// AT+CMOD - Configrue alternating mode calls
	AT_UC15_CFGRI,				// AT+CFGRI - Indicate RI when using URC
	AT_UC15_CLTS,					// AT+CLTS - Get local timestamp
	AT_UC15_CEXTHS,				// AT+CEXTHS - External headset jack control
	AT_UC15_CEXTBUT,				// AT+CEXTBUT - Headset button status reporting
	AT_UC15_CSMINS,				// AT+CSMINS - SIM inserted status reporting
	AT_UC15_CLDTMF,				// AT+CLDTMF - Local DTMF tone generation
	AT_UC15_CDRIND,				// AT+CDRIND - CS voice/data/fax call or GPRS PDP context termination indication
	AT_UC15_CSPN,					// AT+CSPN - Get service provider name from SIM
	AT_UC15_CCVM,					// AT+CCVM - Get and set the voice mail number on the SIM
	AT_UC15_CBAND,				// AT+CBAND - Get and set mobile operation band
	AT_UC15_CHF,					// AT+CHF - Configures hands free operation
	AT_UC15_CHFA,					// AT+CHFA - Swap the audio channels
	AT_UC15_CSCLK,				// AT+CSCLK - Configure slow clock
	AT_UC15_CENG,					// AT+CENG - Switch on or off engineering mode
	AT_UC15_SCLASS0,				// AT+SCLASS0 - Store class 0 SMS to SIM when received class 0 SMS
	AT_UC15_QCCID,					// AT+CCID - Show ICCID
	AT_UC15_CMTE,					// AT+CMTE - Set critical temperature operating mode or query temperature
	AT_UC15_CBTE,					// AT+CBTE - Read temperature of module
	AT_UC15_CSDT,					// AT+CSDT - Switch on or off detecting SIM card
	AT_UC15_CMGDA,				// AT+CMGDA - Delete all SMS
	AT_UC15_STTONE,				// AT+STTONE - Play SIM Toolkit tone
	AT_UC15_SIMTONE,				// AT+SIMTONE - Generate specifically tone
	AT_UC15_CCPD,					// AT+CCPD - Connected line identification presentation without alpha string
	AT_UC15_CGID,					// AT+CGID - Get SIM card group identifier
	AT_UC15_MORING,				// AT+MORING - Show state of mobile originated call
	AT_UC15_CMGHEX,				// AT+CMGHEX - Enable to send non-ASCII character SMS
	AT_UC15_CCODE,				// AT+CCODE - Configrue SMS code mode
	AT_UC15_CIURC,				// AT+CIURC - Enable or disable initial URC presentation
	AT_UC15_CPSPWD,				// AT+CPSPWD - Change PS super password
	AT_UC15_EXUNSOL,				// AT+EXUNSOL - Extra unsolicited indications
	AT_UC15_CGMSCLASS,			// AT+CGMSCLASS - Change GPRS multislot class
	AT_UC15_CDEVICE,				// AT+CDEVICE - View current flash device type
	AT_UC15_CCALR,				// AT+CCALR - Call ready query
	AT_UC15_GSV,					// AT+GSV - Display product identification information
	AT_UC15_SGPIO,				// AT+SGPIO - Control the GPIO
	AT_UC15_SPWM,					// AT+SPWM - Generate PWM
	AT_UC15_ECHO,					// AT+ECHO - Echo cancellation control
	AT_UC15_CAAS,					// AT+CAAS - Control auto audio switch
	AT_UC15_SVR,					// AT+SVR - Configrue voice coding type for voice calls
	AT_UC15_GSMBUSY,				// AT+GSMBUSY - Reject incoming call
	AT_UC15_CEMNL,				// AT+CEMNL - Set the list of emergency number
	AT_UC15_CELLLOCK,				// AT+CELLLOCK - Set the list of arfcn which needs to be locked
	AT_UC15_SLEDS,				// AT+SLEDS - Set the timer period of net light

    AT_UC15_AUDG,                 // AT+AUDG undocummented command to set gain for audio channels
    AT_UC15_EGMR,                // AT+EGMR=1,7,"new_imei" write command to change the first IMEI
    AT_UC15_QTONEDET,                 // AT+QTONEDET=0(disable)/1(enable) - DTMF detection
    AT_UC15_QINISTAT,             // AT+QINISTAT - Query status of SIM Card initialization
    AT_UC15_QURCCFG,             //Configuration of URC output port
    AT_UC15_QAUDPATH,             //Set the Audio Output Path
	//
	AT_UC15_MAXNUM,
};
//------------------------------------------------------------------------------
// uc15 AT command parameters
// read
// cmic read
struct at_uc15_cmic_read{
	// integer (mandatory)
	int main_hs_mic;	// Main handset microphone gain level
	// integer (mandatory)
	int aux_hs_mic;	// Aux handset microphone gain level
	// integer (mandatory)
	int main_hf_mic;	// Main handfree microphone gain level
	// integer (mandatory)
	int aux_hf_mic;	// Aux handfree microphone gain level
};

// csmins read
struct at_uc15_csmins_read{
	// integer (mandatory)
	int n;	// Unsolicited event code status
	// integer (mandatory)
	int sim_inserted;	// SIM inserted status
};

//------------------------------------------------------------------------------

// prototype parse function
// read
extern int at_uc15_cmic_read_parse(const char *fld, int fld_len, struct at_uc15_cmic_read *cmic);
extern int at_uc15_csmins_read_parse(const char *fld, int fld_len, struct at_uc15_csmins_read *csmins);

//at command processor for UC15
extern void uc15_atp_handle_response(struct gsm_pvt* pvt);

/**
 * rgsm_lock and pvt->lock must not be locked before this call
 * return 0 on success or !=0 on failure
*/
extern int uc15_fw_update(struct gsm_pvt* pvt, char *msgbuf, int msgbuf_len);

/**
 * uc15_init - initialisation of driver
 *
 */

extern int uc15_init(struct gsm_pvt* pvt);

//------------------------------------------------------------------------------
#endif //__RGSM_UC15_H__
