#ifndef RGSM_TIMERS_H_INCLUDED
#define RGSM_TIMERS_H_INCLUDED

#include <sys/types.h>

struct rgsm_timer {
	int enable;
	struct timeval start;
	struct timeval timeout;
	struct timeval expires;
};

//
#define tv_set(_tv, _sec, _usec) \
	do { \
		_tv.tv_sec = _sec; \
		_tv.tv_usec = _usec; \
		} while(0)
//
#define tv_cpy(_dest, _src) \
	do { \
		_dest.tv_sec = _src.tv_sec; \
		_dest.tv_usec = _src.tv_usec; \
		} while(0)
//
#define tv_add(_tv_1, _tv_2) \
	do { \
		_tv_1.tv_sec += _tv_2.tv_sec; \
		_tv_1.tv_usec += _tv_2.tv_usec; \
		if(_tv_1.tv_usec >= 1000000){ \
			_tv_1.tv_usec -= 1000000; \
			_tv_1.tv_sec += 1; \
			} \
		} while(0)
//
#define tv_cmp(tv_1, tv_2) \
	({ \
		int res = 0; \
		if((tv_1)->tv_sec < (tv_2)->tv_sec) \
			res = -1; \
		else if((tv_1)->tv_sec > (tv_2)->tv_sec) \
			res = 1; \
		else if((tv_1)->tv_sec == (tv_2)->tv_sec) { \
			if((tv_1)->tv_usec < (tv_2)->tv_usec) \
				res = -1; \
			else if((tv_1)->tv_usec > (tv_2)->tv_usec) \
				res = 1; \
			else \
				res = 0; \
			} \
		res; \
	})
//
#define rgsm_timer_set(_timer, _timeout) \
	do { \
		struct timeval __curr_time; \
		gettimeofday(&__curr_time, NULL); \
		_timer.enable = 1; \
		tv_cpy(_timer.start, __curr_time); \
		tv_cpy(_timer.timeout, _timeout); \
		tv_cpy(_timer.expires, _timer.start); \
		tv_add(_timer.expires, _timer.timeout); \
		} while(0)
//
#define rgsm_timer_stop(_timer) \
	do { \
		_timer.enable = 0; \
		} while(0)
//
#define is_rgsm_timer_enable(_timer) \
	({ \
		int _res = 0; \
		_res = _timer.enable; \
		_res; \
		})
//
#define is_rgsm_timer_active(_timer) \
	({ \
		int _res = 0; \
		struct timeval __curr_time; \
		gettimeofday(&__curr_time, NULL); \
		if(tv_cmp(&(_timer.expires), &(__curr_time)) > 0) \
			_res = 1; \
		else \
			_res = 0; \
		_res; \
		})
//
#define is_rgsm_timer_fired(_timer) \
	({ \
		int _res = 0; \
		struct timeval __curr_time; \
		gettimeofday(&__curr_time, NULL); \
		if(tv_cmp(&(_timer.expires), &(__curr_time)) > 0) \
			_res = 0; \
		else \
			_res = 1; \
		_res; \
		})


typedef struct rgsm_timers {
	struct rgsm_timer waitrdy;
	struct rgsm_timer waitsuspend;
	struct rgsm_timer testfun;
	struct rgsm_timer testfunsend;
	struct rgsm_timer callready;
/*! May 22, 2013: retired
	struct rgsm_timer runhalfsecond;
*/
	struct rgsm_timer runonesecond;
	struct rgsm_timer runfivesecond;
	struct rgsm_timer runhalfminute;
	struct rgsm_timer runoneminute;
//	struct rgsm_timer waitviodown;
//	struct rgsm_timer testviodown;
	struct rgsm_timer dial;
	struct rgsm_timer smssend;
	struct rgsm_timer simpoll;
	struct rgsm_timer pinwait;
	struct rgsm_timer abaudwait;
//	struct rgsm_timer abaudsend;
	struct rgsm_timer dtmf;
	struct rgsm_timer trysend;
	struct rgsm_timer initready;
	struct rgsm_timer initstat;
} rgsm_timers_t;

extern struct timeval waitrdy_timeout;
extern struct timeval waitsuspend_timeout;
extern struct timeval testfun_timeout;
extern struct timeval testfunsend_timeout;
extern struct timeval callready_timeout;
extern struct timeval runhalfsecond_timeout;
extern struct timeval runonesecond_timeout;
extern struct timeval runfivesecond_timeout;
extern struct timeval halfminute_timeout;
extern struct timeval runoneminute_timeout;
//extern struct timeval waitviodown_timeout;
//extern struct timeval testviodown_timeout;
//extern struct timeval onesec_timeout;
extern struct timeval zero_timeout;
extern struct timeval simpoll_timeout;
extern struct timeval pinwait_timeout;
extern struct timeval abaudwait_timeout;
extern struct timeval trysend_timeout;
extern struct timeval initready_timeout;

//extern struct timeval abaudsend_timeout;

#endif // RGSM_TIMERS_H_INCLUDED