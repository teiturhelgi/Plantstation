#Sensor ESP32 Main Code


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_adc_cal.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "driver/gpio.h"
#include "driver/adc.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "dht.h"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling
#define CONFIG_EXAMPLE_TYPE_AM2301
#define CONFIG_EXAMPLE_DATA_GPIO 5
#define DHT_GPIO 12
#define LDR_GPIO 13
#define SMS_GPIO 14


#if defined(CONFIG_EXAMPLE_TYPE_DHT11)
#define SENSOR_TYPE DHT_TYPE_DHT11
#endif
#if defined(CONFIG_EXAMPLE_TYPE_AM2301)
#define SENSOR_TYPE DHT_TYPE_AM2301
#endif
#if defined(CONFIG_EXAMPLE_TYPE_SI7021)
#define SENSOR_TYPE DHT_TYPE_SI7021
#endif

static uint SLEEP_TIME = 70;

static uint8_t dht_out = 0;
static uint8_t ldr_out = 0;
static uint8_t sms_out = 0;

static esp_adc_cal_characteristics_t *adc_chars1;
static esp_adc_cal_characteristics_t *adc_chars2;

static const adc_channel_t channel1 = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t channel2 = ADC_CHANNEL_7;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;

static const adc_atten_t atten1 = ADC_ATTEN_DB_0; //change to calibrate min & max brightness
static const adc_atten_t atten2 = ADC_ATTEN_DB_11; //change to calibrate min & max moisture
static const adc_unit_t unit1 = ADC_UNIT_1;
static const adc_unit_t unit2 = ADC_UNIT_1;

static const char *TAG = "MQTT_EXAMPLE";
static esp_mqtt_client_handle_t client;

char data_temp[100];
char data_humid[100];
char data_moist[100];
char data_light[100];
char data_all[500];

static char dht_flag = 0;
static char ldr_flag = 0;
static char sms_flag = 0;

SemaphoreHandle_t Mutexx;

void dht_test(void *pvParameters)
{
    float temperature, humidity;

#ifdef CONFIG_EXAMPLE_INTERNAL_PULLUP
    gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);
#endif

    while (1)
    {
        
        dht_out = 1;
        gpio_set_level(DHT_GPIO, dht_out);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        xSemaphoreTake(Mutexx, 100 / portTICK_PERIOD_MS);
        if (dht_read_float_data(SENSOR_TYPE, CONFIG_EXAMPLE_DATA_GPIO, &humidity, &temperature) == ESP_OK)
        {
            dht_out = 0;
            gpio_set_level(DHT_GPIO, dht_out);
            printf("Humidity: %.1f%% Temp: %.1fC\n", humidity, temperature);

            sprintf(data_humid,"% 5.1f", humidity);
            sprintf(data_temp,"% 4.1f", temperature);

            /*
            int msg_id;
            msg_id = esp_mqtt_client_publish(client, "humidity", data_humid , 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, "temperature", data_temp , 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            */

            dht_flag = 1;
        }
        else
            printf("Could not read data from sensor\n");

        // If you read the sensor data too often, it will heat up
        // http://www.kandrsmith.org/RJS/Misc/Hygrometers/dht_sht_how_fast.html
    
        if (dht_flag == 1 && ldr_flag == 1 && sms_flag == 1){
            printf("Entering Deep Sleep...\n");
            dht_flag = 0;
            ldr_flag = 0;
            sms_flag = 0;
            sprintf(data_all,"%s;%s;%s;%s;",data_temp, data_humid, data_moist, data_light);
            int msg_id;
            msg_id = esp_mqtt_client_publish(client, "allsensors", data_all , 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            
            esp_sleep_enable_timer_wakeup(1000000 * SLEEP_TIME);
            esp_deep_sleep_start();
        }
        xSemaphoreGive(Mutexx);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void ldr_sample(void *pvParameters)
{
    while (1) {
        xSemaphoreTake(Mutexx, 100 / portTICK_PERIOD_MS);
        
        uint32_t adc_reading1 = 0;
        ldr_out = 1;
        gpio_set_level(LDR_GPIO, ldr_out);
        usleep(1);
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit1 == ADC_UNIT_1) {
                adc_reading1 += adc1_get_raw((adc1_channel_t)channel1);
            } else {
                int raw1;
                adc2_get_raw((adc2_channel_t)channel1, width, &raw1);
                adc_reading1 += raw1;
            }
        }
        ldr_out = 0;
        gpio_set_level(LDR_GPIO, ldr_out);
        adc_reading1 /= NO_OF_SAMPLES;
        //uint32_t voltage1 = esp_adc_cal_raw_to_voltage(adc_reading1, adc_chars1);
        uint32_t light_i=(4095-adc_reading1);
        float light = (float)light_i/4095*100;
        sprintf(data_light,"% 5.1f", light);

        /*
        int msg_id;
        msg_id = esp_mqtt_client_publish(client, "light", data_light , 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        */
        ldr_flag = 1;

        printf("Light: %.2f%%\n", light);
        //printf("Light ADC: %d\n", adc_reading1);
        if (dht_flag == 1 && ldr_flag == 1 && sms_flag == 1){
            printf("Entering Deep Sleep...\n");
            dht_flag = 0;
            ldr_flag = 0;
            sms_flag = 0;
            sprintf(data_all,"%s;%s;%s;%s;",data_temp, data_humid, data_moist, data_light);
            int msg_id;
            msg_id = esp_mqtt_client_publish(client, "allsensors", data_all , 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            
            esp_sleep_enable_timer_wakeup(1000000 * SLEEP_TIME);
            esp_deep_sleep_start();
        }
        xSemaphoreGive(Mutexx);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void sms_sample(void *pvParameters)
{
    while (1) {
        xSemaphoreTake(Mutexx, 100 / portTICK_PERIOD_MS);
        
        uint32_t adc_reading2 = 0;
        sms_out = 1;
        gpio_set_level(SMS_GPIO, sms_out);
        usleep(1);
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit2 == ADC_UNIT_1) {
                adc_reading2 += adc1_get_raw((adc1_channel_t)channel2);
            } else {
                int raw2;
                adc2_get_raw((adc2_channel_t)channel2, width, &raw2);
                adc_reading2 += raw2;
            }
        }
        sms_out = 0;
        gpio_set_level(SMS_GPIO, sms_out);
        adc_reading2 /= NO_OF_SAMPLES;
        //uint32_t voltage2 = esp_adc_cal_raw_to_voltage(adc_reading2, adc_chars2);
        float moisture = (float)adc_reading2/4095*100*100/77;
        if (moisture > 100){
            moisture = 100;
        }
        sprintf(data_moist,"% 5.1f", moisture);
        /*
        int msg_id;
        msg_id = esp_mqtt_client_publish(client, "moisture", data_moist , 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        */
        sms_flag = 1;
        printf("Soil Moisture: %.1f%%\n", moisture);
        //printf("Soil ADC: %d", adc_reading2);
        if (dht_flag == 1 && ldr_flag == 1 && sms_flag == 1){
            printf("Entering Deep Sleep...\n");
            dht_flag = 0;
            ldr_flag = 0;
            sms_flag = 0;
            sprintf(data_all,"%s;%s;%s;%s;",data_temp, data_humid, data_moist, data_light);
            int msg_id;
            msg_id = esp_mqtt_client_publish(client, "allsensors", data_all , 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            
            esp_sleep_enable_timer_wakeup(1000000 * SLEEP_TIME);
            esp_deep_sleep_start();
        }
        xSemaphoreGive(Mutexx);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        /*msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        */

        msg_id = esp_mqtt_client_subscribe(client, "sleeptime", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        //xTaskCreate(dht_test, "dht_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
        //xTaskCreate(adc_sample, "adc_sample", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        /*msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        */
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
        char topiccomp[80];
 
        sprintf(topiccomp, "%.*s", event->topic_len, event->topic);
        if (strcmp(topiccomp, "sleeptime") == 0){
            char newtime[100];
            sprintf(newtime, "%.*s", event->data_len, event->data);
            SLEEP_TIME = atoi(newtime);
            //printf("%d",SLEEP_TIME);
        }
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

static void mqtt_app_start(void)
{
    //xSemaphoreTake(Mutexx,500 / portTICK_PERIOD_MS);
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
        .port = 1883,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    client = client;
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    //xSemaphoreGive(Mutexx);
}

void app_main(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC for LDR
    if (unit1 == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel1, atten1);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel1, atten1);
    }
    adc_chars1 = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type1 = esp_adc_cal_characterize(unit1, atten1, width, DEFAULT_VREF, adc_chars1);
    print_char_val_type(val_type1);
    gpio_reset_pin(LDR_GPIO);
    gpio_set_direction(LDR_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LDR_GPIO, ldr_out);
    
    
    //Configure ADC for SMS
    if (unit2 == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel2, atten2);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel2, atten2);
    }
    adc_chars2 = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type2 = esp_adc_cal_characterize(unit2, atten2, width, DEFAULT_VREF, adc_chars2);
    print_char_val_type(val_type2);
    gpio_reset_pin(SMS_GPIO);
    gpio_set_direction(SMS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(SMS_GPIO, sms_out);

    gpio_reset_pin(DHT_GPIO);
    gpio_set_direction(DHT_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_GPIO, dht_out);

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    Mutexx = xSemaphoreCreateMutex();

    mqtt_app_start();

    xTaskCreate(dht_test, "dht_test", configMINIMAL_STACK_SIZE * 3, NULL, 3, NULL);
    xTaskCreate(ldr_sample, "ldr_sample", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    xTaskCreate(sms_sample, "sms_sample", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);

}
