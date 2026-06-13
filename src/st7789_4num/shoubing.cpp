//
// Created by Shen Shuxin on 2026/6/11.
//
#include <Arduino.h>
#include <cstdint>
#include <string>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFiUdp.h>

//#define DEBUG
const int VRx = 35;
const int VRy = 34;
const int SW  = 39;

//void loop() {
//
//    int x = analogRead(VRx); // 0~4095
//    int y = analogRead(VRy);
//
//    int sw = digitalRead(SW); // 0=按下
//
//    Serial.print("X:");
//    Serial.print(x);
//
//    Serial.print(" Y:");
//    Serial.print(y);
//
//    Serial.print(" SW:");
//    Serial.println(sw == 0 ? "PRESSED" : "UP");
//
//    delay(100);
//}

struct Field {
    int x;
    int y;
    int sw;
};
enum Direction {
    RIGHT=1,LEFT=2,BOTTOM=3,TOP=4,DOWN=5,NONE=0
};

struct Strength {
    float x; // X轴力度：负数表示负方向，正数表示正方向，范围[-1.0, 1.0]
    float y; // Y轴力度：负数表示负方向，正数表示正方向，范围[-1.0, 1.0]
};

class ShouBing{
private:
    static constexpr int shouBingMax=4000;
    static constexpr int shouBingMin=100;
    int defMidX;
    int defMidY;
    TaskHandle_t taskHandle = nullptr;
    std::atomic<Direction> currentDirection{NONE};
    std::atomic<bool> currentPressed{false};
    static constexpr float DEAD_ZONE = 0.1f;
    static constexpr const char *VOLUME_IP = "192.168.0.111";
    static constexpr uint16_t VOLUME_PORT = 5556;
    uint32_t lastVolumeTime = 0;
    uint32_t lastTrackTime = 0;
    WiFiUDP udp;

    static void taskEntry(void *param) {
        auto *self = static_cast<ShouBing *>(param);
        self->taskLoop();
    }

    void sendCommand(const String &cmd) {
        udp.beginPacket(VOLUME_IP, VOLUME_PORT);
        udp.print(cmd);
        udp.endPacket();
    }

    void taskLoop() {
        while (true) {
            Field field = getCurData();
            Strength strength = calcStrength(field);

            Direction dir = NONE;
            uint32_t now = millis();

            if (std::fabs(strength.x) >= DEAD_ZONE) {
                bool volumeUp = strength.x < 0;
                dir = volumeUp ? TOP : BOTTOM;

                float s = std::fabs(strength.x);
                uint32_t interval;
//                力度	间隔	每秒次数
//                5% ~ 20%	800ms	1.25 次
//                20% ~ 40%	500ms	2 次
//                40% ~ 70%	250ms	4 次
//                70% ~ 100%	100ms	10 次
//                效果类似：
//                轻推：慢慢调音量
//                重推：快速调音量
                if (s < 0.2f)
                    interval = 800;
                else if (s < 0.4f)
                    interval = 500;
                else if (s < 0.7f)
                    interval = 250;
                else
                    interval = 100;

                if (now - lastVolumeTime >= interval) {
                    sendCommand(
                        String("volume,") +
                        (volumeUp ? "1" : "-1")
                    );
                    lastVolumeTime = now;
                }
            } else if (std::fabs(strength.y) >= DEAD_ZONE+0.7f) {
                bool nextTrack = strength.y < 0;
                dir = nextTrack ? RIGHT : LEFT;

                if (now - lastTrackTime >= 1000) {
                    sendCommand(
                        nextTrack ? "track,next" : "track,prev"
                    );
                    lastTrackTime = now;
                }
            } else if (field.sw) {
                dir = DOWN;
            }

            currentDirection.store(dir);
            currentPressed.store(field.sw);

            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
public:
    ShouBing(int midX, int midY)
        : defMidX(midX), defMidY(midY) {
        pinMode(SW, INPUT);

        xTaskCreatePinnedToCore(
                taskEntry,
                "shoubing",
                4096,
                this,
                1,
                &taskHandle,
                1
        );
    }
    ShouBing(): ShouBing(1900,1900){}

    static Field getCurData(){
        int x = analogRead(VRx); // 0~4095
        int y = analogRead(VRy);
        int sw = analogRead(SW); // 0=按下
        Field field{};
        field.x=x;
        field.y=y;
        field.sw= sw==0;
#ifdef DEBUG
        Serial.print("X:");
        Serial.print(x);

        Serial.print(" Y:");
        Serial.print(y);

        Serial.print(" SW:");
        Serial.println(sw);
#endif
        return field;
    }

    Direction getCurDirect() {
        return currentDirection.load();
    }

    bool isPressed() {
        return currentPressed.load();
    }

    Strength calcStrength(const Field &field) {
        Strength strength{};

        if (field.x >= defMidX) {
            strength.x = (field.x - defMidX) /
                         static_cast<float>(shouBingMax - defMidX);
        } else {
            strength.x = (field.x - defMidX) /
                         static_cast<float>(defMidX - shouBingMin);
        }

        if (field.y >= defMidY) {
            strength.y = (field.y - defMidY) /
                         static_cast<float>(shouBingMax - defMidY);
        } else {
            strength.y = (field.y - defMidY) /
                         static_cast<float>(defMidY - shouBingMin);
        }

        strength.x = std::clamp(strength.x, -1.0f, 1.0f);
        strength.y = std::clamp(strength.y, -1.0f, 1.0f);

        return strength;
    }

    Strength getStrength() {
        return calcStrength(getCurData());
    }


};
