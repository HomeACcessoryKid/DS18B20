/*  (c) 2020-2022 HomeAccessoryKid
 *  This is a temperature sensor based on a single Dallas DS18B20
 *  It uses any ESP8266 with as little as 1MB flash. 
 *  GPIO-2 is used as a single one-wire DS18B20 sensor to measure the temperature
 *  UDPlogger is used to have remote logging
 *  LCM is enabled in case you want remote updates
 *  MQTT is used to send measurements upstream
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
//#include <espressif/esp_system.h> //for timestamp report only
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <string.h>
#include "lwip/api.h"
#include <udplogger.h>
#include "ds18b20/ds18b20.h"
#include "math.h"
#include "mqtt-client.h"
#include <sysparam.h>

#ifndef VERSION
 #error You must set VERSION=x.y.z to match github version tag x.y.z
#endif

#ifndef SENSOR_PIN
 #error SENSOR_PIN is not specified
#endif

/* ============== BEGIN HOMEKIT CHARACTERISTIC DECLARATIONS =============================================================== */
// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");

// next use these two lines before calling homekit_server_init(&config);
//    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
//                                      &model.value.string_value,&revision.value.string_value);
//    config.accessories[0]->config_number=c_hash;
// end of OTA add-in instructions

homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 99);

// void identify_task(void *_args) {
//     vTaskDelete(NULL);
// }

void identify(homekit_value_t _value) {
    UDPLUS("Identify\n");
//    xTaskCreate(identify_task, "identify", 256, NULL, 2, NULL);
}

/* ============== END HOMEKIT CHARACTERISTIC DECLARATIONS ================================================================= */

char *dmtczidx=NULL;
#define BEAT  10 //in seconds
TimerHandle_t xTimer;
void vTimerCallback( TimerHandle_t xTimer ) {
    uint32_t seconds = ( uint32_t ) pvTimerGetTimerID( xTimer );
    vTimerSetTimerID( xTimer, (void*)seconds+BEAT); //136 year to loop
    uint8_t scratchpad[8];
    float temp=99.99;
    
    if (ds18b20_measure(SENSOR_PIN, DS18B20_ANY, true) && ds18b20_read_scratchpad(SENSOR_PIN, DS18B20_ANY, scratchpad)) {
        temp = ((float)(scratchpad[1] << 8 | scratchpad[0]) * 625.0)/10000;
    }
    float old_temp=temperature.value.float_value;
    temperature.value.float_value=(float)(int)(temp*10+0.5)/10;
    if (old_temp!=temperature.value.float_value) \
        homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature.value.float_value));

    printf("%3d s %2.4f C\n", seconds, temp);
    int n=mqtt_client_publish("{\"idx\":%s,\"nvalue\":0,\"svalue\":\"%.4f\"}", dmtczidx, temp);
    if (n<0) printf("MQTT publish failed because %s\n",MQTT_CLIENT_ERROR(n));
}

mqtt_config_t mqttconf=MQTT_DEFAULT_CONFIG;
char error[]="error";
static void ota_string() {
    char *otas;
    if (sysparam_get_string("ota_string", &otas) == SYSPARAM_OK) {
        mqttconf.host=strtok(otas,";");
        mqttconf.user=strtok(NULL,";");
        mqttconf.pass=strtok(NULL,";");
        dmtczidx=strtok(NULL,";");
    }
    if (mqttconf.host==NULL) mqttconf.host=error;
    if (mqttconf.user==NULL) mqttconf.user=error;
    if (mqttconf.pass==NULL) mqttconf.pass=error;
    if (dmtczidx==NULL) dmtczidx=error;
}

void device_init() {
    gpio_set_pullup(SENSOR_PIN, true, true);
    //sysparam_set_string("ota_string", "192.168.178.5;DS18B20;fakepassword;63"); //can be used if not using LCM
    ota_string();
    mqtt_client_init(&mqttconf);
    xTimer=xTimerCreate( "Timer", BEAT*1000/portTICK_PERIOD_MS, pdTRUE, (void*)0, vTimerCallback);
    xTimerStart(xTimer, 0);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "DS18B20"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Temperature"),
                    &temperature,
                    &ota_trigger,
                    NULL
                }),
            NULL
        }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};


void user_init(void) {
    uart_set_baud(0, 115200);
    udplog_init(3);
    UDPLUS("\n\n\nDS18B20 " VERSION "\n");

    device_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1; revision.value.string_value="0.0.1"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}
