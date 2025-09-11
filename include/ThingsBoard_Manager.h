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

//
// ThingsBoard variables and callbacks
//
constexpr uint8_t MAX_RPC_SUBSCRIPTIONS = 3U;
constexpr uint8_t MAX_RPC_RESPONSE = 5U;
constexpr uint8_t MAX_ATTRIBUTE_REQUESTS = 2U;
constexpr uint8_t MAX_SHARED_ATTRIBUTES_UPDATE = 3U;
constexpr size_t MAX_ATTRIBUTES = 3U;

// Initialize used ThingsBoard APIs
Server_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_RESPONSE> ThingsBoard_rpc;
Attribute_Request<MAX_ATTRIBUTE_REQUESTS, MAX_ATTRIBUTES> ThingsBoard_attribute_request;
Shared_Attribute_Update<MAX_SHARED_ATTRIBUTES_UPDATE, MAX_ATTRIBUTES> ThingsBoard_rpc_shared_update;

const std::array<IAPI_Implementation *, 3U> APIs = {
    &ThingsBoard_rpc, &ThingsBoard_attribute_request, &ThingsBoard_rpc_shared_update};

// Initialize ThingsBoard instance with the maximum needed buffer size and stack size
// APIs are registered on main code
ThingsBoard ThingsBoard_client(MQTT_client, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
                               Default_Max_Stack_Size, APIs.begin(), APIs.end());

void ThingsBoard_setup()
{
    lastThingsBoardStatus = ThingsBoard_client.connected();
}

#endif  // _THINGSBOARD_MANAGER_H