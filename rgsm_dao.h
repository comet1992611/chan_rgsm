#ifndef RGSM_DAO_H_INCLUDED
#define RGSM_DAO_H_INCLUDED

#include "chan_rgsm.h"
#include <sqlite3.h>

#define LOG_PREPARE_ERROR() ast_log(LOG_ERROR, "<%s>: sqlite3_prepare_v2(): %d: %s\n", pvt->name, res, sqlite3_errmsg(smsdb))
#define LOG_STEP_ERROR()    ast_log(LOG_ERROR, "<%s>: sqlite3_step(): %d: %s\n", pvt->name, res, sqlite3_errmsg(smsdb))

extern sqlite3 *smsdb;

void open_sms_db(const char *db_dir);
void close_sms_db();

/**
 * Executes sql statement
 * @param stmt_str - a statement string
 * @param free_str - boolean param to dispose the stmt_str on return
 * @param pvt - a pointer to gsm_pvt, optional, used to unlock/lock the rgsm and channel mutexes during wait.
 * @return a SQLITE error code on failure or SQLITE_OK in success
 */
int dao_exec_stmt(char *stmt_str, int free_str, struct gsm_pvt*	pvt);

/**
 * Queries a sms_db to retrieve value of scalar column or expression
 * @param stmt_str - a statement string
 * @param free_str - boolean param to dispose the stmt_str on return
 * @param pvt - a pointer to gsm_pvt, used to unlock/lock the rgsm and channel mutexes during wait.
 * @param value -  a pointer to int to return value
 * @return a SQLITE error code on failure or SQLITE_OK in success
 */
int dao_query_int(char *stmt_str, int free_str, struct gsm_pvt*	pvt, int *value);

#endif // RGSM_DAO_H_INCLUDED
