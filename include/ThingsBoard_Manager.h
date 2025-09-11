#ifndef _THINGSBOARD_MANAGER_H
#define _THINGSBOARD_MANAGER_H

#include <Arduino.h>
#include <Arduino_MQTT_Client.h>
#include <Attribute_Request.h>
#include <Server_Side_RPC.h>
#include <Shared_Attribute_Update.h>
#include <ThingsBoard.h>
#include <WiFiClient.h>

#include "Configuration.h"

constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 1024U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 1024U;

constexpr uint64_t REQUEST_TIMEOUT_MICROSECONDS = 5000U * 1000U;

uint8_t lastThingsBoardStatus = 0;

// Initialize underlying client, used to establish a connection
WiFiClient WiFi_client;

// Initalize the Mqtt client instance
Arduino_MQTT_Client MQTT_client(WiFi_client);

// Initialize ThingsBoard instance with the maximum needed buffer size and stack size
// APIs are registered on main code
ThingsBoard ThingsBoard_client(MQTT_client, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
                               Default_Max_Stack_Size);

void ThingsBoard_setup()
{
    lastThingsBoardStatus = ThingsBoard_client.connected();
}

#endif  // _THINGSBOARD_MANAGER_H