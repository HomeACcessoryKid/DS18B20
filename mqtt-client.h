/*  (c) 2021-2022 HomeAccessoryKid 
 *  Intended as a Domoticz publish only feed
 */
#ifndef __MQTT_CLIENT_H__
#define __MQTT_CLIENT_H__

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
//#include <espressif/esp_system.h> //for timestamp report only
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>

#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>
#include <semphr.h>
#include <sysparam.h>

#define PUB_MSG_LEN 48  // suitable for a Domoticz counter update
#define MQTT_PORT  1883
#define MQTT_topic "domoticz/in"

extern QueueHandle_t publish_queue;
extern char *dmtczidx;

void mqtt_client_init(int queue_size);

#endif // __MQTT_CLIENT_H__
