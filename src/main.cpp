#include <Arduino.h>
#include <EasyButton.h>
#include <Preferences.h>

#include "ThingsBoard_Manager.h"
#include "WiFi_Manager.h"

void WiFi_task(void* pvParameters);
void ThingsBoard_task(void* pvParameters);

//
// ThinkgsBoard timings
//
constexpr uint64_t THINGSBOARD_ATTRIBUTE__SEND_INTERVAL = 5 * 60 * 1000;  // 5 minute
constexpr uint64_t THINGSBOARD_TELEMETRY_SEND_INTERVAL = 30000;           // 30 seconds
//
// Buttons configuration
//
#define BUTTON_FLASH_POWER_PIN 9
// EasyButton* bt_flash_power = nullptr;
EasyButton bt_flash_power(BUTTON_FLASH_POWER_PIN);
#define BUTTON_SWITCH4_MODE_PIN 3
EasyButton bt_switch4_mode(BUTTON_SWITCH4_MODE_PIN);

constexpr uint64_t BUTTON_LONG_PRESS_TIME = 2000;  // 2 seconds

void bt_flash_power_ISR_handler(void);
void bt_flash_power_handler_onPressed(void);
void bt_flash_power_handler_onPressedFor(void);
void bt_switch4_mode_ISR_handler(void);
void bt_switch4_mode_handler_onPressed(void);
void bt_switch4_mode_handler_onPressedFor(void);

bool switch_state[6] = {false, false, false, false, false, false};

#ifdef BOARD_SUPERMINI
#define LED_BUILTIN_PIN 8
#endif

//
// Main setup
//
void setup()
{
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();

    bt_flash_power.begin();
    bt_flash_power.onPressed(bt_flash_power_handler_onPressed);
    bt_flash_power.onPressedFor(BUTTON_LONG_PRESS_TIME, bt_flash_power_handler_onPressedFor);
    if (bt_flash_power.supportsInterrupt()) {
        bt_flash_power.enableInterrupt(bt_flash_power_ISR_handler);
    }
    bt_switch4_mode.begin();
    bt_switch4_mode.onPressed(bt_switch4_mode_handler_onPressed);
    bt_switch4_mode.onPressedFor(BUTTON_LONG_PRESS_TIME, bt_switch4_mode_handler_onPressedFor);
    if (bt_switch4_mode.supportsInterrupt()) {
        bt_switch4_mode.enableInterrupt(bt_switch4_mode_ISR_handler);
    }

#ifdef BOARD_SUPERMINI
    pinMode(LED_BUILTIN_PIN, OUTPUT);
    digitalWrite(LED_BUILTIN_PIN, HIGH);  // Turn off the LED
#endif

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
void WiFi_task(void* pvParameters)
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
void ThingsBoard_task(void* pvParameters)
{
    Serial.println("ThingsBoard_task()");

    ThingsBoard_setup();

    for (;;) {
        vTaskDelay(10 / portTICK_PERIOD_MS);

        // WiFi status
        if (WiFi.status() != WL_CONNECTED) {
            currentThingsBoardConnectionStatus = false;
            continue;
        }

        // ThingsBoard connection
        if (!currentThingsBoardConnectionStatus) {
            ThingsBoard_connect();
        }
        if (currentThingsBoardConnectionStatus != lastThingsBoardConnectionStatus) {
            if (currentThingsBoardConnectionStatus) {
                Serial.println("Connected to ThingsBoard");
            } else {
                Serial.println("Disconnected from ThingsBoard.");
            }
            lastThingsBoardConnectionStatus = currentThingsBoardConnectionStatus;
        }
        if (currentThingsBoardConnectionStatus) {
            // Sent telemetry and attributes to ThingsBoard

            static unsigned long _lastSentAttributes = 0;
            static unsigned long _lastSentTelemitry = 0;
            if (!currentThingsBoardConnectionStatus) {
                _lastSentTelemitry = 0;
            } else {
                if ((_lastSentAttributes == 0 ||
                     millis() - _lastSentAttributes > THINGSBOARD_ATTRIBUTE__SEND_INTERVAL) &&
                    sharedAttributeSubscribed) {
                    _lastSentAttributes = millis();

                    String hwVersion = DEVICE_HW_VERSION;
                    String hwSerial = "SO-0001";
                    String fwVersion = DEVICE_SW_VERSION;
                    Serial.print("Send attributes: ");
                    Serial.print(hwVersion);
                    Serial.print(", ");
                    Serial.print(hwSerial);
                    Serial.print(", ");
                    Serial.print(fwVersion);
                    Serial.print(", ");
                    Serial.print(WiFi_ssid);
                    Serial.print(", ");
                    Serial.print(WiFi.macAddress());
                    Serial.print(", ");
                    Serial.println(WiFi.localIP());

                    // Send device attributes
                    ThingsBoard_client.sendAttributeData("hwVersion", hwVersion.c_str());
                    ThingsBoard_client.sendAttributeData("hwSerial", hwSerial.c_str());
                    ThingsBoard_client.sendAttributeData("fwVersion", fwVersion.c_str());
                    ThingsBoard_client.sendAttributeData("ssid", WiFi_ssid.c_str());
                    ThingsBoard_client.sendAttributeData("macAddress",
                                                         String(WiFi.macAddress()).c_str());
                    ThingsBoard_client.sendAttributeData("ipAddress",
                                                         String(WiFi.localIP().toString()).c_str());

                    Serial.println("Send switch states");

                    // Send switch states
                    ThingsBoard_client.sendAttributeData("switch_state_0", switch_state[0]);
                    ThingsBoard_client.sendAttributeData("switch_state_1", switch_state[1]);
                    ThingsBoard_client.sendAttributeData("switch_state_2", switch_state[2]);
                    ThingsBoard_client.sendAttributeData("switch_state_3", switch_state[3]);
                    ThingsBoard_client.sendAttributeData("switch_state_4", switch_state[4]);
                    ThingsBoard_client.sendAttributeData("switch_state_5", switch_state[5]);
                }

                if (_lastSentTelemitry == 0 ||
                    millis() - _lastSentTelemitry > THINGSBOARD_TELEMETRY_SEND_INTERVAL) {
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
        }

        ThingsBoard_client.loop();
    }
    vTaskDelete(NULL);
}

/// @brief Processes function for RPC call "switch_set"
/// JsonVariantConst is a JSON variant, that can be queried using operator[]
/// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
/// @param params Data containing the rpc data that was called and its current value
/// @param response Data containgin the response value, any number, string or json, that should be
/// sent to the cloud. Useful for getMethods
void processSwitchState(const JsonVariantConst& params, JsonDocument& response)
{
    Serial.println("Received the switch set method");

    JsonObjectConst json = params.as<JsonObjectConst>();

    for (JsonPairConst kv : json) {
        String key = kv.key().c_str();

        if (key.startsWith("switch_state_")) {
            bool state = kv.value().as<bool>();

            Serial.printf("Switch %s state: %s\n", key.c_str(), state ? "true" : "false");

            int i = key.substring(strlen("switch_state_")).toInt();
            switch_state[i] = state;

#ifdef BOARD_SUPERMINI
            if (i == 0) {
                digitalWrite(LED_BUILTIN_PIN, switch_state[i] ? LOW : HIGH);
            }
#endif
            ThingsBoard_client.sendAttributeData(key.c_str(), switch_state[i]);
            response.set(switch_state[i]);
        }
    }
}

/// @brief Update callback that will be called as soon as one of the provided shared attributes
/// changes value, if none are provided we subscribe to any shared attribute change instead
/// @param json Data containing the shared attributes that were changed and their current value
void processSharedAttributeUpdate(const JsonObjectConst& json)
{
    Serial.println("Received shared attribute update");
    for (auto it = json.begin(); it != json.end(); ++it) {
        String key = it->key().c_str();
        if (key.startsWith("switch_state_")) {
            Serial.printf("Switch %s state: %s\n", it->key().c_str(),
                          it->value().as<boolean>() ? "true" : "false");
            int i = key.substring(strlen("switch_state_")).toInt();
            switch_state[i] = it->value().as<boolean>();
#ifdef BOARD_SUPERMINI
            if (i == 0) {
                digitalWrite(LED_BUILTIN_PIN, switch_state[i] ? LOW : HIGH);
            }
#endif
        }
    }
}

//
// Button handling
//
void bt_flash_power_ISR_handler(void)
{
    // bt_flash_power->read();
    bt_flash_power.read();
}
void bt_flash_power_handler_onPressed(void)
{
    Serial.println("Button button has been pressed!");

    if (currentThingsBoardConnectionStatus) {
        u8_t i = 0;
        switch_state[i] = !switch_state[i];

        char sprintf_buffer[] = "switch_state_x";
        sprintf(sprintf_buffer, "switch_state_%d", i);

        Serial.printf("Send %s: ", sprintf_buffer);
        Serial.println(switch_state[i]);
        ThingsBoard_client.sendAttributeData(sprintf_buffer, switch_state[i]);
    }
}
void bt_flash_power_handler_onPressedFor(void)
{
    Serial.print("Button button has been pressed for ");
    Serial.print(BUTTON_LONG_PRESS_TIME / 1000);
    Serial.println(" secs!");
}
void bt_switch4_mode_ISR_handler(void)
{
    bt_switch4_mode.read();
}
void bt_switch4_mode_handler_onPressed(void)
{
    Serial.println("Button button has been pressed!");

    if (currentThingsBoardConnectionStatus) {
        u8_t i = 1;
        switch_state[i] = !switch_state[i];

        char sprintf_buffer[] = "switch_state_x";
        sprintf(sprintf_buffer, "switch_state_%d", i);

        Serial.printf("Send %s: ", sprintf_buffer);
        Serial.println(switch_state[i]);
        ThingsBoard_client.sendAttributeData(sprintf_buffer, switch_state[i]);
    }
}
void bt_switch4_mode_handler_onPressedFor(void)
{
    Serial.print("Button button has been pressed for ");
    Serial.print(BUTTON_LONG_PRESS_TIME / 1000);
    Serial.println(" secs!");
}

//
// Main loop
//
void loop()
{
    if (bt_flash_power.supportsInterrupt()) {
        bt_flash_power.update();
    }
    if (bt_switch4_mode.supportsInterrupt()) {
        bt_switch4_mode.update();
    }

    delay(10);
}
