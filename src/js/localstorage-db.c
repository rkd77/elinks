/* The localstorage database helper implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/localstorage-db.h"
#include "util/string.h"

int
db_prepare_structure(char *db_name)
{
	ELOG
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc = sqlite3_open(db_name, &db);

	if (rc) {
		//DBG("Error opening localStorage database.");
		rc = sqlite3_close(db);
		return -1;
	}
	sqlite3_busy_timeout(db, 2000);
	rc = sqlite3_prepare_v2(db, "CREATE TABLE storage (key TEXT, value TEXT);", -1, &stmt, NULL);
	rc = sqlite3_step(stmt);
	rc = sqlite3_finalize(stmt);
	rc = sqlite3_close(db);

	return 0;
}

int
db_delete_from(char *db_name, const char *key)
{
	ELOG
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	int affected_rows = 0;
	rc = sqlite3_open(db_name, &db);

	if (rc) {
		//DBG("Error opening localStorage database.");
		rc = sqlite3_close(db);
		return -1;
	}
	sqlite3_busy_timeout(db, 2000);
	rc = sqlite3_prepare_v2(db, "DELETE FROM storage WHERE key = ?;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, key, strlen(key), SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	rc = sqlite3_finalize(stmt);
	affected_rows = sqlite3_changes(db);
	rc = sqlite3_close(db);

	return affected_rows;
}


int
db_insert_into(char *db_name, const char *key, const char *value)
{
	ELOG
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	int affected_rows = 0;
	rc = sqlite3_open(db_name, &db);

	if (rc) {
		//DBG("Error opening localStorage database.");
		rc = sqlite3_close(db);
		return -1;
	}
	sqlite3_busy_timeout(db, 2000);
	rc = sqlite3_prepare_v2(db, "INSERT INTO storage (value,key) VALUES (?,?);", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);
	rc = sqlite3_bind_text(stmt, 2, key, strlen(key), SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	rc = sqlite3_finalize(stmt);
	affected_rows = sqlite3_changes(db);
	rc = sqlite3_close(db);

	return affected_rows;
}

int
db_update_set(char *db_name, const char *key, const char *value)
{
	ELOG
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	int affected_rows = 0;
	rc = sqlite3_open(db_name, &db);

	if (rc) {
		//DBG("Error opening localStorage database.");
		rc = sqlite3_close(db);
		return -1;
	}
	sqlite3_busy_timeout(db, 2000);
	rc = sqlite3_prepare_v2(db, "UPDATE storage SET value = ? where key = ?;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);
	rc = sqlite3_bind_text(stmt, 2, key, strlen(key), SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	rc = sqlite3_finalize(stmt);
	affected_rows = sqlite3_changes(db);
	rc = sqlite3_close(db);

	return affected_rows;
}

char *
db_query_by_value(char *db_name, const char *value)
{
	ELOG
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char *result;
	rc = sqlite3_open(db_name, &db);

	if (rc) {
		//DBG("Error opening localStorage database.");
		rc = sqlite3_close(db);
		return stracpy("");
	}
	sqlite3_busy_timeout(db, 2000);
	rc = sqlite3_prepare_v2(db, "SELECT key FROM storage WHERE value = ? LIMIT 1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);

	if (sqlite3_column_text(stmt, 1) != NULL) {
		result = stracpy((const char *)sqlite3_column_text(stmt, 1));
	} else {
		result = stracpy("");
	}
	rc = sqlite3_finalize(stmt);
	rc = sqlite3_close(db);

	return result;
}

char *
db_query_by_key(char *db_name, const char *key)
{
	ELOG
	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char *result;
	rc = sqlite3_open(db_name, &db);

	if (rc) {
		//DBG("Error opening localStorage database.");
		rc = sqlite3_close(db);
		return NULL;
	}
	sqlite3_busy_timeout(db, 2000);
	rc = sqlite3_prepare_v2(db, "SELECT * FROM storage WHERE key = ? LIMIT 1;", -1, &stmt, NULL);
	rc = sqlite3_bind_text(stmt, 1, key, strlen(key), SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	if (sqlite3_column_text(stmt, 1) != NULL) {
		result = stracpy((const char *)sqlite3_column_text(stmt, 1));
	} else {
		result = NULL;
	}
	rc = sqlite3_finalize(stmt);
	rc = sqlite3_close(db);

	return result;
}
