#include "DHT11.h"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

#define MQTT_PORT 1883

const char *ssid = "test";
const char *pass = "lolkek227";

static const char *TAG = "WiFi_STA";

#define portTICK_RATE_MS 3000

static bool is_wifi_ready = 0;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void wifi_event_handler(void *event_handler_arg,
                               esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
  if (event_id == WIFI_EVENT_STA_START) {
    printf("WIFI CONNECTING....\n");
  } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
    printf("WiFi CONNECTED\n");
  } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
    int retry_num = 0;
    printf("WiFi lost connection\n");
    if (retry_num < 5) {
      esp_wifi_connect();
      retry_num++;
      printf("Retrying to Connect...\n");
    }
  } else if (event_id == IP_EVENT_STA_GOT_IP) {
    printf("Wifi got IP...\n\n");
    is_wifi_ready = 1;
  }
}

void wifi_init_sta() {
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_initiation);
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler,
                             NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler,
                             NULL);
  wifi_config_t wifi_configuration = {.sta = {
                                          .ssid = "test",
                                          .password = "lolkek227",
                                      }};
  esp_wifi_set_config(
      ESP_IF_WIFI_STA,
      &wifi_configuration); // setting up configs when event ESP_IF_WIFI_STA
  esp_wifi_start();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_connect();
  printf("wifi_init_softap finished. SSID:%s  password:%s\n", ssid, pass);
}

static int client_send_message(const esp_mqtt_client_handle_t *client, const uint8_t* msg, const uint32_t len) {
    return esp_mqtt_client_publish(*client, "test", (const char*)msg, len, 1, 0);
}

void DHT_task(void *pvParameter) {
  // esp_mqtt_client_handle_t* client = (esp_mqtt_client_handle_t*)pvParameter;
  DHT11_init(4);

  while (1) {
    struct dht11_reading data = DHT11_read();
    if (data.temperature == -1) {

    }

    char buffer[100] = {0};
    sprintf(buffer, "Temperature: %d", data.temperature);
    
    // client_send_message(client, (uint8_t*)buffer, sizeof(buffer));

    printf("Temperature %d\n", data.temperature);
    printf("Humandity %d\n", data.humidity);
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "test", "connection established", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "test", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "test", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "test");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


static esp_mqtt_client_handle_t mqtt_app_start(void) {
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker =
          {
              .address.uri = "mqtt://192.168.244.82:1883",
              .verification.skip_cert_common_name_check = true,
          },
  };
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

  esp_mqtt_client_start(client);

  return client;
};

void app_main() {
  printf("Esp32 serial on\n");
  ESP_ERROR_CHECK(nvs_flash_init()); // Initialize NVS flash
  // ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize Wi-Fi Access Point

  /* Print chip information */
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU core(s), %s%s%s%s, ", CONFIG_IDF_TARGET,
         chip_info.cores,
         (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
         (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
         (chip_info.features & CHIP_FEATURE_IEEE802154)
             ? ", 802.15.4 (Zigbee/Thread)"
             : "");

  unsigned major_rev = chip_info.revision / 100;
  unsigned minor_rev = chip_info.revision % 100;
  printf("silicon revision v%d.%d, ", major_rev, minor_rev);
  if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
    printf("Get flash size failed");
    return;
  }

  printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                       : "external");

  printf("Minimum free heap size: %" PRIu32 " bytes\n",
         esp_get_minimum_free_heap_size());

  wifi_init_sta(); // Initialize Wi-Fi connection
  //
  while(!is_wifi_ready) {
  }
  printf("Continue to mqtt\n");

  // esp_mqtt_client_handle_t client = mqtt_app_start();

  xTaskCreate(&DHT_task, "DHT_task", 2048, NULL, 5, NULL);
  // Keep the task running
  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
