#include "boorubot.h"
#include <errno.h>
#include <jansson.h>
#include <string.h>

#define DANBOORU_ROOT "http://danbooru.donmai.us/%s"
#define AUTOCOMPLETE_URL                                                       \
  "https://danbooru.donmai.us/tags/autocomplete.json?search[name_matches]=%s*"
#define POSTS_URL                                                              \
  "https://danbooru.donmai.us/posts.json?tags=%s*%%20rating:s&page=%s"
#define ANSWERQUERY_URL                                                        \
  "https://api.telegram.org/bot" TELEGRAM_TOKEN "/answerInlineQuery"

extern CURLSH *telegram_handle;
extern CURLSH *booru_handle;

/*------------------------------------------------------
|  Wrapper around json_object_set(_new).
|  In case of failure, frees object and returns NULL.
------------------------------------------------------*/
#define JSON_SET(func, obj, field, param)                                      \
  do {                                                                         \
    if (func(obj, field, param)) {                                             \
      json_decref(obj);                                                        \
      return NULL;                                                             \
    }                                                                          \
  } while (0)

#define JSON_SET_NEW_S(obj, field, param)                                      \
  do {                                                                         \
    JSON_SET(json_object_set_new, obj, field, param);                          \
  } while (0)

#define JSON_SET_S(obj, field, param)                                          \
  do {                                                                         \
    JSON_SET(json_object_set, obj, field, param);                              \
  } while (0)

/*--------------------------------------------------------
|  Sets query and offset strings from an inline query
|  object.
--------------------------------------------------------*/
_Bool set_params(json_t *inline_query, const char **id, const char **query,
                 const char **offset) {
  json_t *id_json, *query_json, *offset_json;

  id_json = json_object_get(inline_query, "id");
  if (!json_is_string(id_json))
    return 1;

  *id = json_string_value(id_json);
  if (!id)
    return 1;

  query_json = json_object_get(inline_query, "query");
  if (!json_is_string(query_json))
    return 1;

  *query = json_string_value(query_json);
  if (!*query)
    return 1;

  for (char *i = (char *)query; *i != '\0'; ++i)
    if (*i == ' ')
      *i = '_';

  offset_json = json_object_get(inline_query, "offset");
  if (!json_is_string(offset_json))
    return 1;

  *offset = json_string_value(offset_json);
  if (!*offset)
    return 1;

  return 0;
}

/*--------------------------------------------
|  Queries Danbooru for search suggestions.
--------------------------------------------*/
json_t *autocomplete(const char *user_query) {
  char url[5000];
  int written_chars;
  struct response http;
  json_t *json;

  written_chars = snprintf(url, 5000, AUTOCOMPLETE_URL, user_query);
  if (written_chars <= 0)
    return NULL;

  http = request(url, NULL, 0, 0, booru_handle);
  if (!http.size)
    return NULL;

  json = json_loadb(http.data, http.size, 0, NULL);
  free(http.data);
  if (!json_array_size(json)) {
    json_decref(json);
    return NULL;
  }

  return json;
}

/*--------------------------------------------------------
|  Searches Danbooru for posts.
--------------------------------------------------------*/
json_t *query(const char *search_term, const char *page) {
  char url[5000];
  int written_chars;
  struct response http;
  json_t *root;

  written_chars = snprintf(url, 5000, POSTS_URL, search_term, page);
  if (written_chars <= 0)
    return NULL;

  http = request(url, NULL, 0, 0, booru_handle);
  if (!http.size)
    return NULL;

  root = json_loadb(http.data, http.size, 0, NULL);
  free(http.data);
  if (!json_is_array(root)) {
    json_decref(root);
    return NULL;
  }

  return root;
}

/*-------------------------------------------------------------------------
|  Takes a Danbooru image and returns an inlineQueryResult object.
-------------------------------------------------------------------------*/
json_t *create_result(json_t *image) {
  json_t *result, *width, *height, *image_path_json, *thumbnail_json, *md5,
      *file_ext_json;
  const char *image_path, *thumbnail, *file_ext;
  int written_chars;
  char buffer[5000];

  file_ext_json = json_object_get(image, "file_ext");
  if (!json_is_string(file_ext_json))
    return NULL;

  file_ext = json_string_value(file_ext_json);
  if (!file_ext || !strcmp(file_ext, "webm"))
    return NULL;

  image_path_json = json_object_get(image, "large_file_url");
  if (!json_is_string(image_path_json))
    return NULL;
  image_path = json_string_value(image_path_json);
  if (!image_path)
    return NULL;

  thumbnail_json = json_object_get(image, "preview_file_url");
  if (!json_is_string(thumbnail_json))
    return NULL;
  thumbnail = json_string_value(thumbnail_json);
  if (!thumbnail)
    return NULL;

  width = json_object_get(image, "image_width");
  if (!json_is_integer(width))
    return NULL;

  height = json_object_get(image, "image_height");
  if (!json_is_integer(height))
    return NULL;

  md5 = json_object_get(image, "md5");
  if (!json_is_string(md5))
    return NULL;

  result = json_object();
  if (!result)
    return NULL;

  written_chars = snprintf(buffer, 3000, DANBOORU_ROOT, image_path);
  if (written_chars <= 0) {
    json_decref(result);
    return NULL;
  }

  if (!strcmp(file_ext, "gif")) {
    JSON_SET_S(result, "gif_width", width);
    JSON_SET_S(result, "gif_height", height);
    JSON_SET_NEW_S(result, "type", json_string("gif"));
    JSON_SET_NEW_S(result, "gif_url", json_string(buffer));
  } else if (!strcmp(file_ext, "mp4")) {
    JSON_SET_S(result, "video_width", width);
    JSON_SET_S(result, "video_height", height);
    JSON_SET_NEW_S(result, "type", json_string("video"));
    JSON_SET_NEW_S(result, "mime_type", json_string("video/mp4"));
    JSON_SET_NEW_S(result, "title", json_string("Video (MP4)"));
    JSON_SET_NEW_S(result, "video_url", json_string(buffer));
  } else {
    JSON_SET_S(result, "photo_width", width);
    JSON_SET_S(result, "photo_height", height);
    JSON_SET_NEW_S(result, "type", json_string("photo"));
    JSON_SET_NEW_S(result, "photo_url", json_string(buffer));
  }

  written_chars = snprintf(buffer, 3000, DANBOORU_ROOT, thumbnail);
  if (!written_chars) {
    json_decref(result);
    return NULL;
  }

  thumbnail_json = json_string(buffer);
  if (!thumbnail_json) {
    json_decref(result);
    return NULL;
  }

  JSON_SET_S(result, "id", md5);
  JSON_SET_NEW_S(result, "thumb_url", thumbnail_json);

  return result;
}

/*-----------------------------------------------------------
|  Returns a serialized answerInlineQuery object containing
|  the Danbooru search results.
-----------------------------------------------------------*/
const char *telegram_result(json_t *danbooru, const char *id,
                            json_int_t next_offset) {
  json_t *object, *results, *image;
  const char *dump;

  object = json_object();
  if (!object) {
    return NULL;
  }

  JSON_SET_NEW_S(object, "inline_query_id", json_string(id));
  JSON_SET_NEW_S(object, "cache_time", json_integer(3600));
  JSON_SET_NEW_S(object, "next_offset", json_integer(next_offset));

  results = json_array();
  if (!results) {
    json_decref(object);
    return NULL;
  }

  for (size_t i = 0; (image = json_array_get(danbooru, i)); ++i) {
    image = create_result(image);
    if (image)
      json_array_append_new(results, image);
  }

  if (json_object_set_new(object, "results", results)) {
    json_decref(object);
    json_decref(results);
    return NULL;
  }

  dump = json_dumps(object, 0);
  json_decref(object);
  return dump;
}

/*---------------------------------------
|  Processes and answers an incoming
|  inline query.
---------------------------------------*/
void answer_query(json_t *inline_query) {
  const char *user_query, *offset, *search_query, *post_data, *id;
  json_t *s_arr, *s_obj, *s_name, *danbooru;
  json_int_t next_offset;

  if (set_params(inline_query, &id, &user_query, &offset))
    return;

  if (*offset == '\0') {
    offset = "1";
    next_offset = 2;
  } else {
    next_offset = strtoll(offset, NULL, 0);
    if (errno) {
      errno = 0;
      return;
    }
    next_offset += 1;
  }

  s_arr = autocomplete(user_query);
  if (!s_arr)
    search_query = user_query;
  else {
    s_obj = json_array_get(s_arr, 0);
    if (!json_is_object(s_obj)) {
      json_decref(s_arr);
      return;
    }

    s_name = json_object_get(s_obj, "name");
    if (!json_is_string(s_name)) {
      json_decref(s_arr);
      return;
    }

    search_query = json_string_value(s_name);
    if (!search_query) {
      json_decref(s_arr);
      return;
    }
  }

  danbooru = query(search_query, offset);
  json_decref(s_arr);
  if (!danbooru)
    return;

  post_data = telegram_result(danbooru, id, next_offset);
  json_decref(danbooru);
  if (!post_data)
    return;

  free(request(ANSWERQUERY_URL, post_data, 0, 1, telegram_handle).data);
  free((void *)post_data);
}
