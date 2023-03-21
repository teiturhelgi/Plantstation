/* 
   Output control ESP32
   Code by Inti Holmfred
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "i2cdev.h"
#include "math.h"
#include "ssd1306.h"
#include "driver/gpio.h"


static const char *TAG = "MQTT_EXAMPLE";
static esp_mqtt_client_handle_t client;

SemaphoreHandle_t Mutexx = NULL;
TaskHandle_t OLED1;
float tempdata;
float humidata;
float moistdata;
float lightdata;


typedef struct measuringtype
{
    float max;         //!< max value
    float min;        //!< min value
    int test;
    int led;

} measureClass;
measureClass temperatureClass;
measureClass humClass;
measureClass moistClass;
measureClass lightClass;
void warningtest(float tester, measureClass tclass){

    if (tester >= tclass.max)
    {
        printf("Turning on the LED, high warning\n");
        gpio_set_level(tclass.led, 1); 
    } else if (tester <= tclass.min)
    {
        printf("Turning on the LED, low warning\n");
        gpio_set_level(tclass.led, 1);
    } else {
        printf("Turning off the LED\n");
        gpio_set_level(tclass.led, 0);
    }

}


static void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}
void set_config(int tocon){

    switch (tocon)
    {
    case 1: // plant-type 1
        temperatureClass.max = 25;
        temperatureClass.min = 10;
        humClass.max = 100;
        humClass.min = 10;
        moistClass.max = 15;
        moistClass.min = 10;
        lightClass.max = 100;
        lightClass.min = 10;
        printf("Setting config 1\r\n");
        break;
    case 2: // plant-type 2
        temperatureClass.max = 25;
        temperatureClass.min = 10;
        humClass.max = 100;
        humClass.min = 10;
        moistClass.max = 35;
        moistClass.min = 10;
        lightClass.max = 100;
        lightClass.min = 10;
        printf("Setting config 2\r\n");
        break;
    case 3:
        temperatureClass.max = 25;
        temperatureClass.min = 10;
        humClass.max = 100;
        humClass.min = 10;
        moistClass.max = 75;
        moistClass.min = 10;
        lightClass.max = 100;
        lightClass.min = 10;
        printf("Setting config 3\r\n");
        break;
        
    default:
        break;
    }
}
void decombiner(char str[80]){

    const char s[2] = ";";
    

    

    tempdata = atof(strtok(str,s));
    humidata = atof(strtok(NULL,s));
    moistdata = atof(strtok(NULL,s));
    lightdata = atof(strtok(NULL,s));

}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            //ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            //msg_id = esp_mqtt_client_publish(client, "temp", "data_3", 0, 1, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            if (0) // TRUE to use four channel
            {
            msg_id = esp_mqtt_client_subscribe(client, "temperature", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "humidity", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "moisture", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "light", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            }
            msg_id = esp_mqtt_client_subscribe(client, "config", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "allsensors", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);


            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
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
            char tempcomp[80];
            char temphold[80];

            sprintf(tempcomp, "%.*s", event->topic_len, event->topic);
            if (strcmp(tempcomp, "temperature") == 0){

                sprintf(temphold, "%.*s", event->data_len, event->data);                
                tempdata = atof(temphold);

                vTaskResume(OLED1);
                

            } else if (strcmp(tempcomp, "humidity") == 0){
                
                sprintf(temphold, "%.*s", event->data_len, event->data);                
                humidata = atof(temphold);

                vTaskResume(OLED1);
            } else if (strcmp(tempcomp, "moisture") == 0){
                sprintf(temphold, "%.*s", event->data_len, event->data);                
                moistdata = atof(temphold);
                vTaskResume(OLED1);
                

            } else if (strcmp(tempcomp, "light") == 0){
                sprintf(temphold, "%.*s", event->data_len, event->data);                
                lightdata = atof(temphold);

                vTaskResume(OLED1);
            } else if (strcmp(tempcomp, "config") == 0){

                set_config(atoi(event->data));

                vTaskResume(OLED1);

            } else if (strcmp(tempcomp, "allsensors") == 0){
                
                sprintf(temphold, "%.*s", event->data_len, event->data);   
                decombiner(temphold);

                vTaskResume(OLED1);

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
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    client = client;
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void OLED( void *pvParameters ){
    //char i = 0;
    //char str2[80];
    InitializeDisplay();
    clear_display();
    char tempstring[100];
    char humistring[100];
    char moiststring[100];
    char lightstring[100];
    
    
    for( ;; ){
        sprintf(tempstring, "Temp: %2.1f ", tempdata);
        sprintf(humistring, "Humidity: %2.1f %%", humidata);
        sprintf(moiststring, "Moisture: %2.1f %%", moistdata);
        sprintf(lightstring, "Light: %2.1f %%", lightdata);

        printf(tempstring);
        printf(humistring);
        printf("%f\r\n",moistdata);
        printf(lightstring);

        warningtest(tempdata, temperatureClass);
        warningtest(moistdata, moistClass);
        sendStrXY(tempstring,1,0);
        sendStrXY(humistring,2,0);
        sendStrXY(moiststring,3,0);
        //sprintf(str2, "Running for %d seconds\n", i);
        sendStrXY(lightstring,4,0);
        
        //i++;
        //xSemaphoreGive(Mutexx);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        vTaskSuspend(NULL);
        //}
    }
    vTaskDelete(NULL);
}






void ledsetup(){



    temperatureClass.max = 25;
    temperatureClass.min = 10;
    temperatureClass.led = 5;

    gpio_reset_pin(temperatureClass.led);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(temperatureClass.led, GPIO_MODE_OUTPUT);

    humClass.max = 100;
    humClass.min = 10;
    humClass.led = 17;
    gpio_reset_pin(humClass.led);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(humClass.led, GPIO_MODE_OUTPUT);

    moistClass.max = 70;
    moistClass.min = 10;
    moistClass.led = 16;
    gpio_reset_pin(moistClass.led);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(moistClass.led, GPIO_MODE_OUTPUT);


    lightClass.max = 100;
    lightClass.min = 10;
    lightClass.led = 4;
    gpio_reset_pin(lightClass.led);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(lightClass.led, GPIO_MODE_OUTPUT);

}





void app_main(void)
{
    ledsetup();
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    Mutexx = xSemaphoreCreateMutex();
    mqtt_app_start();

    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(i2c_example_master_init());

    
    xTaskCreate( OLED, "OLED", 4000, NULL, 10, &OLED1 );


}
