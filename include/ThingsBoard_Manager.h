#ifndef _THINGSBOARD_MANAGER_H
#define _THINGSBOARD_MANAGER_H

#include <Arduino.h>
#include <Arduino_MQTT_Client.h>
#include <Attribute_Request.h>
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

#define THINGSBOARD_CONNECT_ATTEMPS_TIMOUT 10000
#define THINGSBOARD_ATTEMPS_MAX 5

constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 1024U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 1024U;

constexpr uint64_t REQUEST_TIMEOUT_MICROSECONDS = 5000U * 1000U;

uint8_t currentThingsBoardConnectionStatus = 0;
uint8_t lastThingsBoardConnectionStatus = 0;

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
Provision<> prov;
Server_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_RESPONSE> ThingsBoard_rpc;
Attribute_Request<MAX_ATTRIBUTE_REQUESTS, MAX_ATTRIBUTES> ThingsBoard_attribute_request;
Shared_Attribute_Update<MAX_SHARED_ATTRIBUTES_UPDATE, MAX_ATTRIBUTES> ThingsBoard_shared_update;

// Provisioning related constants
constexpr char CREDENTIALS_TYPE[] = "credentialsType";
constexpr char CREDENTIALS_VALUE[] = "credentialsValue";
constexpr char CLIENT_ID[] = "clientId";
constexpr char CLIENT_PASSWORD[] = "password";
constexpr char CLIENT_USERNAME[] = "userName";
constexpr char TEMPERATURE_KEY[] = "temperature";
constexpr char HUMIDITY_KEY[] = "humidity";
constexpr char ACCESS_TOKEN_CRED_TYPE[] = "ACCESS_TOKEN";
constexpr char MQTT_BASIC_CRED_TYPE[] = "MQTT_BASIC";
constexpr char X509_CERTIFICATE_CRED_TYPE[] = "X509_CERTIFICATE";

// Server Side RPC related constants
constexpr const char RPC_POWER_SET_METHOD[] = "power_set";
// constexpr const char RPC_TEMPERATURE_METHOD[] = "example_set_temperature";
// constexpr const char RPC_SWITCH_METHOD[] = "example_set_switch";
// constexpr const char RPC_TEMPERATURE_KEY[] = "temp";
constexpr const char RPC_POWER_SET_KEY[] = "power_state";

// Shared Attribute Update related constants
constexpr char POWER_STATE_KEY[] = "power_state";
// constexpr char FW_VER_KEY[] = "fw_version";
// constexpr char FW_TITLE_KEY[] = "fw_title";
// constexpr char FW_CHKS_KEY[] = "fw_checksum";
// constexpr char FW_CHKS_ALGO_KEY[] = "fw_checksum_algorithm";
// constexpr char FW_SIZE_KEY[] = "fw_size";

const std::array<IAPI_Implementation*, 4U> APIs = {
    &prov, &ThingsBoard_rpc, &ThingsBoard_attribute_request, &ThingsBoard_shared_update};

// Initialize ThingsBoard instance with the maximum needed buffer size and stack size
// APIs are registered on main code
ThingsBoard ThingsBoard_client(MQTT_client, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE,
                               Default_Max_Stack_Size, APIs.begin(), APIs.end());

// Statuses for provisioning and subscribing
bool provisionRequestSent = false;
bool provisionResponseProcessed = false;
bool rpcSubscribed = false;
bool sharedAttributeSubscribed = false;

// Struct for client connecting after provisioning
struct Credentials {
    std::string client_id;
    std::string username;
    std::string password;
} credentials;

unsigned long _lastConnectAttempt = 0;

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
    _lastConnectAttempt = 0;
    provisionResponseProcessed = true;
}

/// @brief Processes function for RPC call "power_set"
/// JsonVariantConst is a JSON variant, that can be queried using operator[]
/// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
/// @param json Data containing the rpc data that was called and its current value
/// @param response Data containgin the response value, any number, string or json, that should be
/// sent to the cloud. Useful for getMethods
void processPowerState(const JsonVariantConst& json, JsonDocument& response)
{
    Serial.println("Received the set power method");

    // Process json
    const bool power_state = json[RPC_POWER_SET_KEY];

    Serial.print("Example power state: ");
    Serial.println(power_state);

    response.set(22.02);
}

/// @brief Update callback that will be called as soon as one of the provided shared attributes
/// changes value, if none are provided we subscribe to any shared attribute change instead
/// @param json Data containing the shared attributes that were changed and their current value
void processSharedAttributeUpdate(const JsonObjectConst& json)
{
    for (auto it = json.begin(); it != json.end(); ++it) {
        Serial.println(it->key().c_str());
        // Shared attributes have to be parsed by their type.
        Serial.println(it->value().as<const char*>());
    }

    const size_t jsonSize = Helper::Measure_Json(json);
    char buffer[jsonSize];
    serializeJson(json, buffer, jsonSize);
    Serial.println(buffer);
}

/// @brief Setup ThingsBoard device information
void ThingsBoard_setup()
{
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
        provisionResponseProcessed = false;
        rpcSubscribed = false;
        sharedAttributeSubscribed = false;

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
    } else if (provisionResponseProcessed) {
        if (!ThingsBoard_client.connected()) {
            rpcSubscribed = false;
            sharedAttributeSubscribed = false;

            // Connect to the ThingsBoard server, as the provisioned client
            Serial.printf("Connecting to %s after provision\n", ThingsBoard_server.c_str());
            if (!ThingsBoard_client.connect(
                    ThingsBoard_server.c_str(), credentials.username.c_str(), ThingsBoard_port,
                    credentials.client_id.c_str(), credentials.password.c_str())) {
                Serial.println("Failed to connect");
                return;
            } else {
                // Serial.println("Connected!");
                _lastConnectAttempt = 0;
            }
        } else {
            currentThingsBoardConnectionStatus = true;

            if (!rpcSubscribed) {
                Serial.println("Subscribing for RPC...");
                const std::array<RPC_Callback, MAX_RPC_SUBSCRIPTIONS> callbacks = {
                    // Requires additional memory in the JsonDocument for the JsonDocument that will
                    // be copied into the response
                    RPC_Callback{RPC_POWER_SET_METHOD, processPowerState},
                    // Requires additional memory in the JsonDocument for 5 key-value pairs that do
                    // not copy their value into the JsonDocument itself
                    // RPC_Callback{RPC_TEMPERATURE_METHOD, processTemperatureChange},
                    // Internal size can be 0, because if we use the JsonDocument as a JsonVariant
                    // and then set the value we do not require additional memory
                    // RPC_Callback{RPC_SWITCH_METHOD, processSwitchChange}
                };
                // Perform a subscription. All consequent data processing will happen in
                // processTemperatureChange() and processSwitchChange() functions,
                // as denoted by callbacks array.
                if (!ThingsBoard_rpc.RPC_Subscribe(callbacks.cbegin(), callbacks.cend())) {
                    Serial.println("Failed to subscribe for RPC");
                    return;
                }

                Serial.println("Subscribe done");
                rpcSubscribed = true;
            }

            if (!sharedAttributeSubscribed) {
                Serial.println("Subscribing for shared attribute updates...");

                // Shared attributes we want to request from the server
                constexpr std::array<const char*, MAX_ATTRIBUTES> SUBSCRIBED_SHARED_ATTRIBUTES = {
                    POWER_STATE_KEY};
                const Shared_Attribute_Callback<MAX_ATTRIBUTES> callback(
                    &processSharedAttributeUpdate, SUBSCRIBED_SHARED_ATTRIBUTES);
                if (!ThingsBoard_shared_update.Shared_Attributes_Subscribe(callback)) {
                    Serial.println("Failed to subscribe for shared attribute updates");
                    return;
                }

                Serial.println("Subscribe done");
                sharedAttributeSubscribed = true;
            }
        }
    }
}
#endif  // _THINGSBOARD_MANAGER_H