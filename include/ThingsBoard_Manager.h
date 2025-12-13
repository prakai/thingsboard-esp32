#ifndef _THINGSBOARD_MANAGER_H
#define _THINGSBOARD_MANAGER_H

#include <Arduino.h>
#include <Arduino_MQTT_Client.h>
#include <Attribute_Request.h>
#include <Client_Side_RPC.h>
#include <Preferences.h>
#include <Provision.h>
#include <Server_Side_RPC.h>
#include <Shared_Attribute_Update.h>
#include <ThingsBoard.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "Configuration.h"

String deviceName = "";
String deviceId = "";
String deviceMac = "";
String deviceModel = "";

Preferences ThingsBoard_pref;
const char* PREFS_NAMESPACE = "tb_prefs";
const char* PREFS_DEVICE_ID = "dev_id";
const char* PREFS_DEVICE_USER = "dev_user";
const char* PREFS_DEVICE_PASS = "dev_pass";

// Initialize underlying client, used to establish a connection
WiFiClient WiFi_client;

constexpr uint64_t THINGSBOARD_CONNECT_ATTEMPS_TIMOUT = 10000;  // 10 seconds
constexpr uint8_t THINGSBOARD_ATTEMPS_MAX = 5;

constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 1024U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 1024U;

constexpr uint64_t REQUEST_TIMEOUT_MICROSECONDS = 5000U * 1000U;

uint8_t currentThingsBoardConnectionStatus = 0;
uint8_t lastThingsBoardConnectionStatus = 0;

// Initalize the Mqtt client instance
Arduino_MQTT_Client MQTT_client(WiFi_client);

//
// ThingsBoard variables and callbacks
//
constexpr uint8_t MAX_RPC_SUBSCRIPTIONS = 4U;
constexpr uint8_t MAX_RPC_REQUEST = 5U;
constexpr uint8_t MAX_RPC_RESPONSE = 8U;
constexpr uint8_t MAX_ATTRIBUTE_REQUESTS = 8U;
constexpr uint8_t MAX_SHARED_ATTRIBUTES_UPDATE = 8U;
constexpr uint8_t MAX_ATTRIBUTES = 8U;

// Initialize used ThingsBoard APIs
Provision<> prov;
Client_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_REQUEST> TB_client_rpc;
Server_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_RESPONSE> TB_server_rpc;
Attribute_Request<MAX_ATTRIBUTE_REQUESTS, MAX_ATTRIBUTES> TB_attribute_request;
Shared_Attribute_Update<MAX_SHARED_ATTRIBUTES_UPDATE, MAX_ATTRIBUTES> TB_shared_update;

// Provisioning related constants
constexpr char CREDENTIALS_TYPE[] = "credentialsType";
constexpr char CREDENTIALS_VALUE[] = "credentialsValue";
constexpr char CLIENT_ID[] = "clientId";
constexpr char CLIENT_PASSWORD[] = "password";
constexpr char CLIENT_USERNAME[] = "userName";
constexpr char ACCESS_TOKEN_CRED_TYPE[] = "ACCESS_TOKEN";
constexpr char MQTT_BASIC_CRED_TYPE[] = "MQTT_BASIC";
constexpr char X509_CERTIFICATE_CRED_TYPE[] = "X509_CERTIFICATE";

// Server Side RPC related constants
constexpr char RPC_SWITCH_SET_METHOD[] = "switch_set";

// Shared Attribute Update related constants
constexpr char SWITCH_STATE_0_KEY[] = "switch_state_0";
constexpr char SWITCH_STATE_1_KEY[] = "switch_state_1";
constexpr char SWITCH_STATE_2_KEY[] = "switch_state_2";
constexpr char SWITCH_STATE_3_KEY[] = "switch_state_3";
constexpr char SWITCH_STATE_4_KEY[] = "switch_state_4";
constexpr char SWITCH_STATE_5_KEY[] = "switch_state_5";

const std::array<IAPI_Implementation*, 5U> APIs = {&prov, &TB_client_rpc, &TB_server_rpc,
                                                   &TB_attribute_request, &TB_shared_update};

// Initialize ThingsBoard instance with the maximum needed buffer size and stack size
// APIs are registered on main code
ThingsBoard ThingsBoard_client(MQTT_client, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
                               Default_Max_Stack_Size, APIs.begin(), APIs.end());

// Statuses for provisioning and subscribing
bool provisionRequestSent = false;
bool clientRpcRequested = false;
bool serverRpcSubscribed = false;
bool sharedAttributeSubscribed = false;
bool sharedAttributeRequested = false;

// Struct for client connecting after provisioning
struct Credentials {
    std::string client_id;
    std::string username;
    std::string password;
} credentials;

unsigned long _lastConnectAttempt = 0;
uint8_t _thingsBoardConnectAttempts = 0;

extern void processSwitchState(const JsonVariantConst& json, JsonDocument& response);
extern void processSharedAttributeUpdate(const JsonObjectConst& json);

bool saveThingsBoardPreferences()
{
    Serial.println("Saving device ThingsBoard preferences to flash...");
    // Open the NVS namespace in read/write mode (false)
    if (!ThingsBoard_pref.begin(PREFS_NAMESPACE, false)) {
        Serial.println("Failed to open Preferences namespace for writing");
        return false;
    }

    // Put the string value into the NVS with the defined key
    ThingsBoard_pref.putString(PREFS_DEVICE_ID, credentials.client_id.c_str());
    ThingsBoard_pref.putString(PREFS_DEVICE_USER, credentials.username.c_str());
    ThingsBoard_pref.putString(PREFS_DEVICE_PASS, credentials.password.c_str());

    // Close the preferences
    ThingsBoard_pref.end();

    Serial.println("Device credentials saved to flash.");
    return true;
}

bool loadThingsBoardPreferences()
{
    Serial.println("Loading device ThingsBoard preferences from flash...");
    // Open NVS namespace in read-only mode
    if (!ThingsBoard_pref.begin(PREFS_NAMESPACE, true)) {
        Serial.println("Failed to open Preferences namespace for reading!");
        return false;
    }

    // Load each item. The second argument ("") is the default if the key isn't found.
    String str;
    str = ThingsBoard_pref.getString(PREFS_DEVICE_ID, "");
    credentials.client_id = str.c_str();
    str = ThingsBoard_pref.getString(PREFS_DEVICE_USER, "");
    credentials.username = str.c_str();
    str = ThingsBoard_pref.getString(PREFS_DEVICE_PASS, "");
    credentials.password = str.c_str();

    // Close the preferences
    ThingsBoard_pref.end();

    return true;
}

/// @brief Provision request did not receive a response in the expected amount of microseconds
void provisionTimedOut()
{
    Serial.printf(
        "Provision request timed out did not receive a response in (%llu) microseconds. Ensure "
        "client is connected to the MQTT broker\n",
        REQUEST_TIMEOUT_MICROSECONDS);
}

/// @brief Process the provisioning response received from the server
/// @param json Reference to the object containing the provisioning response
void processProvisionResponse(const JsonDocument& json)
{
    const size_t jsonSize = Helper::Measure_Json(json);
    char buffer[jsonSize];
    serializeJson(json, buffer, jsonSize);
    Serial.printf("Received device provision response (%s)\n", buffer);

    if (strncmp(json["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
        Serial.printf("Provision response contains the error: (%s)\n",
                      json["errorMsg"].as<const char*>());
        provisionRequestSent = false;
        return;
    }

    if (strncmp(json[CREDENTIALS_TYPE], ACCESS_TOKEN_CRED_TYPE, strlen(ACCESS_TOKEN_CRED_TYPE)) ==
        0) {
        credentials.client_id = "";
        credentials.username = json[CREDENTIALS_VALUE].as<std::string>();
        credentials.password = "";
    } else if (strncmp(json[CREDENTIALS_TYPE], MQTT_BASIC_CRED_TYPE,
                       strlen(MQTT_BASIC_CRED_TYPE)) == 0) {
        auto credentials_value = json[CREDENTIALS_VALUE].as<JsonObjectConst>();
        credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
        credentials.username = credentials_value[CLIENT_USERNAME].as<std::string>();
        credentials.password = credentials_value[CLIENT_PASSWORD].as<std::string>();
    } else {
        Serial.printf("Unexpected provision credentialsType: (%s)\n",
                      json[CREDENTIALS_TYPE].as<const char*>());
        provisionRequestSent = false;
        return;
    }

    // Disconnect from the cloud client connected to the provision account, because it is no longer
    // needed the device has been provisioned and we can reconnect to the cloud with the newly
    // generated credentials.
    if (ThingsBoard_client.connected()) {
        ThingsBoard_client.disconnect();
    }

    saveThingsBoardPreferences();

    _lastConnectAttempt = 0;
    provisionRequestSent = false;
}

/// @brief Attribute request did not receive a response in the expected amount of microseconds
void requestTimedOut()
{
    if (sharedAttributeRequested) {
        Serial.printf(
            "Attribute request timed out did not receive a response in (%llu) microseconds. Ensure "
            "client is connected to the MQTT broker and that the keys actually exist on the target "
            "device\n",
            REQUEST_TIMEOUT_MICROSECONDS);
        sharedAttributeRequested = false;
    }
    if (clientRpcRequested) {
        Serial.printf(
            "Client RPC request timed out did not receive a response in (%llu) microseconds. "
            "Ensure client is connected to the MQTT broker\n",
            REQUEST_TIMEOUT_MICROSECONDS);
        clientRpcRequested = false;
    }
}

/// @brief Setup ThingsBoard device information
void ThingsBoard_setup()
{
    loadThingsBoardPreferences();

    String macUpper = WiFi.macAddress();
    macUpper.toUpperCase();
    String macUCNo = macUpper;
    macUCNo.replace(":", "");
    String macLCNo = macUCNo;
    macLCNo.toLowerCase();

    deviceName = "Smart Office - " + macUCNo;  // upper, no colons
    deviceId = "smartoffice-" + macLCNo;       // lower, no colons
    deviceMac = macUpper;                      // upper, with colons
    deviceModel = "Smart Office " + String(DEVICE_MODEL);

    currentThingsBoardConnectionStatus = ThingsBoard_client.connected();
    lastThingsBoardConnectionStatus = ThingsBoard_client.connected();
}

void ThingsBoard_connect()
{
    if (currentThingsBoardConnectionStatus) {
        return;
    }

    if (_lastConnectAttempt != 0 &&
        millis() - _lastConnectAttempt < THINGSBOARD_CONNECT_ATTEMPS_TIMOUT) {
        return;
    }
    _lastConnectAttempt = millis();

    if (!provisionRequestSent) {
        if (credentials.username.empty()) {
            serverRpcSubscribed = false;
            sharedAttributeSubscribed = false;
            sharedAttributeRequested = false;

            Serial.printf("Connecting to %s for provisioning...\n", ThingsBoard_server.c_str());
            if (!ThingsBoard_client.connect(ThingsBoard_server.c_str(), "provision",
                                            ThingsBoard_port)) {
                Serial.println("Failed to connect");
                provisionRequestSent = false;
                return;
            }

            // Provision device if provision key and secret are set
            Serial.println("Sending provisioning request");

            const Provision_Callback provisionCallback(
                Access_Token(), &processProvisionResponse, ThingsBoard_Provision_Device_Key,
                ThingsBoard_Provision_Device_Secret, deviceId.c_str(), REQUEST_TIMEOUT_MICROSECONDS,
                &provisionTimedOut);
            provisionRequestSent = prov.Provision_Request(provisionCallback);
        }
    }

    if (!credentials.username.empty()) {
        if (!ThingsBoard_client.connected()) {
            serverRpcSubscribed = false;
            sharedAttributeSubscribed = false;
            sharedAttributeRequested = false;

            // Connect to the ThingsBoard server, as the provisioned client
            Serial.printf("Connecting to %s after provision\n", ThingsBoard_server.c_str());
            if (!ThingsBoard_client.connect(
                    ThingsBoard_server.c_str(), credentials.username.c_str(), ThingsBoard_port,
                    credentials.client_id.c_str(), credentials.password.c_str())) {
                Serial.println("Failed to connect");
                _thingsBoardConnectAttempts++;
                if (_thingsBoardConnectAttempts >= THINGSBOARD_ATTEMPS_MAX) {
                    Serial.println(
                        "Max ThingsBoard connection attempts reached, "
                        "do re-provisioning...");
                    credentials.username = "";
                    _thingsBoardConnectAttempts = 0;
                }
                return;
            } else {
                // Serial.println("Connected!");
                _lastConnectAttempt = 0;
            }
        } else {
            currentThingsBoardConnectionStatus = true;

            if (!serverRpcSubscribed) {
                Serial.println("Subscribing for RPC...");
                const std::array<RPC_Callback, MAX_RPC_SUBSCRIPTIONS> callbacks = {
                    // Requires additional memory in the JsonDocument for the JsonDocument that
                    // will be copied into the response
                    RPC_Callback{RPC_SWITCH_SET_METHOD, processSwitchState},
                };
                // Perform a subscription. All consequent data processing will happen in
                // processTemperatureChange() and processSwitchChange() functions,
                // as denoted by callbacks array.
                if (!TB_server_rpc.RPC_Subscribe(callbacks.cbegin(), callbacks.cend())) {
                    Serial.println("Failed to subscribe for RPC");
                    return;
                }

                Serial.println("Subscribe done");
                serverRpcSubscribed = true;
            }

            if (!sharedAttributeSubscribed) {
                Serial.println("Subscribing for shared attribute updates...");

                // Shared attributes we want to request from the server
                constexpr std::array<const char*, MAX_ATTRIBUTES> SUBSCRIBED_SHARED_ATTRIBUTES = {
                    SWITCH_STATE_0_KEY, SWITCH_STATE_1_KEY, SWITCH_STATE_2_KEY,
                    SWITCH_STATE_3_KEY, SWITCH_STATE_4_KEY, SWITCH_STATE_5_KEY};
                const Shared_Attribute_Callback<MAX_ATTRIBUTES> callback(
                    &processSharedAttributeUpdate, SUBSCRIBED_SHARED_ATTRIBUTES);
                if (!TB_shared_update.Shared_Attributes_Subscribe(callback)) {
                    Serial.println("Failed to subscribe for shared attribute updates");
                    return;
                }

                Serial.println("Subscribe done");
                sharedAttributeSubscribed = true;
            }

            if (!sharedAttributeRequested && sharedAttributeSubscribed) {
                Serial.println("Requesting shared attributes...");

                // Shared attributes we want to request from the server
                constexpr std::array<const char*, MAX_ATTRIBUTES> REQUESTED_SHARED_ATTRIBUTES = {
                    SWITCH_STATE_0_KEY, SWITCH_STATE_1_KEY, SWITCH_STATE_2_KEY,
                    SWITCH_STATE_3_KEY, SWITCH_STATE_4_KEY, SWITCH_STATE_5_KEY};
                const Attribute_Request_Callback<MAX_ATTRIBUTES> sharedCallback(
                    &processSharedAttributeUpdate, REQUEST_TIMEOUT_MICROSECONDS, &requestTimedOut,
                    REQUESTED_SHARED_ATTRIBUTES);
                if (!TB_attribute_request.Shared_Attributes_Request(sharedCallback)) {
                    Serial.println("Failed to request shared attributes");
                    return;
                }

                Serial.println("Request done");
                sharedAttributeRequested = true;
            }

            currentThingsBoardConnectionStatus = ThingsBoard_client.connected();
        }
    }
}
#endif  // _THINGSBOARD_MANAGER_H