#include <sys/types.h>
#include <asterisk/paths.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "chan_rgsm.h"
#include "at.h"
#include "rgsm_sim900.h"
#include "rgsm_utilities.h"
#include "rgsm_manager.h"

const struct at_command sim900_at_com_list[/*AT_SIM900_MAXNUM*/] = {
	// int id; u_int32_t operations; char name[16]; char response[MAX_AT_CMD_RESP][16]; char description[256]; add_check_at_resp_fun_t *check_fun;
	{AT_SIM900_UNKNOWN, AT_OPER_EXEC, "", {"", ""}, "", is_str_printable},
	// SIM900 V.25TER V1.04
	{AT_SIM900_A_SLASH, AT_OPER_EXEC, "A/", {"", ""}, "Re-issues last AT command given", NULL},
	{AT_SIM900_A, AT_OPER_EXEC, "ATA", {"", ""}, "Answer an incoming call", NULL},
	{AT_SIM900_D, AT_OPER_EXEC, "ATD", {"", ""}, "Mobile originated call to dial a number", NULL},
	{AT_SIM900_D_CURMEM, AT_OPER_EXEC, "ATD", {"", ""}, "Originate call to phone number in current memory", NULL},
	{AT_SIM900_D_PHBOOK, AT_OPER_EXEC, "ATD", {"", ""}, "Originate call to phone number in memory which corresponds to field <STR>", NULL},
	{AT_SIM900_DL, AT_OPER_EXEC, "ATDL",  {"", ""}, "Redial last telephone number used", NULL},
	{AT_SIM900_E, AT_OPER_EXEC, "ATE",  {"ATE", ""}, "Set command echo mode", NULL},
	{AT_SIM900_H, AT_OPER_EXEC, "ATH",  {"", ""}, "Disconnect existing connection", NULL},
	{AT_SIM900_I, AT_OPER_EXEC, "ATI",  {"", ""}, "Display product identification information", is_str_non_unsolicited},
	{AT_SIM900_L, AT_OPER_EXEC, "ATL",  {"", ""}, "Set monitor speaker loudness", NULL},
	{AT_SIM900_M, AT_OPER_EXEC, "ATM",  {"", ""}, "Set monitor speaker mode", NULL},
	{AT_SIM900_3PLUS, AT_OPER_EXEC, "+++",  {"", ""}, "Switch from data mode or PPP online mode to command mode", NULL},
	{AT_SIM900_O, AT_OPER_EXEC, "ATO",  {"", ""}, "Switch from command mode to data mode", NULL},
	{AT_SIM900_P, AT_OPER_EXEC, "ATP",  {"", ""}, "Select pulse dialling", NULL},
	{AT_SIM900_Q, AT_OPER_EXEC, "ATQ",  {"", ""}, "Set result code presentation mode", NULL},
	{AT_SIM900_S0, AT_OPER_READ|AT_OPER_WRITE, "ATS0",  {"", ""}, "Set number of rings before automatically answering the call", NULL},
	{AT_SIM900_S3, AT_OPER_READ|AT_OPER_WRITE, "ATS3",  {"", ""}, "Set command line termination character", NULL},
	{AT_SIM900_S4, AT_OPER_READ|AT_OPER_WRITE, "ATS4",  {"", ""}, "Set response formatting character", NULL},
	{AT_SIM900_S5, AT_OPER_READ|AT_OPER_WRITE, "ATS5",  {"", ""}, "Set command line editing character", NULL},
	{AT_SIM900_S6, AT_OPER_READ|AT_OPER_WRITE, "ATS6",  {"", ""}, "Set pause before blind dialling", NULL},
	{AT_SIM900_S7, AT_OPER_READ|AT_OPER_WRITE, "ATS7",  {"", ""}, "Set number of seconds to wait for connection completion", NULL},
	{AT_SIM900_S8, AT_OPER_READ|AT_OPER_WRITE, "ATS8",  {"", ""}, "Set number of seconds to wait when comma dial modifier used", NULL},
	{AT_SIM900_S10, AT_OPER_READ|AT_OPER_WRITE, "ATS10",  {"", ""}, "Set disconnect delay after indicating the absence of data carrier", NULL},
	{AT_SIM900_T, AT_OPER_EXEC, "ATT",  {"", ""}, "Select tone dialling", NULL},
	{AT_SIM900_V, AT_OPER_EXEC, "ATV",  {"", ""}, "Set result code format mode", NULL},
	{AT_SIM900_X, AT_OPER_EXEC, "ATX",  {"", ""}, "Set connect result code format and monitor call progress", NULL},
	{AT_SIM900_Z, AT_OPER_EXEC, "ATZ",  {"", ""}, "Set all current parameters to user defined profile", NULL},
	{AT_SIM900_andC, AT_OPER_EXEC, "AT&C",  {"", ""}, "Set DCD function mode", NULL},
	{AT_SIM900_andD, AT_OPER_EXEC, "AT&D",  {"", ""}, "Set DTR function mode", NULL},
	{AT_SIM900_andF, AT_OPER_EXEC, "AT&F",  {"", ""}, "Set all current parameters to manufacturer defaults", NULL},
	{AT_SIM900_andV, AT_OPER_EXEC, "AT&V",  {"", ""}, "Display current configuration", NULL},
	{AT_SIM900_andW, AT_OPER_EXEC, "AT&W",  {"", ""}, "Store current parameter to user defined profile", NULL},
	{AT_SIM900_GCAP, AT_OPER_TEST|AT_OPER_EXEC, "AT+GCAP",  {"+GCAP:", ""}, "Request complete TA capabilities list", NULL},
	{AT_SIM900_GMI, AT_OPER_TEST|AT_OPER_EXEC, "AT+GMI",  {"", ""}, "Request manufacturer identification", is_str_printable},
	{AT_SIM900_GMM, AT_OPER_TEST|AT_OPER_EXEC, "AT+GMM",  {"", ""}, "Request TA model identification", is_str_printable},
	{AT_SIM900_GMR, AT_OPER_TEST|AT_OPER_EXEC, "AT+GMR",  {"", ""}, "Request TA revision indentification of software release", is_str_printable},
	{AT_SIM900_GOI, AT_OPER_TEST|AT_OPER_EXEC, "AT+GOI",  {"", ""}, "Request global object identification", is_str_printable},
	{AT_SIM900_GSN, AT_OPER_TEST|AT_OPER_EXEC, "AT+GSN",  {"", ""}, "Request ta serial number identification (IMEI)", is_str_digit},
	{AT_SIM900_ICF, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+ICF",  {"+ICF:", ""}, "Set TE-TA control character framing", NULL},
	{AT_SIM900_IFC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+IFC",  {"+IFC:", ""}, "Set TE-TA local data flow control", NULL},
	{AT_SIM900_ILRR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+ILRR",  {"+ILRR:", ""}, "Set TE-TA local rate reporting mode", NULL},
	{AT_SIM900_IPR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+IPR",  {"+IPR:", ""}, "Set TE-TA fixed local rate", NULL},
	{AT_SIM900_HVOIC, AT_OPER_EXEC, "AT+HVOIC",  {"", ""}, "Disconnect voive call only", NULL},

	// SIM900 GSM07.07 V1.04
	{AT_SIM900_CACM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CACM",  {"+CACM:", ""}, "Accumulated call meter(ACM) reset or query", NULL},
	{AT_SIM900_CAMM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CAMM",  {"+CAMM:", ""}, "Accumulated call meter maximum(ACMMAX) set or query", NULL},
	{AT_SIM900_CAOC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CAOC",  {"+CAOC:", ""}, "Advice of charge", NULL},
	{AT_SIM900_CBST, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CBST",  {"+CBST:", ""}, "Select bearer service type", NULL},
	{AT_SIM900_CCFC, AT_OPER_TEST|AT_OPER_WRITE, "AT+CCFC",  {"+CCFC:", ""}, "Call forwarding number and conditions control", NULL},
	{AT_SIM900_CCWA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCWA",  {"+CCWA:", ""}, "Call waiting control", NULL},
	{AT_SIM900_CEER, AT_OPER_EXEC|AT_OPER_TEST, "AT+CEER",  {"+CEER:", ""}, "Extended error report", NULL},
	{AT_SIM900_CGMI, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGMI",  {"", ""}, "Request manufacturer identification", is_str_printable},
	{AT_SIM900_CGMM, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGMM",  {"", ""}, "Request model identification", is_str_printable},
	{AT_SIM900_CGMR, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGMR",  {"", ""}, "Request TA revision identification of software release", is_str_printable},
	{AT_SIM900_CGSN, AT_OPER_EXEC|AT_OPER_TEST, "AT+CGSN",  {"", ""}, "Request product serial number identification (identical with +GSN)", is_str_digit},
	{AT_SIM900_CSCS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCS",  {"+CSCS:", ""}, "Select TE character set", NULL},
	{AT_SIM900_CSTA, AT_OPER_TEST|AT_OPER_READ, "AT+CSTA",  {"+CSTA:", ""}, "Select type of address", NULL},
	{AT_SIM900_CHLD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CHLD",  {"+CHLD:", ""}, "Call hold and multiparty", NULL},
	{AT_SIM900_CIMI, AT_OPER_EXEC|AT_OPER_TEST, "AT+CIMI",  {"", ""}, "Request international mobile subscriber identity", is_str_digit},
	{AT_SIM900_CKPD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CKPD",  {"", ""}, "Keypad control", NULL},
	{AT_SIM900_CLCC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE|AT_OPER_EXEC, "AT+CLCC",  {"+CLCC:", ""}, "List current calls of ME", NULL},
	{AT_SIM900_CLCK, AT_OPER_TEST|AT_OPER_WRITE, "AT+CLCK",  {"+CLCK:", ""}, "Facility lock", NULL},
	{AT_SIM900_CLIP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CLIP",  {"+CLIP:", ""}, "Calling line identification presentation", NULL},
	{AT_SIM900_CLIR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CLIR",  {"+CLIR:", ""}, "Calling line identification restriction", NULL},
	{AT_SIM900_CMEE, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMEE",  {"+CMEE:", ""}, "Report mobile equipment error", NULL},
	{AT_SIM900_COLP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+COLP",  {"+COLP:", ""}, "Connected line identification presentation", NULL},
	{AT_SIM900_COPS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+COPS",  {"+COPS:", ""}, "Operator selection", NULL},
	{AT_SIM900_CPAS, AT_OPER_EXEC|AT_OPER_TEST, "AT+CPAS",  {"+CPAS:", ""}, "Mobile equipment activity status", NULL},
	{AT_SIM900_CPBF, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPBF",  {"+CPBF:", ""}, "Find phonebook entries", NULL},
	{AT_SIM900_CPBR, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPBR",  {"+CPBR:", ""}, "Read current phonebook entries", NULL},
	{AT_SIM900_CPBS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPBS",  {"+CPBS:", ""}, "Select phonebook memory storage", NULL},
	{AT_SIM900_CPBW, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPBW",  {"+CPBW:", ""}, "Write phonebook entry", NULL},
	{AT_SIM900_CPIN, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPIN",  {"+CPIN:", ""}, "Enter PIN", NULL},
	{AT_SIM900_CPWD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CPWD",  {"+CPWD:", ""}, "Change password", NULL},
	{AT_SIM900_CR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CR",  {"+CR:", ""}, "Service reporting control", NULL},
	{AT_SIM900_CRC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CRC",  {"+CRC:", ""}, "Set cellular result codes for incoming call indication", NULL},
	{AT_SIM900_CREG, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CREG",  {"+CREG:", ""}, "Network registration", NULL},
	{AT_SIM900_CRLP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CRLP",  {"+CRLP:", ""}, "Select radio link protocol parameter", NULL},
	{AT_SIM900_CRSM, AT_OPER_TEST|AT_OPER_WRITE, "AT+CRSM",  {"+CRSM:", ""}, "Restricted SIM access", NULL},
	{AT_SIM900_CSQ, AT_OPER_TEST|AT_OPER_EXEC, "AT+CSQ",  {"+CSQ:", ""}, "Signal quality report", NULL},
	{AT_SIM900_FCLASS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+FCLASS",  {"+FCLASS:", ""}, "Fax: select, read or test service class", NULL},
	{AT_SIM900_FMI, AT_OPER_TEST|AT_OPER_READ, "AT+FMI",  {"", ""}, "Fax: report manufactured ID", is_str_printable},
	{AT_SIM900_FMM, AT_OPER_TEST|AT_OPER_READ, "AT+FMM",  {"", ""}, "Fax: report model ID", is_str_printable},
	{AT_SIM900_FMR, AT_OPER_TEST|AT_OPER_EXEC, "AT+FMR",  {"", ""}, "Fax: report revision ID", is_str_printable},
	{AT_SIM900_VTD, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+VTD",  {"+VTD:", ""}, "Tone duration", NULL},
	{AT_SIM900_VTS, AT_OPER_TEST|AT_OPER_WRITE, "AT+VTS",  {"+VTS:", ""}, "DTMF and tone generation", NULL},
	{AT_SIM900_CMUX, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMUX",  {"+CMUX:", ""}, "Multiplexer control", NULL},
	{AT_SIM900_CNUM, AT_OPER_TEST|AT_OPER_EXEC, "AT+CNUM",  {"+CNUM:", ""}, "Subscriber number", NULL},
	{AT_SIM900_CPOL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPOL",  {"+CPOL:", ""}, "Preferred operator list", NULL},
	{AT_SIM900_COPN, AT_OPER_TEST|AT_OPER_EXEC, "AT+COPN",  {"+COPN:", ""}, "Read operator names", NULL},
	{AT_SIM900_CFUN, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CFUN",  {"+CFUN:", ""}, "Set phone functionality", NULL},
	{AT_SIM900_CCLK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCLK",  {"+CCLK:", ""}, "Clock", NULL},
	{AT_SIM900_CSIM, AT_OPER_TEST|AT_OPER_WRITE, "AT+CSIM",  {"+CSIM:", ""}, "Generic SIM access", NULL},
	{AT_SIM900_CALM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CALM",  {"+CALM:", ""}, "Alert sound mode", NULL},
	{AT_SIM900_CALS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CALS", {"+CALS", ""},  "Alert sound select", NULL},
	{AT_SIM900_CRSL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CRSL",  {"+CRSL:", ""}, "Ringer sound level", NULL},
	{AT_SIM900_CLVL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CLVL",  {"+CLVL:", ""}, "Loud speaker volume level", NULL},
	{AT_SIM900_CMUT, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMUT",  {"+CMUT:", ""}, "Mute control", NULL},
	{AT_SIM900_CPUC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPUC",  {"+CPUC:", ""}, "Price per unit currency table", NULL},
	{AT_SIM900_CCWE, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCWE",  {"+CCWE:", ""}, "Call meter maximum event", NULL},
	{AT_SIM900_CBC, AT_OPER_TEST|AT_OPER_EXEC, "AT+CBC",  {"+CBC:", ""}, "Battery charge", NULL},
	{AT_SIM900_CUSD, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CUSD",  {"+CUSD:", ""}, "Unstructured supplementary service data", NULL},
	{AT_SIM900_CSSN, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSSN",  {"+CSSN:", ""}, "Supplementary services notification", NULL},

	// SIM900 GSM07.05 V1.04
	{AT_SIM900_CMGD, AT_OPER_READ|AT_OPER_WRITE, "AT+CMGD",  {"+CMGD:", ""}, "Delete SMS message", NULL},
	{AT_SIM900_CMGF, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMGF",  {"+CMGF:", ""}, "Select SMS message format", NULL},
	{AT_SIM900_CMGL, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGL",  {"", ""}, "List SMS messages from preferred store", is_str_xdigit},
	{AT_SIM900_CMGR, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGR",  {"+CMGR:", ""}, "Read SMS message", is_str_xdigit},
	{AT_SIM900_CMGS, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGS",  {"+CMGS:", ""}, "Send SMS message", NULL},
	{AT_SIM900_CMGW, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGW",  {"+CMGW:", ""}, "Write SMS message to memory", NULL},
	{AT_SIM900_CMSS, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMSS",  {"+CMSS:", ""}, "Send SMS message from storage", NULL},
	{AT_SIM900_CNMI, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CNMI",  {"+CNMI:", ""}, "New SMS message indications", NULL},
	{AT_SIM900_CPMS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CPMS",  {"+CPMS:", ""}, "Preferred SMS message storage", NULL},
	{AT_SIM900_CRES, AT_OPER_TEST|AT_OPER_WRITE, "AT+CRES",  {"+CRES:", ""}, "Restore SMS settings", NULL},
	{AT_SIM900_CSAS, AT_OPER_TEST|AT_OPER_WRITE, "AT+CSAS",  {"+CSAS:", ""}, "Save SMS settings", NULL},
	{AT_SIM900_CSCA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCA",  {"+CSCA:", ""}, "SMS service center address", NULL},
	{AT_SIM900_CSCB, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCB",  {"+CSCB:", ""}, "Select cell broadcast SMS messages", NULL},
	{AT_SIM900_CSDH, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSDH",  {"+CSDH:", ""}, "Show SMS text mode parameters", NULL},
	{AT_SIM900_CSMP, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSMP",  {"+CSMP:", ""}, "Set SMS text mode parameters", NULL},
	{AT_SIM900_CSMS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSMS",  {"+CSMS:", ""}, "Select message service", NULL},

    //SIM Toolkit
	{AT_SIM900_PSSTKI, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT*PSSTKI",  {"*PSSTKI:", ""}, "SIM Toolkit integrafce configuration", NULL},
	{AT_SIM900_PSSTK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT*PSSTK",  {"*PSSTK:", ""}, "SIM Toolkit control", NULL},

	// SIM900 SIMCOM V1.04
	{AT_SIM900_SIDET, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+SIDET",  {"+SIDET:", ""}, "Change the side tone gain level", NULL},
	{AT_SIM900_CPOWD, AT_OPER_WRITE, "AT+CPOWD",  {"", ""}, "Power off", NULL},
	{AT_SIM900_SPIC, AT_OPER_EXEC, "AT+SPIC",  {"+SPIC:", ""}, "Times remain to input SIM PIN/PUK", NULL},
	{AT_SIM900_CMIC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMIC",  {"+CMIC:", ""}, "Change the micophone gain level", NULL},
	{AT_SIM900_CALA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CALA",  {"+CALA:", ""}, "Set alarm time", NULL},
	{AT_SIM900_CALD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CALD",  {"+CALD:", ""}, "Delete alarm", NULL},
	{AT_SIM900_CADC, AT_OPER_TEST|AT_OPER_READ, "AT+CADC",  {"+CADC:", ""}, "Read adc", NULL},
	{AT_SIM900_CSNS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSNS",  {"+CSNS:", ""}, "Single numbering scheme", NULL},
	{AT_SIM900_CDSCB, AT_OPER_EXEC, "AT+CDSCB",  {"", ""}, "Reset cellbroadcast", NULL},
	{AT_SIM900_CMOD, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMOD",  {"+CMOD:", ""}, "Configrue alternating mode calls", NULL},
	{AT_SIM900_CFGRI, AT_OPER_READ|AT_OPER_WRITE, "AT+CFGRI",  {"+CFGRI:", ""}, "Indicate RI when using URC", NULL},
	{AT_SIM900_CLTS, AT_OPER_TEST|AT_OPER_EXEC, "AT+CLTS",  {"+CLTS:", ""}, "Get local timestamp", NULL},
	{AT_SIM900_CEXTHS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CEXTHS",  {"+CEXTHS:", ""}, "External headset jack control", NULL},
	{AT_SIM900_CEXTBUT, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CEXTBUT",  {"+CEXTBUT:", ""}, "Headset button status reporting", NULL},
	{AT_SIM900_CSMINS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSMINS",  {"+CSMINS:", ""}, "SIM inserted status reporting", NULL},
	{AT_SIM900_CLDTMF, AT_OPER_EXEC|AT_OPER_WRITE, "AT+CLDTMF",  {"", ""}, "Local DTMF tone generation", NULL},
	{AT_SIM900_CDRIND, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CDRIND",  {"+CDRIND:", ""}, "CS voice/data/fax call or GPRS PDP context termination indication", NULL},
	{AT_SIM900_CSPN, AT_OPER_READ, "AT+CSPN",  {"+CSPN:", ""}, "Get service provider name from SIM", NULL},
	{AT_SIM900_CCVM, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCVM",  {"+CCVM:", ""}, "Get and set the voice mail number on the SIM", NULL},
	{AT_SIM900_CBAND, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CBAND",  {"+CBAND:", ""}, "Get and set mobile operation band", NULL},
	{AT_SIM900_CHF, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CHF",  {"+CHF:", ""}, "Configures hands free operation", NULL},
	{AT_SIM900_CHFA, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CHFA",  {"+CHFA:", ""}, "Swap the audio channels", NULL},
	{AT_SIM900_CSCLK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSCLK",  {"+CSCLK:", ""}, "Configure slow clock", NULL},
	{AT_SIM900_CENG, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CENG",  {"+CENG:", ""}, "Switch on or off engineering mode", NULL},
	{AT_SIM900_SCLASS0, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+SCLASS0",  {"+SCLASS0:", ""}, "Store class 0 SMS to SIM when received class 0 SMS", NULL},
	{AT_SIM900_CCID, AT_OPER_TEST|AT_OPER_EXEC, "AT+CCID",  {"", ""}, "Show ICCID", is_str_xdigit},
	{AT_SIM900_CMTE, AT_OPER_READ, "AT+CMTE",  {"+CMTE:", ""}, "Read temperature of module", NULL},
	{AT_SIM900_CBTE, AT_OPER_READ, "AT+CBTE",  {"+CBTE:", ""}, "Battery temperature query", NULL},
	{AT_SIM900_CSDT, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CSDT",  {"+CSDT:", ""}, "Switch on or off detecting SIM card", NULL},
	{AT_SIM900_CMGDA, AT_OPER_TEST|AT_OPER_WRITE, "AT+CMGDA",  {"+CMGDA:", ""}, "Delete all SMS", NULL},
	{AT_SIM900_STTONE, AT_OPER_TEST|AT_OPER_WRITE, "AT+STTONE",  {"+STTONE:", ""}, "Play SIM Toolkit tone", NULL},
	{AT_SIM900_SIMTONE, AT_OPER_TEST|AT_OPER_WRITE, "AT+SIMTONE",  {"+SIMTONE:", ""}, "Generate specifically tone", NULL},
	{AT_SIM900_CCPD, AT_OPER_READ|AT_OPER_WRITE, "AT+CCPD",  {"+CCPD:", ""}, "Connected line identification presentation without alpha string", NULL},
	{AT_SIM900_CGID, AT_OPER_EXEC, "AT+CGID",  {"GID", ""}, "Get SIM card group identifier", NULL},
	{AT_SIM900_MORING, AT_OPER_TEST|AT_OPER_WRITE, "AT+MORING",  {"+MORING:", ""}, "Show state of mobile originated call", NULL},
	{AT_SIM900_CMGHEX, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CMGHEX",  {"CMGHEX:", ""}, "Enable to send non-ASCII character SMS", NULL},
	{AT_SIM900_CCODE, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CCODE",  {"+CCODE:", ""}, "Configrue SMS code mode", NULL},
	{AT_SIM900_CIURC, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CIURC",  {"+CIURC:", ""}, "Enable or disable initial URC presentation", NULL},
	{AT_SIM900_CPSPWD, AT_OPER_WRITE, "AT+CPSPWD",  {"", ""}, "Change PS super password", NULL},
	{AT_SIM900_EXUNSOL, AT_OPER_TEST|AT_OPER_WRITE, "AT+EXUNSOL",  {"+EXUNSOL:", ""}, "Extra unsolicited indications", NULL},
	{AT_SIM900_CGMSCLASS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CGMSCLASS",  {"MULTISLOT CLASS:", ""}, "Change GPRS multislot class", NULL},
	{AT_SIM900_CDEVICE, AT_OPER_READ, "AT+CDEVICE",  {"", ""}, "View current flash device type", is_str_printable},
	{AT_SIM900_CCALR, AT_OPER_TEST|AT_OPER_READ, "AT+CCALR",  {"+CCALR:", ""}, "Call ready query", NULL},
	{AT_SIM900_GSV, AT_OPER_EXEC, "AT+GSV",  {"", ""}, "Display product identification information", is_str_printable},
	{AT_SIM900_SGPIO, AT_OPER_TEST|AT_OPER_WRITE, "AT+SGPIO",  {"+SGPIO:", ""}, "Control the GPIO", NULL},
	{AT_SIM900_SPWM, AT_OPER_TEST|AT_OPER_WRITE, "AT+SPWM",  {"+SPWM:", ""}, "Generate PWM", NULL},
	{AT_SIM900_ECHO, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+ECHO",  {"+ECHO:", ""}, "Echo cancellation control", NULL},
	{AT_SIM900_CAAS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CAAS",  {"+CAAS:", ""}, "Control auto audio switch", NULL},
	{AT_SIM900_SVR, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+SVR",  {"+SVR:", ""}, "Configrue voice coding type for voice calls", NULL},
	{AT_SIM900_GSMBUSY, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+GSMBUSY",  {"+GSMBUSY:", ""}, "Reject incoming call", NULL},
	{AT_SIM900_CEMNL, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+CEMNL",  {"+CEMNL:", ""}, "Set the list of emergency number", NULL},
	{AT_SIM900_CELLLOCK, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT*CELLLOCK",  {"*CELLLOCK:", ""}, "Set the list of arfcn which needs to be locked", NULL},
	{AT_SIM900_SLEDS, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+SLEDS",  {"+SLEDS:", ""}, "Set the timer period of net light", NULL},

    {AT_SIM900_AUDG, AT_OPER_TEST|AT_OPER_READ|AT_OPER_WRITE, "AT+AUDG",  {"+AUDG:", ""}, "Change the audi system gain levels", NULL},
    {AT_SIM900_SIMEI, AT_OPER_READ|AT_OPER_WRITE, "AT+SIMEI",  {"+SIMEI:", ""}, "Change IMEI", NULL},
    {AT_SIM900_DDET, AT_OPER_WRITE, "AT+DDET",  {"+DDET:", ""}, "DTMF detection", NULL},
};


int at_sim900_csmins_read_parse(const char *fld, int fld_len, struct at_sim900_csmins_read *csmins){

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

	// init at_sim900_csmins_read
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
// end of at_sim900_csmins_read_parse()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// at_sim900_parse_cmic_read_parse()
//------------------------------------------------------------------------------
int at_sim900_cmic_read_parse(const char *fld, int fld_len, struct at_sim900_cmic_read *cmic){

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

	// init at_sim900_cmic_read
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

void sim900_atp_handle_response(struct gsm_pvt* pvt)
{
    parser_ptrs_t parser_ptrs;

    if (!pvt->cmd_queue.first) return;

    // select by operation
    if(pvt->cmd_queue.first->oper == AT_OPER_EXEC){
        // EXEC operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_SIM900_CCID:
                if(is_str_xdigit(pvt->recv_buf)){
                    ast_copy_string(pvt->iccid, pvt->recv_buf, sizeof(pvt->iccid));
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
            case AT_SIM900_CMIC:
                if(strstr(pvt->recv_buf, "+CMIC:")){
                    parser_ptrs.mic_rd = (void *)&pvt->parser_buf;
                    if(at_sim900_cmic_read_parse(pvt->recv_buf, pvt->recv_len, (struct at_sim900_cmic_read *)parser_ptrs.mic_rd) < 0){
                        // parsing error
                        ast_log(LOG_ERROR, "<%s>: at_sim900_cmic_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
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
                        pvt->mic_gain_curr = ((struct at_sim900_cmic_read *)parser_ptrs.mic_rd)->main_hs_mic;
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
            case AT_SIM900_CSMINS:
                if(strstr(pvt->recv_buf, "+CSMINS:")){
                    // parse csmins
                    parser_ptrs.simstat_rd = (void *)&pvt->parser_buf;
                    if(at_sim900_csmins_read_parse(pvt->recv_buf, pvt->recv_len, (struct at_sim900_csmins_read *)parser_ptrs.simstat_rd) < 0){
                        ast_log(LOG_ERROR, "<%s>: at_sim900_csmins_read_parse(%.*s) error\n", pvt->name, pvt->recv_len, pvt->recv_buf);
                        pvt->recv_buf[0] = '\0';
                        pvt->recv_buf_valid = 0;
                        pvt->recv_len = 0;
                    }
                    else{
                        if(((struct at_sim900_csmins_read *)parser_ptrs.simstat_rd)->sim_inserted != pvt->flags.sim_present){
                            pvt->flags.sim_present = ((struct at_sim900_csmins_read *)parser_ptrs.simstat_rd)->sim_inserted;
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
            default:
                break;
        }
    } //read operations
    else if(pvt->cmd_queue.first->oper == AT_OPER_WRITE){
        // WRITE operations
        switch(pvt->cmd_queue.first->id){
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            case AT_SIM900_CMIC:
                if(strstr(pvt->recv_buf, "OK"))
                    pvt->mic_gain_curr = pvt->mic_gain_conf;
                break;
            case AT_SIM900_DDET:
                ast_verbose("rgsm: <%s>: DTMF detection %s\n", pvt->name, (strstr(pvt->recv_buf, "OK")?"enable":"disable"));
                break;
            //++++++++++++++++++++++++++++++++++++++++++++++++++
            default:
                break;
        }
    }
} // end of SIM900 AT commands processing

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

static void _calc_checksum(char *buf, int buf_bytes, unsigned int *checksum)
{
    unsigned char *pi;
    for (pi = (unsigned char*)buf; buf_bytes; pi++,buf_bytes--) {
        *checksum += *pi;
    }
}

int sim900_fw_update(struct gsm_pvt* pvt, char *msgbuf, int msgbuf_len)
{
    int i, rc, result;
    char chr;
	struct rgsm_timer timer;
	struct timeval timeout;
	char buf[256];
	FILE *fp = NULL;
	int fsize;
	int code_page_bytes;
	unsigned int checksum;
	char extra_buf[16];

    char sim900_code_page[0x800/*GGW8_ATOUT_BUFSIZE*/];
    char sim900_set_storage_equipment_s0[9] = {
        0x04,
        0x00, 0x00, 0x00, 0x90,
        0x00, 0x00, 0x00, 0x00
    };
    char sim900_configuration_for_erased_area_s0[9] = {
        0x09,
        0x00, 0x00, 0x00, 0x90,
        0x00, 0x00, 0x7f, 0x00
    };
    char sim900_set_for_downloaded_code_information[9] = {
        0x04,
        0x00, 0x00, 0x00, 0x90,
        0x00, 0x00, 0x00, 0x00
    };
    char sim900_set_for_downloaded_code_section[5] = {
        0x01,
        0x00, 0x08, 0x00, 0x00
        //LO_UINT16(GGW8_ATOUT_BUFSIZE), HI_UINT16(GGW8_ATOUT_BUFSIZE), 0x00, 0x00
    };
    char sim900_compare_for_downloaded_information[13] = {
        0x15,
        0x00, 0x00, 0x00, 0x90,     //start address: offset=1
        0x00, 0x00, 0x00, 0x00,     //checksum offset=5
        0x00, 0x00, 0x00, 0x00      //actual file size offset=9
    };




    //
    ast_log(AST_LOG_DEBUG, "rgsm: <%s> FW update: started\n", pvt->name);

    result = -1;
    if(pvt->flags.enable){
        gsm_shutdown_channel(pvt);
        ast_log(AST_LOG_DEBUG, "rgsm: <%s> wait for channel disabled...\n", pvt->name);
        // wait for channel shutdown
        while (1) {
            //
            if (!pvt->flags.enable) break;
            us_sleep(10000);   //sleep 10ms
        }
        ast_log(AST_LOG_DEBUG, "rgsm: <%s> disabled\n", pvt->name);
    }
    // reset shutdown flag
    pvt->flags.shutdown = 0;
    pvt->flags.shutdown_now = 0;

    //
    pvt->man_chstate = MAN_CHSTATE_ONSERVICE;
    rgsm_man_event_channel_state(pvt);

    //2sec
    us_sleep(2000000);

    // set enable flag
    //pvt->flags.enable = 1;

    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: set baudrate 115200\n", pvt->name);
    if (ggw8_baudrate_ctl(pvt->ggw8_device, pvt->modem_id, BR_115200))
    {
        strncpy(msgbuf, "Couldn't set baudrate 115200", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_1;
    }

    // send boot control command
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: supply modem power for boot\n", pvt->name);
    if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_BOOT))
    {
        strncpy(msgbuf, "Couldn't supply modem power", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _exit;
    }

    pvt->at_fd = ggw8_open_at(pvt->ggw8_device, pvt->modem_id);
    if (pvt->at_fd == -1) {
        strncpy(msgbuf, "Can't open AT channel", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        //pvt->flags.enable = 0;
        goto _cleanup_1;
    }

    // Detection of synchronous bytes
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Detection of synchronous bytes: started\n", pvt->name);
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    rgsm_timer_set(timer, timeout);
    do {
        // writing synchronous octet
        chr = 0x16;
        if(write(pvt->at_fd, &chr, 1) < 0){
            snprintf(msgbuf, msgbuf_len, "Detection of synchronous bytes: write() fail: %s", strerror(errno));
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            //pvt->flags.enable = 0;
            goto _cleanup_2;
        }
        //ast_log(AST_LOG_DEBUG,"rgsm: <%s>: Sync byte: write=0x16\n", pvt->name);

        // wait for synchronous octet round trip
        us_sleep(5000);

        // wait for synchronous octet
        rc = read(pvt->at_fd, &chr, 1);
        if(rc < 0){
            if(errno != EAGAIN){
                snprintf(msgbuf, msgbuf_len, "Detection of synchronous bytes: read() fail: %s", strerror(errno));
                ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
                goto _cleanup_2;
            }
        }
        else if(rc == 1){
            //ast_log(AST_LOG_DEBUG,"rgsm: <%s>: Sync byte: read=0x%02x\n", pvt->name, chr);
            if(chr == 0x16) break;	// synchronous octet received
        }
    } while(is_rgsm_timer_active(timer));

    // check for entering into downloading procedure
    if(is_rgsm_timer_fired(timer)){
        strncpy(msgbuf, "Detection of synchronous bytes failed: timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    // entering into downloading procedure
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Detected synchronous bytes: module entered into downloading procedure\n", pvt->name);

    //! *** Step #3 ***
    // File of Intel HEX download
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: File of Intel HEX download: started\n", pvt->name);
    sprintf(buf, "%s/firmware/rgsm/%s", ast_config_AST_DATA_DIR, sim900_config.hex_download);
    if(!(fp = fopen(buf, "r"))){
        snprintf(msgbuf, msgbuf_len, "File of Intel HEX download: fopen(%s) fail: %s", buf, strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }

    fsize = 0;
    i = 0;
    // copy hex file into device
    while(fgets(buf, sizeof(buf), fp)){
        i = strlen(buf);
        rc = _write_request(pvt, buf, i);
        if(rc){
            snprintf(msgbuf, msgbuf_len, "File of Intel HEX download: write() fail: %s", strerror(rc));
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }
        fsize += i;
    }
    // wait for success download indication, previous _write_request already waited appropriate timeout
    rc = _wait_response(pvt, 0, 0x30, extra_buf, 1, 10000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "File of Intel HEX download: wait_response() read fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "File of Intel HEX download: wait_response() timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (*extra_buf != 0x00) {
        strncpy(msgbuf, "File of Intel HEX download: unsuccess indication of downloading ", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: File of Intel HEX download: succeeded - total %d bytes\n", pvt->name, fsize);
    }

    //! *** Step #4 - Set the storage equipment ***
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Set the storage equipment: started\n", pvt->name);
    i = sizeof(sim900_set_storage_equipment_s0);
    //set size=0 in struct
    *(unsigned int*)(&sim900_set_storage_equipment_s0[5]) = 0x00;
    if(write(pvt->at_fd, sim900_set_storage_equipment_s0, i) < 0){
        snprintf(msgbuf, msgbuf_len, "Set the storage equipment: write() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }

    rc = _wait_response(pvt, i, sim900_set_storage_equipment_s0[0], NULL, 0, 10000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "Set the storage equipment: read() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "Set the storage equipment: timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Set the storage equipment: succeeded\n", pvt->name);
    }


    //! **** Step #5 Read the flash manufacturer information ****
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Read the flash manufacturer information: started\n", pvt->name);
    chr = 0x02;
    if(write(pvt->at_fd, &chr, 1) < 0){
        snprintf(msgbuf, msgbuf_len, "Read the flash manufacturer information: write() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    us_sleep(100);

    rc = _wait_response(pvt, 1, 0x02, extra_buf, 4, 10000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "Read the flash manufacturer information: read() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "Read the flash manufacturer information: timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Read the flash manufacturer information: succeeded\n", pvt->name);
    }


    //! *** Step #6 - Configuration for erased area of FLASH ***
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Configuration for erased area of FLASH: started\n", pvt->name);
    i = sizeof(sim900_configuration_for_erased_area_s0);
    if(write(pvt->at_fd, sim900_configuration_for_erased_area_s0, i) < 0){
        snprintf(msgbuf, msgbuf_len, "Configuration for erased area of FLASH: write() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }

    rc = _wait_response(pvt, i, sim900_configuration_for_erased_area_s0[0], NULL, 0, 10000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "Configuration for erased area of FLASH: read() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "Configuration for erased area of FLASH: timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Configuration for erased area of FLASH: succeeded\n", pvt->name);
    }

    //! *** Step #7 FLASH erasen ***
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: FLASH erase: started\n", pvt->name);
    chr = 0x03;
    if(write(pvt->at_fd, &chr, 1) < 0){
        snprintf(msgbuf, msgbuf_len, "FLASH erase: write() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    us_sleep(100);
    // read marker
    rc = _wait_response(pvt, 1, 0x03, NULL, 0, 10000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "FLASH erase marker: read() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "FLASH erase marker: timeout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    //wait for flash erase 3min
    rc = _wait_response(pvt, 0, 0x30, NULL, 0, 3*60000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "FLASH erase: read() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "FLASH erase: timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: FLASH erase: succeeded\n", pvt->name);
    }

    //! *** Step #8 - Set for downloaded code information ***
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Set for downloaded code information: started\n", pvt->name);
    sprintf(buf, "%s/firmware/rgsm/%s", ast_config_AST_DATA_DIR, sim900_config.fw_image);
    if(!(fp = fopen(buf, "rb"))){
        snprintf(msgbuf, msgbuf_len, "File of SIM900 Firmware Image: fopen(%s) fail: %s", buf, strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    // obtain file size:
    fseek(fp, 0, SEEK_END);
    fsize = (int)ftell(fp);
    rewind(fp);

    //set size of fw in struct
    *(int*)(&sim900_set_for_downloaded_code_information[5]) = fsize;

    i = sizeof(sim900_set_for_downloaded_code_information);
    if(write(pvt->at_fd, sim900_set_for_downloaded_code_information, i) < 0){
        snprintf(msgbuf, msgbuf_len, "Set for downloaded code information: write() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }

    rc = _wait_response(pvt, i, sim900_set_for_downloaded_code_information[0], NULL, 0, 10000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "Set for downloaded code information: read() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "Set for downloaded code information: timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Set for downloaded code information: succeeded - total %d bytes\n", pvt->name, fsize);
    }

    //! *** Step #8 - Set for downloaded code information ***
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Download code: started\n", pvt->name);
    checksum = 0;
    int total = 0;
    while (1) {
        memset(sim900_code_page, 0, sizeof(sim900_code_page));
        code_page_bytes = (int)fread(sim900_code_page, 1, sizeof(sim900_code_page), fp);
        if (code_page_bytes != sizeof(sim900_code_page)) {
            if (ferror(fp)) {
                snprintf(msgbuf, msgbuf_len, "Download code section: read() fail: %s", strerror(errno));
                ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
                goto _cleanup_2;
            } else if (code_page_bytes == 0) {
                //zero length block, don't transmit it to device
                break;
            }
        }

        *(int*)(&sim900_set_for_downloaded_code_section[1]) = code_page_bytes;
        i = sizeof(sim900_set_for_downloaded_code_section);
        rc = _write_request(pvt, sim900_set_for_downloaded_code_section, i);
        if(rc){
            snprintf(msgbuf, msgbuf_len, "Set for downloaded code section: write() fail: %s", strerror(rc));
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }
        rc = _wait_response(pvt, 0, sim900_set_for_downloaded_code_section[0], NULL, 0, 10000000);
        if (rc == -1) {
            snprintf(msgbuf, msgbuf_len, "Set for downloaded code section: read() fail: %s", strerror(errno));
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }
        else if (rc == -2) {
            strncpy(msgbuf, "Set for downloaded code section: timedout", msgbuf_len);
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }
        else {
            //ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Set for downloaded code section - total %d bytes\n", pvt->name);
        }

        //calc crc
        _calc_checksum(sim900_code_page, code_page_bytes, &checksum);

        //write code page
        rc = _write_request(pvt, sim900_code_page, code_page_bytes);
        if(rc){
            snprintf(msgbuf, msgbuf_len, "Code section: write() fail: %s", strerror(rc));
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }

        total += code_page_bytes;

        //not wait for transmission code_page_bytes bytes because _write_request already did that
        rc = _wait_response(pvt, 0, 0x2e, extra_buf, 1, 10000000);
        if (rc == -1) {
            snprintf(msgbuf, msgbuf_len, "Code section: read() fail: %s", strerror(errno));
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }
        else if (rc == -2) {
            strncpy(msgbuf, "Code section: timedout", msgbuf_len);
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }
        else if (*extra_buf != 0x30) {
            snprintf(msgbuf, msgbuf_len, "Code section: downloaded unsuccess: 0x2e, 0x%2x", *extra_buf);
            ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
            goto _cleanup_2;
        }
        else {
            //ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Set for downloaded code information: section - total %d bytes\n", pvt->name);
        }
    }
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Download code: succeeded - total %d bytes\n", pvt->name, total);


    //! *** Step #9 - Comparison for downloaded information ***
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Comparison for downloaded information: started\n", pvt->name);

    *(unsigned int*)(&sim900_compare_for_downloaded_information[5]) = checksum;
    *(unsigned int*)(&sim900_compare_for_downloaded_information[9]) = fsize;

    i = sizeof(sim900_compare_for_downloaded_information);
    if(write(pvt->at_fd, sim900_compare_for_downloaded_information, i) < 0){
        snprintf(msgbuf, msgbuf_len, "Comparison for downloaded information: write() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    rc = _wait_response(pvt, i, sim900_compare_for_downloaded_information[0], extra_buf, 5, 10000000);
    if (rc == -1) {
        snprintf(msgbuf, msgbuf_len, "Comparison for downloaded information: read() fail: %s", strerror(errno));
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (rc == -2) {
        strncpy(msgbuf, "Comparison for downloaded information: timedout", msgbuf_len);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else if (extra_buf[4] != 0x30) {
        snprintf(msgbuf, msgbuf_len, "Comparison for downloaded information: checksums mismatch: expected=0x%x but calculated=0x%x",
                 *(unsigned int*)(&extra_buf[0]), (unsigned int)checksum);
        ast_log(AST_LOG_ERROR, "%s\n", msgbuf);
        goto _cleanup_2;
    }
    else {
        //ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Set for downloaded code information: section - total %d bytes\n", pvt->name);
    }

    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: Comparison for downloaded information: success\n", pvt->name);
    result = 0;
_cleanup_2:
    if (fp) fclose(fp);
    if (pvt->at_fd != -1) {
        ggw8_close_at(pvt->ggw8_device, pvt->modem_id);
        pvt->at_fd = -1;
    }
_cleanup_1:
    ast_log(AST_LOG_DEBUG, "rgsm: <%s>: modem power off\n", pvt->name);
    if (ggw8_modem_ctl(pvt->ggw8_device, pvt->modem_id, GGW8_CTL_MODEM_POWEROFF))
    {
        ast_log(AST_LOG_ERROR, "<%s>: couldn't modem power off\n", pvt->name);
    } else {
        us_sleep(500000);
    }
_exit:
    if (result) {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s> FW update: failed: Reason: %s\n", pvt->name, msgbuf);
    } else {
        ast_log(AST_LOG_DEBUG, "rgsm: <%s> FW update: succeed\n", pvt->name);
    }
    return result;
}

void sim900_check_sim_status(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_SIM900_CSMINS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
}

void sim900_set_sim_poll(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_SIM900_CSMINS, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "0");
}

void sim900_gsm_query_sim_data(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_SIM900_CCID, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, NULL);
    //enable DTMF detection^M
    //Aug 19, 2014: moved here from query_module_data() according to Application Notes
    rgsm_atcmd_queue_append(pvt, AT_SIM900_DDET, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "1");
}

void sim900_hangup(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_H, AT_OPER_EXEC, 0, gen_config.timeout_at_response, 0, "0");
}

void sim900_change_imei(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_SIM900_SIMEI, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0,
                                    "\"%s\"",
                                    pvt->new_imei);
}

void sim900_setup_audio_channel(struct gsm_pvt* pvt)
{
    rgsm_atcmd_queue_append(pvt, AT_SIM900_CHFA, AT_OPER_WRITE, 0, gen_config.timeout_at_response, 0, "%d", 0);
}

int sim900_init(struct gsm_pvt* pvt)
{
    pvt->functions.atp_handle_response = &sim900_atp_handle_response;
    pvt->functions.set_sim_poll = &sim900_set_sim_poll;
    pvt->functions.gsm_query_sim_data = &sim900_gsm_query_sim_data;
    pvt->functions.check_sim_status = &sim900_check_sim_status;
    pvt->functions.hangup = &sim900_hangup;
    pvt->functions.change_imei = &sim900_change_imei;
    pvt->functions.setup_audio_channel = &sim900_setup_audio_channel;
    
    pvt->mic_gain_conf = sim900_config.mic_gain;
    pvt->spk_gain_conf = sim900_config.spk_gain;
    pvt->spk_audg_conf = sim900_config.spk_audg;

    
    rgsm_atcmd_queue_append(pvt, AT_SIM900_CSMINS, AT_OPER_READ, 0, gen_config.timeout_at_response, 0, NULL);
    
    return 0;
}
