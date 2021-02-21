/* The SpiderMonkey localstorage database helper implementation. */

//
// create table storage (key text, value text);
//

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

extern const int delete_db(char *db_name, char *key) {

	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char sqlbuffer[8192];
	char result[8192];
	int affected_rows = 0;

	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//printf("Error opening localStorage database.");
		sqlite3_close(db);
		return(-1);
	}
	sqlite3_busy_timeout(db, 5000);
	snprintf(sqlbuffer, 256, "DELETE FROM storage where key = ?;");
	sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, key, strlen(key), SQLITE_STATIC);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	affected_rows=sqlite3_changes(db);
	sqlite3_close(db);
	return(affected_rows);

}


extern const int insert_db(char *db_name, char *key, char *value) {

	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char sqlbuffer[256];
	char result[1024];
	int affected_rows = 0;
	
	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//printf("Error opening localStorage database.");
		sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 5000);
	snprintf(sqlbuffer, 256, "insert into storage (value,key) values (?,?);");
	sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, key, strlen(key), SQLITE_STATIC);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	affected_rows=sqlite3_changes(db);
	sqlite3_close(db);
	return(affected_rows);

}

extern const int delete_tmp_db(char *db_name, char *key) {

	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char sqlbuffer[256];
	char result[1024];
	int affected_rows = 0;
	
	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//printf("Error opening localStorage database.");
		sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 5000);
	snprintf(sqlbuffer, 256, "delete from storage where value not in (select key from storage) and key like '6.\045';");
	sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	//sqlite3_bind_text(stmt, 1, key, strlen(key), SQLITE_STATIC);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	affected_rows=sqlite3_changes(db);
	sqlite3_close(db);
	return(affected_rows);

}

extern const int update_db(char *db_name, char *key, char *value) {

	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char sqlbuffer[256];
	char result[1024];
	int affected_rows = 0;
	
	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//printf("Error opening localStorage database.");
		sqlite3_close(db);
		return("");
	}
	sqlite3_busy_timeout(db, 5000);
	snprintf(sqlbuffer, 256, "UPDATE storage SET value = ? where key = ?;");
	sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, value, strlen(value), SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, key, strlen(key), SQLITE_STATIC);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	affected_rows=sqlite3_changes(db);
	sqlite3_close(db);
	return(affected_rows);

}

extern const char * qry_db_val(char *db_name, char *val) {

	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char sqlbuffer[256];
	//char result[1024]="";
	char *result = "";
	result = (char * ) malloc(8192);
	sprintf(result,"%s","");
	
	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//printf("Error opening localStorage database.");
		//sqlite3_close(db);
		//r eturn(-1);
		return("");
	}
	sqlite3_busy_timeout(db, 2000);
	snprintf(sqlbuffer, 256, "SELECT * FROM storage WHERE value like ?;");
	sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, val, strlen(val) + 1, SQLITE_STATIC);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if ((const char*) sqlite3_column_text(stmt,1)!= NULL) {
			sprintf(result,"%s",(const char *)sqlite3_column_text(stmt, 1));
			//printf("%s\n",result);
		} else {
			result=="";
		}
		//sqlite3_close(db);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return(result);

}

extern const char * qry_db(char *db_name, char *key) {

	sqlite3_stmt *stmt;
	sqlite3 *db;
	int rc;
	char sqlbuffer[256];
	//char result[1024]="";
	char *result = "";
	result = (char * ) malloc(8192);
	sprintf(result,"%s","");
	
	rc = sqlite3_open(db_name, &db);
	if (rc) {
		//printf("Error opening localStorage database.");
		//sqlite3_close(db);
		//r eturn(-1);
		return("");
	}
	sqlite3_busy_timeout(db, 2000);
	snprintf(sqlbuffer, 256, "SELECT * FROM storage WHERE key like ?;");
	sqlite3_prepare_v2(db, sqlbuffer, strlen(sqlbuffer) + 1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, key, strlen(key) + 1, SQLITE_STATIC);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if ((const char*) sqlite3_column_text(stmt,1)!= NULL) {
			sprintf(result,"%s",(const char *)sqlite3_column_text(stmt, 1));
			//printf("%s\n",result);
		} else {
			result=="";
		}
		//sqlite3_close(db);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return(result);

}
