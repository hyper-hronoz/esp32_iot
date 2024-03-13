#include "dht.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include <string.h>
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

#define SENSOR_TYPE DHT_TYPE_DHT11

esp_mqtt_client_handle_t client;

bool is_connection_established = 0;
bool is_wifi_connected = 0;

static const char *TAG = "WiFi_STA";

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
      is_wifi_connected = 0;
    }
  } else if (event_id == IP_EVENT_STA_GOT_IP) {
    printf("Wifi got IP...\n\n");
    is_wifi_connected = 1;
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
                                          .ssid = "tp_freak_F",
                                          .password = "lolkek227@",
                                      }};
  esp_wifi_set_config(
      ESP_IF_WIFI_STA,
      &wifi_configuration); // setting up configs when event ESP_IF_WIFI_STA
  esp_wifi_start();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_connect();
}



void dht_task(void *pvParameters) {
  while(!is_connection_established) {
  }

  float temperature, humidity;

#ifdef CONFIG_EXAMPLE_INTERNAL_PULLUP
  gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);
#endif

  while (1) {
    if (dht_read_float_data(SENSOR_TYPE, 4, &humidity, &temperature) == ESP_OK) {
      printf("Humidity: %.1f%% Temp: %.1fC\n", humidity, temperature);
      char buffer[48] = {0};
      sprintf(buffer, "{\"temperature\":%.2f, \"humidity\": %.2f}", temperature, humidity);
      esp_mqtt_client_publish(client, "test", buffer, 0, 1, 0);
    }
    else {
      printf("Could not read data from sensor\n");
    }

    // If you read the sensor data too often, it will heat up
    // http://www.kandrsmith.org/RJS/Misc/Hygrometers/dht_sht_how_fast.html
    vTaskDelay(pdMS_TO_TICKS(5000));
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

        is_connection_established = 0;
        // msg_id = esp_mqtt_client_unsubscribe(client, "test");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        is_connection_established = 0;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        is_connection_established = 1;
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        is_connection_established = 0;
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        is_connection_established = 1;
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
        is_connection_established = 0;
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        is_connection_established = 0;
        break;
    }
}

static esp_mqtt_client_handle_t mqtt_app_start(void) {
  while(!is_wifi_connected) {
  }

  esp_mqtt_client_config_t mqtt_cfg = {
      .broker =
          {
              .address.uri = "mqtt://192.168.3.4:1883",
              .verification.skip_cert_common_name_check = true,
          },
  };
  client = esp_mqtt_client_init(&mqtt_cfg);

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

  wifi_init_sta();
  printf("Wifi ready!\n");

  printf("starting mqtt\n");
  mqtt_app_start();
  printf("mqtt started\n");

  xTaskCreate(dht_task, "dht_test", configMINIMAL_STACK_SIZE * 3, NULL, 5,
              NULL);
}

