#ifndef EL__ECMASCRIPT_SPIDERMONKEY_LOCALSTORAGE_DB_H
#define EL__ECMASCRIPT_SPIDERMONKEY_LOCALSTORAGE_DB_H

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const int delete_db(char *db_name, char *key);
extern const int insert_db(char *db_name, char *key, char *value);
extern const int update_db(char *db_name, char *key, char *value);
extern const char * qry_db(char *db_name, char *key);
extern const char * qry_db_val(char *db_name, char *val);

#endif
