#include "boorubot.h"

#ifdef THREADED
#include "C-Thread-Pool/thpool.h"
#endif

#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>

#define getUpdatesURL                                                          \
  "https://api.telegram.org/bot" TELEGRAM_TOKEN "/getUpdates"

CURLSH *telegram_handle, *booru_handle;
struct curl_slist *json_headers = NULL;
#ifdef THREADED
threadpool pool;
#endif

/*-----------------------------------------------------
|  Wrapper around share handle creation. Exits on
|  failure.
-----------------------------------------------------*/
#define CREATE_SHARE_HANDLE(handle)                                            \
  do {                                                                         \
    int res;                                                                   \
    handle = curl_share_init();                                                \
    if (!handle) {                                                             \
      fputs("Error creating http share handle. Shutting down...\n", stderr);   \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
                                                                               \
    res = curl_share_setopt(handle, CURLSHOPT_SHARE,                           \
                            CURL_LOCK_DATA_SSL_SESSION);                       \
    if (res != CURLSHE_OK) {                                                   \
      fprintf(stderr,                                                          \
              "Error %d locking share handle ssl session. Shutting down...\n", \
              res);                                                            \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
                                                                               \
    res = curl_share_setopt(handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);      \
    if (res != CURLSHE_OK) {                                                   \
      fprintf(stderr,                                                          \
              "Error %d locking share handle DNS data. Shutting down...\n",    \
              res);                                                            \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

/*-----------------------------------------------------
|  Forks the bot into the background if daemon
|  flag is set at compile time.
-----------------------------------------------------*/
void daemonize(void) {
  pid_t pid, sid;

  pid = fork();
  if (pid < 0) {
    fputs("Error forking into background.\n", stderr);
    exit(EXIT_FAILURE);
  }

  if (pid > 0) {
    fputs("Forking to background...\n", stdout);
    exit(EXIT_SUCCESS);
  }

  umask(0);

  sid = setsid();
  if (sid < 0)
    exit(EXIT_FAILURE);

  if ((chdir("/")) < 0)
    exit(EXIT_FAILURE);

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}

/*-----------------------------------------------------
|  Initializes global variables.
-----------------------------------------------------*/
void init(void) {
  const char *header = "Content-Type: application/json";

  CREATE_SHARE_HANDLE(telegram_handle);
  CREATE_SHARE_HANDLE(booru_handle);

#ifdef THREADED
  pool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN));
  if (!pool) {
    fputs("Error initializing threadpool. Shutting down...\n", stderr);
    exit(EXIT_FAILURE);
  }
#endif

  json_headers = curl_slist_append(json_headers, header);
  if (!json_headers) {
    fputs("Error initializing http headers. Shutting down...\n", stderr);
    exit(EXIT_FAILURE);
  }
}

/*-----------------------------------------------------
| Checks a getUpdate response for any errors, routes
| the update type to the appropriate function, and
| updates the offset.
-----------------------------------------------------*/
void *route_updates(json_t *json) {
  json_t *result, *update;

  result = json_object_get(json, "result");
  for (size_t i = 0; (update = json_array_get(result, i)); ++i) {
    json_t *inline_query = json_object_get(update, "inline_query");
    json_t *message = json_object_get(update, "message");

    if (json_is_object(inline_query))
      answer_query(inline_query);
    else if (json_is_object(message))
      route_message(message);
  }

#ifdef THREADED
  json_decref(json);
#endif

  return NULL;
}

/*----------------------------------------------------
|  Sets the offset for updates to the newest
|  update_id + 1.
----------------------------------------------------*/
void update_offset(json_t *json, long long *offset) {
  json_t *result, *update, *update_id_json;
  json_int_t update_id;
  size_t length;

  result = json_object_get(json, "result");
  if (!json_is_array(result))
    return;

  length = json_array_size(result);
  if (!length)
    return;

  update = json_array_get(result, length - 1);
  if (!json_is_object(update))
    return;

  update_id_json = json_object_get(update, "update_id");
  if (!json_is_integer(update_id_json))
    return;

  update_id = json_integer_value(update_id_json);
  if (update_id)
    *offset = update_id + 1;
}

/*-------------------------------------------
|  Fetches updates and passes them to
|  route_updates.
-------------------------------------------*/
void getUpdates(void) {
  long long offset = 0;
  char post[100];
  struct response result;
  json_t *json;

  for (;;) {
    snprintf(post, 100, "offset=%lld&timeout=100", offset);

    result = request(getUpdatesURL, post, 100, 0, telegram_handle);
    if (result.size) {
      json = json_loadb(result.data, result.size, 0, NULL);
      if (json) {
#ifdef THREADED
        json_incref(json);
        thpool_add_work(pool, (void (*)(void *))route_updates, json);
#else
        route_updates(json);
#endif
        update_offset(json, &offset);
        json_decref(json);
      }

      free(result.data);
    }
  }

#ifdef THREADED
  thpool_wait(pool);
#endif
}

/*-----------------------------------------------------
|  Clean up global variables and curl.
-----------------------------------------------------*/
void cleanup(void) {
#ifdef THREADED
  thpool_destroy(pool);
#endif
  curl_share_cleanup(telegram_handle);
  curl_share_cleanup(booru_handle);
  curl_slist_free_all(json_headers);
  curl_global_cleanup();
}

/*-----------------------------------------------------
|  Program entry point.
-----------------------------------------------------*/
int main(void) {
  init();
#ifdef DAEMON
  daemonize();
#endif
  getUpdates();
  cleanup();
  exit(EXIT_SUCCESS);
}
