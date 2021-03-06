#include <curl/curl.h>
#include <jansson.h>

/*-------------------------------
|  SET BEFORE COMPILING
-------------------------------*/
#define BOT_NAME "Yotsugi"
#define BOT_USERNAME "@boorubot"
#define TELEGRAM_TOKEN "123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ"

struct response {
  size_t size;
  char *data;
};

void answer_query(json_t *inline_query);
void answer_message(json_t *message);
struct response request(const char *url, const char *post, long timeout,
                        int use_json_headers, CURLSH *share);
