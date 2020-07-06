/*
 * publisher.c
 * 
 * Copyright 2014-2020 netico <netico.42@protonmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */
 
#include "marhaba.h"

#include <stdio.h>  // C Standard Input and Output Library
#include <stdlib.h> // Standard Library
#include <string.h> // String operations
#include <locale.h> // Definition of locale datatype

/*
 * SQLite - Small. Fast. Reliable. Choose any three.
 * http://www.sqlite.org/cintro.html
 */
#include <sqlite3.h>

/*
 * JSON-C - A JSON implementation in C
 * https://github.com/json-c/json-c/wiki
 */
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
		//json_object_object_add(jobj, "records", nested);
		//json_object_to_json_string(jobj));
		
    return 0;
}

int main(int argc, char *argv[])
{
	
		setlocale (LC_ALL, "");
		
    sqlite3 *db;
    char *message = 0;
    int rc;
    char* data = "";
    
    jobj = json_object_new_object();

    rc = sqlite3_open(DATABASE, &db);

    rc = sqlite3_exec(db, argv[1], to_json_object, (void*) data, &message);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", message);
        sqlite3_free(message);
    }

    sqlite3_close(db);
    printf ("%s\n", json_object_to_json_string(jobj));
    json_object_put(jobj);
		return 0;
}
