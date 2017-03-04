#boorubot
boorubot is a [Telegram chat bot](https://core.telegram.org/bots/api) written entirely in C. Images are fetched 
using the Danbooru json api and displayed via inline mode. You can demo it [here](https://t.me/boorubot).

### Screenshots
![](https://my.mixtape.moe/wfohxm.png)![](https://my.mixtape.moe/yybvai.png)

### Requirements
- Unix Environment
- Libcurl
- Jansson

### Instructions
First clone the repo and cd into the src directory.
```bash
git clone https://github.com/github/gitignore.git --recursive
cd boorubot/src
```
Edit the boorubot.h and set the following parameters with your favorite text editor.
```C
#define BOT_NAME "Yotsugi"       // Your bot name here
#define BOT_USERNAME "@boorubot" // Your bot username here
#define TELEGRAM_TOKEN "123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ" // Your bot token here
```
Finally make and run the bot.
```bash
cd ..
make
./boorubot
```

### TODO
- Logging
- Openrc and Systemd service
