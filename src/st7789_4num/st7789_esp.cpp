#include <TFT_eSPI.h>
#include <LittleFS.h>

#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>

#define DEBUG

class St7789Esp {
private:
    TFT_eSPI tft;
    WiFiUDP udp;

    static constexpr uint16_t UDP_PORT = 9998;

    uint8_t udpBuffer[1500]{};
    bool isScreenDimmed = false;
    unsigned long lastPacketTime = 0;

    TaskHandle_t loopTaskHandle = nullptr;

    static void loopTaskEntry(void *param)
    {
        auto *self = static_cast<St7789Esp *>(param);

        while (true)
        {
            self->loop();
            vTaskDelay(pdMS_TO_TICKS(10));
            if (isScreenDimmed)
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }

public:
    St7789Esp() = default;

    void lcdInit();
    void showRGB565(const char* path);
    void updateRegionFast(int x, int y, int w, int h, uint16_t* data);
    void setScreenBrightness(uint8_t value);
    void screenSleep();
    void screenWakeup();
    void processPacket(uint8_t* data, int len);
    void starting_play();
    void setup();
    void loop();
};

void St7789Esp::lcdInit()
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
    Serial.println("LittleFS mounted");

}


#define BLOCK_LINES 32

void St7789Esp::showRGB565(const char* path)
{
    File file = LittleFS.open(path);

    if (!file)
    {
        Serial.println();
        Serial.print("open file failed: ");
        Serial.println(path);
        return;
    }
    Serial.print("open file success, size=");
    Serial.println(file.size());
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


void St7789Esp::updateRegionFast(
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


void St7789Esp::setScreenBrightness(uint8_t value)
{
    analogWrite(TFT_BL, value);
}

void St7789Esp::screenSleep()
{
    tft.writecommand(0x10);
}

void St7789Esp::screenWakeup()
{
    tft.writecommand(0x11);
    delay(120);
}

void St7789Esp::processPacket(uint8_t* data, int len)
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
#ifdef DEBUG
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
#endif

    tft.startWrite();
    tft.setAddrWindow(x, y, w, h);
    tft.pushPixels((uint16_t*)(data + 8), w * h);
    tft.endWrite();
}

void St7789Esp::starting_play()
{
    setScreenBrightness(255);

    constexpr int width = 240;
    constexpr int height = 240;
    constexpr int blockLines = 20;

    static uint16_t buf[width * blockLines];

    struct BootImage
    {
        const char* path;
        int delaySec;
    };

    const BootImage images[] = {
            {"/winxp_load.rgb565", 10},
            {"/winxp_welcome.rgb565", 3},
            {"/winxp_desk.rgb565", 1}
    };

    for (const auto& img : images)
    {
        File file = LittleFS.open(img.path, "r");

        if (!file)
        {
            Serial.print("open file failed: ");
            Serial.println(img.path);
            continue;
        }

        for (int y = 0; y < height; y += blockLines)
        {
            int h = min(blockLines, height - y);
            int size = width * h * 2;

            int bytesRead = file.read((uint8_t*)buf, size);
            if (bytesRead != size)
            {
                break;
            }

            tft.pushImage(0, y, width, h, buf);
        }

        file.close();

        if (strcmp(img.path, "/winxp_load.rgb565") == 0)
        {
            File loading = LittleFS.open("/winxp_loading.rgb565", "r");

            if (loading)
            {
                uint16_t loadingBuf[24 * 9];
                static uint16_t loadingBlack[87 * 9] = {0};

                loading.read((uint8_t*)loadingBuf, sizeof(loadingBuf));
                loading.close();

                for (int restart = 0; restart < 5; restart++)
                {
                    for (int loadingX = 66; loadingX < 138; loadingX += 8)
                    {
                        tft.pushImage(66, 199, 87, 9, loadingBlack);
                        tft.pushImage(loadingX, 199, 24, 9, loadingBuf);
                        delay(180);
                    }
                }
            }
        }
        else
        {
            delay(img.delaySec * 1000);
        }
    }
}



void St7789Esp::setup()
{

    lcdInit();

    starting_play();

    udp.begin(UDP_PORT);

    setScreenBrightness(255);

    lastPacketTime = millis();

    xTaskCreatePinnedToCore(
            loopTaskEntry,
            "st7789_loop",
            4096,
            this,
            1,
            &loopTaskHandle,
            1
    );

    Serial.println("st7789 UDP listen 9998");
}

static void setWifiLowMode(bool setLowFlag)
{
    WiFi.setSleep(setLowFlag);
    if (setLowFlag)
    {
        esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    }
    else
    {
        esp_wifi_set_ps(WIFI_PS_NONE);
    }

}

void St7789Esp::loop()
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
            setWifiLowMode(false);
            //关闭Wi-Fi省电模式
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
            setWifiLowMode(true);
            //设置Wi-Fi超级省电模式 max 避免esp32发热
        }

        lastPacketTime = millis();
    }
}

//static St7789Esp app;
//
//void setup()
//{
//    app.setup();
//}
//
//void loop()
//{
//    vTaskDelay(pdMS_TO_TICKS(1000));
//}
