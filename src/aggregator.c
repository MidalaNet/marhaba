#include "marhaba.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>

#include <mxml.h>
#include <tidy/tidy.h>
#include <tidy/tidybuffio.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <sqlite3.h>

char *rtrim(char *str) {
	size_t len;
	char *p;
	len = strlen(str);
	if (len > 0) {
		p = str + len;
		do {
			p--;
			if (!isspace((unsigned char)*p)) break;
			*p = '\0';
		} while (p > str);
	}
	return str;
}

char *ltrim(char *str) {
	char *pstart = str;
	char *p;
	while (isspace((unsigned char)*pstart)) pstart++;
	if (pstart > str) {
		p = str;
		while (*pstart) { *p = *pstart; pstart++; p++; }
		*p = '\0';
	}
	return str;
}

char *trim(char *str) {
	rtrim(str);
	ltrim(str);
	return str;
}

char *plaintext(char *xhtml) {
	int i = 0;
	mxml_node_t *tree = NULL;
	mxml_node_t *node = NULL;

	tree = mxmlLoadString(NULL, xhtml, MXML_OPAQUE_CALLBACK);

	char *out = calloc(LLIMIT, sizeof(char));
	if (!out) {
		mxmlDelete(tree);
		return NULL;
	}
	size_t out_len = 0;

	for (node = mxmlFindElement(tree, tree, "body", NULL, NULL, MXML_DESCEND); node != NULL; node = mxmlWalkNext(node, NULL, MXML_DESCEND)) {
		if (mxmlGetType(node) == MXML_OPAQUE) {
			char *value = (char *)mxmlGetOpaque(node);
			size_t val_len = strlen(trim(value));
			if (out_len + (i > 0 ? 1 : 0) + val_len + 1 > LLIMIT) break;
			if (i > 0) { strncat(out, " ", LLIMIT - out_len - 1); out_len++; }
			strncat(out, trim(value), LLIMIT - out_len - 1);
			out_len += val_len;
			i++;
		}
	}
	mxmlDelete(tree);

	for (size_t j = 0; out[j] != '\0'; j++) {
		if (out[j] == '\n') out[j] = ' ';
	}

	return out;
}

char *tidy_html(char *html) {
	TidyBuffer output = {0};
	TidyDoc tdoc = tidyCreate();
	TidyBuffer errbuf = {0};
	char *out;

	tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
	tidySetErrorBuffer(tdoc, &errbuf);
	tidyOptSetBool(tdoc, TidyQuiet, yes);
	tidyOptSetBool(tdoc, TidyForceOutput, yes);
	tidySetInCharEncoding(tdoc, "utf8");
	tidySetOutCharEncoding(tdoc, "utf8");
	tidyParseString(tdoc, html);
	tidyCleanAndRepair(tdoc);
	tidySaveBuffer(tdoc, &output);

	out = calloc(output.size + 1, sizeof(char));
	if (out) memcpy(out, output.bp, output.size);

	tidyRelease(tdoc);
	tidyBufFree(&errbuf);
	tidyBufFree(&output);

	return out;
}

void parser(char *xml, char *source) {
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
	mxml_node_t *node = NULL;
	sqlite3 *db;
	sqlite3_stmt *res;
	int c = 0;
	int i = 0;
	int item = 0;
	char *value = NULL;
	char *element = NULL;

	tree = mxmlLoadString(NULL, xml, MXML_OPAQUE_CALLBACK);

	int items = 0;
	for (node = mxmlFindElement(tree, tree, "item", NULL, NULL, MXML_DESCEND); node != NULL; node = mxmlWalkNext(node, NULL, MXML_DESCEND)) {
		if (mxmlGetType(node) == MXML_ELEMENT) {
			element = (char *)mxmlGetElement(node);
			if (strcmp(element, "item") == 0) items++;
		}
	}

	struct Item *item_data = calloc(items + 1, sizeof(struct Item));
	if (!item_data) { mxmlDelete(tree); return; }

	for (node = mxmlFindElement(tree, tree, "item", NULL, NULL, MXML_DESCEND); node != NULL; node = mxmlWalkNext(node, NULL, MXML_DESCEND)) {
		if (mxmlGetType(node) == MXML_ELEMENT) {
			element = (char *)mxmlGetElement(node);
			if (strcmp(element, "item") == 0) {
				item++;
				if (item > items) break;
				item_data[item].id = item;
			}
			if (strcmp(element, "title") == 0 || strcmp(element, "description") == 0 ||
			    strcmp(element, "link") == 0 || strcmp(element, "pubDate") == 0) {
				c = i + 1;
			}
		}

		if (c == i) {
			strncpy(item_data[item].source, trim(source), LLIMIT - 1);
			item_data[item].source[LLIMIT - 1] = '\0';

			if (mxmlGetCDATA(node) == NULL) {
				if (mxmlGetType(node) == MXML_OPAQUE) value = (char *)mxmlGetOpaque(node);

				if (strcmp(element, "title") == 0) {
					strncpy(item_data[item].title, trim(value), LLIMIT - 1);
					item_data[item].title[LLIMIT - 1] = '\0';
				}
				if (strcmp(element, "link") == 0) {
					strncpy(item_data[item].link, trim(value), LLIMIT - 1);
					item_data[item].link[LLIMIT - 1] = '\0';
				}
				if (strcmp(element, "pubDate") == 0) {
					strncpy(item_data[item].pubDate, trim(value), LLIMIT - 1);
					item_data[item].pubDate[LLIMIT - 1] = '\0';

					char year[5] = {0}, month_name[4] = {0}, day[3] = {0}, hour[9] = {0}, month[3] = {0};
					strncpy(year,       value + 12, 4);
					strncpy(month_name, value + 8,  3);
					strncpy(day,        value + 5,  2);
					strncpy(hour,       value + 17, 8);

					if      (strcmp(month_name, "Jan") == 0) strncpy(month, "01", 2);
					else if (strcmp(month_name, "Feb") == 0) strncpy(month, "02", 2);
					else if (strcmp(month_name, "Mar") == 0) strncpy(month, "03", 2);
					else if (strcmp(month_name, "Apr") == 0) strncpy(month, "04", 2);
					else if (strcmp(month_name, "May") == 0) strncpy(month, "05", 2);
					else if (strcmp(month_name, "Jun") == 0) strncpy(month, "06", 2);
					else if (strcmp(month_name, "Jul") == 0) strncpy(month, "07", 2);
					else if (strcmp(month_name, "Aug") == 0) strncpy(month, "08", 2);
					else if (strcmp(month_name, "Sep") == 0) strncpy(month, "09", 2);
					else if (strcmp(month_name, "Oct") == 0) strncpy(month, "10", 2);
					else if (strcmp(month_name, "Nov") == 0) strncpy(month, "11", 2);
					else if (strcmp(month_name, "Dec") == 0) strncpy(month, "12", 2);

					char dbDate[20] = {0};
					snprintf(dbDate, sizeof(dbDate), "%.4s-%.2s-%.2s %.8s", year, month, day, hour);
					strncpy(item_data[item].dbDate, trim(dbDate), LLIMIT - 1);
					item_data[item].dbDate[LLIMIT - 1] = '\0';
				}
				if (strcmp(element, "description") == 0) {
					strncpy(item_data[item].description, trim(value), LLIMIT - 1);
					item_data[item].description[LLIMIT - 1] = '\0';
				}
			} else {
				char *pch = strstr(element, "![CDATA[");
				strncpy(pch, "<div><p>", 8);
				element[strlen(element) - 2] = '\0';

				char *xhtml = tidy_html(element);
				if (xhtml) {
					char *plain = plaintext(xhtml);
					if (plain) {
						strncpy(item_data[item].description, trim(plain), LLIMIT - 1);
						item_data[item].description[LLIMIT - 1] = '\0';
						free(plain);
					}
					free(xhtml);
				}
			}
		}
		i++;
	}
	mxmlDelete(tree);

	char *sql = "INSERT INTO news (source, title, link, description, pubDate, dbDate) VALUES (@source, @title, @link, @description, @pubDate, @dbDate)";
	int rc = sqlite3_open(DATABASE, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		free(item_data);
		return;
	}
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		free(item_data);
		return;
	}

	for (int s = 1; s <= items; s++) {
		int sourcex      = sqlite3_bind_parameter_index(res, "@source");
		int titlex       = sqlite3_bind_parameter_index(res, "@title");
		int pubDatex     = sqlite3_bind_parameter_index(res, "@pubDate");
		int dbDatex      = sqlite3_bind_parameter_index(res, "@dbDate");
		int linkx        = sqlite3_bind_parameter_index(res, "@link");
		int descriptionx = sqlite3_bind_parameter_index(res, "@description");

		sqlite3_bind_text(res, sourcex,      item_data[s].source,      -1, SQLITE_STATIC);
		sqlite3_bind_text(res, titlex,       item_data[s].title,       -1, SQLITE_STATIC);
		sqlite3_bind_text(res, pubDatex,     item_data[s].pubDate,     -1, SQLITE_STATIC);
		sqlite3_bind_text(res, dbDatex,      item_data[s].dbDate,      -1, SQLITE_STATIC);
		sqlite3_bind_text(res, linkx,        item_data[s].link,        -1, SQLITE_STATIC);
		sqlite3_bind_text(res, descriptionx, item_data[s].description, -1, SQLITE_TRANSIENT);

		if (strlen(item_data[s].description) > WLIMIT) {
			rc = sqlite3_step(res);
			if (rc != SQLITE_DONE) fprintf(stderr, "Insert error: %s\n", sqlite3_errmsg(db));
		}
		sqlite3_reset(res);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);
	free(item_data);
}

void get_xml(char *url, char *source) {
	FILE *fp;
	CURL *rss;
	CURLcode curl_rc;

	curl_global_init(CURL_GLOBAL_ALL);
	rss = curl_easy_init();
	if (!rss) {
		fprintf(stderr, "curl_easy_init() failed\n");
		curl_global_cleanup();
		return;
	}

	fp = tmpfile();
	if (!fp) {
		fprintf(stderr, "tmpfile() failed\n");
		curl_easy_cleanup(rss);
		curl_global_cleanup();
		return;
	}

	curl_easy_setopt(rss, CURLOPT_URL, url);
	curl_easy_setopt(rss, CURLOPT_WRITEDATA, fp);
	curl_rc = curl_easy_perform(rss);
	curl_easy_cleanup(rss);
	curl_global_cleanup();

	if (curl_rc != CURLE_OK) {
		fprintf(stderr, "curl error for %s: %s\n", url, curl_easy_strerror(curl_rc));
		fclose(fp);
		return;
	}

	fseek(fp, 0L, SEEK_END);
	long int res = ftell(fp);
	rewind(fp);

	char *out = calloc(res + 1, sizeof(char));
	if (!out) { fclose(fp); return; }
	fread(out, res, 1, fp);
	fclose(fp);

	parser(out, source);
	free(out);
}

void get_sources(void) {
	struct Source {
		int id;
		char name[LLIMIT];
		char url[LLIMIT];
	};

	sqlite3 *db;
	sqlite3_stmt *res;
	int rc = sqlite3_open(DATABASE, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	int sources = 0;
	rc = sqlite3_prepare_v2(db, "select max(rowid) as n from sources", -1, &res, 0);
	if (rc == SQLITE_OK) {
		rc = sqlite3_step(res);
		if (rc == SQLITE_ROW && sqlite3_column_text(res, 0))
			sources = atoi((char *)sqlite3_column_text(res, 0));
		sqlite3_reset(res);
		sqlite3_finalize(res);
	}

	if (sources <= 0) { sqlite3_close(db); return; }

	struct Source *source_data = calloc(sources + 1, sizeof(struct Source));
	if (!source_data) { sqlite3_close(db); return; }

	char *sql = "select rowid as id, url, name from sources where id = ?";
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		free(source_data);
		sqlite3_close(db);
		return;
	}

	for (int i = 1; i <= sources; i++) {
		sqlite3_bind_int(res, 1, i);
		rc = sqlite3_step(res);
		if (rc == SQLITE_ROW) {
			source_data[i].id = i;
			strncpy(source_data[i].url,  (char *)sqlite3_column_text(res, 1), LLIMIT - 1);
			strncpy(source_data[i].name, (char *)sqlite3_column_text(res, 2), LLIMIT - 1);
		}
		sqlite3_reset(res);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	for (int s = 1; s <= sources; s++) get_xml(source_data[s].url, source_data[s].name);

	free(source_data);
}

int main(void) {
	setlocale(LC_ALL, "");
	get_sources();
	return 0;
}
