/******************************************************************************/
/* sms.c                                                                      */
/******************************************************************************/
/* $Rev:: 119                        $                                        */
/* $Author:: maksym                  $                                        */
/* $Date:: 2011-11-30 12:48:52 +0200#$                                        */
/******************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <iconv.h>

#include "rgsm_utilities.h"
#include "rgsm_sms.h"
#include "rgsm_dao.h"
#include "at.h"

#include "asterisk/md5.h"

//#define LOG_PREPARE_ERROR() ast_log(LOG_ERROR, "<%s>: sqlite3_prepare_v2(): %d: %s\n", pvt->name, res, sqlite3_errmsg(smsdb))
//#define LOG_STEP_ERROR()    ast_log(LOG_ERROR, "<%s>: sqlite3_step(): %d: %s\n", pvt->name, res, sqlite3_errmsg(smsdb))

//------------------------------------------------------------------------------
static const unsigned short gsm_to_unicode_le[128] = {
	// 000xxxx
	0x0040,		// @ - 00
	0x00A3,		// £ - 01
	0x0024,		// $ - 02
	0x00A5,		// ¥ - 03
	0x00E8,		// è - 04
	0x00E9,		// é - 05
	0x00F9,		// ù - 06
	0x00EC,		// ì - 07
	0x00F2,		// ò - 08
	0x00E7,		// ç - 09
	0x000A,		// LF- 0A
	0x00D8,		// Ø - 0B
	0x00F8,		// ø - 0C
	0x000D,		// CR- 0D
	0x00C5,		// Å - 0E
	0x00E5,		// å - 0F
	// 001xxxx
	0x0394,		// Δ - 10
	0x005F,		// _ - 11
	0x03A6,		// Φ - 12
	0x0393,		// Γ - 13
	0x039B,		// Λ - 14
	0x03A9,		// Ω - 15
	0x03A0,		// Π - 16
	0x03A8,		// Ψ - 17
	0x03A3,		// Σ - 18
	0x0398,		// Θ - 19
	0x039E,		// Ξ - 1A
	0x001B,		//ESC- 1B
	0x00C6,		// Æ - 1C
	0x00E6,		// æ - 1D
	0x00DF,		// ß - 1E
	0x00C9,		// É - 1F
	// 010xxxx
	0x0020,		// SP- 20
	0x0021,		// ! - 21
	0x0022,		// " - 22
	0x0023,		// # - 23
	0x00A4,		// ¤ - 24
	0x0025,		// % - 25
	0x0026,		// & - 26
	0x0027,		// ' - 27
	0x0028,		// ( - 28
	0x0029,		// ) - 29
	0x002A,		// * - 2A
	0x002B,		// + - 2B
	0x002C,		// , - 2C
	0x002D,		// - - 2D
	0x002E,		// . - 2E
	0x002F,		// / - 2F
	// 011xxxx
	0x0030,		// 0 - 30
	0x0031,		// 1 - 31
	0x0032,		// 2 - 32
	0x0033,		// 3 - 33
	0x0034,		// 4 - 34
	0x0035,		// 5 - 35
	0x0036,		// 6 - 36
	0x0037,		// 7 - 37
	0x0038,		// 8 - 38
	0x0039,		// 9 - 39
	0x003A,		// : - 3A
	0x003B,		// ; - 3B
	0x003C,		// < - 3C
	0x003D,		// = - 3D
	0x003E,		// > - 3E
	0x003F,		// ? - 3F
	// 100xxxx
	0x00A1,		// ¡ - 40
	0x0041,		// A - 41
	0x0042,		// B - 42
	0x0043,		// C - 43
	0x0044,		// D - 44
	0x0045,		// E - 45
	0x0046,		// F - 46
	0x0047,		// G - 47
	0x0048,		// H - 48
	0x0049,		// I - 49
	0x004A,		// J - 4A
	0x004B,		// K - 4B
	0x004C,		// L - 4C
	0x004D,		// M - 4D
	0x004E,		// N - 4E
	0x004F,		// O - 4F
	// 101xxxx
	0x0050,		// P - 50
	0x0051,		// Q - 51
	0x0052,		// R - 52
	0x0053,		// S - 53
	0x0054,		// T - 54
	0x0055,		// U - 55
	0x0056,		// V - 56
	0x0057,		// W - 57
	0x0058,		// X - 58
	0x0059,		// Y - 59
	0x005A,		// Z - 5A
	0x00C4,		// Ä - 5B
	0x00D6,		// Ö - 5C
	0x00D1,		// Ñ - 5D
	0x00DC,		// Ü - 5E
	0x00A7,		// § - 5F
	// 110xxxx
	0x00BF,		// ¿ - 60
	0x0061,		// a - 61
	0x0062,		// b - 62
	0x0063,		// c - 63
	0x0064,		// d - 64
	0x0065,		// e - 65
	0x0066,		// f - 66
	0x0067,		// g - 67
	0x0068,		// h - 68
	0x0069,		// i - 69
	0x006A,		// j - 6A
	0x006B,		// k - 6B
	0x006C,		// l - 6C
	0x006D,		// m - 6D
	0x006E,		// n - 6E
	0x006F,		// o - 6F
	// 111xxxx
	0x0070,		// p - 70
	0x0071,		// q - 71
	0x0072,		// r - 72
	0x0073,		// s - 73
	0x0074,		// t - 74
	0x0075,		// u - 75
	0x0076,		// v - 76
	0x0077,		// w - 77
	0x0078,		// x - 78
	0x0079,		// y - 79
	0x007A,		// z - 7A
	0x00E4,		// ä - 7B
	0x00F6,		// ö - 7C
	0x00F1,		// ñ - 7D
	0x00FC,		// ü - 7E
	0x00E0,		// à - 7F
	};
//------------------------------------------------------------------------------
static const unsigned short gsm_to_unicode_be[128] = {
	// 000xxxx
	0x4000,		// @ - 00
	0xA300,		// £ - 01
	0x2400,		// $ - 02
	0xA500,		// ¥ - 03
	0xE800,		// è - 04
	0xE900,		// é - 05
	0xF900,		// ù - 06
	0xEC00,		// ì - 07
	0xF200,		// ò - 08
	0xE700,		// ç - 09
	0x0A00,		// LF- 0A
	0xD800,		// Ø - 0B
	0xF800,		// ø - 0C
	0x0D00,		// CR- 0D
	0xC500,		// Å - 0E
	0xE500,		// å - 0F
	// 001xxxx
	0x9403,		// Δ - 10
	0x5F00,		// _ - 11
	0xA603,		// Φ - 12
	0x9303,		// Γ - 13
	0x9B03,		// Λ - 14
	0xA903,		// Ω - 15
	0xA003,		// Π - 16
	0xA803,		// Ψ - 17
	0xA303,		// Σ - 18
	0x9803,		// Θ - 19
	0x9E03,		// Ξ - 1A
	0x1B00,		//ESC- 1B
	0xC600,		// Æ - 1C
	0xE600,		// æ - 1D
	0xDF00,		// ß - 1E
	0xC900,		// É - 1F
	// 010xxxx
	0x2000,		// SP- 20
	0x2100,		// ! - 21
	0x2200,		// " - 22
	0x2300,		// # - 23
	0xA400,		// ¤ - 24
	0x2500,		// % - 25
	0x2600,		// & - 26
	0x2700,		// ' - 27
	0x2800,		// ( - 28
	0x2900,		// ) - 29
	0x2A00,		// * - 2A
	0x2B00,		// + - 2B
	0x2C00,		// , - 2C
	0x2D00,		// - - 2D
	0x2E00,		// . - 2E
	0x2F00,		// / - 2F
	// 011xxxx
	0x3000,		// 0 - 30
	0x3100,		// 1 - 31
	0x3200,		// 2 - 32
	0x3300,		// 3 - 33
	0x3400,		// 4 - 34
	0x3500,		// 5 - 35
	0x3600,		// 6 - 36
	0x3700,		// 7 - 37
	0x3800,		// 8 - 38
	0x3900,		// 9 - 39
	0x3A00,		// : - 3A
	0x3B00,		// ; - 3B
	0x3C00,		// < - 3C
	0x3D00,		// = - 3D
	0x3E00,		// > - 3E
	0x3F00,		// ? - 3F
	// 100xxxx
	0xA100,		// ¡ - 40
	0x4100,		// A - 41
	0x4200,		// B - 42
	0x4300,		// C - 43
	0x4400,		// D - 44
	0x4500,		// E - 45
	0x4600,		// F - 46
	0x4700,		// G - 47
	0x4800,		// H - 48
	0x4900,		// I - 49
	0x4A00,		// J - 4A
	0x4B00,		// K - 4B
	0x4C00,		// L - 4C
	0x4D00,		// M - 4D
	0x4E00,		// N - 4E
	0x4F00,		// O - 4F
	// 101xxxx
	0x5000,		// P - 50
	0x5100,		// Q - 51
	0x5200,		// R - 52
	0x5300,		// S - 53
	0x5400,		// T - 54
	0x5500,		// U - 55
	0x5600,		// V - 56
	0x5700,		// W - 57
	0x5800,		// X - 58
	0x5900,		// Y - 59
	0x5A00,		// Z - 5A
	0xC400,		// Ä - 5B
	0xD600,		// Ö - 5C
	0xD100,		// Ñ - 5D
	0xDC00,		// Ü - 5E
	0xA700,		// § - 5F
	// 110xxxx
	0xBF00,		// ¿ - 60
	0x6100,		// a - 61
	0x6200,		// b - 62
	0x6300,		// c - 63
	0x6400,		// d - 64
	0x6500,		// e - 65
	0x6600,		// f - 66
	0x6700,		// g - 67
	0x6800,		// h - 68
	0x6900,		// i - 69
	0x6A00,		// j - 6A
	0x6B00,		// k - 6B
	0x6C00,		// l - 6C
	0x6D00,		// m - 6D
	0x6E00,		// n - 6E
	0x6F00,		// o - 6F
	// 111xxxx
	0x7000,		// p - 70
	0x7100,		// q - 71
	0x7200,		// r - 72
	0x7300,		// s - 73
	0x7400,		// t - 74
	0x7500,		// u - 75
	0x7600,		// v - 76
	0x7700,		// w - 77
	0x7800,		// x - 78
	0x7900,		// y - 79
	0x7A00,		// z - 7A
	0xE400,		// ä - 7B
	0xF600,		// ö - 7C
	0xF100,		// ñ - 7D
	0xFC00,		// ü - 7E
	0xE000,		// à - 7F
	};
#define UCS2_UNKNOWN_SYMBOL 0x3F00
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// pdu_parser()
//------------------------------------------------------------------------------
struct pdu *pdu_parser(char *pduhex, int pduhexlen, int pdulen, time_t ltime, int *err){

	struct pdu *pdu;

	char *ip, *op;
	int ilen, olen;
	char *cp;
	char *tp;
	int tlen;

	char ucs2data[320];

	struct timeval tv;
	struct tm tmc, *tml;

    if (err) *err=0;

	// check for valid input
	if(!pduhex || !pduhexlen || !pdulen){
		if(err) *err = -1;
		return NULL;
    }

	// create storage
	if(!(pdu = malloc(sizeof(struct pdu)))){
		if(err) *err = -2;
		return NULL;
    }
	memset(pdu, 0, sizeof(struct pdu));

	// set PDU length
	pdu->len = pdulen;
	// convert PDU from hex to bin
	ip = pduhex;
	ilen = pduhexlen;
	op = pdu->buf;
	olen = MAX_PDU_BIN_SIZE;
	if(str_hex_to_bin(&ip, &ilen, &op, &olen)){
		free(pdu);
		if(err) *err = -3;
		return NULL;
    }
	pdu->full_len = MAX_PDU_BIN_SIZE - olen;
    //ast_log(AST_LOG_DEBUG, "SMS DELIVER full_len=%d, bin=\"%s\"\n", pdu->full_len, pdu->buf);
	cp = pdu->buf;

	// check for SCA present in PDU
	if(pdu->len > pdu->full_len){
		// is abnormally - sanity check
		free(pdu);
		if(err) *err = -4;
		return NULL;
    } else if (pdu->len == pdu->full_len) {
		// SCA not present in PDU
		unknown_address(&pdu->scaddr);
		//sprintf(pdu->scaddr.value, "unknown");
		//pdu->scaddr.length = strlen(pdu->scaddr.value);
	} else {
		// SCA present in PDU
		tlen = (unsigned char)(*cp++ & 0xff);
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
            if(err) *err = -5;
			return NULL;
        }
		if(tlen){
			pdu->scaddr.type.full = *cp++; // get type of sms center address
			pdu->scaddr.length = 0;
			tlen--;
			tp = pdu->scaddr.value;
			while(tlen > 0){
				// low nibble
				if(((*cp & 0x0f) !=  0x0f)){
					*tp++ = (*cp & 0x0f) + '0';
					pdu->scaddr.length++;
					}
				// high nibble
				if((((*cp >> 4) & 0x0f) !=  0x0f)){
					*tp++ = ((*cp >> 4) & 0x0f) + '0';
					pdu->scaddr.length++;
					}
				tlen--;
				cp++;
				if((cp - pdu->buf) > pdu->full_len){
					free(pdu);
                    if(err) *err = -6;
					return NULL;
                }
            }
        } else {
			sprintf(pdu->scaddr.value, "unknown");
			pdu->scaddr.length = strlen(pdu->scaddr.value);
        }
		address_normalize(&pdu->scaddr);
    }
    //ast_debug(2, "SCA=%s\n", pdu->scaddr.value);
	// check PDU length
	if((cp - pdu->buf) > pdu->full_len){
		free(pdu);
		if(err) *err = -7;
		return NULL;
    }

	// get first byte of PDU
	pdu->fb.full = *cp++;
	// check PDU length
	if((cp - pdu->buf) > pdu->full_len){
		free(pdu);
		if(err) *err = -8;
		return NULL;
    }
	// select PDU type
	if(pdu->fb.general.mti == MTI_SMS_DELIVER){
		// originating address
		tlen = pdu->raddr.length = (unsigned char)(*cp++ & 0xff); // get length
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
            if(err) *err = -9;
			return NULL;
        }
		if(tlen){
	        //ast_log(AST_LOG_DEBUG, "SMS DELIVER tlen=%d\n", tlen);
			pdu->raddr.type.full = *cp++; // get type
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
                if(err) *err = -10;
				return NULL;
            }
			tp = pdu->raddr.value;
			if(pdu->raddr.type.bits.typenumb == TYPE_OF_NUMBER_ALPHANUMGSM7){
				//
				ip = cp;
				ilen = (tlen * 4)/7;
				op = pdu->raddr.value;
				olen = MAX_ADDRESS_LENGTH;
				//
				if(gsm7_to_ucs2(&ip, &ilen, 0, &op, &olen)){
				    unknown_address(&pdu->raddr);
					//sprintf(pdu->raddr.value, "unknown");
					//pdu->raddr.length = strlen(pdu->raddr.value);
					//pdu->raddr.type.bits.reserved = 1;
					//pdu->raddr.type.bits.numbplan = NUMBERING_PLAN_UNKNOWN;
					//pdu->raddr.type.bits.typenumb = TYPE_OF_NUMBER_SUBSCRIBER;
                } else {
					pdu->raddr.length = MAX_ADDRESS_LENGTH - olen;
                }
				//
				cp += tlen/2;
				if(tlen%2) cp++;
				if((cp - pdu->buf) > pdu->full_len){
					free(pdu);
					if(err) *err = -11;
					return NULL;
                }
            } else {
				while(tlen > 0){
					// low nibble
					if(((*cp & 0x0f) !=  0x0f))
						*tp++ = (*cp & 0x0f) + '0';
					// high nibble
					if((((*cp >> 4) & 0x0f) !=  0x0f))
						*tp++ = ((*cp >> 4) & 0x0f) + '0';
					tlen -= 2;
					cp++;
					if((cp - pdu->buf) > pdu->full_len){
						free(pdu);
                        if(err) *err = -12;
						return NULL;
                    }
                }
            }
        } else { // length = 0
            unknown_address(&pdu->raddr);
			//sprintf(pdu->raddr.value, "unknown");
			//pdu->raddr.length = strlen(pdu->raddr.value);
			//pdu->raddr.type.bits.reserved = 1;
			//pdu->raddr.type.bits.numbplan = NUMBERING_PLAN_UNKNOWN;
			//pdu->raddr.type.bits.typenumb = TYPE_OF_NUMBER_SUBSCRIBER;
			cp++; // skip type position
        }
		address_normalize(&pdu->raddr);
		// check PDU length
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
			if(err) *err = -13;
			return NULL;
        }
		// protocol identifier
		pdu->pid = (unsigned char)(*cp++ & 0xff);
		// check PDU length
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
			if(err) *err = -14;
			return NULL;
        }
		// data coding scheme
		pdu->dacosc = (unsigned char)(*cp++ & 0xff);
		// check PDU length
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
			if(err) *err = -15;
			return NULL;
        }
		if(dcs_parser(pdu->dacosc, &pdu->dcs)){
			free(pdu);
			if(err) *err = -16;
			return NULL;
        }
		// service centre time stamp
		gettimeofday(&tv, NULL);
		if((tml = localtime(&tv.tv_sec))){
			// year
			tmc.tm_year = (tml->tm_year/100)*100 + (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			// check PDU length
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -17;
				return NULL;
            }
			// month
			tmc.tm_mon = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f)) - 1;
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -18;
				return NULL;
            }
			// day of the month
			tmc.tm_mday = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -19;
				return NULL;
            }
			// hours
			tmc.tm_hour = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -20;
				return NULL;
            }
			// minutes
			tmc.tm_min = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -21;
				return NULL;
            }
			// seconds
			tmc.tm_sec = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -22;
				return NULL;
            }
			// timezone - daylight savings
// 			tmc.tm_isdst = 0;
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -23;
				return NULL;
            }
			// make time_t data
			pdu->sent = mktime(&tmc);
        } else {
			pdu->sent = tv.tv_sec;
			cp += 7;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -24;
				return NULL;
			}

        }
		// user data length
		pdu->udl = (unsigned char)(*cp++ & 0xff);
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
			if(err) *err = -25;
			return NULL;
        }
		//----------------------------------------------------------------------
		//
		pdu->delivered = ltime;
		//
    } // end of sms-deliver
/*
	else if(pdu->fb.general.mti == MTI_SMS_SUBMIT){
		;
		}
*/
	else if(pdu->fb.general.mti == MTI_SMS_STATUS_REPORT){
		// message reference
		pdu->mr = (unsigned char)(*cp++ & 0xff);
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
			if(err) *err = -26;
			return NULL;
			}
		// recipient address
		tlen = pdu->raddr.length = (unsigned char)(*cp++ & 0xff); // get length
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
			if(err) *err = -27;
			return NULL;
			}
		if(tlen){
			pdu->raddr.type.full = *cp++; // get type
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -28;
				return NULL;
				}
			tp = pdu->raddr.value;
			if(0 /*pdu->raddr.type.bits.typenumb == TYPE_OF_NUMBER_ALPHANUMGSM7*/){
				//
				ip = cp;
				ilen = (tlen * 4)/7;
				op = pdu->raddr.value;
				olen = MAX_ADDRESS_LENGTH;
				//
				if(gsm7_to_ucs2(&ip, &ilen, 0, &op, &olen)){
				    unknown_address(&pdu->raddr);
					//sprintf(pdu->raddr.value, "unknown");
					//pdu->raddr.length = strlen(pdu->raddr.value);
					//pdu->raddr.type.bits.reserved = 1;
					//pdu->raddr.type.bits.numbplan = NUMBERING_PLAN_UNKNOWN;
					//pdu->raddr.type.bits.typenumb = TYPE_OF_NUMBER_SUBSCRIBER;
                } else {
                	pdu->raddr.length = MAX_ADDRESS_LENGTH - olen;
                }
				//
				cp += tlen/2;
				if(tlen%2) cp++;
				if((cp - pdu->buf) > pdu->full_len){
					free(pdu);
					if(err) *err = -29;
					return NULL;
                }
            } else {
				while(tlen > 0){
					// low nibble
					if(((*cp & 0x0f) !=  0x0f))
						*tp++ = (*cp & 0x0f) + '0';
					// high nibble
					if((((*cp >> 4) & 0x0f) !=  0x0f))
						*tp++ = ((*cp >> 4) & 0x0f) + '0';
					tlen -= 2;
					cp++;
					if((cp - pdu->buf) > pdu->full_len){
						free(pdu);
						if(err) *err = -30;
						return NULL;
                    }
                }
            }
        } else { // length = 0
            unknown_address(&pdu->raddr);
			//sprintf(pdu->raddr.value, "unknown");
			//pdu->raddr.length = strlen(pdu->raddr.value);
			//pdu->raddr.type.bits.reserved = 1;
			//pdu->raddr.type.bits.numbplan = NUMBERING_PLAN_UNKNOWN;
			//pdu->raddr.type.bits.typenumb = TYPE_OF_NUMBER_SUBSCRIBER;
			cp++; // skip type position
        }
		address_normalize(&pdu->raddr);
		// check PDU length
		if((cp - pdu->buf) > pdu->full_len){
			free(pdu);
			if(err) *err = -31;
			return NULL;
        }
		// service centre time stamp
		gettimeofday(&tv, NULL);
		if((tml = localtime(&tv.tv_sec))){
			// year
			tmc.tm_year = (tml->tm_year/100)*100 + (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			// check PDU length
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -32;
				return NULL;
            }
			// month
			tmc.tm_mon = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f)) - 1;
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -33;
				return NULL;
            }
			// day of the month
			tmc.tm_mday = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -34;
				return NULL;
            }
			// hours
			tmc.tm_hour = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -35;
				return NULL;
            }
			// minutes
			tmc.tm_min = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -36;
				return NULL;
            }
			// seconds
			tmc.tm_sec = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -37;
				return NULL;
            }
			// timezone - daylight savings
// 			tmc.tm_isdst = 0;
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -38;
				return NULL;
            }
			// make time_t data
			pdu->sent = mktime(&tmc);
        } else {
			pdu->sent = tv.tv_sec;
			cp += 7;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -39;
				return NULL;
            }
        }
		// discharge time
		if((tml = localtime(&tv.tv_sec))){
			// year
			tmc.tm_year = (tml->tm_year/100)*100 + (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			// check PDU length
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -40;
				return NULL;
            }
			// month
			tmc.tm_mon = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f)) - 1;
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -41;
				return NULL;
            }
			// day of the month
			tmc.tm_mday = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -42;
				return NULL;
            }
			// hours
			tmc.tm_hour = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -43;
				return NULL;
            }
			// minutes
			tmc.tm_min = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -44;
				return NULL;
            }
			// seconds
			tmc.tm_sec = (((*cp) & 0x0f)*10 + ((*cp >> 4) & 0x0f));
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -45;
				return NULL;
            }
			// timezone - daylight savings
// 			tmc.tm_isdst = 0;
			cp++;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -46;
				return NULL;
            }
			// make time_t data
			pdu->delivered = mktime(&tmc);
        } else {
			pdu->sent = tv.tv_sec;
			cp += 7;
			if((cp - pdu->buf) > pdu->full_len){
				free(pdu);
				if(err) *err = -47;
				return NULL;
            }
        }
		// status
		pdu->status = (unsigned char)(*cp++ & 0xff);
		// optional
		// parameter indicator (optional)
		if((cp - pdu->buf) <= pdu->full_len)
			pdu->paramind.full = (unsigned char)(*cp++ & 0xff);
		// protocol identifier (optional)
		if(pdu->paramind.bits.pid){
			if((cp - pdu->buf) <= pdu->full_len)
				pdu->pid = (unsigned char)(*cp++ & 0xff);
        }
		// data coding  scheme (optional)
		if(pdu->paramind.bits.dcs){
			if((cp - pdu->buf) <= pdu->full_len)
				pdu->dacosc = (unsigned char)(*cp++ & 0xff);
        }
		if(dcs_parser(pdu->dacosc, &pdu->dcs)){
			free(pdu);
			if(err) *err = -48;
			return NULL;
        }
		// user data length (optional)
		if(pdu->paramind.bits.udl){
			if((cp - pdu->buf) <= pdu->full_len)
				pdu->udl = (unsigned char)(*cp++ & 0xff);
        }
    } else{
		free(pdu);
		if(err) *err = -49;
		return NULL;
    }
	// processing user data
	pdu->concat_ref = 0;
	pdu->concat_cnt = 1;
	pdu->concat_num = 1;
	if(pdu->udl){
		// processing user data header
		if(pdu->fb.general.udhi){
			tlen = (unsigned char)(*cp & 0xff);
			tp = cp+1;
			while(tlen > 0){
				switch((int)(*tp & 0xff)){
					//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					case 0x00:
						pdu->concat_ref = *(tp+2);
						pdu->concat_cnt = *(tp+3);
						pdu->concat_num = *(tp+4);
						break;
					//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					default:
						break;
                }
				tlen -= (*(tp+1) + 2);
				tp += (*(tp+1) + 2);
            }
        }
		// get user data
		if(pdu->dcs.charset == DCS_CS_GSM7){
			// gsm7
			// check for user data header
			tlen = 0;
			if(pdu->fb.general.udhi){
				ilen = (int)(*cp & 0xff);
				tlen = ((ilen+1)/7)*8;
				if((ilen+1)%7)
					tlen += ((ilen+1)%7) + 1;
				pdu->udl -= tlen;
            } // end udhi
			//
			ip = cp;
			ilen = pdu->udl;
			op = ucs2data;
			olen = 320;
			if(gsm7_to_ucs2(&ip, &ilen, tlen, &op, &olen)){
				free(pdu);
				if(err) *err = -50;
				return NULL;
            }
			tlen = 320 - olen;
			// convert to utf8
			ip = ucs2data;
			ilen = tlen;
			op = pdu->ud;
			olen = 640;
			if(from_ucs2_to_specset("UTF-8", &ip, &ilen, &op, &olen)){
				free(pdu);
				if(err) *err = -51;
				return NULL;
            }
			pdu->udl = 640 - olen;
        } else if(pdu->dcs.charset == DCS_CS_8BIT){
/*
			// 8-bit
			// check for user data header
			if(pdu->fb.general.udhi){
				tlen = (int)(*cp++ & 0xff);
				pdu->udl -= (tlen + 1);
				cp += tlen;
            }
			//
// 			memcpy(pdu->ud, cp, pdu->udl);
			olen = 0;
			for(tlen=0; tlen<pdu->udl; tlen++) {
				olen += sprintf(cp+olen,"%02x ", (unsigned char)*(cp+tlen));
			}
*/
            //Mar 9, 2016: fixed SIGSEGV on long binary SMS
            // 8-bit
			// check for user data header
			if (pdu->fb.general.udhi) {
				tlen = (unsigned char)(*cp++ & 0xff);
				pdu->udl -= (tlen + 1);
				cp += tlen;
			}
			olen = 0;
			for (tlen=0; tlen<pdu->udl; tlen++)
				olen += sprintf(pdu->ud + olen, "%02x", (unsigned char)*(cp+tlen));

        } else if (pdu->dcs.charset == DCS_CS_UCS2) {
			// ucs2
			// check for user data header
			if(pdu->fb.general.udhi){
				tlen = (unsigned char)(*cp++ & 0xff);
				pdu->udl -= (tlen + 1);
				cp += tlen;
				}
			// convert to utf8
			ip = cp;
			ilen = pdu->udl;
			op = pdu->ud;
			olen = 640;
			if(from_ucs2_to_specset("UTF-8", &ip, &ilen, &op, &olen)){
				free(pdu);
				if(err) *err = -52;
				return NULL;
				}
			pdu->udl = 640 - olen;
        } else {
			// reserved
			free(pdu);
			if(err) *err = -53;
			return NULL;
        }
    }
	// return on success
	return pdu;
}
//------------------------------------------------------------------------------
// end of pdu_parser()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// dcs_parser()
//------------------------------------------------------------------------------
int dcs_parser(unsigned char inp, struct dcs *dcs){
	//
	if(!dcs) return -1;
	//
	if(inp == 0x00){ // 0000 0000
		// Special Case
		dcs->group = DCS_GROUP_GENERAL;
		dcs->charset = DCS_CS_GSM7;
		dcs->isclass = 0;
		dcs->compres = 0;
    }
	else if((inp & 0xC0) == 0x00){ // 00xx xxxx
		// General Data Coding indication
		dcs->group = DCS_GROUP_GENERAL;
		dcs->charset = (inp >> 2) & 0x03;
		dcs->isclass = (inp >> 4) & 0x01;
		dcs->classid = inp & 0x03;
		dcs->compres = (inp >> 5) & 0x01;
    }
	else if((inp & 0xC0) == 0x40){ // 01xx xxxx
		// Automatic Deletion Group
		dcs->group = DCS_GROUP_AUTODEL;
		dcs->charset = (inp >> 2) & 0x03;
		dcs->isclass = (inp >> 4) & 0x01;
		dcs->classid = inp & 0x03;
		dcs->compres = (inp >> 5) & 0x01;
    }
	else if((inp & 0xF0) == 0xC0){ // 1100 xxxx
		// Message Waiting Indication: Discard Message
		dcs->group = DCS_GROUP_MWI;
		dcs->charset = DCS_CS_GSM7;
		dcs->isclass = 0;
		dcs->compres = 0;
		dcs->mwistore = 0;
		dcs->mwiind = (inp >> 3) & 0x01;
		dcs->mwitype = inp & 0x03;
    }
	else if((inp & 0xF0) == 0xD0){ // 1101 xxxx
		// Message Waiting Indication: Store Message
		dcs->group = DCS_GROUP_MWI;
		dcs->charset = DCS_CS_GSM7;
		dcs->isclass = 0;
		dcs->compres = 0;
		dcs->mwistore = 1;
		dcs->mwiind = (inp >> 3) & 0x01;
		dcs->mwitype = inp & 0x03;
    }
	else if((inp & 0xF0) == 0xE0){ // 1110 xxxx
		// Message Waiting Indication: Store Message
		dcs->group = DCS_GROUP_MWI;
		dcs->charset = DCS_CS_UCS2;
		dcs->isclass = 0;
		dcs->compres = 0;
		dcs->mwistore = 1;
		dcs->mwiind = (inp >> 3) & 0x01;
		dcs->mwitype = inp & 0x03;
    }
	else if((inp & 0xF0) == 0xF0){ // 1111 xxxx
		// Data coding/message class
		dcs->group = DCS_GROUP_DCMC;
		dcs->charset = (inp >> 2) & 0x01;
		dcs->isclass = 1;
		dcs->classid = inp & 0x03;
		dcs->compres = 0;
    }
	else
		return -1;
	return 0;
}
//------------------------------------------------------------------------------
// end of dcs_parser()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// gsm7_to_ucs2()
//------------------------------------------------------------------------------
int gsm7_to_ucs2(char **instr, int *inlen, int start, char **outstr, int *outlen){

	int len;
	int rest;
	char *rdpos;
	unsigned short *wrpos;

	int i;
	unsigned char chridx;
	unsigned int sym4grp;

	// check input
	if(!instr || !*instr || !inlen || !outstr || !*outstr || !outlen)
		return -1;

	len = *inlen;
	rdpos = *instr;
	rest = *outlen;
	wrpos = (unsigned short *)*outstr;

	memset(wrpos, 0, rest);

	i = start;
	while(len > 0){
		//
		if(i%8 < 4){
			memcpy(&sym4grp, (rdpos+((i/8)*7)), 4);
			chridx = (sym4grp >> ((i%4)*7)) & 0x7f;
        }
		else{
			memcpy(&sym4grp, (rdpos+((i/8)*7)+3), 4);
			sym4grp >>= 4;
			chridx = (sym4grp >> ((i%4)*7)) & 0x7f;
        }
		//
		*wrpos = gsm_to_unicode_be[chridx];
		//
		rest -= 2;
		wrpos++;
		i++;
		len--;
    }

	*instr = rdpos;
	*inlen = len;
	*outstr = (char *)wrpos;
	*outlen = rest;

	return 0;
}
//------------------------------------------------------------------------------
// end of gsm7_to_ucs2()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// ucs2_to_gsm7()
//------------------------------------------------------------------------------
int ucs2_to_gsm7(char **instr, int *inlen, int start, char **outstr, int *outlen){

	int len;
	int rest;
	unsigned short *rdpos;
	char *wrpos;

	int i, j;
	unsigned int sym4grp;

	char *tbuf;

	// check input
	if(!instr || !*instr || !inlen || !outstr || !*outstr || !outlen)
		return -1;
	//
	len = *inlen;
	rdpos = (unsigned short *)*instr;
	rest = *outlen;
	wrpos = *outstr;

	// create temp buffer
	if(!(tbuf = malloc(len/2)))
		return -1;
	memset(tbuf, 0, len/2);
	// get gsm7 alphabet symbols
	for(i=0; i<(len/2); i++){
		for(j=0; j<128; j++){
			if(*(rdpos+i) == gsm_to_unicode_be[j]){
				*(tbuf+i) = (char)j;
				break;
				}
			}
		}
	// pack into output buffer
	len /= 2;
	i=start;
	while(len){
		//
		if(i%8 < 4){
			memcpy(&sym4grp, (wrpos+((i/8)*7)), 4);
			sym4grp |= ((*(tbuf+(i-start)) & 0x7f) << ((i%4)*7));
			memcpy((wrpos+((i/8)*7)), &sym4grp, 4);
			}
		else{
			memcpy(&sym4grp, (wrpos+((i/8)*7)+3), 4);
			sym4grp |= ((*(tbuf+(i-start)) & 0x7f) << (((i%4)*7) + 4));
			memcpy((wrpos+((i/8)*7)+3), &sym4grp, 4);
			}
		len--;
		i++;
		}
	// insert stuff CR symbol
	if((i&7) == 7){
		memcpy(&sym4grp, (wrpos+((i/8)*7)+3), 4);
		sym4grp |= ( 0x0d << (((i%4)*7) + 4));
		memcpy((wrpos+((i/8)*7)+3), &sym4grp, 4);
		}
	//
	*instr = (char *)rdpos;
	*inlen = len;
	*outstr = wrpos;
	*outlen = rest;

	free(tbuf);
	return 0;
}
//------------------------------------------------------------------------------
// end of ucs2_to_gsm7()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// is_gsm7_string()
//------------------------------------------------------------------------------
int is_gsm7_string(char *buf){

	int res;

	iconv_t tc;
	char *ibuf;
	size_t ilen;
	char *obuf;
	size_t olen;

	unsigned short *ucs2buf;
	int ucs2len;
	int i, j;

	//
	ibuf = buf;
	ilen = strlen(ibuf);
	ucs2len = olen = ilen * 2;
	if(!(obuf = malloc(olen)))
		return -1;
	ucs2buf = (unsigned short *)obuf;
	// convert from utf-8 to ucs-2be - prepare converter
	tc = iconv_open("UCS-2BE", "UTF-8");
	if(tc == (iconv_t)-1){
		// converter not created
		free(obuf);
		return -1;
    }
	res = iconv(tc, &ibuf, &ilen, &obuf, &olen);
	if(res == (size_t)-1){
		free(obuf);
		return -1;
    }
	ucs2len -= olen;
	// close converter
	iconv_close(tc);

	res = 1;
	// test for data in gsm7 default alphabet
	for(i=0; i<ucs2len; i++){
		j = 0;
		for(j=0; j<128; j++){
			if(*(ucs2buf+i) == gsm_to_unicode_be[j])
				break;
        }
		if(j >= 128){
			res = 0;
			break;
        }
		if(!res) break;
    }

	free(obuf);
	return res;
}
//------------------------------------------------------------------------------
// end of is_gsm7_string()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// get_parts_count()
//------------------------------------------------------------------------------
int get_parts_count(char *buf){

	int isgsm7;

	iconv_t tc;
	char *ibuf;
	size_t ilen;
	char *obuf;
	size_t olen;

	unsigned short *ucs2buf;
	int ucs2len;
	int i, j;

	//
	ibuf = buf;
	ilen = strlen(ibuf);
	ucs2len = olen = ilen * 2;
	if(!(obuf = malloc(olen)))
		return -1;
	ucs2buf = (unsigned short *)obuf;
	// convert from utf-8 to ucs-2be - prepare converter
	tc = iconv_open("UCS-2BE", "UTF-8");
	if(tc == (iconv_t)-1){
		// converter not created
		free(obuf);
		return -1;
    }
	isgsm7 = iconv(tc, &ibuf, &ilen, &obuf, &olen);
	if(isgsm7 == (size_t)-1){
		free(obuf);
		return -1;
    }
	ucs2len -= olen;
	// close converter
	iconv_close(tc);

	isgsm7 = 1;
	// test for data in gsm7 default alphabet
	for(i=0; i<(ucs2len/2); i++){
		j = 0;
		for(j=0; j<128; j++){
			if(*(ucs2buf+i) == gsm_to_unicode_be[j])
				break;
        }
		if(j >= 128){
			isgsm7 = 0;
			break;
        }
		if(!isgsm7) break;
    }

	ucs2len /= 2;

	if(isgsm7){
		if(ucs2len <= 160)
			i = 1;
		else{
			i = ucs2len / 153;
			if(ucs2len % 153) i++;
        }
    }
	else{
		if(ucs2len <= 70)
			i = 1;
		else{
			i = ucs2len / 67;
			if(ucs2len % 67) i++;
        }
    }

	free(obuf);
	return i;
}
//------------------------------------------------------------------------------
// end of get_parts_count()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// calc_submit_pdu()
//------------------------------------------------------------------------------
static struct pdu *calc_submit_pdu(char *content, char *destination, int flash,
							struct address *sca, int id){

	int isgsm7;

	iconv_t tc;
	char *ibuf;
	size_t ilen;
	char *obuf;
	size_t olen;

	char *bldp;

	unsigned short *ucs2buf;
	int ucs2len;
	int symcnt;
	int i, j;

	int part;
	int part_count;

	struct pdu *pdu;
	struct pdu *curr;
	struct pdu *prev;

	//
	ibuf = content;
	ilen = strlen(ibuf);
	ucs2len = olen = ilen * 2;
	if(!(ucs2buf = malloc(olen)))
		return NULL;
	obuf = (char *)ucs2buf;
	// convert from utf-8 to ucs-2be - prepare converter
	tc = iconv_open("UCS-2BE", "UTF-8");
	if(tc == (iconv_t)-1){
		// converter not created
		free(ucs2buf);
		return NULL;
    }
	isgsm7 = iconv(tc, &ibuf, &ilen, &obuf, &olen);
	if(isgsm7 == (size_t)-1){
		free(ucs2buf);
		return NULL;
    }
	ucs2len -= olen;
	// close converter
	iconv_close(tc);

	isgsm7 = 1;
	// test for data in gsm7 default alphabet
	for(i=0; i<(ucs2len/2); i++){
		j = 0;
		for(j=0; j<128; j++){
			if(*(ucs2buf+i) == gsm_to_unicode_be[j])
				break;
        }
		if(j >= 128){
			isgsm7 = 0;
			break;
        }
		if(!isgsm7) break;
    }

	symcnt = olen = ucs2len / 2;

	if(isgsm7){
		if(olen <= 160)
			part_count = 1;
		else{
			part_count = olen / 153;
			if(olen % 153) part_count++;
        }
    }
	else{
		if(olen <= 70)
			part_count = 1;
		else{
			part_count = olen / 67;
			if(olen % 67) part_count++;
        }
    }

	//
	pdu = NULL;
	prev = NULL;
	curr = NULL;
	for(part=0; part<part_count; part++){
		// create pdu storage
		if(!(curr = malloc(sizeof(struct pdu)))){
			free(ucs2buf);
			pdu_free(pdu);
			return NULL;
        }
		memset(curr, 0, sizeof(struct pdu));
		if(!pdu)
			pdu = curr;
		if(prev)
			prev->next = curr;
		prev = curr;
		// build pdu
		bldp = curr->buf;
		// sms center address
		if(sca){
			memcpy(&curr->scaddr, sca, sizeof(struct address));
			*bldp++ = (unsigned char)((curr->scaddr.length/2) + (curr->scaddr.length%2) + 1);
			*bldp++ = (unsigned char)curr->scaddr.type.full;
			for(i=0; i<curr->scaddr.length; i++){
				if(i%2)
					*bldp++ |= (((curr->scaddr.value[i] - '0') << 4) & 0xf0);
				else
					*bldp = ((curr->scaddr.value[i] - '0') & 0x0f);
            }
			if(curr->scaddr.length%2)
				*bldp++ |= 0xf0;
        }
		// first byte
		curr->fb.submit.mti = MTI_SMS_SUBMIT; // message type indicator - bit: 0,1
		curr->fb.submit.rd = 1; // reject duplicates - bit 2
		//curr->fb.submit.vpf = 0; // validity period format - bit 3,4
		curr->fb.submit.vpf = 2; // VP field present an integer representation (relative)
		curr->fb.submit.sri = 1; // status report indication - bit 5
		curr->fb.submit.udhi = (part_count > 1)?1:0; // user data header indication - bit 6
		curr->fb.submit.rp = 0; // reply path - bit 7
		*bldp++ = curr->fb.full;
		// message refernce
		*bldp++ = curr->mr = 0;
		// destination address
		address_classify(destination, &curr->raddr);
		*bldp++ = (unsigned char)curr->raddr.length;
		*bldp++ = (unsigned char)curr->raddr.type.full;
		for(i=0; i<curr->raddr.length; i++){
			if(i%2)
				*bldp++ |= (((curr->raddr.value[i] - '0') << 4) & 0xf0);
			else
				*bldp = ((curr->raddr.value[i] - '0') & 0x0f);
        }
		if(curr->raddr.length%2)
			*bldp++ |= 0xf0;
		// protocol id
		*bldp++ = curr->pid = 0;
		// data coding scheme
		if(isgsm7 && !flash)
			curr->dacosc = 0x00;
		else if(isgsm7 && flash)
			curr->dacosc = 0x10;
		else if(!isgsm7 && !flash)
			curr->dacosc = 0x08;
		else
			curr->dacosc = 0x18;
		*bldp++ = curr->dacosc;
		// validity period

		if(curr->fb.submit.vpf){
			*bldp++ = 0xAA;
        }

		// user data length
		if(isgsm7){
			// gsm7 default alphabet
			if(part_count > 1){
				// set user data length
				curr->udl = ((symcnt/153)?(153):(symcnt%153)) + 7;
				*bldp++ = curr->udl;
				// set user data header
				*(bldp + 0) = 5; // udhi length
				*(bldp + 1) = 0; // ie id - concatenated message
				*(bldp + 2) = 3; // ie length - concatenated message
				*(bldp + 3) = (unsigned char)((id & 0xff)?(id & 0xff):(0x5a)); // concatenated message - message reference
				*(bldp + 4) = part_count; // concatenated message - parts count
				*(bldp + 5) = part+1; // concatenated message - current part
				*(bldp + 6) = 0x00; // fill bits
				// set user data
				ibuf = (char *)(ucs2buf + part*153);
				ilen = ((symcnt/153)?(153):(symcnt%153)) * 2;
				obuf = curr->ud;
				olen = 640;
				if(from_ucs2_to_specset("UTF-8", &ibuf, (int *)&ilen, &obuf, (int *)&olen)){
					free(ucs2buf);
					pdu_free(pdu);
					return NULL;
                }
				//
				ibuf = (char *)(ucs2buf + part*153);
				ilen = ((symcnt/153)?(153):(symcnt%153)) * 2;
				obuf = bldp;
				olen = 153;
				if(ucs2_to_gsm7(&ibuf, (int *)&ilen, 7, &obuf, (int *)&olen)){
					free(ucs2buf);
					pdu_free(pdu);
					return NULL;
                }
				curr->len = (int)(bldp - curr->buf) + ((curr->udl * 7) / 8);
				if((curr->udl * 7) % 8) curr->len++;
				symcnt -= 153;
            }
			else{
				// set user data length
				curr->udl = symcnt;
				*bldp++ = curr->udl;
				// set user data
				ibuf = (char *)ucs2buf;
				ilen = ucs2len;
				obuf = curr->ud;
				olen = 640;
				if(from_ucs2_to_specset("UTF-8", &ibuf, (int *)&ilen, &obuf, (int *)&olen)){
					free(ucs2buf);
					pdu_free(pdu);
					return NULL;
                }
				//
				ibuf = (char *)ucs2buf;
				ilen = ucs2len;
				obuf = bldp;
				olen = 140;
				if(ucs2_to_gsm7(&ibuf, (int *)&ilen, 0, &obuf, (int *)&olen)){
					free(ucs2buf);
					pdu_free(pdu);
					return NULL;
                }
				curr->len = (int)(bldp - curr->buf) + ((curr->udl * 7) / 8);
				if((curr->udl * 7) % 8) curr->len++;
            }
        }
		else{
			// ucs2
			if(part_count > 1){
				// set user data length
				curr->udl = ((ucs2len/134)?(134):(ucs2len%134)) + 6;
				*bldp++ = curr->udl;
				// set user data header
				*(bldp + 0) = 5; // udhi length
				*(bldp + 1) = 0; // ie id - concatenated message
				*(bldp + 2) = 3; // ie length - concatenated message
				*(bldp + 3) = (unsigned char)((id & 0xff)?(id & 0xff):(0x5a)); // concatenated message - message reference
				*(bldp + 4) = part_count; // concatenated message - parts count
				*(bldp + 5) = part+1; // concatenated message - current part
				// set user data
				ibuf = (char *)(ucs2buf + part*67);
				ilen = (ucs2len/134)?(134):(ucs2len%134);
				obuf = curr->ud;
				olen = 640;
				if(from_ucs2_to_specset("UTF-8", &ibuf, (int *)&ilen, &obuf, (int *)&olen)){
					free(ucs2buf);
					pdu_free(pdu);
					return NULL;
                }
				//
				memcpy(bldp+6, (ucs2buf + part*67), (ucs2len/134)?(134):(ucs2len%134));
				ucs2len -= 134;
				curr->len = (int)(bldp - curr->buf) + curr->udl;
            }
			else{
				// set user data length
				curr->udl = ucs2len;
				*bldp++ = curr->udl;
				// set user data
				ibuf = (char *)ucs2buf;
				ilen = ucs2len;
				obuf = curr->ud;
				olen = 640;
				if(from_ucs2_to_specset("UTF-8", &ibuf, (int *)&ilen, &obuf, (int *)&olen)){
					free(ucs2buf);
					pdu_free(pdu);
					return NULL;
                }
				//
				memcpy(bldp, ucs2buf, ucs2len);
				curr->len = (int)(bldp - curr->buf) + curr->udl;
            }
        }
		// final adjust
		if(part_count > 1){
			curr->concat_ref = (unsigned char)((id & 0xff)?(id & 0xff):(0x5a));
			curr->concat_cnt = part_count;
			curr->concat_num = part+1;
        }
		else{
			curr->concat_ref = 0;
			curr->concat_cnt = 1;
			curr->concat_num = 1;
        }
		// set full length
		curr->full_len = curr->len;
		if(curr->scaddr.length)
			curr->len -= ((curr->scaddr.length/2) + (curr->scaddr.length%2) + 2);
    }

	free(ucs2buf);
	return pdu;
}
//------------------------------------------------------------------------------
// end of calc_submit_pdu()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// pdu_free()
//------------------------------------------------------------------------------
void pdu_free(struct pdu *pdu){

	struct pdu *next;

	while(pdu) {
		next = pdu->next;
		free(pdu);
		pdu = next;
    }
}
//------------------------------------------------------------------------------
// end of pdu_free()
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// ussd_decode()
//------------------------------------------------------------------------------
char *get_ussd_decoded(char *ussdhex, int ussdhexlen, int dcs){

	char *res;

	char ussdbin[256];
	int ussdbinlen;

	char *ip, *op;
	int ilen, olen;

	// get buffer for result
	if(!(res = malloc(512)))
		return NULL;
	memset(res, 0, 512);

	// convert from hex
	ip = ussdhex;
	ilen = ussdhexlen;
	op = ussdbin;
	olen = 256;
	if(str_hex_to_bin(&ip, &ilen, &op, &olen))
	    {
		// Text
		memcpy(res, ussdhex, ussdhexlen);
		return res;
	    }
	ussdbinlen = 256 - olen;

	// sim300 ucs2 or gsm7
	if((dcs == 0x11) ||
		(((dcs & 0xc0) == 0x40) && ((dcs & 0x0c) == 0x08)) ||
		(((dcs & 0xf0) == 0x90) && ((dcs & 0x0c) == 0x08))) {
		// ucs2
		ip = ussdbin;
		ilen = ussdbinlen;
		op = res;
		olen = 512;
		if(from_ucs2_to_specset("UTF-8", &ip, &ilen, &op, &olen)){
			free(res);
			return NULL;
        }
    } else {
		// gsm7
		memcpy(res, ussdbin, ussdbinlen);
    }

	return res;
}


void process_cmgs_response(struct gsm_pvt *pvt)
{
    //ast_verbose("rgsm: CMGS resonse processor not implemented\n");

    char *tp, *str0, *str1;
    sqlite3_stmt *sql0;
	int res;
	int row;
	int rc;
	int maxmsgid, msgid;


    if(strstr(pvt->recv_buf, "+CMGS:")){
        if((tp = strchr(pvt->recv_buf, SP))){
            tp++;
            if(is_str_digit(tp)){
                rc = atoi(tp);
                // copy sent pdu from preparing to sent
                char *p2s_owner = NULL;
                int p2s_scatype = 0;
                char *p2s_scaname = NULL;
                int p2s_datype = 0;
                char *p2s_daname = NULL;
                int p2s_dcs = 0;
                int p2s_partid = 0;
                int p2s_partof = 0;
                int p2s_part = 0;
                int p2s_submitpdulen = 0;
                char *p2s_submitpdu = NULL;
                int p2s_attempt = 0;
                int p2s_flash = 0;
                char *p2s_hash = NULL;
                char *p2s_content = NULL;
                // get info from preparing
                str0 = sqlite3_mprintf("SELECT owner,scatype,scaname,datype,daname,dcs,partid,partof,part,submitpdulen,submitpdu,attempt,hash,flash,content FROM '%q-preparing' WHERE msgno=%d;",
                                       pvt->chname, pvt->now_send_pdu_id);
                while(1){
                    res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
                    if(res == SQLITE_OK){
                        row = 0;
                        while(1){
                            res = sqlite3_step(sql0);
                            if(res == SQLITE_ROW){
                                row++;
                                p2s_owner = (sqlite3_column_text(sql0, 0))?(ast_strdup((char *)sqlite3_column_text(sql0, 0))):("unknown");
                                p2s_scatype = sqlite3_column_int(sql0, 1);
                                p2s_scaname = (sqlite3_column_text(sql0, 2))?(ast_strdup((char *)sqlite3_column_text(sql0, 2))):("unknown");
                                p2s_datype = sqlite3_column_int(sql0, 3);
                                p2s_daname = (sqlite3_column_text(sql0, 4))?(ast_strdup((char *)sqlite3_column_text(sql0, 4))):("unknown");
                                p2s_dcs = sqlite3_column_int(sql0, 5);
                                p2s_partid = sqlite3_column_int(sql0, 6);
                                p2s_partof = sqlite3_column_int(sql0, 7);
                                p2s_part = sqlite3_column_int(sql0, 8);
                                p2s_submitpdulen = sqlite3_column_int(sql0, 9);
                                p2s_submitpdu = (sqlite3_column_text(sql0, 10))?(ast_strdup((char *)sqlite3_column_text(sql0, 10))):("unknown");
                                p2s_attempt = sqlite3_column_int(sql0, 11);
                                p2s_hash = (sqlite3_column_text(sql0, 12))?(ast_strdup((char *)sqlite3_column_text(sql0, 12))):("unknown");
                                p2s_flash = sqlite3_column_int(sql0, 13);
                                p2s_content = (sqlite3_column_text(sql0, 14))?(ast_strdup((char *)sqlite3_column_text(sql0, 14))):("unknown");
                            }
                            else if(res == SQLITE_DONE) {
                                break;
                            }
                            else if(res == SQLITE_BUSY){
                                rgsm_usleep(pvt, 1);
                                continue;
                            }
                            else{
                                LOG_STEP_ERROR();
                                break;
                            }
                        }
                        sqlite3_finalize(sql0);
                        break;
                    }
                    else if(res == SQLITE_BUSY){
                        rgsm_usleep(pvt, 1);
                        continue;
                    }
                    else{
                        LOG_PREPARE_ERROR();
                        break;
                    }
                }
                sqlite3_free(str0);

                // get msgid from sent
                maxmsgid = msgid = 0;
                str0 = sqlite3_mprintf("SELECT msgid,hash FROM '%q-sent';", pvt->chname);
                while(1){
                    res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
                    if(res == SQLITE_OK){
                        row = 0;
                        while(1){
                            res = sqlite3_step(sql0);
                            if(res == SQLITE_ROW){
                                row++;
                                maxmsgid = MAX(sqlite3_column_int(sql0, 0), maxmsgid);
                                if((p2s_hash) && (sqlite3_column_text(sql0, 1)) && (!strcmp(p2s_hash, (char *)sqlite3_column_text(sql0, 1)))){
                                    msgid = sqlite3_column_int(sql0, 0);
                                    break;
                                }
                            }
                            else if(res == SQLITE_DONE) {
                                break;
                            }
                            else if(res == SQLITE_BUSY){
                                rgsm_usleep(pvt, 1);
                                continue;
                            }
                            else{
                                LOG_STEP_ERROR();
                                break;
                            }
                        }
                        sqlite3_finalize(sql0);
                        break;
                    }
                    else if(res == SQLITE_BUSY){
                        rgsm_usleep(pvt, 1);
                        continue;
                    }
                    else{
                        LOG_PREPARE_ERROR();
                        break;
                    }
                }
                sqlite3_free(str0);
                //
                if(!msgid) msgid = maxmsgid + 1;

                // write pdu to sent database
                str0 = sqlite3_mprintf("INSERT INTO '%q-sent' ("
                                    "owner, " // TEXT
                                    "msgid, " // INTEGER
                                    "status, " // INTEGER
                                    "mr, " // INTEGER
                                    "scatype, " // INTEGER
                                    "scaname, " // TEXT
                                    "datype, " // INTEGER
                                    "daname, " //
                                    "dcs, " // INTEGER
                                    "sent, " // INTEGER
                                    "received, " // INTEGER
                                    "partid, " // INTEGER
                                    "partof, " // INTEGER
                                    "part, " // INTEGER
                                    "submitpdulen, " // INTEGER
                                    "submitpdu, " // TEXT
                                    "attempt, " // INTEGER
                                    "hash, " // VARCHAR(32)
                                    "flash, " // INTEGER
                                    "content" // TEXT
                                ") VALUES ("
                                    "'%q', " // owner TEXT
                                    "%d, " // msgid INTEGER
                                    "%d, " // status INTEGER
                                    "%d, " // mr INTEGER
                                    "%d, " // scatype INTEGER
                                    "'%q', " // scaname TEXT
                                    "%d, " // datype INTEGER
                                    "'%q', " // daname TEXT
                                    "%d, " // dcs INTEGER
                                    "%ld, " // sent INTEGER
                                    "%ld, " // received INTEGER
                                    "%d, " // partid INTEGER
                                    "%d, " // partof INTEGER
                                    "%d, " // part INTEGER
                                    "%d, " // submitpdulen INTEGER
                                    "'%q', " // submitpdu TEXT
                                    "%d, " // attempt INTEGER
                                    "'%q', " // hash VARCHAR(32)
                                    "%d, " // flash INTEGER
                                    "'%q'" // content TEXT
                                    ");",
                                    pvt->chname,
                                    p2s_owner, // owner TEXT
                                    msgid, // msgid INTEGER
                                    0, // status INTEGER
                                    rc, // mr INTEGER
                                    p2s_scatype, // scatype INTEGER
                                    p2s_scaname, // scaname TEXT
                                    p2s_datype, // datype INTEGER
                                    p2s_daname, // daname TEXT
                                    p2s_dcs, // dcs INTEGER
                                    pvt->curr_tv.tv_sec, // sent INTEGER
                                    0, // received INTEGER
                                    p2s_partid, // partid INTEGER
                                    p2s_partof, // partof INTEGER
                                    p2s_part, // part INTEGER
                                    p2s_submitpdulen, // submitpdulen INTEGER
                                    p2s_submitpdu, // submitpdu TEXT
                                    p2s_attempt, // attempt INTEGER
                                    p2s_hash, // hash VARCHAR(32)
                                    p2s_flash, // flash INTEGER
                                    p2s_content); // submitpdu TEXT

                dao_exec_stmt(str0, 1, pvt);
                //
                if(p2s_owner) ast_free(p2s_owner);
                if(p2s_scaname) ast_free(p2s_scaname);
                if(p2s_daname) ast_free(p2s_daname);
                if(p2s_submitpdu) ast_free(p2s_submitpdu);
                if(p2s_hash) ast_free(p2s_hash);
                if(p2s_content) ast_free(p2s_content);
                // delete pdu from preparing
                str0 = sqlite3_mprintf("DELETE FROM '%q-preparing' WHERE msgno=%d;", pvt->chname, pvt->now_send_pdu_id);
                dao_exec_stmt(str0, 1, pvt);
                // set new zero interval if is multipart message
                if(p2s_part != p2s_partof){
                    rgsm_timer_set(pvt->timers.smssend, zero_timeout);
                    ast_debug(2, "<%s>: set 0 msec timeout to send multi part sms\n", pvt->name);
                }
            }
        }
        pvt->now_send_pdu_id = 0;
    }
    else if(strstr(pvt->recv_buf, "ERROR")){
        if((tp = strrchr(pvt->recv_buf, SP))){
            tp++;
            if(is_str_digit(tp))
                ast_log(LOG_ERROR, "<%s>: message send error: %s\n", pvt->name, cms_error_print(atoi(tp)));
            else
                ast_log(LOG_ERROR, "<%s>: message send error: %s\n", pvt->name, pvt->recv_buf);
            // check for send attempt count
            if(((pvt->now_send_attempt+1) >= pvt->sms_sendattempt) || atoi(tp) == CMS_ERR_PS_BUSY){
                // copy pdu from preparing to sent
                char *assembled_content;
                int assembled_content_len;
                char assembled_destination[MAX_ADDRESS_LENGTH];
                //
                char *p2d_owner = NULL;
                int p2d_partof = 0;
                int p2d_part = 0;
                char *p2d_hash = NULL;
                int p2d_flash = 0;
                // allocating memory for content assembling
                assembled_content = ast_calloc(pvt->sms_maxpartcount, 640);
                // get info from current preparing PDU
                str0 = sqlite3_mprintf("SELECT owner,datype,daname,partof,hash,flash FROM '%q-preparing' WHERE msgno=%d;",
                                       pvt->chname, pvt->now_send_pdu_id);
                while(1){
                    res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
                    if(res == SQLITE_OK){
                        row = 0;
                        while(1){
                            res = sqlite3_step(sql0);
                            if(res == SQLITE_ROW){
                                row++;
                                p2d_owner = ast_strdup((char *)sqlite3_column_text(sql0, 0));
                                sprintf(assembled_destination, "%s%s", (sqlite3_column_int(sql0, 1)==145)?("+"):(""), sqlite3_column_text(sql0, 2));
                                p2d_partof = sqlite3_column_int(sql0, 3);
                                p2d_hash = ast_strdup((char *)sqlite3_column_text(sql0, 4));
                                p2d_flash = sqlite3_column_int(sql0, 5);
                            }
                            else if(res == SQLITE_DONE) {
                                break;
                            }
                            else if(res == SQLITE_BUSY){
                                rgsm_usleep(pvt, 1);
                                continue;
                            }
                            else{
                                LOG_STEP_ERROR();
                                break;
                            }
                        }
                        sqlite3_finalize(sql0);
                        break;
                    }
                    else if(res == SQLITE_BUSY){
                        rgsm_usleep(pvt, 1);
                        continue;
                    }
                    else{
                        LOG_PREPARE_ERROR();
                        break;
                    }
                }
                sqlite3_free(str0);

                // search all parts of this message
                if(p2d_owner && p2d_hash){
                    //
                    int traverse_continue;
                    assembled_content_len = 0;
                    // tracking through all parts in all list
                    for(p2d_part=1; p2d_part<=p2d_partof; p2d_part++){
                        //
                        traverse_continue = 0;
                        // preparing
                        str0 = sqlite3_mprintf("SELECT content FROM '%q-preparing' WHERE part=%d AND hash='%q';", pvt->chname, p2d_part, p2d_hash);
                        while(1){
                            res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
                            if(res == SQLITE_OK){
                                row = 0;
                                while(1){
                                    res = sqlite3_step(sql0);
                                    if(res == SQLITE_ROW){
                                        row++;
                                        if(row == 1){
                                            if(assembled_content)
                                                assembled_content_len += sprintf(assembled_content+assembled_content_len, "%s", sqlite3_column_text(sql0, 0));
                                        }
                                        else
                                            ast_log(LOG_ERROR, "<%s>: duplicated(%d) part=%d of message \"%s\"\n", pvt->name, row, p2d_part, p2d_hash);
                                    }
                                    else if(res == SQLITE_DONE) {
                                        break;
                                    }
                                    else if(res == SQLITE_BUSY){
                                        rgsm_usleep(pvt, 1);
                                        continue;
                                    }
                                    else{
                                        LOG_STEP_ERROR();
                                        break;
                                    }
                                }
                                // check result
                                if(row){
                                    // delete from preparing list
                                    str1 = sqlite3_mprintf("DELETE FROM '%q-preparing' WHERE part=%d AND hash='%q';", pvt->chname, p2d_part, p2d_hash);
                                    dao_exec_stmt(str1, 1, pvt);
                                    // to next step
                                    traverse_continue = 1;
                                }
                                sqlite3_finalize(sql0);
                                break;
                            }
                            else if(res == SQLITE_BUSY){
                                rgsm_usleep(pvt, 1);
                                continue;
                            }
                            else{
                                LOG_PREPARE_ERROR();
                                break;
                            }
                        }
                        sqlite3_free(str0);

                        if(traverse_continue) continue;

                        // sent
                        str0 = sqlite3_mprintf("SELECT content FROM '%q-sent' WHERE part=%d AND hash='%q';", pvt->chname, p2d_part, p2d_hash);
                        while(1){
                            res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
                            if(res == SQLITE_OK){
                                row = 0;
                                while(1){
                                    res = sqlite3_step(sql0);
                                    if(res == SQLITE_ROW){
                                        row++;
                                        if(row == 1){
                                            if(assembled_content) {
                                                assembled_content_len += sprintf(assembled_content+assembled_content_len, "%s", sqlite3_column_text(sql0, 0));
                                            }
                                        }
                                        else {
                                            ast_log(LOG_ERROR, "<%s>: duplicated(%d) part=%d of message \"%s\"\n", pvt->name, row, p2d_part, p2d_hash);
                                        }
                                    }
                                    else if(res == SQLITE_DONE) {
                                        break;
                                    }
                                    else if(res == SQLITE_BUSY){
                                        rgsm_usleep(pvt, 1);
                                        continue;
                                    }
                                    else{
                                        LOG_STEP_ERROR();
                                        break;
                                    }
                                }

                                // check result
                                if(row){
                                    // delete from preparing list
                                    str1 = sqlite3_mprintf("DELETE FROM '%q-sent' WHERE part=%d AND hash='%q';", pvt->chname, p2d_part, p2d_hash);
                                    dao_exec_stmt(str1, 1, pvt);
                                }
                                sqlite3_finalize(sql0);
                                break;
                            }
                            else if(res == SQLITE_BUSY){
                                rgsm_usleep(pvt, 1);
                                continue;
                            }
                            else{
                                LOG_PREPARE_ERROR();
                                break;
                            }
                        }
                        sqlite3_free(str0);
                    } // end of tracking section

                    // move mesage to discard/outbox list
                    if(strcmp(p2d_owner, "this")){
                        // is not owner - move to outbox
                        str0 = sqlite3_mprintf("INSERT INTO '%q-outbox' ("
                                                        "destination, " // TEXT
                                                        "content, " // TEXT
                                                        "flash, " // INTEGER
                                                        "enqueued, " // INTEGER
                                                        "hash" // VARCHAR(32) UNIQUE
                                                    ") VALUES ("
                                                        "'%q', " // destination TEXT
                                                        "'%q', " // content TEXT
                                                        "%d, " // flash INTEGER
                                                        "%ld, " // enqueued INTEGER
                                                        "'%q');", // hash  VARCHAR(32) UNIQUE
                                                    pvt->chname,
                                                    assembled_destination, // destination TEXT
                                                    assembled_content, // content TEXT
                                                    p2d_flash, // flash TEXT
                                                    pvt->curr_tv.tv_sec, // enqueued INTEGER
                                                    p2d_hash); // hash  VARCHAR(32) UNIQUE
                        dao_exec_stmt(str0, 1, pvt);
                    } // end of move to owner outbox
                    else {
                        // is owner - move to discard
                        str0 = sqlite3_mprintf("INSERT INTO '%q-discard' ("
                                                        "destination, " // TEXT
                                                        "content, " // TEXT
                                                        "flash, " // INTEGER
                                                        "cause, " // TEXT
                                                        "timestamp, " // INTEGER
                                                        "hash" // VARCHAR(32) UNIQUE
                                                    ") VALUES ("
                                                        "'%q', " // destination TEXT
                                                        "'%q', " // content TEXT
                                                        "%d, " // flash INTEGER
                                                        "'%q', " // cause TEXT
                                                        "%ld, " // timestamp INTEGER
                                                        "'%q');", // hash  VARCHAR(32) UNIQUE
                                                    pvt->chname,
                                                    assembled_destination, // destination TEXT
                                                    assembled_content, // content TEXT
                                                    p2d_flash, // flash TEXT
                                                    (is_str_digit(tp))?(cms_error_print(atoi(tp))):(pvt->recv_buf), // cause TEXT
                                                    pvt->curr_tv.tv_sec, // timestamp INTEGER
                                                    p2d_hash); // hash  VARCHAR(32) UNIQUE

                        dao_exec_stmt(str0, 1, pvt);
                    }
                }
                // free allocated memory
                if(assembled_content) ast_free(assembled_content);
                if(p2d_owner) ast_free(p2d_owner);
                if(p2d_hash) ast_free(p2d_hash);
                } // end of check for attempt count
            } // end of searching last SPACE
        //
        pvt->now_send_pdu_id = 0;
    }
    else if(strstr(pvt->recv_buf, "OK")){
        ;
    }
}

void smssend_timer_handler(struct gsm_pvt *pvt)
{
    // prepare new pdu for sending from outbox
    // get maxmsgid from preparing
    int maxmsgid = 0;
    int res, row, i, olen, rc;
    char * str0;
    sqlite3_stmt *sql0;
    struct pdu *pdu, *curr;

    dao_query_int(sqlite3_mprintf("SELECT MAX(msgid) FROM '%q-preparing';", pvt->chname), 1, pvt, &maxmsgid);

    // increment maxmsgid
    maxmsgid++;

    // get message from outbox
    char *o2p_msgdest = NULL;
    char *o2p_msgcontent = NULL;
    char o2p_msghash[40];
    int o2p_msgflash = 0;
    int o2p_msgno = 0;
    char hsec[24]; // for hash calc
    char husec[8]; // for hash calc
    struct MD5Context Md5Ctx; // for hash calc
    unsigned char hashbin[16]; // for hash calc
    o2p_msghash[0] = '\0';

    str0 = sqlite3_mprintf("SELECT msgno,destination,content,flash,hash FROM '%q-outbox' ORDER BY enqueued;", pvt->chname);

    while(1){
        res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
        if(res == SQLITE_OK){
            row = 0;
            while(1){
                res = sqlite3_step(sql0);
                if(res == SQLITE_ROW){
                    row++;
                    if(!o2p_msgno) {
                        // msgno
                        o2p_msgno = sqlite3_column_int(sql0, 0);
                        // destination
                        o2p_msgdest = ast_strdup((sqlite3_column_text(sql0, 1))?((char *)sqlite3_column_text(sql0, 1)):("null"));
                        // content
                        o2p_msgcontent = ast_strdup((sqlite3_column_text(sql0, 2))?((char *)sqlite3_column_text(sql0, 2)):(""));
                        // flash
                        o2p_msgflash = sqlite3_column_int(sql0, 3);
                        // hash
                        ast_copy_string(o2p_msghash, (sqlite3_column_text(sql0, 4))?((char *)sqlite3_column_text(sql0, 4)):(""), sizeof(o2p_msghash));
                        if(!strlen(o2p_msghash)){
                            //
                            sprintf(hsec, "%ld", pvt->curr_tv.tv_sec);
                            sprintf(husec, "%ld", pvt->curr_tv.tv_usec);
                            //
                            MD5Init(&Md5Ctx);
                            MD5Update(&Md5Ctx, (unsigned char *)pvt->name, strlen(pvt->name));
                            MD5Update(&Md5Ctx, (unsigned char *)o2p_msgdest, strlen(o2p_msgdest));
                            MD5Update(&Md5Ctx, (unsigned char *)o2p_msgcontent, strlen(o2p_msgcontent));
                            MD5Update(&Md5Ctx, (unsigned char *)hsec, strlen(hsec));
                            MD5Update(&Md5Ctx, (unsigned char *)husec, strlen(husec));
                            MD5Final(hashbin, &Md5Ctx);
                            i = olen = 0;
                            for(i=0; i<16; i++) {
                                olen += sprintf(o2p_msghash+olen, "%02x", (unsigned char)hashbin[i]);
                            }
                        }
                    }
                    else {
                        break;
                    }
                }
                else if(res == SQLITE_DONE) {
                    break;
                }
                else if(res == SQLITE_BUSY){
                    rgsm_usleep(pvt, 1);
                    continue;
                }
                else{
                    LOG_STEP_ERROR();
                    break;
                }
            }
            sqlite3_finalize(sql0);
            break;
        }
        else if(res == SQLITE_BUSY){
            rgsm_usleep(pvt, 1);
            continue;
        }
        else{
            LOG_PREPARE_ERROR();
            break;
        }
    }
    sqlite3_free(str0);

    // check for outbox is not empty
    if(o2p_msgno > 0) {
        // calc pdu
        pdu = calc_submit_pdu(o2p_msgcontent, o2p_msgdest, o2p_msgflash, &pvt->smsc_number, pvt->sms_ref++);
        if(pdu){
            curr = pdu;
            i=0;
            while(curr){
                i++;
                curr = curr->next;
            }
            // check for max parts count
            if((i <= pvt->sms_maxpartcount) && (is_address_string(o2p_msgdest))){
                curr = pdu;
                while(curr){
                    // enqueue new pdu into queue sent
                    memset(pvt->prepare_pdu_buf, 0, 512);
                    rc = 0;
                    for(i=0; i < curr->full_len; i++) {
                        rc += sprintf(pvt->prepare_pdu_buf+rc, "%02X", (unsigned char)curr->buf[i]);
                    }
                    // insert new pdu
                    str0 = sqlite3_mprintf("INSERT INTO '%q-preparing' ("
                                        "owner, " // TEXT
                                        "msgid, " // INTEGER
                                        "status, " // INTEGER
                                        "scatype, " // INTEGER
                                        "scaname, " // TEXT
                                        "datype, " // INTEGER
                                        "daname, " // TEXT
                                        "dcs, " // INTEGER
                                        "partid, " // INTEGER
                                        "partof, " // INTEGER
                                        "part, " // INTEGER
                                        "submitpdulen, " // INTEGER
                                        "submitpdu, " // TEXT
                                        "attempt, " // INTEGER
                                        "hash, " // VARCHAR(32)
                                        "flash, " // INTEGER
                                        "content" // TEXT
                                    ") VALUES ("
                                        "'%q', " // owner TEXT
                                        "%d, " // msgid INTEGER
                                        "%d, " // status INTEGER
                                        "%d, " // scatype INTEGER
                                        "'%q', " // scaname TEXT
                                        "%d, " // datype INTEGER
                                        "'%q', " // daname TEXT
                                        "%d, " // dcs INTEGER
                                        "%d, " // partid INTEGER
                                        "%d, " // partof INTEGER
                                        "%d, " // part INTEGER
                                        "%d, " // submitpdulen INTEGER
                                        "'%q', " // submitpdu TEXT
                                        "%d, " // attempt INTEGER
                                        "'%q', " // hash VARCHAR(32)
                                        "%d, " // flash INTEGER
                                        "'%q'" // content TEXT
                                        ");",
                                pvt->chname,
                                "this", // owner TEXT
                                maxmsgid, // msgid INTEGER
                                0, // status INTEGER
                                curr->scaddr.type, // scatype INTEGER
                                curr->scaddr.value, // scaname TEXT
                                curr->raddr.type, // datype INTEGER
                                curr->raddr.value, // daname TEXT
                                curr->dacosc, // dcs INTEGER
                                curr->concat_ref, // partid INTEGER
                                curr->concat_cnt, // partof INTEGER
                                curr->concat_num, // part INTEGER
                                curr->len, // submitpdulen INTEGER
                                pvt->prepare_pdu_buf, // submitpdu TEXT
                                0, // attempt INTEGER
                                o2p_msghash, // hash VARCHAR(32)
                                o2p_msgflash, // flash INTEGER
                                curr->ud); // content TEXT

                    dao_exec_stmt(str0, 1, pvt);
                    // try next
                    curr = curr->next;
                }
            }
            else { // too many parts or invalid destination
                if(is_address_string(o2p_msgdest)) {
                    ast_log(LOG_NOTICE, "<%s>: message fragmented in %d parts - but max part count is %d\n", pvt->name, i, pvt->sms_maxpartcount);
                } else {
                    ast_log(LOG_NOTICE, "<%s>: invalid destination \"%s\"\n", pvt->name, o2p_msgdest);
                }
                // move to discard
                str0 = sqlite3_mprintf("INSERT INTO '%q-discard' ("
                                                    "destination, " // TEXT
                                                    "content, " // TEXT
                                                    "flash, " // INTEGER
                                                    "cause, " // TEXT
                                                    "timestamp, " // INTEGER
                                                    "hash" // VARCHAR(32) UNIQUE
                                                ") VALUES ("
                                                    "'%q', " // destination TEXT
                                                    "'%q', " // content TEXT
                                                    "%d, " // flash INTEGER
                                                    "'%q', " // cause TEXT
                                                    "%ld, " // timestamp INTEGER
                                                    "'%q');", // hash  VARCHAR(32) UNIQUE
                                                pvt->chname,
                                                o2p_msgdest, // destination TEXT
                                                o2p_msgcontent, // content TEXT
                                                o2p_msgflash, // flash TEXT
                                                (is_address_string(o2p_msgdest))?("too many parts"):("invalid destination"), // cause TEXT
                                                pvt->curr_tv.tv_sec, // timestamp INTEGER
                                                o2p_msghash); // hash  VARCHAR(32) UNIQUE
                dao_exec_stmt(str0, 1, pvt);
            } // end too many parts
            pdu_free(pdu);
         }
        if(o2p_msgdest) ast_free(o2p_msgdest);
        if(o2p_msgcontent) ast_free(o2p_msgcontent);
        // delete message from outbox queue
        str0 = sqlite3_mprintf("DELETE FROM '%q-outbox' WHERE msgno=%d;", pvt->chname, o2p_msgno);
        dao_exec_stmt(str0, 1, pvt);
    } // end of o2p_msgno > 0

    // try to sending pdu
    if(!pvt->now_send_pdu_id){
        // get pdu from database with unsent status
        str0 = sqlite3_mprintf("SELECT attempt,msgno,submitpdu,submitpdulen,datype,daname FROM '%q-preparing' WHERE status=0 ORDER BY msgno;", pvt->chname);
        while(1){
            res = sqlite3_prepare_v2(smsdb, str0, strlen(str0), &sql0, NULL);
            if(res == SQLITE_OK) {
                row = 0;
                while(1) {
                    res = sqlite3_step(sql0);
                    if(res == SQLITE_ROW) {
                        row++;
                        if(!pvt->now_send_pdu_id) {
                            pvt->now_send_attempt = sqlite3_column_int(sql0, 0);
                            // check attempts counter
                            if(pvt->now_send_attempt < pvt->sms_sendattempt){
                                // set now send pdu id
                                pvt->now_send_pdu_id = sqlite3_column_int(sql0, 1);
                                // copy submit pdu
                                ast_copy_string(pvt->now_send_pdu_buf, (char *)sqlite3_column_text(sql0, 2), sizeof(pvt->now_send_pdu_buf));
                                pvt->now_send_pdu_len = strlen(pvt->now_send_pdu_buf);
                                // send message command
                                rgsm_atcmd_queue_append(pvt, AT_CMGS, AT_OPER_WRITE, 0, 30, 0, "%d", sqlite3_column_int(sql0, 3));
                                ast_debug(2, "<%s>: sending prepared pdu=\"%s\" to \"%s%s\"\n", pvt->name, pvt->now_send_pdu_buf, (sqlite3_column_int(sql0, 4) == 145)?("+"):(""), sqlite3_column_text(sql0, 5));
                            }
                        }
                    }
                    else if(res == SQLITE_DONE) {
                        break;
                    }
                    else if(res == SQLITE_BUSY) {
                        rgsm_usleep(pvt, 1);
                        continue;
                    }
                    else{
                        LOG_STEP_ERROR();
                        break;
                    }
                }
                sqlite3_finalize(sql0);
                break;
            }
            else if(res == SQLITE_BUSY) {
                rgsm_usleep(pvt, 1);
                continue;
            }
            else{
                LOG_PREPARE_ERROR();
                break;
            }
        }

        sqlite3_free(str0);

        // update send attempts counter
        if(pvt->now_send_pdu_id) {
            str0 = sqlite3_mprintf("UPDATE '%q-preparing' SET attempt=%d WHERE msgno=%d;",
                                   pvt->chname, pvt->now_send_attempt+1, pvt->now_send_pdu_id);
            dao_exec_stmt(str0, 1, pvt);
        }
    }
}

void sms_pdu_response_handler(struct gsm_pvt* pvt)
{
	char tmpbuf[256];
	struct ast_channel *tmp_ast_chnl = NULL;
    char *str0;
    int row;
    int err;

    //CMT PDU
    if (pvt->cmt_pdu_wait) {
        if (!is_str_xdigit(pvt->recv_buf))
        {
            ast_debug(2, "CMT: non hex recv_buf=\"%s\"\n", pvt->recv_buf);
            return;
        }
        // parsing PDU
        struct pdu *pdu;
        ast_debug(2, "CMT: recv_len=%d, pdu_len=%d, hex=\"%s\"\n", pvt->recv_len, pvt->pdu_len, pvt->recv_buf);
        if ((pdu = pdu_parser(pvt->recv_buf, pvt->recv_len, pvt->pdu_len, pvt->curr_tv.tv_sec, &err))) {
            //
            ast_verb(2, "<%s>: received message from \"%s%s\"\n", pvt->name, (pdu->raddr.type.full == 0x91)?("+"):(""), pdu->raddr.value);
            // SMS dialplan notification
            if (pvt->sms_notify_enable) {
                // prevent deadlock while asterisk channel is allocating
                // unlock pvt channel
                ast_mutex_unlock(&pvt->lock);
                // unlock rgsm subsystem
                ast_mutex_unlock(&rgsm_lock);
                // allocation channel in pbx spool
                sprintf(tmpbuf, "%s%s", (pdu->raddr.type.full == 0x91)?("+"):(""), pdu->raddr.value);

                tmp_ast_chnl = ast_channel_alloc(0,												/* int needqueue */
                                                AST_STATE_DOWN,									/* int state */
                                                tmpbuf,									/* const char *cid_num */
                                                tmpbuf,									/* const char *cid_name */
                                                NULL,											/* const char *acctcode */
                                                S_OR(pvt->sms_notify_extension, "s"),			/* const char *exten */
                                                S_OR(pvt->sms_notify_context, "default"),		/* const char *context */
#if ASTERISK_VERSION_NUM >= 10800
                                                NULL,											/* const char *linkedid */
#endif
												pvt->owner,
                                                0,												/* const int amaflag */
												"RGSM/%s-%08x",								/* const char *name_fmt, ... */
                                                pvt->name, channel_id);

                // lock rgsm subsystem
                ast_mutex_lock(&rgsm_lock);
                // lock pvt channel
                ast_mutex_lock(&pvt->lock);
                // increment channel ID
                channel_id++;
                // check channel
                if (tmp_ast_chnl) {
                    // set channel variables
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSCHANNEL", pvt->name); // Channel name
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSCENTERADDRESS", pdu->scaddr.value); // SMS Center Address
                    sprintf(tmpbuf, "%d", pdu->scaddr.type.full);
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSCENTERADDRESSTYPE", tmpbuf); // SMS Center Address Type
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSORIGADDRESS", pdu->raddr.value); // SMS Originator Address
                    sprintf(tmpbuf, "%d", pdu->raddr.type.full);
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSORIGADDRESSTYPE", tmpbuf); // SMS Originator Address Type
                    sprintf(tmpbuf, "%d", pdu->concat_num);
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSPART", tmpbuf); // SMS current part number
                    sprintf(tmpbuf, "%d", pdu->concat_cnt);
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSPARTOF", tmpbuf); // SMS parts count
                    sprintf(tmpbuf, "%ld", pdu->sent);
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSSENT", tmpbuf); // Sent time (UNIX-time)
                    pbx_builtin_setvar_helper(tmp_ast_chnl, "RSMSCONTENT", pdu->ud); // SMS content
                    // start pbx
                    if (ast_pbx_start(tmp_ast_chnl)) {
                        ast_log(LOG_ERROR, "<%s>: unable to start pbx - SMS receiving notification from \"%s\" failed\n", pvt->name, tmpbuf);
                        ast_hangup(tmp_ast_chnl);
                    }
                } else {
                    ast_log(LOG_ERROR, "<%s>: unable to allocate channel - SMS receiving notification from \"%s\" failed\n", pvt->name, tmpbuf);
                }
            }
            // get max message id

            int msgid = 0;
            str0 = sqlite3_mprintf("SELECT MAX(msgid) FROM '%q-inbox';", pvt->chname);
            dao_query_int(str0, 1, pvt, &msgid);
            // check for concatenated message
            int concatmsgid = 0;
            if (pdu->concat_cnt > 1) {
                // is concatenated message
                str0 = sqlite3_mprintf("SELECT MAX(msgid) FROM '%q-inbox' "
                                        "WHERE "
                                        "scatype=%d AND "
                                        "scaname='%q' AND "
                                        "oatype=%d AND "
                                        "oaname='%q' AND "
                                        "partid=%d AND "
                                        "partof=%d;",
                                        pvt->chname,
                                        pdu->scaddr.type,
                                        pdu->scaddr.value,
                                        pdu->raddr.type,
                                        pdu->raddr.value,
                                        pdu->concat_ref,
                                        pdu->concat_cnt);

                dao_query_int(str0, 1, pvt, &concatmsgid);

                // check for concatenated message is complete
                if (concatmsgid) {
                    row = 0;
                    str0 = sqlite3_mprintf("SELECT msgno FROM '%q-inbox' WHERE msgid=%d;", pvt->chname, concatmsgid);
                    dao_query_int(str0, 1, pvt, &row);

                    // check part of concatenated message
                    if (row < pdu->concat_cnt) {
                        msgid = concatmsgid;
                    } else if (row == pdu->concat_cnt) {
                        msgid++;
                    } else {
                        ast_log(LOG_ERROR, "<%s>: inbox message=%d has too more parts (%d of %d)\n",
                                pvt->name, concatmsgid, row, pdu->concat_cnt);
                        msgid++;
                    }
                } else { // increment max id number
                    msgid++;
                }
            } else {
                msgid++;
            }
            // insert new message into sms database
            str0 = sqlite3_mprintf("INSERT INTO '%q-inbox' ("
                                    "pdu, " // TEXT
                                    "msgid, " // INTEGER
                                    "status, " // INTEGER
                                    "scatype, " // INTEGER
                                    "scaname, " // TEXT
                                    "oatype, " // INTEGER
                                    "oaname, " // TEXT
                                    "dcs, " // INTEGER
                                    "sent, " // INTEGER
                                    "received," // INTEGER
                                    "partid, " // INTEGER,
                                    "partof, " // INTEGER,
                                    "part, " // INTEGER,
                                    "content" // TEXT
                                    ") VALUES ("
                                    "'%q', " // pdu TEXT
                                    "%d, " // msgid INTEGER
                                     "%d, " // status INTEGER
                                    "%d, " // scatype INTEGER
                                    "'%q', " // scaname TEXT
                                    "%d, " // oatype INTEGER
                                    "'%q', " // oaname TEXT
                                    "%d, " // dcs INTEGER
                                    "%ld, " // sent INTEGER
                                    "%ld, " // received INTEGER
                                    "%d, " // partid INTEGER,
                                    "%d, " // partof INTEGER,
                                    "%d, " // part INTEGER,
                                    "'%q');", // content TEXT
                                    pvt->chname,
                                    pvt->recv_buf, // pdu TEXT
                                    msgid, // msgid INTEGER
                                    1, // status INTEGER
                                    pdu->scaddr.type, // scatype INTEGER
                                    pdu->scaddr.value, // scaname TEXT
                                    pdu->raddr.type, // oatype INTEGER
                                    pdu->raddr.value, // oaname TEXT
                                    pdu->dacosc, // dcs INTEGER
                                    pdu->sent, // sent INTEGER
                                    pdu->delivered, // received INTEGER
                                    pdu->concat_ref, // partid INTEGER,
                                    pdu->concat_cnt, // partof INTEGER,
                                    pdu->concat_num, // part INTEGER,
                                    pdu->ud); // content TEXT

            dao_exec_stmt(str0, 1, pvt);
            //
            pdu_free(pdu);
        } else {
            ast_log(LOG_ERROR, "<%s>: PDU parsing error=%d\n", pvt->name, err);
            str0 = sqlite3_mprintf("INSERT INTO '%q-inbox' (pdu) VALUES ('%q');", pvt->chname, pvt->recv_buf);
            dao_exec_stmt(str0, 1, pvt);
        }
        // reset wait flag
        pvt->cmt_pdu_wait = 0;
    } else if (strstr(pvt->recv_buf, "+CDS:")) {
        if ((str0 = strchr(pvt->recv_buf, SP))) {
            str0++;
            if (is_str_digit(str0)) {
                pvt->pdu_len = atoi(str0);
                pvt->cds_pdu_wait = 1;
            }
        }
    } else if ((pvt->cds_pdu_wait) && (is_str_xdigit(pvt->recv_buf))) {
        // CDS PDU
        // parsing PDU
        struct pdu *pdu;
        ast_debug(2, "CDS: len=%d, hex=\"%s\"\n", pvt->recv_len, pvt->recv_buf);
        if ((pdu = pdu_parser(pvt->recv_buf, pvt->recv_len, pvt->pdu_len, 0, &err))) {
            //
            if (!pdu->status) {
                ast_verb(2, "<%s>: message delivered to \"%s%s\"\n", pvt->name, (pdu->raddr.type.full == 0x91)?("+"):(""), pdu->raddr.value);

                str0 = sqlite3_mprintf("UPDATE '%q-sent' SET status=1, received=%ld, stareppdulen=%d, stareppdu='%q' WHERE mr=%d AND daname='%q';",
                                        pvt->chname,
                                        pdu->delivered,
                                        pvt->pdu_len,
                                        pvt->recv_buf,
                                        pdu->mr,
                                        pdu->raddr.value);

                dao_exec_stmt(str0, 1, pvt);
            } else {
                ast_verb(2, "<%s>: message undelivered to \"%s%s\" - status=%d\n",
                            pvt->name, (pdu->raddr.type.full == 0x91)?("+"):(""), pdu->raddr.value, pdu->status);
            }
            pdu_free(pdu);
        } else {
            ast_log(LOG_ERROR, "<%s>: PDU parsing error=%d\n", pvt->name, err);
        }
        // reset wait flag
        pvt->cds_pdu_wait = 0;
    }
}


