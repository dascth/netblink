
#ifndef _NETBLINK_H_
#define _NETBLINK_H_

#include "sdkconfig.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID               CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS               CONFIG_WIFI_PASSWORD
#define LIGHTSWITCH_GPIO				CONFIG_OUTPUT_GPIO
#define LIGHTSWITCH_POLARITY_SWAP		CONFIG_OUTPUT_POLARITY_SWAP


#define NETBLINK_TASK_NAME        "netblink"
#define NETBLINK_TASK_STACK_WORDS 10240
#define NETBLINK_TASK_PRIORITY    8

#define NETBLINK_RECV_BUF_LEN       1024

#define NETBLINK_LOCAL_TCP_PORT     443


#define BLINK_ONCE_TASK_NAME        "blink_once"
#define BLINK_ONCE_TASK_STACK_WORDS 5120
#define BLINK_ONCE_TASK_PRIORITY    8


#endif

