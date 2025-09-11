#include <Arduino.h>

#include "ThingsBoard_Manager.h"
#include "WiFi_Manager.h"

void WiFi_task(void *pvParameters);
void ThingsBoard_task(void *pvParameters);

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

//
// Main setup
//
void setup()
{
    Serial.begin(115200);
    Serial.println();

    // Create tasks for WiFi
    xTaskCreate(WiFi_task,   /* Task function. */
                "WiFi_task", /* String with name of task. */
                8192,        /* Stack size in bytes. */
                NULL,        /* Parameter passed as input of the task */
                1,           /* Priority of the task. */
                NULL);       /* Task handle. */

    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Create tasks for ThingsBoard
    xTaskCreate(ThingsBoard_task,   /* Task function. */
                "ThingsBoard_task", /* String with name of task. */
                8192,               /* Stack size in bytes. */
                NULL,               /* Parameter passed as input of the task */
                1,                  /* Priority of the task. */
                NULL);              /* Task handle. */
}

//
// Task of WiFi connection management
//
void WiFi_task(void *pvParameters)
{
    Serial.println("WiFi_task()");

    WiFi_setup();

    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            WiFi_connect();
        }
        if (WiFi.status() == WL_CONNECTED) {
            if (WiFi.status() != lastWiFiStatus) {
                // Serial.println();
                // Serial.println("WiFi connected.");
                // Serial.println("IP address: ");
                // Serial.println(WiFi.localIP());
            }
        }
        lastWiFiStatus = WiFi.status();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

//
// Task of ThingsBoard connection management
//
void ThingsBoard_task(void *pvParameters)
{
    Serial.println("ThingsBoard_task()");

    ThingsBoard_setup();

    ThingsBoard_client.Subscribe_API_Implementation(ThingsBoard_rpc);
    ThingsBoard_client.Subscribe_API_Implementation(ThingsBoard_attribute_request);
    ThingsBoard_client.Subscribe_API_Implementation(ThingsBoard_rpc_shared_update);
    // ThingsBoard_client.Subscribe_API_Implementations()

    for (;;) {
        vTaskDelay(500 / portTICK_PERIOD_MS);

        if (WiFi.status() != WL_CONNECTED) {
            continue;
        }

        if (!ThingsBoard_client.connected()) {
            Serial.println("Connecting to ThingsBoard...");
            ThingsBoard_client.connect(ThingsBoard_server.c_str(), ThingsBoard_token.c_str(),
                                       ThingsBoard_port);
        }
        if (ThingsBoard_client.connected() != lastThingsBoardStatus) {
            lastThingsBoardStatus = ThingsBoard_client.connected();
            if (lastThingsBoardStatus) {
                Serial.println("Connected to ThingsBoard.");
            } else {
                Serial.println("Disconnected from ThingsBoard.");
            }
        }

        ThingsBoard_client.loop();
    }
    vTaskDelete(NULL);
}

//
// Main loop
//
void loop()
{
    // put your main code here, to run repeatedly:

    static unsigned long _lastSentAttributes = 0;
    static unsigned long _lastSentTelemitry = 0;
    if (ThingsBoard_client.connected()) {
        if (_lastSentAttributes == 0) {  //|| millis() - _lastSentAttributes > 60000) {
            _lastSentAttributes = millis();
            String hw_version = "A.0.1";
            String hw_serial = "SN-0001";
            String fw_version = "A.0.1";
            Serial.print("Send attributes: ");
            Serial.print(hw_version);
            Serial.print(", ");
            Serial.print(hw_serial);
            Serial.print(", ");
            Serial.print(fw_version);
            Serial.print(", ");
            Serial.print(WiFi_ssid);
            Serial.print(", ");
            Serial.print(WiFi.macAddress());
            Serial.print(", ");
            Serial.println(WiFi.localIP());
            ThingsBoard_client.sendAttributeData("hw_version", hw_version.c_str());
            ThingsBoard_client.sendAttributeData("hw_serial", hw_serial.c_str());
            ThingsBoard_client.sendAttributeData("fw_version", fw_version.c_str());
            ThingsBoard_client.sendAttributeData("ssid", WiFi_ssid.c_str());
            ThingsBoard_client.sendAttributeData("mac_address", String(WiFi.macAddress()).c_str());
            ThingsBoard_client.sendAttributeData("ip_address",
                                                 String(WiFi.localIP().toString()).c_str());
        }
        if (_lastSentTelemitry == 0 || millis() - _lastSentTelemitry > 30000) {
            _lastSentTelemitry = millis();
            float temperature = 24.0 + (rand() % 100) / 10.0;
            float humidity = 50.0 + (rand() % 100) / 10.0;
            Serial.print("Send telemetry: ");
            Serial.print(temperature);
            Serial.print(", ");
            Serial.print(humidity);
            Serial.print(", ");
            Serial.println(WiFi.RSSI());
            ThingsBoard_client.sendTelemetryData("temperature", temperature);
            ThingsBoard_client.sendTelemetryData("humidity", humidity);
            ThingsBoard_client.sendTelemetryData("rssi", WiFi.RSSI());
        }
    }
    delay(10);
}
