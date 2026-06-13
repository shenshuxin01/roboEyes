//
// Created by Shen Shuxin on 2026/6/7.
//

#include <Arduino.h>
#include <time.h>
#include <atomic>

class Display4Number {
private:
    int seg_1 = 2;
    int seg_2 = 19;
    int seg_3 = 4;
    int seg_4 = 21;

    int seg_array[4] = {seg_1, seg_2, seg_3, seg_4};

    int a = 16;
    int b = 23;
    int c = 25;
    int d = 12;
    int e = 13;
    int f = 18;
    int g = 32;
    int dp = 26;

    int led_array[8] = {a, b, c, d, e, f, g, dp};

    int logic_array[10][8] = {
            {1, 1, 1, 1, 1, 1, 0, 0},
            {0, 1, 1, 0, 0, 0, 0, 0},
            {1, 1, 0, 1, 1, 0, 1, 0},
            {1, 1, 1, 1, 0, 0, 1, 0},
            {0, 1, 1, 0, 0, 1, 1, 0},
            {1, 0, 1, 1, 0, 1, 1, 0},
            {1, 0, 1, 1, 1, 1, 1, 0},
            {1, 1, 1, 0, 0, 0, 0, 0},
            {1, 1, 1, 1, 1, 1, 1, 0},
            {1, 1, 1, 1, 0, 1, 1, 0},
    };

    std::atomic<bool> enabled{false};
    std::atomic<int> currentNumber{0};
    std::atomic<int> currentDotPos{-1};
    std::atomic<bool> clockMode{false};
    int lastMinute = -1;
    // Removed:
    // uint32_t baseMillis = 0;
    // int baseHour = 0;
    // int baseMinute = 0;
    // On ESP32 Arduino, a FreeRTOS task is usually preferred over std::thread, but keep the implementation unchanged.
    TaskHandle_t refreshTaskHandle = nullptr;

    static void refreshTask(void *param) {
        auto *self = static_cast<Display4Number *>(param);
        self->refreshLoop();
    }

    void clear() {
        for (int i = 0; i < 4; i++) {
            digitalWrite(seg_array[i], HIGH);
        }

        for (int i = 0; i < 8; i++) {
            digitalWrite(led_array[i], LOW);
        }
    }

    void displayOnce(int number, int dotPos) {
        if (number < 0) {
            number = 0;
        }

        number %= 10000;

        int number_array[4];

        for (int i = 3; i >= 0; i--) {
            number_array[i] = number % 10;
            number /= 10;
        }

        for (int i = 0; i < 4; i++) {
            clear();

            digitalWrite(seg_array[i], LOW);

            for (int j = 0; j < 8; j++) {
                digitalWrite(led_array[j], logic_array[number_array[i]][j]);
            }

            if (i == dotPos) {
                digitalWrite(dp, HIGH);
            }

            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }

    void displaySinglePosition(int position, int digit, int dot = -1) {
        if (position < 0 || position > 3) {
            return;
        }

        clear();

        digitalWrite(seg_array[position], LOW);

        for (int j = 0; j < 8; j++) {
            digitalWrite(led_array[j], logic_array[digit % 10][j]);
        }

        if (position == dot) {
            digitalWrite(dp, HIGH);
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    void minuteChangeAnimation(int hh, int mm) {
        int digits[4] = {hh / 10, hh % 10, mm / 10, mm % 10};

        // 从左往右滚动两次
        for (int round = 0; round < 3; round++) {

            // 已显示的位数：1→2→3→4
            for (int visibleCount = 1; visibleCount <= 4; visibleCount++) {

                // 保持当前动画帧一段时间
                for (int repeat = 0; repeat < 60; repeat++) {

                    for (int pos = 0; pos < visibleCount; pos++) {
                        clear();

                        digitalWrite(seg_array[pos], LOW);

                        for (int j = 0; j < 8; j++) {
                            digitalWrite(
                                    led_array[j],
                                    logic_array[digits[pos]][j]
                            );
                        }

                        // 冒号（第二位后的小数点）
                        if (pos == 1) {
                            digitalWrite(dp, HIGH);
                        }

                        vTaskDelay(pdMS_TO_TICKS(5));
                    }
                }
            }
        }
    }

    void refreshLoop() {
        while (true) {
            if (enabled.load()) {
                if (clockMode.load()) {
                    struct tm timeinfo{};

                    if (getLocalTime(&timeinfo, 100)) {
                        int hh = timeinfo.tm_hour;
                        int mm = timeinfo.tm_min;

                        if (lastMinute != -1 && lastMinute != mm) {
                            minuteChangeAnimation(hh, mm);
                        }
                        lastMinute = mm;

                        displayOnce(hh * 100 + mm, 1);
                    } else {
                        displayOnce(0, 1);
                    }
                } else {
                    displayOnce(currentNumber.load(), currentDotPos.load());
                }
            } else {
                clear();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }

public:
    Display4Number() {
        for (int i = 0; i < 4; i++) {
            pinMode(seg_array[i], OUTPUT);
            digitalWrite(seg_array[i], HIGH);
        }

        for (int i = 0; i < 8; i++) {
            pinMode(led_array[i], OUTPUT);
            digitalWrite(led_array[i], LOW);
        }

        xTaskCreatePinnedToCore(
                refreshTask,
                "display4num",
                4096,
                this,
                1,
                &refreshTaskHandle,
                1
        );
    }

    static void syncNetworkTime(const char* ntpServer = "pool.ntp.org",
                         long gmtOffsetSec = 8 * 3600,
                         int daylightOffsetSec = 0) {
        configTime(gmtOffsetSec,
                   daylightOffsetSec,
                   ntpServer);
    }

    void setNumber(int number, int dotPos = -1) {
        clockMode.store(false);
        currentNumber.store(number);
        currentDotPos.store(dotPos);
    }

    void setTime() {
        clockMode.store(true);
    }

    void enable() {
        enabled.store(true);
    }

    void disable() {
        enabled.store(false);
        clear();
    }
};

//Display4Number display;
//
//void setup() {
//    display.setNumber(1234, 1);
//    display.enable();
//}
//
//void loop() {
//    delay(1000);
//}
