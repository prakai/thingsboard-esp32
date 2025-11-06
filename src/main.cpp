#include <Arduino.h>
#include <EasyButton.h>

#include "ThingsBoard_Manager.h"
#include "WiFi_Manager.h"

void WiFi_task(void* pvParameters);
void ThingsBoard_task(void* pvParameters);

//
// Buttons configuration
//
#define BUTTON_PIN 0
EasyButton* button = nullptr;

#define button_long_press_time 2000  // ms

void button_ISR_handler(void);
void button_handler_onPressed(void);
void button_handler_onPressedFor(void);

//
// Main setup
//
void setup()
{
    Serial.begin(115200);
    Serial.println();

    button = new EasyButton(BUTTON_PIN);
    button->begin();
    button->onPressed(button_handler_onPressed);
    button->onPressedFor(button_long_press_time, button_handler_onPressedFor);
    if (button->supportsInterrupt()) {
        button->enableInterrupt(button_ISR_handler);
    }

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
        vTaskDelay(500 / portTICK_PERIOD_MS);

        if (WiFi.status() != WL_CONNECTED) {
            continue;
        }

        if (!ThingsBoard_client.connected()) {
            Serial.println("Connecting to ThingsBoard");
            ThingsBoard_client.connect(ThingsBoard_server.c_str(), ThingsBoard_token.c_str(),
                                       ThingsBoard_port);
        }
        if (ThingsBoard_client.connected() != lastThingsBoardStatus) {
            if (ThingsBoard_client.connected()) {
                Serial.println("Connected to ThingsBoard");
            } else {
                Serial.println("Disconnected from ThingsBoard.");
            }
        }
        lastThingsBoardStatus = ThingsBoard_client.connected();

        ThingsBoard_client.loop();
    }
    vTaskDelete(NULL);
}

//
// Button handling
//
void button_ISR_handler(void)
{
    button->read();
}
void button_handler_onPressed(void)
{
    Serial.println("Button button has been pressed!");
}
void button_handler_onPressedFor(void)
{
    Serial.print("Button button has been pressed for ");
    Serial.print(button_long_press_time / 1000);
    Serial.println(" secs!");
}

//
// Main loop
//
void loop()
{
    if (button->supportsInterrupt()) {
        button->update();
    }

    static unsigned long _lastSentAttributes = 0;
    static unsigned long _lastSentTelemitry = 0;
    if (!ThingsBoard_client.connected()) {
        _lastSentTelemitry = 0;
    } else {
        if (_lastSentAttributes == 0) {  //|| millis() - _lastSentAttributes > 60000) {
            _lastSentAttributes = millis();
            String hwVersion = "A.0.1";
            String hwSerial = "SN-0001";
            String fwVersion = "A.0.1";
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
            ThingsBoard_client.sendAttributeData("hwVersion", hwVersion.c_str());
            ThingsBoard_client.sendAttributeData("hwSerial", hwSerial.c_str());
            ThingsBoard_client.sendAttributeData("fwVersion", fwVersion.c_str());
            ThingsBoard_client.sendAttributeData("ssid", WiFi_ssid.c_str());
            ThingsBoard_client.sendAttributeData("macAddress", String(WiFi.macAddress()).c_str());
            ThingsBoard_client.sendAttributeData("ipAddress",
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
