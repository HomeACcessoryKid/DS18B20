/*  (c) 2021-2022 HomeAccessoryKid */
#include "mqtt-client.h"

QueueHandle_t publish_queue;
char    *mqtthost=NULL, *mqttuser=NULL, *mqttpass=NULL, *dmtczidx=NULL;

static const char *  get_my_id(void) {
    // Use MAC address for Station as unique ID
    static char my_id[13];
    static bool my_id_done = false;
    int8_t i;
    uint8_t x;
    if (my_id_done)
        return my_id;
    if (!sdk_wifi_get_macaddr(STATION_IF, (uint8_t *)my_id))
        return NULL;
    for (i = 5; i >= 0; --i) {
        x = my_id[i] & 0x0F;
        if (x > 9) x += 7;
        my_id[i * 2 + 1] = x + '0';
        x = my_id[i] >> 4;
        if (x > 9) x += 7;
        my_id[i * 2] = x + '0';
    }
    my_id[12] = '\0';
    my_id_done = true;
    return my_id;
}

#define BACKOFF1 100/portTICK_PERIOD_MS
static void  mqtt_task(void *pvParameters) {
    int ret = 0;
    int backoff = BACKOFF1;
    struct mqtt_network network;
    mqtt_client_t client = mqtt_client_default;
    char mqtt_client_id[20];
    uint8_t mqtt_buf[100];
    uint8_t mqtt_readbuf[100];
    mqtt_packet_connect_data_t data = mqtt_packet_connect_data_initializer;
    char msg[PUB_MSG_LEN];

    mqtt_network_new( &network );
    memset(mqtt_client_id, 0, sizeof(mqtt_client_id));
    strcpy(mqtt_client_id, "MAC-");
    strcat(mqtt_client_id, get_my_id());

    data.willFlag           = 0;
    data.MQTTVersion        = 3;
    data.clientID.cstring   = mqtt_client_id;
    data.username.cstring   = mqttuser;
    data.password.cstring   = mqttpass;
    data.keepAliveInterval  = 10;
    data.cleansession       = 0;

    printf("%s: started\n", __func__);
    while(1) {
        while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) vTaskDelay(200/portTICK_PERIOD_MS); //Check if we have an IP
        printf("%s: (re)connecting to MQTT server %s ... ",__func__, mqtthost);
        ret = mqtt_network_connect(&network, mqtthost, MQTT_PORT);
        if( ret ){
            printf("error: %d\n", ret);
            vTaskDelay(backoff);
            if (backoff<BACKOFF1*128) backoff*=2; //max out at 12.8 seconds
            continue;
        }
        printf("done\n");
        mqtt_client_new(&client, &network, 5000, mqtt_buf, 100, mqtt_readbuf, 100);
        printf("%s: send MQTT connect ... ", __func__);
        ret = mqtt_connect(&client, &data);
        if(ret){
            printf("error: %d\n", ret);
            mqtt_network_disconnect(&network);
            vTaskDelay(backoff);
            if (backoff<BACKOFF1*128) backoff*=2; //max out at 12.8 seconds
            continue;
        }
        printf("done\n");
        backoff = BACKOFF1;
        xQueueReset(publish_queue);

        while(1) {
            msg[PUB_MSG_LEN - 1] = 0;
            while(xQueueReceive(publish_queue, (void *)msg, 0) == pdTRUE){
                mqtt_message_t message;
                message.payload = msg;
                message.payloadlen = strlen(msg);
                message.dup = 0;
                message.qos = MQTT_QOS1;
                message.retained = 0;
                ret = mqtt_publish(&client, MQTT_topic , &message);
                if (ret != MQTT_SUCCESS ){
                    printf("%s: error while publishing message: %d\n", __func__, ret );
                    break;
                }
            }
            ret = mqtt_yield(&client, 1000);
            if (ret == MQTT_DISCONNECTED) break;
        }
        printf("%s: connection dropped, connecting again\n", __func__);
        mqtt_network_disconnect(&network);
    }
}

char error[]="error";
static void ota_string() {
    char *otas;
    if (sysparam_get_string("ota_string", &otas) == SYSPARAM_OK) {
        mqtthost=strtok(otas,";");
        mqttuser=strtok(NULL,";");
        mqttpass=strtok(NULL,";");
        dmtczidx=strtok(NULL,";");
    }
    if (mqtthost==NULL) mqtthost=error;
    if (mqttuser==NULL) mqttuser=error;
    if (mqttpass==NULL) mqttpass=error;
    if (dmtczidx==NULL) dmtczidx=error;
}

void mqtt_client_init(int queue_size) {
    publish_queue = xQueueCreate(queue_size, PUB_MSG_LEN);
    ota_string();
    xTaskCreate(&mqtt_task, "mqtt_task", 1024, NULL, 2, NULL);
}
