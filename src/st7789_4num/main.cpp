//
// Created by Shen Shuxin on 2026/6/11.
//
#include "st7789_esp.cpp"
#include "display_4_number.cpp"
#include "shoubing.cpp"
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

IPAddress local_IP(192, 168, 0, 108);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 0, 1);


static St7789Esp app;
static Display4Number display4Number;
static ShouBing shouBing;
static WiFiServer tcpServer(82);


void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println("===== starting esp32 =====");

    if (!WiFi.config(
            local_IP,
            gateway,
            subnet,
            primaryDNS))
    {
        Serial.println("STA Failed to configure");
    }
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("WiFi connecting");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    esp_wifi_set_ps(WIFI_PS_NONE);
    Serial.println();
    Serial.println(WiFi.localIP());

    tcpServer.begin();
    Serial.println("TCP server listen 82");

    Display4Number::syncNetworkTime();
    display4Number.enable();
    display4Number.setNumber(8888, 1);

    app.setup();

    Serial.println();
    Serial.println("===== running esp32 =====");

    delay(5000);
    display4Number.setTime();


}

void loop()
{
    WiFiClient client = tcpServer.available();

    if (!client)
    {
        delay(10);
        return;
    }

    client.setTimeout(2000);

    String request = client.readStringUntil('\n');

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, request);

    JsonDocument resp;

    if (err)
    {
        resp["success"] = false;
        resp["message"] = "invalid json";
    }
    else
    {
        String cmd = doc["cmd"] | "";

        if (cmd == "setNumber")
        {
            int value = doc["value"] | 0;
            int dotPos = doc["dotPos"] | -1;

            display4Number.setNumber(value, dotPos);

            resp["success"] = true;
        }
        else if (cmd == "displayEnable")
        {
            display4Number.enable();
            resp["success"] = true;
        }
        else if (cmd == "displayDisable")
        {
            display4Number.disable();
            resp["success"] = true;
        }
        else if (cmd == "displayTime")
        {
            display4Number.setTime();
            resp["success"] = true;
        }
        else if (cmd == "screenWakeup")
        {
            app.screenWakeup();
            resp["success"] = true;
        }
        else if (cmd == "screenSleep")
        {
            app.screenSleep();
            resp["success"] = true;
        }
        else if (cmd == "getDirection")
        {
            resp["success"] = true;
            resp["direction"] = static_cast<int>(shouBing.getCurDirect());
        }
        else
        {
            resp["success"] = false;
            resp["message"] = "unknown command";
        }
    }

    serializeJson(resp, client);
    client.println();

    client.stop();
}
