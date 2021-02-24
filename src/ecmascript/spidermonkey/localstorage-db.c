/* The SpiderMonkey localstorage database helper implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"
#include "src/ecmascript/ecmascript.h"

extern const int
db_prepare_structure(char *db_name)
{
	sqlite3_stmt *stmt;
	sqlite3 *db;

	int rc;

	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return(-1);
	}
	sqlite3_busy_timeout(db, 2000);
	rc=sqlite3_prepare_v2(db, "CREATE TABLE storage (key TEXT, value TEXT);", -1, &stmt, NULL);
	rc=sqlite3_step(stmt);
	rc=sqlite3_finalize(stmt);
	rc=sqlite3_close(db);
	return(0);
}

extern const int
db_delete_from(char *db_name, char *key)
{

	sqlite3_stmt *stmt;
	sqlite3 *db;

	int rc;
	int affected_rows = 0;

	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return(-1);
	}
	sqlite3_busy_timeout(db, 2000);
	rc=sqlite3_prepare_v2(db, "DELETE FROM storage WHERE key = ?;", -1, &stmt, NULL);
	rc=sqlite3_bind_text(stmt, 1, key, strlen(key), SQLITE_STATIC);
	rc=sqlite3_step(stmt);
	rc=sqlite3_finalize(stmt);
	affected_rows=sqlite3_changes(db);
	rc=sqlite3_close(db);
	return(affected_rows);

}


extern const int
db_insert_into(char *db_name, char *key, char *value)
{
	sqlite3_stmt *stmt;
	sqlite3 *db;

	int rc;
	int affected_rows = 0;

	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return(-1);
	}
	sqlite3_busy_timeout(db, 2000);
	rc=sqlite3_prepare_v2(db, "INSERT INTO storage (value,key) VALUES (?,?);", -1, &stmt, NULL);
	rc=sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);
	rc=sqlite3_bind_text(stmt, 2, key, strlen(key), SQLITE_STATIC);
	rc=sqlite3_step(stmt);
	rc=sqlite3_finalize(stmt);
	affected_rows=sqlite3_changes(db);
	rc=sqlite3_close(db);
	return(affected_rows);

}

extern const int
db_update_set(char *db_name, char *key, char *value)
{

	sqlite3_stmt *stmt;
	sqlite3 *db;

	int rc;
	int affected_rows = 0;

	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return(-1);
	}
	sqlite3_busy_timeout(db, 2000);
	rc=sqlite3_prepare_v2(db, "UPDATE storage SET value = ? where key = ?;", -1, &stmt, NULL);
	rc=sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);
	rc=sqlite3_bind_text(stmt, 2, key, strlen(key), SQLITE_STATIC);
	rc=sqlite3_step(stmt);
	rc=sqlite3_finalize(stmt);
	affected_rows=sqlite3_changes(db);
	rc=sqlite3_close(db);
	return(affected_rows);

}

extern const char *
db_query_by_value(char *db_name, char *value)
{

	sqlite3_stmt *stmt;
	sqlite3 *db;

	int rc;
	char *result;

	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 2000);
	rc=sqlite3_prepare_v2(db, "SELECT key FROM storage WHERE value = ? LIMIT 1;", -1, &stmt, NULL);
	rc=sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);
	result=stracpy("");
	if ((const char*) sqlite3_column_text(stmt,1)!= NULL) {
		result=stracpy((const char *)sqlite3_column_text(stmt, 1));
	}
	rc=sqlite3_finalize(stmt);
	rc=sqlite3_close(db);
	return(result);

}

extern const char *
db_query_by_key(char *db_name, char *key)
{

	sqlite3_stmt *stmt;
	sqlite3 *db;

	int rc;
	char *result;

	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 2000);
	rc=sqlite3_prepare_v2(db, "SELECT * FROM storage WHERE key = ? LIMIT 1;", -1, &stmt, NULL);
	rc=sqlite3_bind_text(stmt, 1, key, strlen(key), SQLITE_STATIC);
	result=stracpy("");
	rc=sqlite3_step(stmt);
	if ((const char*) sqlite3_column_text(stmt,1)!= NULL) {
		result=stracpy((const unsigned char *)sqlite3_column_text(stmt, 1));
	}
	rc=sqlite3_finalize(stmt);
	rc=sqlite3_close(db);
	return(result);

}
