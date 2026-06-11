//
// Created by Shen Shuxin on 2026/6/11.
//
#include <Arduino.h>
#include <cstdint>
#include <string>
#include <cmath>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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

class ShouBing{
private:
    static constexpr int shouBingMax=4000;
    static constexpr int shouBingMin=100;
    int defMidX;
    int defMidY;
    TaskHandle_t taskHandle = nullptr;
    std::atomic<Direction> currentDirection{NONE};
    std::atomic<bool> currentPressed{false};

    static void taskEntry(void *param) {
        auto *self = static_cast<ShouBing *>(param);
        self->taskLoop();
    }

    void taskLoop() {
        while (true) {
            Field field = getCurData();

            int delta = 500;
            Direction dir = NONE;

            if (field.x <= shouBingMin && std::abs(defMidY - field.y) < delta) {
                dir =  TOP;//上
            } else if (field.x >= shouBingMax && std::abs(defMidY - field.y) < delta) {
                dir =  BOTTOM;//下
            } else if (field.y <= shouBingMin && std::abs(defMidX - field.x) < delta) {
                dir = RIGHT;//右边
            } else if (field.y >= shouBingMax && std::abs(defMidX - field.x) < delta) {
                dir = LEFT;//左边
            } else if (field.sw) {
                dir = DOWN;
            }

            currentDirection.store(dir);
            currentPressed.store(field.sw);

            vTaskDelay(pdMS_TO_TICKS(200));
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


};

