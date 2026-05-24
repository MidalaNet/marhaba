#include "marhaba.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>

#include <sqlite3.h>
#include <json-c/json.h>

static json_object * jobj;
int record = 0;

int to_json_object(void *data, int argc, char **argv, char **column){

	record++;
	json_object * jarray = json_object_new_array();
	json_object * nested = json_object_new_object();
	for(int i = 0; i < argc; i++) {
		json_object_object_add(nested, column[i], json_object_new_string(argv[i]));
	}
	json_object_array_add(jarray, nested);
	char recordn[5];
	sprintf(recordn, "%d", record);
	json_object_object_add(jobj, recordn, jarray);

	return 0;
}

static int is_select_query(const char *sql) {
	if (!sql) return 0;
	while (isspace((unsigned char)*sql)) sql++;
	return (strncasecmp(sql, "SELECT", 6) == 0);
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <SELECT query>\n", argv[0]);
		return 1;
	}

	if (!is_select_query(argv[1])) {
		fprintf(stderr, "Error: only SELECT queries are allowed\n");
		return 1;
	}

	sqlite3 *db;
	char *message = 0;
	int rc;
	char* data = "";

	jobj = json_object_new_object();

	rc = sqlite3_open(DATABASE, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

	rc = sqlite3_exec(db, argv[1], to_json_object, (void*) data, &message);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", message);
		sqlite3_free(message);
	}

	sqlite3_close(db);
	printf("%s\n", json_object_to_json_string(jobj));
	json_object_put(jobj);
	return 0;
}
