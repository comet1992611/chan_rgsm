#include "rgsm_timers.h"

struct timeval waitrdy_timeout = {10, 0};
struct timeval waitsuspend_timeout = {30, 0};
struct timeval testfun_timeout = {10, 0};
struct timeval testfunsend_timeout = {1, 0};
struct timeval callready_timeout = {300, 0};
struct timeval runhalfsecond_timeout = {0, 500000};
struct timeval runonesecond_timeout = {1, 0};
struct timeval runfivesecond_timeout = {5, 0};
struct timeval halfminute_timeout = {30, 0};
struct timeval runoneminute_timeout = {60, 0};
//struct timeval waitviodown_timeout = {20, 0};
//struct timeval testviodown_timeout = {1, 0};
struct timeval onesec_timeout = {1, 0};
struct timeval zero_timeout = {0, 1000};
struct timeval simpoll_timeout = {5, 0}; //{2, 0};
struct timeval pinwait_timeout = {40, 0}; //{20, 0}; //{8, 0};
struct timeval abaudwait_timeout = {5, 0};
struct timeval abaudsend_timeout = {1, 0};
struct timeval trysend_timeout = {0, 20000};    //initial timeout, will be corrected per gsm module type
struct timeval initready_timeout = {10, 0};
