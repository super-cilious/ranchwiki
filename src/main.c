#include <assert.h>
#include <err.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"
#include "filemap.h"
#include "hashtable.h"
#include "threads.h"
#include "tinydir.h"
#include "util.h"
#include "vector.h"
#include "web.h"
#include "wiki.h"

const char* TEMPLATE_EXT = ".html";

void cleanup_callback(evutil_socket_t fd, short what, void* arg) {
  ctx_t* ctx = arg;
  cleanup_sessions(ctx);
}

void interrupt_callback(int signal, short events, void* arg) {
  ctx_t* ctx = arg;
  event_base_loopbreak(ctx->evbase);
}

int util_main(void* udata) {
  ctx_t* ctx = udata;

  char* in;
  size_t sz;

  while ((in = fgetln(stdin, &sz))) {
    in = heapcpy(sz+1, in);
    in[sz] = 0;

    vector_t arg = vector_split_str(in, " ");

    if (strcmp(vector_getstr(&arg, 0), "rank")==0 && arg.length == 3) {
      char* name = vector_getstr(&arg, 1);

      filemap_partial_object name_user_ref = filemap_find(&ctx->user_by_name, name, strlen(name)+1);
      if (!name_user_ref.exists) perror("User not found");

      filemap_partial_object list_user = filemap_deref(&ctx->user_id, &name_user_ref);

      user_t user = filemap_cpy(&ctx->user_fmap, &list_user);

      userdata_t* data = (userdata_t*)user.fields[2];
      data->perms = (char)atoi(vector_getstr(&arg, 2));

      map_remove(&ctx->user_sessions_by_idx, &list_user.index);
      update_user(ctx, &list_user, &user, user.fields, user.lengths);
      printf("Permissions updated\n");
    } else {
      perror("Action not found");
    }

    drop(in);
  }

  return 0;
}

int main(int argc, char** argv) {
  // filemap_t fmap = filemap_new("./test-fmap", 2, 1);
  // filemap_ordered_list_t list =
  // filemap_ordered_list_new("./test-fmap-ordered", 2, 1);
  //
  // filemap_object obj = filemap_push(&fmap, (char*[2]){"hello", "world"},
  // (uint64_t[2]){strlen("hello"), strlen("world")}); filemap_ord_partial_object
  // partial = filemap_ordered_insert(&list, 2, &obj);
  //
  // obj = filemap_push(&fmap, (char*[2]){"hello2", "world2"},
  // (uint64_t[2]){strlen("hellox"), strlen("worldx")});
  // filemap_ordered_insert(&list, 1, &obj);
  //
  // obj = filemap_push(&fmap, (char*[2]){"hello3", "world3"},
  // (uint64_t[2]){strlen("hellox"), strlen("worldx")});
  // filemap_ordered_insert(&list, 3, &obj);
  //
  // vector_t vec = filemap_ordered_page(&list, 0, 1);
  // vector_t vec2 = filemap_ordered_page(&list, 1, 2);
  //
  // vector_iterator iter = vector_iterate(&vec);
  // while (vector_next(&iter)) {
  //	filemap_ord_partial_object* partial = iter.x;
  //	printf("%llu\n", partial->partial.index);
  //}
  //
  // printf("p. 2\n");
  //
  // iter = vector_iterate(&vec2);
  // while (vector_next(&iter)) {
  //	filemap_ord_partial_object* partial = iter.x;
  //	printf("%llu\n", partial->partial.index);
  //}
  //
  // filemap_ordered_remove(&list, &partial);
  //
  // filemap_free(&fmap);
  // filemap_ordered_free(&list);
  //
  // return 0;

  if (argc < 3) {
    errx(1, "need templates directory and port as arguments");
  }

  evthread_use_pthreads();

  ctx_t ctx;
  ctx.evbase = event_base_new();

  ctx.digest_ctx = EVP_MD_CTX_create();

  ctx.templates = map_new();
  map_configure_string_key(&ctx.templates, sizeof(template_t));

  ctx.resources = map_new();
  map_configure_string_key(&ctx.resources, sizeof(resource));

  // user, email, data, bio
  ctx.user_id = filemap_list_new("./user_id", 0);

  ctx.user_fmap = filemap_new("./users", 4, 0);
  ctx.user_fmap.alias = &ctx.user_id;

  ctx.user_by_name = filemap_index_new(&ctx.user_fmap, "./users_by_name", 0, 0);
  ctx.user_by_email =
      filemap_index_new(&ctx.user_fmap, "./users_by_email", 1, 0);

  ctx.user_sessions = map_new();
  ctx.user_sessions.free = free_string;

  map_distribute(&ctx.user_sessions);
  map_configure_string_key(&ctx.user_sessions, sizeof(user_session*));

  ctx.user_sessions_by_idx = map_new();
  map_distribute(&ctx.user_sessions_by_idx);
  map_configure_ulong_key(&ctx.user_sessions_by_idx, sizeof(user_session*));

  ctx.article_lock = map_new();
  ctx.article_lock.free = free_sized;

  map_distribute(&ctx.article_lock);
  map_configure_sized_key(&ctx.article_lock, sizeof(mtx_t));

  ctx.article_id = filemap_list_new("./article_id", 0);  // avoid update hell

  // data, referenced by/items, path, contributors
  ctx.article_fmap = filemap_new("./articles", 4, 0);
  ctx.article_fmap.alias = &ctx.article_id;

  ctx.article_by_name =
      filemap_index_new(&ctx.article_fmap, "./articles_by_name", 2, 0);

  tinydir_dir dir;
  tinydir_open(&dir, argv[1]);

  tinydir_next(&dir);  // skip .
  tinydir_next(&dir);  // skip ..

  while (dir.has_next) {
    tinydir_file file;
    tinydir_readfile(&dir, &file);

    char* filename = heapcpystr(file.name);

    char* content = read_file(file.path);

    if (strlen(filename) > strlen(TEMPLATE_EXT) &&
        strcmp(filename + strlen(filename) - strlen(TEMPLATE_EXT),
               TEMPLATE_EXT) == 0) {
      memset(filename + strlen(filename) - strlen(TEMPLATE_EXT), 0,
             strlen(TEMPLATE_EXT));

      if (strcmp(filename, GLOBAL_TEMPLATE) == 0) {
        ctx.global = content;
      } else {
        template_t template = template_new(content);
        map_insertcpy(&ctx.templates, &filename, &template);
        drop(content);
      }
    } else {
      char* extension = ext(filename);
      char* mime;

      if (strcmp(extension, ".css") == 0) {
        mime = "text/css";
      } else {
        mime = "application/octet-stream";
      }

      map_insertcpy(
          &ctx.resources, &filename,
          &(resource){
              .content = content, .len = strlen(content), .mime = mime});
    }

    tinydir_next(&dir);
  }

  tinydir_close(&dir);

  thrd_t util;
  thrd_create(&util, util_main, &ctx);

  start_listen(&ctx, argv[2]);

  struct event* cleanup = event_new(ctx.evbase, -1, EV_TIMEOUT | EV_PERSIST,
                                    cleanup_callback, &ctx);
  event_add(cleanup, &(struct timeval){CLEANUP_INTERVAL, 0});

  struct event* interrupt =
      evsignal_new(ctx.evbase, SIGINT, interrupt_callback, &ctx);
  event_add(interrupt, NULL);

  event_base_loop(ctx.evbase, 0);
  event_base_free(ctx.evbase);

  EVP_MD_CTX_destroy(ctx.digest_ctx);
  EVP_cleanup();

  printf("Saving...\n");
  filemap_free(&ctx.user_fmap);
  filemap_list_free(&ctx.user_id);
  filemap_index_free(&ctx.user_by_name);
  filemap_index_free(&ctx.user_by_email);

  filemap_free(&ctx.article_fmap);
  filemap_list_free(&ctx.article_id);
  filemap_index_free(&ctx.article_by_name);

  return 0;
}
