#include "netblink.h"

#include <string.h>

#include "openssl/ssl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"

static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const static int CONNECTED_BIT = BIT0;

const static int ON = LIGHTSWITCH_POLARITY_SWAP ? 0: 1;
const static int OFF = LIGHTSWITCH_POLARITY_SWAP ? 1: 0;

const static char *TAG = "netblink";

typedef enum
{
	on,
	off,
	blink
} switch_action_t;

#define OPENSSL_BLINK_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
                                "Content-Type: text/html\r\n" \
                                "Content-Length: 98\r\n\r\n" \
                                "<html>\r\n" \
                                "<head>\r\n" \
                                "<title>OpenSSL example</title></head><body>\r\n" \
                                "I blinked an LED, just for you!\r\n" \
                                "</body>\r\n" \
                                "</html>\r\n" \
                                "\r\n"

#define OPENSSL_SWITCHON_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
                                "Content-Type: text/html\r\n" \
                                "Content-Length: 98\r\n\r\n" \
                                "<html>\r\n" \
                                "<head>\r\n" \
                                "<title>OpenSSL example</title></head><body>\r\n" \
                                "I switched an LED on, just for you!\r\n" \
                                "</body>\r\n" \
                                "</html>\r\n" \
                                "\r\n"

#define OPENSSL_SWITCHOFF_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
                                "Content-Type: text/html\r\n" \
                                "Content-Length: 98\r\n\r\n" \
                                "<html>\r\n" \
                                "<head>\r\n" \
                                "<title>OpenSSL example</title></head><body>\r\n" \
                                "I switched an LED off, just for you!\r\n" \
                                "</body>\r\n" \
                                "</html>\r\n" \
                                "\r\n"

static void blink_once_task(void *p)
{
    gpio_set_direction(LIGHTSWITCH_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_level(LIGHTSWITCH_GPIO, OFF);
	vTaskDelay(300 / portTICK_PERIOD_MS);

    gpio_set_level(LIGHTSWITCH_GPIO, ON);
	vTaskDelay(300 / portTICK_PERIOD_MS);

    gpio_set_level(LIGHTSWITCH_GPIO, OFF);

    vTaskDelete(NULL);
    return ;
}

static void switch_on_task(void *p)
{
    gpio_set_direction(LIGHTSWITCH_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LIGHTSWITCH_GPIO, ON);

    vTaskDelete(NULL);
    return ;
}

static void switch_off_task(void *p)
{
    gpio_set_direction(LIGHTSWITCH_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LIGHTSWITCH_GPIO, OFF);

    vTaskDelete(NULL);
    return ;
}

static void flip_switch(switch_action_t action)
{
    int ret;
    xTaskHandle blink_once_handle;

    switch (action) {
    case on:
    	break;
    case off:
    	break;
    case blink:
        ret = xTaskCreate(blink_once_task,
        		BLINK_ONCE_TASK_NAME,
    			BLINK_ONCE_TASK_STACK_WORDS,
    			NULL,
    			BLINK_ONCE_TASK_PRIORITY,
    			&blink_once_handle);

        if (ret != pdPASS)  {
            ESP_LOGI(TAG, "create task %s failed", BLINK_ONCE_TASK_NAME);
        }
    	break;
    }
    //TaskFuntion_t my_func;
    void *my_func = blink_once_task;


}


static void netblink_task(void *p)
{
    int ret;

    SSL_CTX *ctx;
    SSL *ssl;

    int sockfd, new_sockfd;
    socklen_t addr_len;
    struct sockaddr_in sock_addr;

    char recv_buf[NETBLINK_RECV_BUF_LEN];

    const char blink_response[] = OPENSSL_BLINK_SERVER_ACK;
    const char switchon_response[] = OPENSSL_SWITCHON_SERVER_ACK;
    const char switchoff_response[] = OPENSSL_SWITCHOFF_SERVER_ACK;
    const int blink_bytes = sizeof(blink_response);
    const int switchon_bytes = sizeof(switchon_response);
    const int switchoff_bytes = sizeof(switchoff_response);

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    const unsigned int cacert_pem_bytes = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    const unsigned int prvtkey_pem_bytes = prvtkey_pem_end - prvtkey_pem_start;   

    ESP_LOGI(TAG, "SSL server context create ......");
    /* For security reasons, it is best if you can use
       TLSv1_2_server_method() here instead of TLS_server_method().
       However some old browsers may not support TLS v1.2.
    */
    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ESP_LOGI(TAG, "failed");
        goto failed1;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set own certification......");
    ret = SSL_CTX_use_certificate_ASN1(ctx, cacert_pem_bytes, cacert_pem_start);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set private key......");
    ret = SSL_CTX_use_PrivateKey_ASN1(0, ctx, prvtkey_pem_start, prvtkey_pem_bytes);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server create socket ......");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket bind ......");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(NETBLINK_LOCAL_TCP_PORT);
    ret = bind(sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket listen ......");
    ret = listen(sockfd, 32);
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

reconnect:
	ESP_LOGI(TAG, "----------------------------------");
    ESP_LOGI(TAG, "SSL server create ......");
    ssl = SSL_new(ctx);
    if (!ssl) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket accept client ......");
    new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
    if (new_sockfd < 0) {
        ESP_LOGI(TAG, "failed" );
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    SSL_set_fd(ssl, new_sockfd);

    ESP_LOGI(TAG, "SSL server accept client ......");
    ret = SSL_accept(ssl);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed5;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server read message ......");
    do {
        memset(recv_buf, 0, NETBLINK_RECV_BUF_LEN);
        ret = SSL_read(ssl, recv_buf, NETBLINK_RECV_BUF_LEN - 1);
        if (ret <= 0) {
            break;
        }
        ESP_LOGI(TAG, "SSL read: %s", recv_buf);
        if (strstr(recv_buf, "GET /switchon") &&
            strstr(recv_buf, " HTTP/1.1")) {
            ESP_LOGI(TAG, "SSL get matched message")
            ESP_LOGI(TAG, "SSL write message")
            ret = SSL_write(ssl, switchon_response, switchon_bytes);
            if (ret > 0) {
                ESP_LOGI(TAG, "OK")
    	    	ESP_LOGI(TAG, "Switch On......");
                gpio_set_level(LIGHTSWITCH_GPIO, ON);
                ESP_LOGI(TAG, "OK")

            } else {
                ESP_LOGI(TAG, "error")
            }
            break;
        }else if (strstr(recv_buf, "GET /switchoff") &&
                strstr(recv_buf, " HTTP/1.1")) {
                ESP_LOGI(TAG, "SSL get matched message")
                ESP_LOGI(TAG, "SSL write message")
                ret = SSL_write(ssl, switchoff_response, switchoff_bytes);
                if (ret > 0) {
                    ESP_LOGI(TAG, "OK")
        	    	ESP_LOGI(TAG, "Switch Off......");
                    gpio_set_level(LIGHTSWITCH_GPIO, OFF);
                    ESP_LOGI(TAG, "OK")

                } else {
                    ESP_LOGI(TAG, "error")
                }
                break;
        }else if (strstr(recv_buf, "GET /blink") &&
                strstr(recv_buf, " HTTP/1.1")) {
                ESP_LOGI(TAG, "SSL get matched message")
                ESP_LOGI(TAG, "SSL write message")
                ret = SSL_write(ssl, blink_response, blink_bytes);
                if (ret > 0) {
                    ESP_LOGI(TAG, "OK")
        	    	ESP_LOGI(TAG, "Blink Once......");
    				flip_switch(blink);
                    ESP_LOGI(TAG, "OK")

                } else {
                    ESP_LOGI(TAG, "error")
                }
                break;
        }else if (strstr(recv_buf, "GET ") &&
                strstr(recv_buf, " HTTP/1.1")) {
                ESP_LOGI(TAG, "SSL get matched message")
                ESP_LOGI(TAG, "SSL write message")
                ret = SSL_write(ssl, blink_response, blink_bytes);
                if (ret > 0) {
                    ESP_LOGI(TAG, "OK")
                } else {
                    ESP_LOGI(TAG, "error")
                }
                break;
		}

    } while (1);
    
    ESP_LOGI(TAG, "SSL server shutdown ......");
    SSL_shutdown(ssl);
    ESP_LOGI(TAG, "OK");
failed5:
    close(new_sockfd);
    new_sockfd = -1;
failed4:
    SSL_free(ssl);
    ssl = NULL;
    goto reconnect;
failed3:
    close(sockfd);
    sockfd = -1;
failed2:
    SSL_CTX_free(ctx);
    ctx = NULL;
failed1:
    vTaskDelete(NULL);
    return ;
} 

static void openssl_server_init(void)
{
    int ret;
    xTaskHandle openssl_handle;

    ret = xTaskCreate(netblink_task,
                      NETBLINK_TASK_NAME,
                      NETBLINK_TASK_STACK_WORDS,
                      NULL,
                      NETBLINK_TASK_PRIORITY,
                      &openssl_handle); 

    if (ret != pdPASS)  {
        ESP_LOGI(TAG, "create task %s failed", NETBLINK_TASK_NAME);
    }
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        openssl_server_init();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect(); 
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void wifi_conn_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    //ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]\n", EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS);
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static esp_err_t setup_light_gpio()
{
	if (!GPIO_IS_VALID_OUTPUT_GPIO(LIGHTSWITCH_GPIO))  //this may be redundant based on what gpio_set_direction checks
		return ESP_ERR_NOT_SUPPORTED;

	esp_err_t retval = gpio_set_direction(LIGHTSWITCH_GPIO, GPIO_MODE_OUTPUT);

	if (retval != ESP_OK)
		return retval;

	retval = gpio_set_level(LIGHTSWITCH_GPIO, OFF);
	return retval;

}
void app_main(void)
{
	ESP_ERROR_CHECK( setup_light_gpio() );
	ESP_ERROR_CHECK( nvs_flash_init() );
    wifi_conn_init();
}
