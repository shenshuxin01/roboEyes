#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

TFT_eSPI tft = TFT_eSPI();

WiFiUDP udp;

constexpr uint16_t UDP_PORT = 9998;

static uint8_t udpBuffer[1500];

bool isScreenDimmed = false;
unsigned long lastPacketTime = 0;

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

void lcdInit()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    if (!LittleFS.begin(true))
    {
        Serial.println("LittleFS mount failed");
        return;
    }
}


#define BLOCK_LINES 32

void showRGB565(const char* path)
{

    File file = LittleFS.open(path);

    if (!file)
        Serial.println();
        Serial.print("open file failed: ");
        Serial.println(path);
        return;
    tft.fillScreen(TFT_BLACK);
    static uint16_t buffer[240 * BLOCK_LINES];

    int y = 0;

    while (file.available())
    {
        int lines = min(
                BLOCK_LINES,
                (240 - y)
        );

        int bytesToRead =
                240 * lines * 2;

        int bytesRead =
                file.read(
                        (uint8_t*)buffer,
                        bytesToRead
                );

        if (bytesRead <= 0)
            break;

        tft.pushImage(
                0,
                y,
                240,
                lines,
                buffer
        );

        y += lines;
    }

    file.close();
}


void updateRegionFast(
        int x,
        int y,
        int w,
        int h,
        uint16_t* data
)
{
    tft.startWrite();

    tft.setAddrWindow(
            x,
            y,
            w,
            h
    );

    tft.pushPixels(
            data,
            w * h
    );

    tft.endWrite();
}


void setScreenBrightness(uint8_t value)
{
    analogWrite(TFT_BL, value);
}

void screenSleep()
{
    tft.writecommand(0x10);
}

void screenWakeup()
{
    tft.writecommand(0x11);
    delay(120);
}

void processPacket(uint8_t* data, int len)
{
    if (len == 4 && memcmp(data, "zzzz", 4) == 0)
    {
        tft.fillScreen(TFT_BLACK);
        return;
    }

    if (len < 8)
        return;

    uint16_t x = ((uint16_t)data[0] << 8) | data[1];
    uint16_t y = ((uint16_t)data[2] << 8) | data[3];
    uint16_t w = ((uint16_t)data[4] << 8) | data[5];
    uint16_t h = ((uint16_t)data[6] << 8) | data[7];

    if (w == 0 || h == 0)
        return;

    if (x >= 240 || y >= 240)
        return;
    Serial.print("udp_receive x: ");
    Serial.print(x);
    Serial.print(" y: ");
    Serial.print(y);
    Serial.print(" w: ");
    Serial.print(w);
    Serial.print(" h: ");
    Serial.print(h);
    Serial.print(" package_len: ");
    Serial.println(len);

    tft.startWrite();
    tft.setAddrWindow(x, y, w, h);
    tft.pushPixels((uint16_t*)(data + 8), w * h);
    tft.endWrite();
}



void setup()
{
    Serial.begin(115200);

    lcdInit();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("WiFi connecting");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println(WiFi.localIP());

    showRGB565("/winxp_desk.rgb565");

    udp.begin(UDP_PORT);

    setScreenBrightness(255);

    lastPacketTime = millis();

    Serial.println("st7789 UDP listen 9998");
}

void loop()
{
    int packetSize = udp.parsePacket();

    if (packetSize > 0)
    {
        int len = udp.read(
                udpBuffer,
                sizeof(udpBuffer)
        );

        if (isScreenDimmed)
        {
            setScreenBrightness(255);
            screenWakeup();
            isScreenDimmed = false;
        }

        processPacket(
                udpBuffer,
                len
        );

        lastPacketTime = millis();
        return;
    }

    unsigned long idleMs = millis() - lastPacketTime;

    if (idleMs > 300000)
    {
        if (!isScreenDimmed)
        {
            setScreenBrightness(50);
            isScreenDimmed = true;
        }
        else
        {
            setScreenBrightness(0);
            screenSleep();
        }

        lastPacketTime = millis();
    }
}

