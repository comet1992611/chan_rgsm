#include <sys/types.h>
#include <asterisk/paths.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "chan_rgsm.h"
#include "at.h"
#include "rgsm_uc15.h"
#include "rgsm_utilities.h"
#include "rgsm_manager.h"

const struct at_command uc15_at_com_list[/*AT_UC15_MAXNUM*/] = {
	// int id; u_int32_t operations; char name[16]; char response[MAX_AT_CMD_RESP][16]; char description[256]; add_check_at_resp_fun_t *check_fun;
	{AT_UC15_UNKNOWN, AT_OPER_EXEC, "", {"", ""}, "", is_str_printable},
	// UC15 V.25TER V1.04
	{AT_UC15_A_SLASH, AT_OPER_EXEC, "A/", {"", ""}, "Re-issues last AT command given", NULL},
	{AT_UC15_A, AT_OPER_EXEC, "ATA", {"", ""}, "Answer an incoming call", NULL},
	{AT_UC15_D, AT_OPER_EXEC, "ATD", {"", ""}, "Mobile originated call to dial a number", NULL},
	{AT_UC15_D_CURMEM, AT_OPER_EXEC, "ATD", {"", ""}, "Originate call to phone number in current memory", NULL},
	{AT_UC15_D_PHBOOK, AT_OPER_EXEC, "ATD", {"", ""}, "Originate call to phone number in memory which corresponds to field <STR>", NULL},
	{AT_UC15_DL, AT_OPER_EXEC, "ATDL",  {"", ""}, "Redial last telephone number used", NULL},
	{AT_UC15_E, AT_OPER_EXEC, "ATE",  {"ATE", ""}, "Set command echo mode", NULL},
	{AT_UC15_H, AT_OPER_EXEC, "ATH",  {"", ""}, "Disconnect existing connection", NULL},
	{AT_UC15_I, AT_OPER_EXEC, "ATI",  {"", ""}, "Display product identification information", is_str_non_unsolicited},
	{AT_UC15_L, AT_OPER_EXEC, "ATL",  {"", ""}, "Set monitor speaker loudness", NULL},
	{AT_UC15_M, AT_OPER_EXEC, "ATM",  {"", ""}, "Set monitor speaker mode", NULL},
	{AT_UC15_3PLUS, AT_OPER_EXEC, "+++",  {"", ""}, "Switch from data mode or PPP online mode to command mode", NULL},
	{AT_UC15_O, AT_OPER_EXEC, "ATO",  {"", ""}, "Switch from command mode to data mode", NULL},
	{AT_UC15_P, AT_OPER_EXEC, "ATP",  {"", ""}, "Select pulse dialling", NULL},
	{AT_UC15_Q, AT_OPER_EXEC, "ATQ",  {"", ""}, "Set result code presentation mode", NULL},
	{AT_UC15_S0, AT_OPER_READ|AT_OPER_WRITE, "ATS0",  {"", ""}, "Set number of rings before automatically answering the call", NULL},
	{AT_UC15_S3, AT_OPER_READ|AT_OPER_WRITE, "ATS3",  {"", ""}, "Set command line termination character", NULL},
	{AT_UC15_S4, AT_OPER_READ|AT_OPER_WRITE, "ATS4",  {"", ""}, "Set response formatting character", NULL},
	{AT_UC15_S5, AT_OPER_READ|AT_OPER_WRITE, "ATS5",  {"", ""}, "Set command line editing character", NULL},
	{AT_UC15_S6, AT_OPER_READ|AT_OPER_WRITE, "ATS6",  {"", ""}, "Set pause before blind dialling", NULL},
	{AT_UC15_S7, AT_OPER_READ|AT_OPER_WRITE, "ATS7",  {"", ""}, "Set number of seconds to wait for connection completion", NULL},
	{AT_UC15_S8, AT_OPER_READ|AT_OPER_WRITE, "ATS8",  {"", ""}, "Set number of seconds to wait when comma dial modifier used", NULL},
	{AT_UC15_S10, AT_OPER_READ|AT_OPER_WRITE, "ATS10",  {"", ""}, "Set disconnect delay after indicating the absence of data carrier", NULL},
	{AT_UC15_T, AT_OPER_EXEC, "ATT",  {"", ""}, "Select tone dialling", NULL},
	{AT_UC15_V, AT_OPER_EXEC, "ATV",  {"", ""}, "Set result code format mode", NULL},
	{AT_UC15_X, AT_OPER_EXEC, "ATX",  {"", ""}, "Set connect result code format and monitor call progress", NULL},
	{AT_UC15_Z, AT_OPER_EXEC, "ATZ",  {"", ""}, "Set all current parameters to user defined profile", NULL},
	{AT_UC15_andC, AT_OPER_EXEC, "AT&C",  {"", ""}, "Set DCD function mode", NULL},
	{AT_UC15_andD, AT_OPER_EXEC, "AT&D",  {"", ""}, "Set DTR function mode", NULL},
	{AT_UC15_andF, AT_OPER_EXEC, "AT&F",  {"", ""}, "Set all current parameters to manufacturer defaults", NULL},
	{AT_UC15_andV, AT_OPER_EXEC, "AT&V",  {"", ""}, "Display current configuration", NULL},
	{AT_UC15_andW, AT_OPER_EXEC, "AT&W",  {"", ""}, "Store current parameter to user defined profile", NULL},
	{AT_UC15_GCAP, AT_OPER_TEST|AT_OPER_EXEC, "AT+GCAP",  {"+GCAP:", ""}, "Request complete TA capabilities list", NULL},
	{AT_UC15_GMI, AT_OPER_TEST|AT_OPER_EXEC, "AT+GMI",  {"", ""}, "Request manufacturer identification", is_str_printable},
	{AT_UC15_GMM, AT_OPER_TEST|AT_OPER_EXEC, "AT+GMM",  {"", ""}, "Request TA model identification", is_str_printable},
	{AT_UC15_GMR, AT_OPER_TEST|AT_OPER_EXEC, "AT+GMR",  {"", ""}, "Request TA revision indentification of software release", is_str_printable},
	{AT_UC15_GOI, AT_OPER_TEST|AT_OPER_EXEC, "AT+GOI",  {"", ""}, "Request global object identification", is_str_printable},
	{AT_UC15_GSN, AT_OPER_TEST|AT_OPER_EXEC, "AT+GSN",  {"", ""}, "Request ta serial number identification (IMEI)", is_str_digit},
	{AT_UC15_ICF, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+ICF",  {"+ICF:", ""}, "Set TE-TA control character framing", NULL},
	{AT_UC15_IFC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+IFC",  {"+IFC:", ""}, "Set TE-TA local data flow control", NULL},
	{AT_UC15_ILRR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+ILRR",  {"+ILRR:", ""}, "Set TE-TA local rate reporting mode", NULL},
	{AT_UC15_IPR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+IPR",  {"+IPR:", ""}, "Set TE-TA fixed local rate", NULL},
	{AT_UC15_HVOIC, AT_OPER_EXEC, "AT+HVOIC",  {"", ""}, "Disconnect voive call only", NULL},

	// UC15 GSM07.07 V1.04
	{AT_UC15_CACM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CACM",  {"+CACM:", ""}, "Accumulated call meter(ACM) reset or query", NULL},
	{AT_UC15_CAMM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CAMM",  {"+CAMM:", ""}, "Accumulated call meter maximum(ACMMAX) set or query", NULL},
	{AT_UC15_CAOC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CAOC",  {"+CAOC:", ""}, "Advice of charge", NULL},
	{AT_UC15_CBST, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CBST",  {"+CBST:", ""}, "Select bearer service type", NULL},
	{AT_UC15_CCFC, AT_OPER_TEST|AT_OPER_WRITE, "AT+CCFC",  {"+CCFC:", ""}, "Call forwarding number and conditions control", NULL},
	{AT_UC15_CCWA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCWA",  {"+CCWA:", ""}, "Call waiting control", NULL},
	{AT_UC15_CEER, AT_OPER_EXEC|AT_OPER_TEST, "AT+CEER",  {"+CEER:", ""}, "Extended error report", NULL},
	{AT_UC15_CGMI, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGMI",  {"", ""}, "Request manufacturer identification", is_str_printable},
	{AT_UC15_CGMM, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGMM",  {"", ""}, "Request model identification", is_str_printable},
	{AT_UC15_CGMR, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGMR",  {"", ""}, "Request TA revision identification of software release", is_str_printable},
	{AT_UC15_CGSN, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGSN",  {"", ""}, "Request product serial number identification (identical with +GSN)", is_str_digit},
	{AT_UC15_CSCS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCS",  {"+CSCS:", ""}, "Select TE character set", NULL},
	{AT_UC15_CSTA, AT_OPER_TEST|AT_OPER_READ, "AT+CSTA",  {"+CSTA:", ""}, "Select type of address", NULL},
	{AT_UC15_CHLD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CHLD",  {"+CHLD:", ""}, "Call hold and multiparty", NULL},
	{AT_UC15_CIMI, AT_OPER_EXEC|AT_OPER_TEST, "AT+CIMI",  {"", ""}, "Request international mobile subscriber identity", is_str_digit},
	{AT_UC15_CKPD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CKPD",  {"", ""}, "Keypad control", NULL},
	{AT_UC15_CLCC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE|AT_OPER_EXEC, "AT+CLCC",  {"+CLCC:", ""}, "List current calls of ME", NULL},
	{AT_UC15_CLCK, AT_OPER_TEST|AT_OPER_WRITE, "AT+CLCK",  {"+CLCK:", ""}, "Facility lock", NULL},
	{AT_UC15_CLIP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CLIP",  {"+CLIP:", ""}, "Calling line identification presentation", NULL},
	{AT_UC15_CLIR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CLIR",  {"+CLIR:", ""}, "Calling line identification restriction", NULL},
	{AT_UC15_CMEE, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMEE",  {"+CMEE:", ""}, "Report mobile equipment error", NULL},
	{AT_UC15_COLP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+COLP",  {"+COLP:", ""}, "Connected line identification presentation", NULL},
	{AT_UC15_COPS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+COPS",  {"+COPS:", ""}, "Operator selection", NULL},
	{AT_UC15_CPAS, AT_OPER_EXEC|AT_OPER_TEST, "AT+CPAS",  {"+CPAS:", ""}, "Mobile equipment activity status", NULL},
	{AT_UC15_CPBF, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPBF",  {"+CPBF:", ""}, "Find phonebook entries", NULL},
	{AT_UC15_CPBR, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPBR",  {"+CPBR:", ""}, "Read current phonebook entries", NULL},
	{AT_UC15_CPBS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPBS",  {"+CPBS:", ""}, "Select phonebook memory storage", NULL},
	{AT_UC15_CPBW, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPBW",  {"+CPBW:", ""}, "Write phonebook entry", NULL},
	{AT_UC15_CPIN, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPIN",  {"+CPIN:", ""}, "Enter PIN", NULL},
	{AT_UC15_CPWD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPWD",  {"+CPWD:", ""}, "Change password", NULL},
	{AT_UC15_CR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CR",  {"+CR:", ""}, "Service reporting control", NULL},
	{AT_UC15_CRC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CRC",  {"+CRC:", ""}, "Set cellular result codes for incoming call indication", NULL},
	{AT_UC15_CREG, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CREG",  {"+CREG:", ""}, "Network registration", NULL},
	{AT_UC15_CRLP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CRLP",  {"+CRLP:", ""}, "Select radio link protocol parameter", NULL},
	{AT_UC15_CRSM, AT_OPER_TEST|AT_OPER_WRITE, "AT+CRSM",  {"+CRSM:", ""}, "Restricted SIM access", NULL},
	{AT_UC15_CSQ, AT_OPER_TEST|AT_OPER_EXEC, "AT+CSQ",  {"+CSQ:", ""}, "Signal quality report", NULL},
	{AT_UC15_FCLASS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+FCLASS",  {"+FCLASS:", ""}, "Fax: select, read or test service class", NULL},
	{AT_UC15_FMI, AT_OPER_TEST|AT_OPER_READ, "AT+FMI",  {"", ""}, "Fax: report manufactured ID", is_str_printable},
	{AT_UC15_FMM, AT_OPER_TEST|AT_OPER_READ, "AT+FMM",  {"", ""}, "Fax: report model ID", is_str_printable},
	{AT_UC15_FMR, AT_OPER_TEST|AT_OPER_EXEC, "AT+FMR",  {"", ""}, "Fax: report revision ID", is_str_printable},
	{AT_UC15_VTD, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+VTD",  {"+VTD:", ""}, "Tone duration", NULL},
	{AT_UC15_VTS, AT_OPER_TEST|AT_OPER_WRITE, "AT+VTS",  {"+VTS:", ""}, "DTMF and tone generation", NULL},
	{AT_UC15_CMUX, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMUX",  {"+CMUX:", ""}, "Multiplexer control", NULL},
	{AT_UC15_CNUM, AT_OPER_TEST|AT_OPER_EXEC, "AT+CNUM",  {"+CNUM:", ""}, "Subscriber number", NULL},
	{AT_UC15_CPOL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPOL",  {"+CPOL:", ""}, "Preferred operator list", NULL},
	{AT_UC15_COPN, AT_OPER_TEST|AT_OPER_EXEC, "AT+COPN",  {"+COPN:", ""}, "Read operator names", NULL},
	{AT_UC15_CFUN, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CFUN",  {"+CFUN:", ""}, "Set phone functionality", NULL},
	{AT_UC15_CCLK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCLK",  {"+CCLK:", ""}, "Clock", NULL},
	{AT_UC15_CSIM, AT_OPER_TEST|AT_OPER_WRITE, "AT+CSIM",  {"+CSIM:", ""}, "Generic SIM access", NULL},
	{AT_UC15_CALM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CALM",  {"+CALM:", ""}, "Alert sound mode", NULL},
	{AT_UC15_CALS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CALS", {"+CALS", ""},  "Alert sound select", NULL},
	{AT_UC15_CRSL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CRSL",  {"+CRSL:", ""}, "Ringer sound level", NULL},
	{AT_UC15_CLVL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CLVL",  {"+CLVL:", ""}, "Loud speaker volume level", NULL},
	{AT_UC15_CMUT, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMUT",  {"+CMUT:", ""}, "Mute control", NULL},
	{AT_UC15_CPUC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPUC",  {"+CPUC:", ""}, "Price per unit currency table", NULL},
	{AT_UC15_CCWE, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCWE",  {"+CCWE:", ""}, "Call meter maximum event", NULL},
	{AT_UC15_CBC, AT_OPER_TEST|AT_OPER_EXEC, "AT+CBC",  {"+CBC:", ""}, "Battery charge", NULL},
	{AT_UC15_CUSD, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CUSD",  {"+CUSD:", ""}, "Unstructured supplementary service data", NULL},
	{AT_UC15_CSSN, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSSN",  {"+CSSN:", ""}, "Supplementary services notification", NULL},

	// UC15 GSM07.05 V1.04
	{AT_UC15_CMGD, AT_OPER_READ|AT_OPER_WRITE, "AT+CMGD",  {"+CMGD:", ""}, "Delete SMS message", NULL},
	{AT_UC15_CMGF, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMGF",  {"+CMGF:", ""}, "Select SMS message format", NULL},
	{AT_UC15_CMGL, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGL",  {"", ""}, "List SMS messages from preferred store", is_str_xdigit},
	{AT_UC15_CMGR, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGR",  {"+CMGR:", ""}, "Read SMS message", is_str_xdigit},
	{AT_UC15_CMGS, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGS",  {"+CMGS:", ""}, "Send SMS message", NULL},
	{AT_UC15_CMGW, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGW",  {"+CMGW:", ""}, "Write SMS message to memory", NULL},
	{AT_UC15_CMSS, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMSS",  {"+CMSS:", ""}, "Send SMS message from storage", NULL},
	{AT_UC15_CNMI, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CNMI",  {"+CNMI:", ""}, "New SMS message indications", NULL},
	{AT_UC15_CPMS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPMS",  {"+CPMS:", ""}, "Preferred SMS message storage", NULL},
	{AT_UC15_CRES, AT_OPER_TEST|AT_OPER_WRITE, "AT+CRES",  {"+CRES:", ""}, "Restore SMS settings", NULL},
	{AT_UC15_CSAS, AT_OPER_TEST|AT_OPER_WRITE, "AT+CSAS",  {"+CSAS:", ""}, "Save SMS settings", NULL},
	{AT_UC15_CSCA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCA",  {"+CSCA:", ""}, "SMS service center address", NULL},
	{AT_UC15_CSCB, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCB",  {"+CSCB:", ""}, "Select cell broadcast SMS messages", NULL},
	{AT_UC15_CSDH, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSDH",  {"+CSDH:", ""}, "Show SMS text mode parameters", NULL},
	{AT_UC15_CSMP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSMP",  {"+CSMP:", ""}, "Set SMS text mode parameters", NULL},
	{AT_UC15_CSMS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSMS",  {"+CSMS:", ""}, "Select message service", NULL},

    //SIM Toolkit
	{AT_UC15_PSSTKI, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT*PSSTKI",  {"*PSSTKI:", ""}, "SIM Toolkit integrafce configuration", NULL},
	{AT_UC15_PSSTK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT*PSSTK",  {"*PSSTK:", ""}, "SIM Toolkit control", NULL},

	// UC15 V1.04
	{AT_UC15_QSIDET, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+QSIDET",  {"+QSIDET:", ""}, "Change the side tone gain level", NULL},
	{AT_UC15_CPOWD, AT_OPER_WRITE, "AT+CPOWD",  {"", ""}, "Power off", NULL},
	{AT_UC15_SPIC, AT_OPER_EXEC, "AT+SPIC",  {"+SPIC:", ""}, "Times remain to input SIM PIN/PUK", NULL},
	{AT_UC15_CMIC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMIC",  {"+CMIC:", ""}, "Change the micophone gain level", NULL},
	{AT_UC15_CALA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CALA",  {"+CALA:", ""}, "Set alarm time", NULL},
	{AT_UC15_CALD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CALD",  {"+CALD:", ""}, "Delete alarm", NULL},
	{AT_UC15_CADC, AT_OPER_TEST|AT_OPER_READ, "AT+CADC",  {"+CADC:", ""}, "Read adc", NULL},
	{AT_UC15_CSNS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSNS",  {"+CSNS:", ""}, "Single numbering scheme", NULL},
	{AT_UC15_CDSCB, AT_OPER_EXEC, "AT+CDSCB",  {"", ""}, "Reset cellbroadcast", NULL},
	{AT_UC15_CMOD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMOD",  {"+CMOD:", ""}, "Configrue alternating mode calls", NULL},
	{AT_UC15_CFGRI, AT_OPER_READ|AT_OPER_WRITE, "AT+CFGRI",  {"+CFGRI:", ""}, "Indicate RI when using URC", NULL},
	{AT_UC15_CLTS, AT_OPER_TEST|AT_OPER_EXEC, "AT+CLTS",  {"+CLTS:", ""}, "Get local timestamp", NULL},
	{AT_UC15_CEXTHS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CEXTHS",  {"+CEXTHS:", ""}, "External headset jack control", NULL},
	{AT_UC15_CEXTBUT, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CEXTBUT",  {"+CEXTBUT:", ""}, "Headset button status reporting", NULL},
	{AT_UC15_CSMINS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSMINS",  {"+CSMINS:", ""}, "SIM inserted status reporting", NULL},
	{AT_UC15_CLDTMF, AT_OPER_EXEC|AT_OPER_WRITE, "AT+CLDTMF",  {"", ""}, "Local DTMF tone generation", NULL},
	{AT_UC15_CDRIND, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CDRIND",  {"+CDRIND:", ""}, "CS voice/data/fax call or GPRS PDP context termination indication", NULL},
	{AT_UC15_CSPN, AT_OPER_READ, "AT+CSPN",  {"+CSPN:", ""}, "Get service provider name from SIM", NULL},
	{AT_UC15_CCVM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCVM",  {"+CCVM:", ""}, "Get and set the voice mail number on the SIM", NULL},
	{AT_UC15_CBAND, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CBAND",  {"+CBAND:", ""}, "Get and set mobile operation band", NULL},
	{AT_UC15_CHF, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CHF",  {"+CHF:", ""}, "Configures hands free operation", NULL},
	{AT_UC15_CHFA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CHFA",  {"+CHFA:", ""}, "Swap the audio channels", NULL},
	{AT_UC15_CSCLK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCLK",  {"+CSCLK:", ""}, "Configure slow clock", NULL},
	{AT_UC15_CENG, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CENG",  {"+CENG:", ""}, "Switch on or off engineering mode", NULL},
	{AT_UC15_SCLASS0, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+SCLASS0",  {"+SCLASS0:", ""}, "Store class 0 SMS to SIM when received class 0 SMS", NULL},
	{AT_UC15_QCCID, AT_OPER_TEST|AT_OPER_EXEC, "AT+QCCID",  {"+QCCID", ""}, "Show ICCID", is_str_xdigit},
	{AT_UC15_CMTE, AT_OPER_READ, "AT+CMTE",  {"+CMTE:", ""}, "Read temperature of module", NULL},
	{AT_UC15_CBTE, AT_OPER_READ, "AT+CBTE",  {"+CBTE:", ""}, "Battery temperature query", NULL},
	{AT_UC15_CSDT, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSDT",  {"+CSDT:", ""}, "Switch on or off detecting SIM card", NULL},
	{AT_UC15_CMGDA, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGDA",  {"+CMGDA:", ""}, "Delete all SMS", NULL},
	{AT_UC15_STTONE, AT_OPER_TEST|AT_OPER_WRITE, "AT+STTONE",  {"+STTONE:", ""}, "Play SIM Toolkit tone", NULL},
	{AT_UC15_SIMTONE, AT_OPER_TEST|AT_OPER_WRITE, "AT+SIMTONE",  {"+SIMTONE:", ""}, "Generate specifically tone", NULL},
	{AT_UC15_CCPD, AT_OPER_READ|AT_OPER_WRITE, "AT+CCPD",  {"+CCPD:", ""}, "Connected line identification presentation without alpha string", NULL},
	{AT_UC15_CGID, AT_OPER_EXEC, "AT+CGID",  {"GID", ""}, "Get SIM card group identifier", NULL},
	{AT_UC15_MORING, AT_OPER_TEST|AT_OPER_WRITE, "AT+MORING",  {"+MORING:", ""}, "Show state of mobile originated call", NULL},
	{AT_UC15_CMGHEX, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMGHEX",  {"CMGHEX:", ""}, "Enable to send non-ASCII character SMS", NULL},
	{AT_UC15_CCODE, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCODE",  {"+CCODE:", ""}, "Configrue SMS code mode", NULL},
	{AT_UC15_CIURC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CIURC",  {"+CIURC:", ""}, "Enable or disable initial URC presentation", NULL},
	{AT_UC15_CPSPWD, AT_OPER_WRITE, "AT+CPSPWD",  {"", ""}, "Change PS super password", NULL},
	{AT_UC15_EXUNSOL, AT_OPER_TEST|AT_OPER_WRITE, "AT+EXUNSOL",  {"+EXUNSOL:", ""}, "Extra unsolicited indications", NULL},
	{AT_UC15_CGMSCLASS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CGMSCLASS",  {"MULTISLOT CLASS:", ""}, "Change GPRS multislot class", NULL},
	{AT_UC15_CDEVICE, AT_OPER_READ, "AT+CDEVICE",  {"", ""}, "View current flash device type", is_str_printable},
	{AT_UC15_CCALR, AT_OPER_TEST|AT_OPER_READ, "AT+CCALR",  {"+CCALR:", ""}, "Call ready query", NULL},
	{AT_UC15_GSV, AT_OPER_EXEC, "AT+GSV",  {"", ""}, "Display product identification information", is_str_printable},
	{AT_UC15_SGPIO, AT_OPER_TEST|AT_OPER_WRITE, "AT+SGPIO",  {"+SGPIO:", ""}, "Control the GPIO", NULL},
	{AT_UC15_SPWM, AT_OPER_TEST|AT_OPER_WRITE, "AT+SPWM",  {"+SPWM:", ""}, "Generate PWM", NULL},
	{AT_UC15_ECHO, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+ECHO",  {"+ECHO:", ""}, "Echo cancellation control", NULL},
	{AT_UC15_CAAS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CAAS",  {"+CAAS:", ""}, "Control auto audio switch", NULL},
	{AT_UC15_SVR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+SVR",  {"+SVR:", ""}, "Configrue voice coding type for voice calls", NULL},
	{AT_UC15_GSMBUSY, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+GSMBUSY",  {"+GSMBUSY:", ""}, "Reject incoming call", NULL},
	{AT_UC15_CEMNL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CEMNL",  {"+CEMNL:", ""}, "Set the list of emergency number", NULL},
	{AT_UC15_CELLLOCK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT*CELLLOCK",  {"*CELLLOCK:", ""}, "Set the list of arfcn which needs to be locked", NULL},
	{AT_UC15_SLEDS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+SLEDS",  {"+SLEDS:", ""}, "Set the timer period of net light", NULL},

    {AT_UC15_AUDG, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+AUDG",  {"+AUDG:", ""}, "Change the audi system gain levels", NULL},
    {AT_UC15_EGMR, AT_OPER_READ|AT_OPER_WRITE, "AT+EGMR",  {"+EGMR:", ""}, "Change IMEI", NULL},
    {AT_UC15_QTONEDET, AT_OPER_WRITE, "AT+QTONEDET",  {"+QTONEDET:", ""}, "DTMF detection", NULL},
    {AT_UC15_QINISTAT, AT_OPER_EXEC|AT_OPER_TEST, "AT+QINISTAT",  {"+QINISTAT:", ""}, "Query status of SIM initialization", NULL},
    {AT_UC15_QURCCFG, AT_OPER_EXEC|AT_OPER_TEST|AT_OPER_WRITE, "AT+QURCCFG",  {"+QURCCFG:", ""}, "Configuration of URC output port", NULL},
    {AT_UC15_QAUDPATH, AT_OPER_EXEC|AT_OPER_TEST|AT_OPER_WRITE, "AT+QAUDPATH",  {"+QAUDPATH:", ""}, "Set the Audio Output Path", NULL}
};


int at_uc15_csmins_read_parse(const char *fld, int fld_len, struct at_uc15_csmins_read *csmins){

	char *sp;
	char *tp;
	char *ep;

#define MAX_CSIMINS_READ_PARAM 2
	struct parsing_param params[MAX_CSIMINS_READ_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!csmins) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CSIMINS_READ_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_uc15_csmins_read
	csmins->n = -1;
	csmins->sim_inserted = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) || (param_cnt < MAX_CSIMINS_READ_PARAM)){
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
	// n (mandatory)
	if(params[0].len > 0){
		tp = params[0].buf;
		while(params[0].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		csmins->n = atoi(params[0].buf);
		}
	else
		return -1;

	// sim_inserted (mandatory)
	if(params[1].len > 0){
		tp = params[1].buf;
		while(params[1].len--){
			if(!isdigit(*tp++))
				return -1;
			}
		csmins->sim_inserted = atoi(params[1].buf);
		}
	else
		return -1;

	return param_cnt;
	}
//------------------------------------------------------------------------------
// end of at_uc15_csmins_read_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_uc15_parse_cmic_read_parse()
//------------------------------------------------------------------------------
int at_uc15_cmic_read_parse(const char *fld, int fld_len, struct at_uc15_cmic_read *cmic){

	char *sp;
	char *tp;
	char *ep;
	int i, ch, val;

#define MAX_CMIC_READ_PARAM 4
	struct parsing_param params[MAX_CMIC_READ_PARAM];
	int param_cnt;

	// check params
	if(!fld) return -1;

	if((fld_len <= 0) || (fld_len > 256)) return -1;

	if(!cmic) return -1;

	// init ptr
	if(!(sp = strchr(fld, ' ')))
		return -1;
	tp = ++sp;
	ep = (char *)fld + fld_len;

	// init params
	for(param_cnt=0; param_cnt<MAX_CMIC_READ_PARAM; param_cnt++){
		params[param_cnt].type = PRM_TYPE_UNKNOWN;
		params[param_cnt].buf = NULL;
		params[param_cnt].len = -1;
		}

	// init at_uc15_cmic_read
	cmic->main_hs_mic = -1;
	cmic->aux_hs_mic = -1;
	cmic->main_hf_mic = -1;
	cmic->aux_hf_mic = -1;

	// search params delimiters
	param_cnt = 0;
	while((tp < ep) && (param_cnt < MAX_CMIC_READ_PARAM)){
		while((tp < ep) && ((*tp == '(') || (*tp == ',')))
			tp++;
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
		while((tp < ep) && (*tp != ')') && (*tp != ',')) tp++;
		if(tp >= ep) tp = ep;
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

	for(i=0; i<param_cnt/2; i+=2){
		if(params[i].len > 0){
			tp = params[i].buf;
			while(params[i].len--){
				if(!isdigit(*tp++))
					return -1;
				}
			ch = atoi(params[i].buf);
			}
		else
			return -1;
		if(params[i+1].len > 0){
			tp = params[i+1].buf;
			while(params[i+1].len--){
				if(!isdigit(*tp++))
					return -1;
				}
			val = atoi(params[i+1].buf);
			}
		else
			return -1;
		switch(ch){
			case 3: // Aux handfree microphone gain level
				cmic->aux_hf_mic = val;
				break;
			case 2: // Main handfree microphone gain level
				cmic->main_hf_mic = val;
				break;
			case 1: // Aux handset microphone gain level
				cmic->aux_hs_mic = val;
				break;
			case 0: // Main handset microphone gain level
				cmic->main_hs_mic = val;
				break;
			default:
				break;
			}
		}

	return param_cnt;
}

void uc15_atp_handle_response(struct gsm_pvt* pvt)
{
    parser_ptrs_t parser_ptrs;

    if (!pvt->cmd_queue.first) return;

    // select by operation
    if(pvt->cmd_queue.first->oper == AT_OPER_EXEC){
        // EXEC operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_UC15_QCCID:
              {
        	char* qccid_ptr =  strstr(pvt->recv_buf, "+QCCID:");
        	if(qccid_ptr !=NULL) qccid_ptr+=8; else qccid_ptr = pvt->recv_buf;
                if(is_str_xdigit(qccid_ptr)){
                    ast_copy_string(pvt->iccid, qccid_ptr, sizeof(pvt->iccid));
                    ast_verbose("rgsm: <%s>: SIM iccid=%s\n", pvt->name, pvt->iccid);
                    //
                    if(pvt->flags.changesim){
                        if(strcmp(pvt->iccid, pvt->iccid_ban)){
                            // new SIM
                            pvt->flags.changesim = 0;
                            pvt->flags.testsim = 0;
                        }
                        else{
                            // old SIM
                            if(pvt->flags.testsim){
                                ast_verbose("rgsm: <%s>: this SIM card used all registration attempts and already inserted\n", pvt->name);
                                pvt->flags.testsim = 0;
                            }
                        }
                    }
                }
    	      }
              break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_UC15_QINISTAT:
              {
//    	        ast_verbose("rgsm: <%s>: AT_UC15_QIINISTAT\n", pvt->name);
        	char* qini_ptr =  strstr(pvt->recv_buf, "+QINISTAT:");
        	if(qini_ptr !=NULL){
//		    ast_verbose("rgsm: <%s>: +QIINISTAT: \n", pvt->name);
                    unsigned char qinistat = qini_ptr[11];
//		    ast_verbose("rgsm: <%s>: +QIINISTAT: %d %d\n", pvt->name, qinistat&0x0F, pvt->mdm_state);
	            if(pvt->mdm_state == MDM_STATE_CHECK_PIN && (qinistat & 0x01)){
    	            // - PIN is not checked
        	    if (!pvt->flags.sim_present) {
            	        ast_verbose("rgsm: <%s>: SIM INSERTED\n", pvt->name);
                	rgsm_man_event_channel(pvt, "SIM inserted", 0);
            	    }
                    if (!pvt->flags.changesim) {
                        ast_verbose("rgsm: <%s>: PIN is not checked!!!\n", pvt->name);
                    }
                    // set SIM present flag
                    pvt->flags.sim_startup = 1;
                    pvt->flags.sim_present = 1;
                    pvt->sim_try_cnt_curr = pvt->sim_try_cnt_conf;
    
	            // stop pinwait timer
	            rgsm_timer_stop(pvt->timers.pinwait);
	            //
	            pvt->flags.cpin_checked = 1;
	            ast_log(AST_LOG_NOTICE, "rgsm: <%s>: %s MDM_STATE=%d\n", pvt->name, pvt->recv_buf, pvt->mdm_state);
	            
                    gsm_query_sim_data(pvt);
    
                    //May 21, 2014
                    //enable sim toolkit notifications as it was configured in chan_rgsm.conf and asap after RDY
                    if (pvt->chnl_config.sim_toolkit) {
                        pvt->flags.stk_capabilities_req = 0;
    
                        //pvt->stk_capabilities equals the number of menu item
                        ast_verbose("rgsm: <%s>: Enable SIM Toolkit capabilities\n", pvt->name);
                        rgsm_atcmd_queue_append(pvt, AT_PSSTKI, AT_OPER_WRITE, 0, 30, 0, "1");
                    }

                    //
                    if (pvt->flags.suspend_now) {
                        // stop all timers
                        memset(&pvt->timers, 0, sizeof(pvt->timers));
                        //
                        pvt->flags.suspend_now = 0;
                        //
                        rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
                        //
                        pvt->mdm_state = MDM_STATE_WAIT_SUSPEND;
                        // start waitsuspend timer
                        rgsm_timer_set(pvt->timers.waitsuspend, waitsuspend_timeout);
                    } else if (pvt->flags.changesim) {
                        gsm_start_simpoll(pvt);
                    } else {
                    	    //
                    	    pvt->mdm_state = MDM_STATE_WAIT_CALL_READY;
                    	    // start callready timer
                    	    rgsm_timer_set(pvt->timers.callready, callready_timeout);
                	}
                    // end of READY
            	    }
		    if (pvt->mdm_state == MDM_STATE_WAIT_CALL_READY && ((qinistat & 0x07) == 0x07)) {
		            //
		    	    if (pvt->flags.suspend_now) {
		                //
		                pvt->flags.suspend_now = 0;
		                // try to set suspend state
		                rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
		                //
		                pvt->mdm_state = MDM_STATE_WAIT_SUSPEND;
		                // start waitsuspend timer
		                rgsm_timer_set(pvt->timers.waitsuspend, waitsuspend_timeout);
		            } else {
		                if (pvt->flags.init ) {
		                    // try to set initial settings
		                    pvt->mdm_state = MDM_STATE_INIT;
		                    // get imsi
		                    rgsm_atcmd_queue_append(pvt, AT_CIMI, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
			            //Stop Call Rerady timer fired
			            rgsm_timer_stop(pvt->timers.initstat);
			            rgsm_timer_stop(pvt->timers.callready);

	            		                    
		                } else {
		            	}
    		            }
    		    }
    		}
    	      }    
	    break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    }
    else if(pvt->cmd_queue.first->oper == AT_OPER_TEST){
        // TEST operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    }
    else if(pvt->cmd_queue.first->oper == AT_OPER_READ){
        // READ operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_UC15_CMIC:
                if(strstr(pvt->recv_buf, "+CMIC:")){
                    parser_ptrs.mic_rd = (void *)&pvt->parser_buf;
                    if(at_uc15_cmic_read_parse(pvt->recv_buf, pvt->recv_len, (struct at_uc15_cmic_read *)parser_ptrs.mic_rd) < 0){
                        // parsing error
                        ast_log(LOG_ERROR, "<%s>: at_uc15_cmic_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                        // check for query
                        if(pvt->querysig.mic_gain){
                            ast_verbose("rgsm: <%s>: qwery(mic_gain): error\n", pvt->name);
                            pvt->querysig.mic_gain = 0;
                        }
                    }
                    else{
                        pvt->mic_gain_curr = ((struct at_uc15_cmic_read *)parser_ptrs.mic_rd)->main_hs_mic;
                        // check for query
                        if(pvt->querysig.mic_gain){
                            ast_verbose("rgsm: <%s>: qwery(mic_gain): %d\n", pvt->name, pvt->mic_gain_curr);
                            pvt->querysig.mic_gain = 0;
                        }
                    }
                }
                else if(strstr(pvt->recv_buf, "ERROR")){
                    // check for query
                    if(pvt->querysig.mic_gain){
                        ast_verbose("rgsm: <%s>: qwery(mic_gain): error\n", pvt->name);
                        pvt->querysig.mic_gain = 0;
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_UC15_CSMINS:
                if(strstr(pvt->recv_buf, "+CSMINS:")){
                    // parse csmins
                    parser_ptrs.simstat_rd = (void *)&pvt->parser_buf;
                    if(at_uc15_csmins_read_parse(pvt->recv_buf, pvt->recv_len, (struct at_uc15_csmins_read *)parser_ptrs.simstat_rd) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_uc15_csmins_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    }
                    else{
                        if(((struct at_uc15_csmins_read *)parser_ptrs.simstat_rd)->sim_inserted != pvt->flags.sim_present){
                            pvt->flags.sim_present = ((struct at_uc15_csmins_read *)parser_ptrs.simstat_rd)->sim_inserted;
                            if(!pvt->flags.sim_present){
                                ast_verbose("rgsm: <%s>: SIM REMOVED UNEXPECTEDLY, start SIM polling\n", pvt->name);
                                //
                                //gsm_next_sim_search(pvt);

                                // reset attempts count

                                gsm_reset_sim_data(pvt);

                                if(pvt->flags.changesim) {
                                    pvt->flags.testsim = 1;
                                }
                                // start simpoll timer
                                if(pvt->mdm_state != MDM_STATE_SUSPEND) {
                                    gsm_start_simpoll(pvt);
                                }
                            } else {
                                ast_verbose("rgsm: <%s>: SIM INSERTED\n", pvt->name);
                                gsm_query_sim_data(pvt);
                            }
                            pvt->sim_try_cnt_curr = pvt->sim_try_cnt_conf;
                        }
                    }
                }
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_UC15_CFUN:
        	if (pvt->mdm_state == MDM_STATE_WAIT_CFUN) {
        	    ast_log(LOG_NOTICE, "<%s>: AT_UC15_CFUN <%s>\n", pvt->name, pvt->recv_buf);
        	    if (!strcasecmp(pvt->recv_buf, "+CFUN: 0")) {
            		// minimum functionality - try to enable GSM module full functionality
            		rgsm_atcmd_queue_append(pvt, AT_CFUN, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "1");
            		// next mgmt state -- check for SIM is READY
            		pvt->mdm_state = MDM_STATE_CHECK_PIN;
//            		rgsm_atcmd_queue_append(pvt, AT_UC15_CPIN, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
            		// start pinwait timer
            		rgsm_timer_set(pvt->timers.pinwait, pinwait_timeout);
            		// end of  CFUN: 0
        	    }
        	    else if (!strcasecmp(pvt->recv_buf, "+CFUN: 1")) {
            	    //
            		if (pvt->flags.cpin_checked) {
                	    //
                	    if (pvt->flags.sim_present) {
                    		gsm_query_sim_data(pvt);
                    		//
                    		pvt->mdm_state = MDM_STATE_INIT;
        		        rgsm_atcmd_queue_append(pvt, AT_CIMI, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
                    		// start callready timer
                	    } else
                    		gsm_start_simpoll(pvt);
            		    } else {
                		pvt->mdm_state = MDM_STATE_CHECK_PIN;
                		// start pinwait timer
                		rgsm_timer_set(pvt->timers.pinwait, pinwait_timeout);
//                		rgsm_atcmd_queue_append(pvt, AT_UC15_CPIN, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
            		    }
        	    } // end of  CFUN: 1
        	}
        	break;
            default:
                break;
        }
    } //read operations
    else if(pvt->cmd_queue.first->oper == AT_OPER_WRITE){
        // WRITE operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_UC15_CMIC:
                if(strstr(pvt->recv_buf, "OK"))
                    pvt->mic_gain_curr = pvt->mic_gain_conf;
                break;
            case AT_UC15_QTONEDET:
                ast_verbose("rgsm: <%s>: DTMF detection %s\n", pvt->name, (strstr(pvt->recv_buf, "OK")?"enable":"disable"));
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    }
} // end of UC15 AT commands processing

static int _write_request(struct gsm_pvt* pvt, char *buf, int bytes_count)
{
    int rc, i, recent_sent, chunk_size;
    recent_sent = i = 0;
    while (bytes_count) {
        chunk_size = bytes_count < GGW8_ATOUT_PAYLOADSIZE ? bytes_count : GGW8_ATOUT_PAYLOADSIZE;
        rc = write(pvt->at_fd, buf+i, chunk_size /*bytes_count*/);
        if(rc < 0) {
            if(errno == EAGAIN){
                us_sleep(recent_sent ? recent_sent*100 : 1000);
                continue;
            }
            return errno;
        }
        bytes_count -= rc;
        i += rc;
        recent_sent = rc;
        //ast_log(AST_LOG_DEBUG, "Written %d bytes\n", rc);

        //approximate time to transmit one byte over 115200 baud is 100us
        us_sleep(rc*200);
    }
    return 0;
}

static int _wait_response(struct gsm_pvt* pvt, int req_bytes_sent, char op_chr, char *extra_bytes_buf, int extra_bytes_count, int resp_timeout_us)
{
  	struct rgsm_timer timer;
	struct timeval timeout;
	char chr;
	int i, rc;

    //approximate time to transmit one byte over 115200 baud is 100us
//    us_sleep(req_bytes_sent*250);

    // read marker
    timeout.tv_sec = resp_timeout_us/1000000;
    timeout.tv_usec = (resp_timeout_us % 1000000);
    rgsm_timer_set(timer, timeout);
    do {
        rc = read(pvt->at_fd, &chr, 1);
        if(rc < 0){
            if(errno != EAGAIN){
                //read error
                return -1;
            }
        }
        else if(rc == 1){
            //ast_log(AST_LOG_DEBUG, "Read response byte=0x%.2x\n", chr);
            if (chr == op_chr) break;
        }
        us_sleep(100);
    } while(is_rgsm_timer_active(timer));

    // check for completion
    if(is_rgsm_timer_fired(timer)){
        //timeout
        return -2;
    }

    // read data
    for(i=0; i < extra_bytes_count; i++) {
        timeout.tv_sec = resp_timeout_us/1000000;
        timeout.tv_usec = (resp_timeout_us % 1000000);
        rgsm_timer_set(timer, timeout);
        do {
            rc = read(pvt->at_fd, &chr, 1);
            if(rc < 0){
                if(errno != EAGAIN){
                    //read error
                    return -1;
                }
            }
            else if(rc == 1) {
                //ast_log(AST_LOG_DEBUG, "Read response byte=0x%.2x\n", chr);
                if (extra_bytes_buf) {
                    *(extra_bytes_buf+i) = chr;
                }
                break;
            }
            us_sleep(100);
        } while(is_rgsm_timer_active(timer));

        // check for completion
        if(is_rgsm_timer_fired(timer)){
            //timeour
            return -2;
        }
    }
    return 0;
}

void uc15_check_sim_status(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_UC15_CSMINS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
}

void uc15_set_sim_poll(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_UC15_CSMINS, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
}

void uc15_gsm_query_sim_data(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_UC15_QCCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
    rgsm_atcmd_queue_append(pvt, AT_UC15_QTONEDET, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "1");
}

void uc15_hangup(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, "0");
}

void uc15_change_imei(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_UC15_EGMR, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0,
                                    "1,7,\"%s\"",
                                    pvt->new_imei);
}

void uc15_setup_audio_channel(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_UC15_QAUDPATH, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 0);
//    rgsm_atcmd_queue_append(pvt, AT_UC15_QSIDET, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 0);
    // Set the URC out port
    rgsm_atcmd_queue_append(pvt, AT_UC15_QURCCFG, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "\"urcport\",\"uart1\"", 0);
}

void uc15_check_init_status(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_UC15_QINISTAT, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
}

void uc15_send_ussd(struct gsm_pvt* pvt, unsigned char sub_cmd, char* ussd_str)
{
    //Send USSD request
//    rgsm_atcmd_queue_append(pvt, AT_SIM5320_CUSD, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%s", 1);
    rgsm_atcmd_queue_append(pvt, AT_UC15_CUSD, AT_OPER_WRITE, sub_cmd, 30, 0, "%d,\"%s\"", 1, ussd_str, NULL);

}

int uc15_init(struct gsm_pvt* pvt)
{
    pvt->functions.atp_handle_response = &uc15_atp_handle_response;
    pvt->functions.set_sim_poll = NULL;//&uc15_set_sim_poll;
    pvt->functions.gsm_query_sim_data = &uc15_gsm_query_sim_data;
    pvt->functions.check_sim_status = NULL;//&uc15_check_sim_status;
    pvt->functions.hangup = &uc15_hangup;
    pvt->functions.change_imei = &uc15_change_imei;
    pvt->functions.setup_audio_channel = &uc15_setup_audio_channel;
    pvt->functions.check_init_status = &uc15_check_init_status;
    pvt->functions.send_ussd = &uc15_send_ussd;
    
    pvt->mic_gain_conf = uc15_config.mic_gain;
    pvt->spk_gain_conf = uc15_config.spk_gain;
    pvt->spk_audg_conf = uc15_config.spk_audg;

    rgsm_timer_set(pvt->timers.initstat, runonesecond_timeout);
//  rgsm_atcmd_queue_append(pvt, AT_UC15_CSMINS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
    if(pvt->mdm_state == MDM_STATE_WAIT_CFUN)
	rgsm_atcmd_queue_append(pvt, AT_UC15_CFUN, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
    
    return 0;
}
