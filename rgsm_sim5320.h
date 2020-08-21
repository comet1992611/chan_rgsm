#ifndef __RGSM_SIM5320_H__
#define __RGSM_SIM5320_H__

#include <sys/types.h>
#include "at.h"

#define SIM5320_MIC_AMP1_DEFAULT     1
#define SIM5320_TX_LVL_DEFAULT     50000
#define SIM5320_RX_LVL_DEFAULT     100
#define SIM5320_TX_GAIN_DEFAULT     50000
#define SIM5320_RX_GAIN_DEFAULT     40000

extern const struct at_command sim5320_at_com_list[/*AT_SIM5320_MAXNUM*/];

//------------------------------------------------------------------------------
// at command id
enum{
	AT_SIM5320_UNKNOWN = AT_UNKNOWN,
	// SIM5320 V.25TER V1.04
	AT_SIM5320_A_SLASH = AT_A_SLASH,		// A/ - Re-issues last AT command given
	AT_SIM5320_A = AT_A,					// ATA - Answer an incoming call
	AT_SIM5320_D = AT_D,					// ATD - Mobile originated call to dial a number
	AT_SIM5320_D_CURMEM = AT_D_CURMEM,	// ATD><N> - Originate call to phone number in current memory
	AT_SIM5320_D_PHBOOK = AT_D_PHBOOK,	// ATD><STR> - Originate call to phone number in memory which corresponds to field <STR>
	AT_SIM5320_DL = AT_DL,				// ATDL - Redial last telephone number used
	AT_SIM5320_E = AT_E,					// ATE - Set command echo mode
	AT_SIM5320_H = AT_H,					// ATH - Disconnect existing connection
	AT_SIM5320_I = AT_I,					// ATI - Display product identification information
	AT_SIM5320_L = AT_L,					// ATL - Set monitor speaker loudness
	AT_SIM5320_M = AT_M,					// ATM - Set monitor speaker mode
	AT_SIM5320_3PLUS = AT_3PLUS,			// +++ - Switch from data mode or PPP online mode to command mode
	AT_SIM5320_O = AT_O,					// ATO - Switch from command mode to data mode
	AT_SIM5320_P = AT_P,					// ATP - Select pulse dialling
	AT_SIM5320_Q = AT_Q,					// ATQ - Set result code presentation mode
	AT_SIM5320_S0 = AT_S0,				// ATS0 - Set number of rings before automatically answering the call
	AT_SIM5320_S3 = AT_S3,				// ATS3 - Set command line termination character
	AT_SIM5320_S4 = AT_S4,				// ATS4 - Set response formatting character
	AT_SIM5320_S5 = AT_S5,				// ATS5 - Set command line editing character
	AT_SIM5320_S6 = AT_S6,				// ATS6 - Set pause before blind dialling
	AT_SIM5320_S7 = AT_S7,				// ATS7 - Set number of seconds to wait for connection completion
	AT_SIM5320_S8 = AT_S8,				// ATS8 - Set number of seconds to wait when comma dial modifier used
	AT_SIM5320_S10 = AT_S10,				// ATS10 - Set disconnect delay after indicating the absence of data carrier
	AT_SIM5320_T = AT_T,					// ATT - Select tone dialling
	AT_SIM5320_V = AT_V,					// ATV - Set result code format mode
	AT_SIM5320_X = AT_X,					// ATX - Set connect result code format and monitor call progress
	AT_SIM5320_Z = AT_Z,					// ATZ - Set all current parameters to user defined profile
	AT_SIM5320_andC = AT_andC,			// AT&C - Set DCD function mode
	AT_SIM5320_andD = AT_andD,			// AT&D - Set DTR function mode
	AT_SIM5320_andF = AT_andF,			// AT&F - Set all current parameters to manufacturer defaults
	AT_SIM5320_andV = AT_andV,			// AT&V - Display current configuration
	AT_SIM5320_andW = AT_andW,			// AT&W - Store current parameter to user defined profile
	AT_SIM5320_GCAP = AT_GCAP,			// AT+GCAP - Request complete TA capabilities list
	AT_SIM5320_GMI = AT_GMI,				// AT+GMI - Request manufacturer identification
	AT_SIM5320_GMM = AT_GMM,				// AT+GMM - Request TA model identification
	AT_SIM5320_GMR = AT_GMR,				// AT+GMR - Request TA revision indentification of software release
	AT_SIM5320_GOI = AT_GOI,				// AT+GOI - Request global object identification
	AT_SIM5320_GSN = AT_GSN,				// AT+GSN - Request ta serial number identification (IMEI)
	AT_SIM5320_ICF = AT_ICF,				// AT+ICF - Set TE-TA control character framing
	AT_SIM5320_IFC = AT_IFC,				// AT+IFC - Set TE-TA local data flow control
	AT_SIM5320_ILRR = AT_ILRR,			// AT+ILRR - Set TE-TA local rate reporting mode
	AT_SIM5320_IPR = AT_IPR,				// AT+IPR - Set TE-TA fixed local rate
	AT_SIM5320_HVOIC,					// AT+HVOIC - Disconnect voive call only

	// SIM5320 GSM07.07 V1.04
	AT_SIM5320_CACM = AT_CACM,		// AT+CACM - Accumulated call meter(ACM) reset or query
	AT_SIM5320_CAMM = AT_CAMM,		// AT+CAMM - Accumulated call meter maximum(ACMMAX) set or query
	AT_SIM5320_CAOC = AT_CAOC,		// AT+CAOC - Advice of charge
	AT_SIM5320_CBST = AT_CBST,		// AT+CBST - Select bearer service type
	AT_SIM5320_CCFC = AT_CCFC,		// AT+CCFC - Call forwarding number and conditions control
	AT_SIM5320_CCWA = AT_CCWA,		// AT+CCWA - Call waiting control
	AT_SIM5320_CEER = AT_CERR,		// AT+CEER - Extended error report
	AT_SIM5320_CGMI = AT_CGMI,		// AT+CGMI - Request manufacturer identification
	AT_SIM5320_CGMM = AT_CGMM,		// AT+CGMM - Request model identification
	AT_SIM5320_CGMR = AT_CGMR,		// AT+CGMR - Request TA revision identification of software release
	AT_SIM5320_CGSN = AT_CGSN,		// AT+CGSN - Request product serial number identification (identical with +GSN)
	AT_SIM5320_CSCS = AT_CSCS,		// AT+CSCS - Select TE character set
	AT_SIM5320_CSTA = AT_CSTA,		// AT+CSTA - Select type of address
	AT_SIM5320_CHLD = AT_CHLD,		// AT+CHLD - Call hold and multiparty
	AT_SIM5320_CIMI = AT_CIMI,		// AT+CIMI - Request international mobile subscriber identity
	AT_SIM5320_CKPD = AT_CKPD,		// AT+CKPD - Keypad control
	AT_SIM5320_CLCC = AT_CLCC,		// AT+CLCC - List current calls of ME
	AT_SIM5320_CLCK = AT_CLCK,		// AT+CLCK - Facility lock
	AT_SIM5320_CLIP = AT_CLIP,		// AT+CLIP - Calling line identification presentation
	AT_SIM5320_CLIR = AT_CLIR,		// AT+CLIR - Calling line identification restriction
	AT_SIM5320_CMEE = AT_CMEE,		// AT+CMEE - Report mobile equipment error
	AT_SIM5320_COLP = AT_COLP,		// AT+COLP - Connected line identification presentation
	AT_SIM5320_COPS = AT_COPS,		// AT+COPS - Operator selection
	AT_SIM5320_CPAS = AT_CPAS,		// AT+CPAS - Mobile equipment activity status
	AT_SIM5320_CPBF = AT_CPBF,		// AT+CPBF - Find phonebook entries
	AT_SIM5320_CPBR = AT_CPBR,		// AT+CPBR - Read current phonebook entries
	AT_SIM5320_CPBS = AT_CPBS,		// AT+CPBS - Select phonebook memory storage
	AT_SIM5320_CPBW = AT_CPBW,		// AT+CPBW - Write phonebook entry
	AT_SIM5320_CPIN = AT_CPIN,		// AT+CPIN - Enter PIN
	AT_SIM5320_CPWD = AT_CPWD,		// AT+CPWD - Change password
	AT_SIM5320_CR = AT_CR,			// AT+CR - Service reporting control
	AT_SIM5320_CRC = AT_CRC,			// AT+CRC - Set cellular result codes for incoming call indication
	AT_SIM5320_CREG = AT_CREG,		// AT+CREG - Network registration
	AT_SIM5320_CRLP = AT_CRLP,		// AT+CRLP - Select radio link protocol parameter
	AT_SIM5320_CRSM = AT_CRSM,		// AT+CRSM - Restricted SIM access
	AT_SIM5320_CSQ = AT_CSQ,			// AT+CSQ - Signal quality report
	AT_SIM5320_FCLASS = AT_FCLASS,	// AT+FCLASS - Fax: select, read or test service class
	AT_SIM5320_FMI = AT_FMI,			// AT+FMI - Fax: report manufactured ID (SIM300)
	AT_SIM5320_FMM = AT_FMM,			// AT+FMM - Fax: report model ID (SIM300)
	AT_SIM5320_FMR = AT_FMR,			// AT+FMR - Fax: report revision ID (SIM300)
	AT_SIM5320_VTD = AT_VTD,			// AT+VTD - Tone duration
	AT_SIM5320_VTS = AT_VTS,			// AT+VTS - DTMF and tone generation
	AT_SIM5320_CMUX = AT_CMUX,		// AT+CMUX - Multiplexer control
	AT_SIM5320_CNUM = AT_CNUM,		// AT+CNUM - Subscriber number
	AT_SIM5320_CPOL = AT_CPOL,		// AT+CPOL - Preferred operator list
	AT_SIM5320_COPN = AT_COPN,		// AT+COPN - Read operator names
	AT_SIM5320_CFUN = AT_CFUN,		// AT+CFUN - Set phone functionality
	AT_SIM5320_CCLK = AT_CCLK,		// AT+CCLK - Clock
	AT_SIM5320_CSIM = AT_CSIM,		// AT+CSIM - Generic SIM access
	AT_SIM5320_CALM = AT_CALM,		// AT+CALM - Alert sound mode
	AT_SIM5320_CALS,					// AT+CALS - Alert sound select
	AT_SIM5320_CRSL = AT_CRSL,		// AT+CRSL - Ringer sound level
	AT_SIM5320_CLVL = AT_CLVL,		// AT+CLVL - Loud speaker volume level
	AT_SIM5320_CMUT = AT_CMUT,		// AT+CMUT - Mute control
	AT_SIM5320_CPUC = AT_CPUC,		// AT+CPUC - Price per unit currency table
	AT_SIM5320_CCWE = AT_CCWE,		// AT+CCWE - Call meter maximum event
	AT_SIM5320_CBC = AT_CBC,			// AT+CBC - Battery charge
	AT_SIM5320_CUSD = AT_CUSD,		// AT+CUSD - Unstructured supplementary service data
	AT_SIM5320_CSSN = AT_CSSN,		// AT+CSSN - Supplementary services notification

	// SIM5320 GSM07.05 V1.04
	AT_SIM5320_CMGD = AT_CMGD,		// AT+CMGD - Delete SMS message
	AT_SIM5320_CMGF = AT_CMGF,		// AT+CMGF - Select SMS message format
	AT_SIM5320_CMGL = AT_CMGL,		// AT+CMGL - List SMS messages from preferred store
	AT_SIM5320_CMGR = AT_CMGR,		// AT+CMGR - Read SMS message
	AT_SIM5320_CMGS = AT_CMGS,		// AT+CMGS - Send SMS message
	AT_SIM5320_CMGW = AT_CMGW,		// AT+CMGW - Write SMS message to memory
	AT_SIM5320_CMSS = AT_CMSS,		// AT+CMSS - Send SMS message from storage
	AT_SIM5320_CNMI = AT_CNMI,		// AT+CNMI - New SMS message indications
	AT_SIM5320_CPMS = AT_CPMS,		// AT+CPMS - Preferred SMS message storage
	AT_SIM5320_CRES = AT_CRES,		// AT+CRES - Restore SMS settings
	AT_SIM5320_CSAS = AT_CSAS,		// AT+CSAS - Save SMS settings
	AT_SIM5320_CSCA = AT_CSCA,		// AT+CSCA - SMS service center address
	AT_SIM5320_CSCB = AT_CSCB,		// AT+CSCB - Select cell broadcast SMS messages
	AT_SIM5320_CSDH = AT_CSDH,		// AT+CSDH - Show SMS text mode parameters
	AT_SIM5320_CSMP = AT_CSMP,		// AT+CSMP - Set SMS text mode parameters
	AT_SIM5320_CSMS = AT_CSMS,		// AT+CSMS - Select message service

	//STK
	AT_SIM5320_PSSTKI = AT_PSSTKI,   //AT*PSSTKI - SIM Toolkit interface configuration
	AT_SIM5320_PSSTK = AT_PSSTK,     //AT*PSSTK  - SIM Toolkit control

	// SIM5320 SIMCOM V1.04
	AT_SIM5320_SIDET,				// AT+SIDET - Change the side tone gain level
	AT_SIM5320_CPOWD,				// AT+CPOWD - Power off
	AT_SIM5320_SPIC,					// AT+SPIC - Times remain to input SIM PIN/PUK
	AT_SIM5320_CMIC,					// AT+CMIC - Change the micophone gain level
	AT_SIM5320_CALA,					// AT+CALA - Set alarm time
	AT_SIM5320_CALD,					// AT+CALD - Delete alarm
	AT_SIM5320_CADC,					// AT+CADC - Read adc
	AT_SIM5320_CSNS,					// AT+CSNS - Single numbering scheme
	AT_SIM5320_CDSCB,				// AT+CDSCB - Reset cellbroadcast
	AT_SIM5320_CMOD,					// AT+CMOD - Configrue alternating mode calls
	AT_SIM5320_CFGRI,				// AT+CFGRI - Indicate RI when using URC
	AT_SIM5320_CLTS,					// AT+CLTS - Get local timestamp
	AT_SIM5320_CEXTHS,				// AT+CEXTHS - External headset jack control
	AT_SIM5320_CEXTBUT,				// AT+CEXTBUT - Headset button status reporting
	AT_SIM5320_CSMINS,				// AT+CSMINS - SIM inserted status reporting
	AT_SIM5320_CLDTMF,				// AT+CLDTMF - Local DTMF tone generation
	AT_SIM5320_CDRIND,				// AT+CDRIND - CS voice/data/fax call or GPRS PDP context termination indication
	AT_SIM5320_CSPN,					// AT+CSPN - Get service provider name from SIM
	AT_SIM5320_CCVM,					// AT+CCVM - Get and set the voice mail number on the SIM
	AT_SIM5320_CBAND,				// AT+CBAND - Get and set mobile operation band
	AT_SIM5320_CHF,					// AT+CHF - Configures hands free operation
	AT_SIM5320_CHFA,					// AT+CHFA - Swap the audio channels
	AT_SIM5320_CSCLK,				// AT+CSCLK - Configure slow clock
	AT_SIM5320_CENG,					// AT+CENG - Switch on or off engineering mode
	AT_SIM5320_SCLASS0,				// AT+SCLASS0 - Store class 0 SMS to SIM when received class 0 SMS
	AT_SIM5320_CCID,					// AT+CCID - Show ICCID
	AT_SIM5320_CMTE,					// AT+CMTE - Set critical temperature operating mode or query temperature
	AT_SIM5320_CBTE,					// AT+CBTE - Read temperature of module
	AT_SIM5320_CSDT,					// AT+CSDT - Switch on or off detecting SIM card
	AT_SIM5320_CMGDA,				// AT+CMGDA - Delete all SMS
	AT_SIM5320_STTONE,				// AT+STTONE - Play SIM Toolkit tone
	AT_SIM5320_SIMTONE,				// AT+SIMTONE - Generate specifically tone
	AT_SIM5320_CCPD,					// AT+CCPD - Connected line identification presentation without alpha string
	AT_SIM5320_CGID,					// AT+CGID - Get SIM card group identifier
	AT_SIM5320_MORING,				// AT+MORING - Show state of mobile originated call
	AT_SIM5320_CMGHEX,				// AT+CMGHEX - Enable to send non-ASCII character SMS
	AT_SIM5320_CCODE,				// AT+CCODE - Configrue SMS code mode
	AT_SIM5320_CIURC,				// AT+CIURC - Enable or disable initial URC presentation
	AT_SIM5320_CPSPWD,				// AT+CPSPWD - Change PS super password
	AT_SIM5320_EXUNSOL,				// AT+EXUNSOL - Extra unsolicited indications
	AT_SIM5320_CGMSCLASS,			// AT+CGMSCLASS - Change GPRS multislot class
	AT_SIM5320_CDEVICE,				// AT+CDEVICE - View current flash device type
	AT_SIM5320_CCALR,				// AT+CCALR - Call ready query
	AT_SIM5320_GSV,					// AT+GSV - Display product identification information
	AT_SIM5320_SGPIO,				// AT+SGPIO - Control the GPIO
	AT_SIM5320_SPWM,					// AT+SPWM - Generate PWM
	AT_SIM5320_ECHO,					// AT+ECHO - Echo cancellation control
	AT_SIM5320_CAAS,					// AT+CAAS - Control auto audio switch
	AT_SIM5320_SVR,					// AT+SVR - Configrue voice coding type for voice calls
	AT_SIM5320_GSMBUSY,				// AT+GSMBUSY - Reject incoming call
	AT_SIM5320_CEMNL,				// AT+CEMNL - Set the list of emergency number
	AT_SIM5320_CELLLOCK,				// AT+CELLLOCK - Set the list of arfcn which needs to be locked
	AT_SIM5320_SLEDS,				// AT+SLEDS - Set the timer period of net light
	AT_SIM5320_CSDVC,				//AT+CSDVC - Switch voice channel device

    AT_SIM5320_AUDG,                 // AT+AUDG undocummented command to set gain for audio channels
    AT_SIM5320_SIMEI,                // AT+SIMEI="new_imei" write command to change module IMEI
    AT_SIM5320_DDET,                 // AT+DDET=0(disable)/1(enable) - DTMF detection
    AT_SIM5320_CVHU,
    AT_SIM5320_CMICAMP1,
    AT_SIM5320_CTXVOL,
    AT_SIM5320_CRXVOL,
    AT_SIM5320_CTXGAIN,
    AT_SIM5320_CRXGAIN,
	//
	AT_SIM5320_MAXNUM,
};
//------------------------------------------------------------------------------
// sim5320 AT command parameters
// read
// cmic read
struct at_sim5320_cmic_read{
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
struct at_sim5320_csmins_read{
	// integer (mandatory)
	int n;	// Unsolicited event code status
	// integer (mandatory)
	int sim_inserted;	// SIM inserted status
};

//------------------------------------------------------------------------------

// prototype parse function
// read
extern int at_sim5320_cmic_read_parse(const char *fld, int fld_len, struct at_sim5320_cmic_read *cmic);
extern int at_sim5320_csmins_read_parse(const char *fld, int fld_len, struct at_sim5320_csmins_read *csmins);

//at command processor for SIM5320
extern void sim5320_atp_handle_response(struct gsm_pvt* pvt);

/**
 * rgsm_lock and pvt->lock must not be locked before this call
 * return 0 on success or !=0 on failure
*/
extern int sim5320_fw_update(struct gsm_pvt* pvt, char *msgbuf, int msgbuf_len);

/**
 * Initialization of driver
 *
 */

extern int sim5320_init(struct gsm_pvt* pvt);

//------------------------------------------------------------------------------
#endif //__RGSM_SIM5320_H__
