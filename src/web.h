// Automatically generated header.

#pragma once
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include "event2/event.h"
#include "event2/buffer.h"
#include "event2/listener.h"
#include "event2/bufferevent.h"
#include "util.h"
#include "vector.h"
#include "hashtable.h"
#include "threads.h"
#include "reasonphrases.h"
int skip_newline(char** cur);
char* percent_decode(char* data, unsigned long* sz);
#include "context.h"
void respond(session_t* session, int stat, char* content, unsigned long len, char* (*headers)[2], int headers_len);
void respond_redirect(session_t* session, char* url);
void escape_html(char** str);
typedef struct {
	char* str;
	unsigned long skip; //if a condition, length, including !%
	unsigned long max_args; //required arguments
	unsigned long max_cond;
	unsigned long max_loop;
	vector_t substitutions;
} template_t;
typedef struct {
	int* cond_args;
	vector_t** loop_args;
	char** sub_args;
} template_args;
template_t template_new(char* data);
void respond_template(session_t* session, int stat, char* template_name, char* title, ...);
void respond_error(session_t* session, int stat, char* err);
vector_t query_find(vector_t *vec, char **params, int num_params, int strict);
vector_t multipart_find(vector_t *vec, char **params, int num_params, int strict);
void start_listen(ctx_t* ctx, const char *port);
