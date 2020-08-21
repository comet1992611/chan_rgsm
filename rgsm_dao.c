#include "rgsm_dao.h"

sqlite3 *smsdb = NULL;

void open_sms_db(const char *db_dir)
{
	char path[PATH_MAX];
	char *stmt_str;
    struct gsm_pvt*	pvt = 0;
	struct gateway* gw;
	int i, res;

    if (smsdb) return;

	// open SMS database
	sprintf(path, "%s/rgsm-sms.db", db_dir);
	res = sqlite3_open(path, &smsdb);
	if(res != SQLITE_OK){
		ast_log(LOG_ERROR, "could not open RGSM SMS database \"%s\"\n", path);
		return;
    }

    ast_log(LOG_DEBUG, "RGSM SMS database open: \"%s\"\n", path);

	AST_LIST_TRAVERSE (&gateways, gw, link)
	{
        ast_log(LOG_DEBUG, "Create tables for gw=%p\n", gw);
        //
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            //
            pvt = gw->gsm_pvts[i];
            //skip unexising modems
            if (!pvt) continue;

            ast_log(LOG_DEBUG, "Create tables for: \"%s\"\n", pvt->chname);

            // create table for inbox SMS
            stmt_str = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS '%q-inbox' ("
                                    "msgno INTEGER PRIMARY KEY, "
                                    "pdu TEXT, "
                                    "msgid INTEGER, "
                                    "status INTEGER, "
                                    "scatype INTEGER, "
                                    "scaname TEXT, "
                                    "oatype INTEGER, "
                                    "oaname TEXT, "
                                    "dcs INTEGER, "
                                    "sent INTEGER, "
                                    "received INTEGER, "
                                    "partid INTEGER, "
                                    "partof INTEGER, "
                                    "part INTEGER, "
                                    "content TEXT"
                                    ");", pvt->chname);

            dao_exec_stmt(stmt_str, 1, NULL);

            // create table for outbox SMS
            stmt_str = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS '%q-outbox' ("
                                    "msgno INTEGER PRIMARY KEY, "
                                    "destination TEXT, "
                                    "content TEXT, "
                                    "flash INTEGER, "
                                    "enqueued INTEGER, "
                                    "hash VARCHAR(32) UNIQUE"
                                    ");", pvt->chname);
            dao_exec_stmt(stmt_str, 1, NULL);

            // create table for preparing message PDU
            stmt_str = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS '%q-preparing' ("
                                    "msgno INTEGER PRIMARY KEY, "
                                    "owner TEXT, "
                                    "msgid INTEGER, "
                                    "status INTEGER, "
                                    "scatype INTEGER, "
                                    "scaname TEXT, "
                                    "datype INTEGER, "
                                    "daname TEXT, "
                                    "dcs INTEGER, "
                                    "partid INTEGER, "
                                    "partof INTEGER, "
                                    "part INTEGER, "
                                    "submitpdulen INTEGER, "
                                    "submitpdu TEXT, "
                                    "attempt INTEGER, "
                                    "hash VARCHAR(32), "
                                    "flash INTEGER, "
                                    "content TEXT"
                                    ");", pvt->chname);
            dao_exec_stmt(stmt_str, 1, NULL);

            // create table for sent SMS
            stmt_str = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS '%q-sent' ("
                                    "msgno INTEGER PRIMARY KEY, "
                                    "owner TEXT, "
                                    "msgid INTEGER, "
                                    "status INTEGER, "
                                    "mr INTEGER, "
                                    "scatype INTEGER, "
                                    "scaname TEXT, "
                                    "datype INTEGER, "
                                    "daname TEXT, "
                                    "dcs INTEGER, "
                                    "sent INTEGER, "
                                    "received INTEGER, "
                                    "partid INTEGER, "
                                    "partof INTEGER, "
                                    "part INTEGER, "
                                    "submitpdulen INTEGER, "
                                    "submitpdu TEXT, "
                                    "stareppdulen INTEGER, "
                                    "stareppdu TEXT, "
                                    "attempt INTEGER, "
                                    "hash VARCHAR(32), "
                                    "flash INTEGER, "
                                    "content TEXT"
                                    ");", pvt->chname);
            dao_exec_stmt(stmt_str, 1, NULL);

            // create table for discard SMS
            stmt_str = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS '%q-discard' ("
                                    "id INTEGER PRIMARY KEY, "
                                    "destination TEXT, "
                                    "content TEXT, "
                                    "flash INTEGER, "
                                    "cause TEXT, "
                                    "timestamp INTEGER, "
                                    "hash VARCHAR(32)"
                                    ");", pvt->chname);
            dao_exec_stmt(stmt_str, 1, NULL);
        }
	}
}

void close_sms_db()
{
    if (smsdb) {
        sqlite3_close(smsdb);
        smsdb = NULL;
        ast_log(LOG_DEBUG, "RGSM SMS database closed\n");
    }
}

int dao_exec_stmt(char *stmt_str, int free_str, struct gsm_pvt*	pvt)
{
    sqlite3_stmt *stmt;
	int res;

    while(1){
        res = sqlite3_prepare_v2(smsdb, stmt_str, strlen(stmt_str), &stmt, NULL);
        if(res == SQLITE_OK){
            while(1){
                res = sqlite3_step(stmt);
                if(res == SQLITE_ROW || res == SQLITE_DONE) {
                    res = SQLITE_OK;
                    break;
                }
                else if(res == SQLITE_BUSY){
                    if (pvt) {
                        rgsm_usleep(pvt, 1);
                    } else {
                        usleep(1);
                    }
                    continue;
                }
                else{
                    ast_log(LOG_ERROR, "sqlite3_step(): %d: %s\n", res, sqlite3_errmsg(smsdb));
                    break;
                }
            }
            sqlite3_finalize(stmt);
            break;
        }
        else if(res == SQLITE_BUSY){
            if (pvt) {
                rgsm_usleep(pvt, 1);
            } else {
                usleep(1);
            }
            continue;
        }
        else{
            ast_log(LOG_ERROR, "sqlite3_prepare_v2(): %d: %s\n", res, sqlite3_errmsg(smsdb));
            break;
        }
    }
    if (free_str) sqlite3_free(stmt_str);
    return res;
}

int dao_query_int(char *stmt_str, int free_str, struct gsm_pvt*	pvt, int *value)
{
    sqlite3_stmt *stmt;
	int res;

    while(1){
        res = sqlite3_prepare_v2(smsdb, stmt_str, strlen(stmt_str), &stmt, NULL);
        if(res == SQLITE_OK){
            while(1){
                res = sqlite3_step(stmt);
                if(res == SQLITE_ROW) {
                    *value = sqlite3_column_int(stmt, 0);
                    break;
                } else if (res == SQLITE_DONE) {
                    //empty result set returned
                    break;
                } else if(res == SQLITE_BUSY){
                    if (pvt) {
                        rgsm_usleep(pvt, 1);
                    } else {
                        usleep(1);
                    }
                    continue;
                }
                else{
                    ast_log(LOG_ERROR, "sqlite3_step(): %d: %s\n", res, sqlite3_errmsg(smsdb));
                    break;
                }
            }
            sqlite3_finalize(stmt);
            break;
        }
        else if(res == SQLITE_BUSY){
            if (pvt) {
                rgsm_usleep(pvt, 1);
            } else {
                usleep(1);
            }
            continue;
        }
        else{
            ast_log(LOG_ERROR, "sqlite3_prepare_v2(): %d: %s\n", res, sqlite3_errmsg(smsdb));
            break;
        }
    }
    if (free_str) sqlite3_free(stmt_str);
    return res;
}
