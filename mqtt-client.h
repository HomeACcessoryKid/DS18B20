/*  (c) 2021-2022 HomeAccessoryKid 
 *  Intended as a Domoticz publish only feed
 */
#ifndef __MQTT_CLIENT_H__
#define __MQTT_CLIENT_H__

// #define PUB_MSG_LEN 48  // suitable for a single Domoticz counter update
// #define MQTT_PORT  1883
// #define MQTT_topic "domoticz/in"
#define MQTT_DEFAULT_CONFIG {0,3,48,NULL,1883,NULL,NULL,"domoticz/in"}

typedef struct mqtt_config {
    int  dummy; //somehow the first entry converts into a pointer so this is a workaround
    int  queue_size;
    int  msg_len;
    char *host;
    int  port;
    char *user;
    char *pass;
    char *topic;
} mqtt_config_t;

void mqtt_client_init(mqtt_config_t *config);
int  mqtt_client_publish(char *format,  ...);

#endif // __MQTT_CLIENT_H__
