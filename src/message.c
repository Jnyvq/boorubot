#include "boorubot.h"
#include <curl/curl.h>
#include <jansson.h>
#include <string.h>

extern CURLSH *telegram_handle;

#define RESPONSE                                                               \
  "Hello, I'm an inline only bot. You can type `" BOT_USERNAME                 \
  " [query]` from any chat to use me.%0A%0A[Source "                           \
  "code](github.com/Jnyvq/boorubot) • [Example "                             \
  "Usage](my.mixtape.moe/yybvai.png) • [Bugs/Feedback](t.me/anymay)"

#define sendMessageURL                                                         \
  "https://api.telegram.org/bot" TELEGRAM_TOKEN "/sendMessage?text=%s&parse_"  \
  "mode=MARKDOWN&chat_id=%lld&"                                                \
  "disable_web_page_preview=1"

/*-----------------------------------------------------
|  Checks if a message contains a command and routes
|  to any match.
-----------------------------------------------------*/
void route_message(json_t *message) {
  json_t *chat_json, *id_json;
  int written_chars;
  json_int_t chat_id;
  char url[5000];

  chat_json = json_object_get(message, "chat");
  if (!json_is_object(chat_json))
    return;

  id_json = json_object_get(chat_json, "id");
  if (!json_is_integer(id_json))
    return;

  chat_id = json_integer_value(id_json);
  if (!chat_id)
    return;

  written_chars = snprintf(url, 5000, sendMessageURL, RESPONSE, chat_id);
  if (written_chars <= 0)
    return;

  free(request(url, NULL, 0, 0, telegram_handle).data);
}
