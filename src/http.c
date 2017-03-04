#include "boorubot.h"
#include <stdlib.h>
#include <string.h>

extern struct curl_slist *json_headers;

/*--------------------------------------------
|  curl_easy_setopt wrapper. On failure
|  cleans up and returns.
--------------------------------------------*/
#define CURL_SETOPT_S(FIELD, OPTION)                                           \
  do {                                                                         \
    res = curl_easy_setopt(handle, FIELD, OPTION);                             \
    if (res != CURLE_OK) {                                                     \
      curl_easy_cleanup(handle);                                               \
      return result;                                                           \
    }                                                                          \
  } while (0)


/*-----------------------------------------------------
|  CURLOPT_WRITEFUNCTION
-----------------------------------------------------*/
size_t write_response(void *response, size_t size, size_t nmemb,
                      void *write_struct) {
  size_t real_size = size * nmemb;
  char *old_data = NULL;
  struct response *mem = (struct response *)write_struct;

  if (mem->size) {
    old_data = mem->data;
    mem->data = realloc(old_data, mem->size + real_size + 1);
  } else
    mem->data = malloc(real_size + 1);

  if (!mem->data) {
    free(old_data);
    mem->size = 0;
    return 0;
  }

  memcpy(&(mem->data[mem->size]), response, real_size);
  mem->size += real_size;
  mem->data[mem->size] = '\0';

  return real_size;
}

/*-----------------------------------------------------
|  HTTP request wrapper.
-----------------------------------------------------*/
struct response request(const char *url, const char *post, long timeout,
                        int use_json_headers, CURLSH *share) {
  CURL *handle;
  CURLcode res;
  struct response result;

  result.size = 0;
  result.data = NULL;

  // Create curl handle
  handle = curl_easy_init();
  if (!handle) {
    // ZLOG
    return result;
  }

  if (post)
    CURL_SETOPT_S(CURLOPT_POSTFIELDS, post);
  if (use_json_headers)
    CURL_SETOPT_S(CURLOPT_HTTPHEADER, json_headers);

  CURL_SETOPT_S(CURLOPT_URL, url);
  CURL_SETOPT_S(CURLOPT_SHARE, share);
  CURL_SETOPT_S(CURLOPT_TIMEOUT, timeout + 10L);
  CURL_SETOPT_S(CURLOPT_WRITEFUNCTION, write_response);
  CURL_SETOPT_S(CURLOPT_WRITEDATA, &result);
  CURL_SETOPT_S(CURLOPT_NOSIGNAL, 1);

  // Make the request
  res = curl_easy_perform(handle);
  if (res != CURLE_OK) {
    // ZLOG
  }

  curl_easy_cleanup(handle);

  return result;
}
