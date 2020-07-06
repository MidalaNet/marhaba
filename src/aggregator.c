/*
 * aggregator.c
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
 * Mini-XML is a small XML parsing library
 * https://www.msweet.org/mxml/mxml.html 
 * 
 * Issues
 * https://github.com/michaelrsweet/mxml/issues/183
 * https://github.com/michaelrsweet/mxml/issues/163
 */
#include <mxml.h>   // Mini-XML, a tiny XML library

/*
 * libtidy is the library version of HTML Tidy
 * https://www.html-tidy.org/developer/
 */
#include <tidy/tidy.h>
#include <tidy/tidybuffio.h>

/* 
 * libcURL - the multiprotocol file transfer library
 * http://curl.haxx.se/libcurl/
 */
#include <curl/curl.h>
#include <curl/easy.h>

/*
 * SQLite - Small. Fast. Reliable. Choose any three.
 * http://www.sqlite.org/cintro.html
 */
#include <sqlite3.h>

// trim, ltrim, rtrim
// https://www.sebcosta.altervista.org/joomla/articles/9-trimstringc
char *rtrim(char *str) {
	size_t len;
	char *p;
	
	len = strlen(str);
	if (len > 0) {
			p = str + len;
			do {
				p--;
				if (!isspace(*p)) {
					break;
				}
				*p = '\0';
			} while (p > str);
	}
	return str;
}

char *ltrim(char *str) {
  char *pstart;
  char *p;

  pstart = str;
  while (isspace(*pstart)) {
    pstart++;
  }

  if (pstart > str) {
		p = str;
		while(*pstart) {
			*p = *pstart;
			pstart++;
			p++;
		}
		*p = '\0';
  }

  return str;
} 

char *trim(char *str) {
  rtrim(str);
  ltrim(str);
  return str;
}

// XHTML to plain text 
char * plaintext (char *xhtml) {

	int i = 0;
  mxml_node_t * tree = NULL;
	mxml_node_t * node  = NULL;
	
	tree = mxmlLoadString(NULL, xhtml, MXML_OPAQUE_CALLBACK);
	
	char * out;
	char * value;
	out = calloc(LLIMIT, sizeof (char));
	
	for (node = mxmlFindElement(tree, tree, "body", NULL, NULL, MXML_DESCEND); node != NULL; node=mxmlWalkNext (node, NULL, MXML_DESCEND)) {
		if (mxmlGetType(node) == MXML_OPAQUE){
			if (i > 0) {
				strncat(out, " ", 1);
			}
			value = (char *) mxmlGetOpaque(node);
			strncat(out, trim(value), strlen(trim(value)));
			i++;
		}
	}
	mxmlDelete(tree);
	
	// Remove newline
	for (size_t i = 0; out[i] != '\0'; i++) {
		if (out[i] == '\n') {
			out[i] = ' ';
		}
	}
	
	return out; 
}

// HTML to XHTML
char * tidy (char* html) {
	TidyBuffer output = {0};
  TidyDoc tdoc = tidyCreate();
  TidyBuffer errbuf = {0};
  size_t size;
  char * out;
  
  // Convert to XHTML and capture diagnostics  
  tidyOptSetBool(tdoc, TidyXhtmlOut, yes );
  tidySetErrorBuffer( tdoc, &errbuf );
  tidyOptSetBool(tdoc, TidyQuiet, yes);
  tidyOptSetBool(tdoc, TidyForceOutput, yes);
	
	// UTF8
	tidySetInCharEncoding(tdoc, "utf8");
	tidySetOutCharEncoding(tdoc, "utf8");
  
  // Parse the input
  tidyParseString( tdoc, html );
  
  // Tidy it up!
  tidyCleanAndRepair( tdoc );               
  
  // Pretty Print
  tidySaveBuffer( tdoc, &output );
	
	// Save to string
	size = output.size;
	out = calloc (size + 1, sizeof (char));
	sprintf(out, "%s", output.bp);
	
	tidyRelease( tdoc );
	tidyBufFree( &errbuf );
  tidyBufFree( &output );
  
	return out; 
}


// RSS parser
void parser (char *xml, char *source) {
		 
	struct Item {
		int id;
		char source[LLIMIT];
		char title[LLIMIT];
		char description[LLIMIT];
		char link[LLIMIT];
		char pubDate[LLIMIT];
		char dbDate[LLIMIT];
	};
		
	mxml_node_t *tree = NULL;
	mxml_node_t *node  = NULL;
	
	sqlite3 *db;
	sqlite3_stmt *res;
	
	int c = 0;
	int i = 0;
	int item = 0;
	
	char *value;
	char *element;

	tree = mxmlLoadString(NULL, xml, MXML_OPAQUE_CALLBACK);
	
	// Items
	int items = 0;
	for (node = mxmlFindElement(tree, tree, "item", NULL, NULL, MXML_DESCEND); node != NULL; node=mxmlWalkNext (node, NULL, MXML_DESCEND)) {
		if ( mxmlGetType(node) == MXML_ELEMENT) {			
			element = (char *) mxmlGetElement(node);
			if (strcmp(element, "item") == 0) {
				items++;
			}
		}
	}
	// Array of structures
	struct Item item_data[items + 1];

	for (node = mxmlFindElement(tree, tree, "item", NULL, NULL, MXML_DESCEND); node != NULL; node=mxmlWalkNext (node, NULL, MXML_DESCEND)) {
		if ( mxmlGetType(node) == MXML_ELEMENT) {			
			element = (char *) mxmlGetElement(node);
			if (strcmp(element, "item") == 0) {
				item++;
				if (item > 0) {
					item_data[item].id = item;
				}	
			}			
			if (strcmp(element, "title") == 0 || strcmp(element, "description") == 0 || strcmp(element, "link") == 0 || strcmp(element, "pubDate") == 0) {
				c = i + 1;				
			}
		} 

		if (c == i) {
			// Source
			strcpy( item_data[item].source, trim(source));
			if (mxmlGetCDATA(node) == NULL) {
				if ( mxmlGetType(node) == MXML_OPAQUE) {
					value = (char *) mxmlGetOpaque(node);
				}
				// Title
				if (strcmp(element, "title") == 0) {
					strcpy(item_data[item].title, trim(value));		
				}
				// Link
				if (strcmp(element, "link") == 0) {
					strcpy(item_data[item].link, trim(value));
				}
				// pubDate
				// For example: Sun, 21 Jun 2020 05:46:22 +0000
				if (strcmp(element, "pubDate") == 0) {
					strcpy(item_data[item].pubDate, trim(value));
					
					// dbDate
					// For example: 2020-06-21 05:46:22
					// Year
					char * year;
					year = calloc (5, sizeof (char));
					strncpy(year, value+12, 4);
					// Month name
					char * month_name;
					month_name = calloc (4, sizeof (char));
					strncpy(month_name, value+8, 3);
					// Day
					char * day;
					day = calloc (3, sizeof (char));
					strncpy(day, value+5, 2);
					// Hour
					char * hour;
					hour = calloc (9, sizeof (char));
					strncpy(hour, value+17, 8);
					// Month
					char * month;
					month = calloc (3, sizeof (char));
					if (strcmp(month_name, "Jan") == 0) {
						strncpy(month, "01", 2);
					} else if (strcmp(month_name, "Feb") == 0) {
						strncpy(month, "02", 2);
					} else if (strcmp(month_name, "Mar") == 0) {
						strncpy(month, "03", 2);
					} else if (strcmp(month_name, "Apr") == 0) {
						strncpy(month, "04", 2);
					} else if (strcmp(month_name, "May") == 0) {
						strncpy(month, "05", 2);
					} else if (strcmp(month_name, "Jun") == 0) {
						strncpy(month, "06", 2);
					} else if (strcmp(month_name, "Jul") == 0) {
						strncpy(month, "07", 2);
					} else if (strcmp(month_name, "Aug") == 0) {
						strncpy(month, "08", 2);
					} else if (strcmp(month_name, "Sep") == 0) {
						strncpy(month, "09", 2);
					} else if (strcmp(month_name, "Oct") == 0) {
						strncpy(month, "10", 2);
					} else if (strcmp(month_name, "Nov") == 0) {
						strncpy(month, "11", 2);
					} else if (strcmp(month_name, "Dec") == 0) {
						strncpy(month, "12", 2);
					}
					
					char * dbDate;
					dbDate = calloc (20, sizeof (char));
					strncat(dbDate, year, 4);
					strncat(dbDate, "-", 1);
					strncat(dbDate, month, 2);
					strncat(dbDate, "-", 1);
					strncat(dbDate, day, 2);
					strncat(dbDate, " ", 1);
					strncat(dbDate, hour, 8);
					
					strcpy(item_data[item].dbDate, trim(dbDate));
					free(year);
					free(month);
					free(month_name);
					free(day);
					free(hour);
					free(dbDate);
				}
				// Description	
				if (strcmp(element, "description") == 0) {
					strcpy(item_data[item].description, trim(value));
				}
			} else {
				
				// Description (XML CDATA section)
				char * pch;
				pch = strstr (element, "![CDATA[");
				strncpy (pch, "<div><p>", 8);
				element[strlen(element) - 2] = '\0';
				
				// Tidy
				// HTML to XHTML
				char * xhtml = tidy (element);
				// XHTML to plain text
				char * plain = plaintext(xhtml);		
				
				strcpy(item_data[item].description, trim(plain));
				free(xhtml);
				free(plain);
			}
		}
		i++;
	}
	mxmlDelete(tree);
	
	// Array of structures to SQL
	char *sql = "INSERT INTO news (source, title, link, description, pubDate, dbDate) VALUES (@source, @title, @link, @description, @pubDate, @dbDate)";
	int rc = sqlite3_open(DATABASE, &db);
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	
	int n = sizeof(item_data)/sizeof(item_data[0]);
	for (int s = 1; s < n; s++) {
		
		// Source
		int sourcex = sqlite3_bind_parameter_index(res, "@source");
		sqlite3_bind_text(res, sourcex, item_data[s].source, strlen(item_data[s].source), SQLITE_STATIC);
		// Title
		int titlex = sqlite3_bind_parameter_index(res, "@title");
		sqlite3_bind_text(res, titlex, item_data[s].title, strlen(item_data[s].title), SQLITE_STATIC);
		// pubDate
		int pubDatex = sqlite3_bind_parameter_index(res, "@pubDate");
		sqlite3_bind_text(res, pubDatex, item_data[s].pubDate, strlen(item_data[s].pubDate), SQLITE_STATIC);
		// dbDate
		int dbDatex = sqlite3_bind_parameter_index(res, "@dbDate");
		sqlite3_bind_text(res, dbDatex, item_data[s].dbDate, strlen(item_data[s].dbDate), SQLITE_STATIC);
		// Link
		int linkx = sqlite3_bind_parameter_index(res, "@link");
		sqlite3_bind_text(res, linkx, item_data[s].link, strlen(item_data[s].link), SQLITE_STATIC);
		// Description
 		int descriptionx = sqlite3_bind_parameter_index(res, "@description");
		sqlite3_bind_text(res, descriptionx, item_data[s].description, strlen(item_data[s].description), SQLITE_TRANSIENT);	
		
		// Query
		if (strlen(item_data[s].description) > WLIMIT) {
			rc = sqlite3_step(res);	

			if (rc != SQLITE_DONE) {					
				char * query = sqlite3_expanded_sql(res);
				/*
				 * SQLite error message
			   * 
				printf("SQL: %s\n", (char *) query);
				printf("Error: %s\n", sqlite3_errmsg(db));
				*/
				sqlite3_free(query);
			}
		}
		sqlite3_reset(res);
	}
	rc = sqlite3_finalize(res);
	sqlite3_close(db);
}

// Get RSS feed
void get_xml (char* url, char* source) {
	
	FILE *fp;
	CURL *rss;
	
	curl_global_init(CURL_GLOBAL_ALL);
	rss = curl_easy_init();
	if (rss) {
		fp = tmpfile();
		curl_easy_setopt(rss, CURLOPT_URL, url);
		/* 
		 * Switch on full protocol/debug output while testing
		 *  
		 * curl_easy_setopt(rss, CURLOPT_VERBOSE, 1L);
		 * 
		 * https://curl.haxx.se/libcurl/c/CURLOPT_WRITEDATA.html 
		 * If you're using libcurl as a win32 DLL, you MUST use a
		 * CURLOPT_WRITEFUNCTION if you set this option or you will 
		 * experience crashes.
		 */ 
		curl_easy_setopt(rss, CURLOPT_WRITEDATA, fp);
		curl_easy_perform(rss);
	}
	// Cleanup
	curl_easy_cleanup(rss);
	curl_global_cleanup();

	fseek(fp, 0L, SEEK_END); 
  long int res = ftell(fp); 

	rewind(fp);
	char * out;
	out = calloc(res + 1, sizeof (char)); 
	fread(out, res + 1, 1, fp);
	fclose(fp);
	
	parser(out, source);
	
	free(out);
}

// From database to array of structures
void get_sources (void) {
	
	struct Source {
		int id;
		char name[LLIMIT];
		char url[LLIMIT];
	};
	
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc = sqlite3_open(DATABASE, &db);
	
	int sources = 0;
	rc = sqlite3_prepare_v2(db, "select max(rowid) as n from sources", -1, &res, 0);
	rc = sqlite3_step(res);
	if (rc == SQLITE_ROW) {
		sources = atoi((char *) sqlite3_column_text(res, 0));
	}
	rc = sqlite3_reset(res);
	rc = sqlite3_finalize(res);
	
	char * sql = "select rowid as id, url, name from sources where id = ?";
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	
	// Array of structures
	struct Source source_data[sources];
	for (int i = 1; i <= sources; i++ ) {
		sqlite3_bind_int(res, 1, i);
		rc = sqlite3_step(res);
		if (rc == SQLITE_ROW) {
			source_data[i].id = i;
			strcpy(source_data[i].url, (char *) sqlite3_column_text(res, 1));
			strcpy(source_data[i].name, (char *) sqlite3_column_text(res, 2));
		} 
		rc = sqlite3_reset(res);
	}
	
	rc = sqlite3_finalize(res);
	rc = sqlite3_close(db);

		
	int n = sizeof(source_data)/sizeof(source_data[0]);
	for (int s = 1; s <= n; s++) {
		get_xml(source_data[s].url, source_data[s].name);
	}
}

int main (void) {
	setlocale (LC_ALL, "");
	
	get_sources();
	
	return 0;
}
