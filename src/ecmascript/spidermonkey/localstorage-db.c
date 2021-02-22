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
	char *sqlbuffer;

	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return(-1);
	}
	sqlite3_busy_timeout(db, 5000);
	sqlbuffer=stracpy("CREATE TABLE storage (key TEXT, value TEXT);");
	rc=sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
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
	char *sqlbuffer;
	int affected_rows = 0;

	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return(-1);
	}
	sqlite3_busy_timeout(db, 5000);
	sqlbuffer=stracpy("DELETE FROM storage WHERE key = ?;");
	rc=sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
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
	char *sqlbuffer;
	int affected_rows = 0;

	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 5000);
	sqlbuffer=stracpy("INSERT INTO storage (value,key) VALUES (?,?);");
	rc=sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
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
	char sqlbuffer[8192];
	char result[8192];
	int affected_rows = 0;

	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 5000);
	snprintf(sqlbuffer, 256, "UPDATE storage SET value = ? where key = ?;");
	rc=sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
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
	char *sqlbuffer;
	char *result;

	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return("");
	}
	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 2000);
	sqlbuffer=stracpy("SELECT * FROM storage WHERE value like ?;");
	rc=sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	rc=sqlite3_bind_text(stmt, 1, value, strlen(value) + 1, SQLITE_STATIC);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if ((const char*) sqlite3_column_text(stmt,1)!= NULL) {
			result=stracpy((const char *)sqlite3_column_text(stmt, 1));
			//DBG("%s",result);
		} else {
			result=stracpy("");
		}
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
	char *sqlbuffer;
	char *result;

	rc = sqlite3_open(db_name, &db);
	if (rc)
	{
		//DBG("Error opening localStorage database.");
		rc=sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 2000);
	sqlbuffer=stracpy("SELECT * FROM storage WHERE key like ?;");
	rc=sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	rc=sqlite3_bind_text(stmt, 1, key, strlen(key) + 1, SQLITE_STATIC);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if ((const char*) sqlite3_column_text(stmt,1)!= NULL) {
			result=stracpy((const char *)sqlite3_column_text(stmt, 1));
			//DBG("%s",result);
		} else {
			result=stracpy("");
		}
	}
	rc=sqlite3_finalize(stmt);
	rc=sqlite3_close(db);
	return(result);

}
