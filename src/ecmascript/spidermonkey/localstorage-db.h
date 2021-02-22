#ifndef EL__ECMASCRIPT_SPIDERMONKEY_LOCALSTORAGE_DB_H
#define EL__ECMASCRIPT_SPIDERMONKEY_LOCALSTORAGE_DB_H

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const int db_prepare_structure(char *db_name);
extern const int db_delete_from(char *db_name, char *key);
extern const int db_insert_into(char *db_name, char *key, char *value);
extern const int db_update_set(char *db_name, char *key, char *value);
extern const char * db_query_by_key(char *db_name, char *key);
extern const char * db_qry_by_value(char *db_name, char *val);

#endif
