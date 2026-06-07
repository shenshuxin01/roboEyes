//
// Created by Shen Shuxin on 2026/6/7.
//手柄
const int VRx = 35;
const int VRy = 34;

//#include "display_4_number.h"
//https://docs.geeksman.com/esp32/Arduino/08.esp32-arduino-4-digits-7segment.html
// 定义位选线引脚
int seg_1 = 2;
int seg_2 = 19;
int seg_3 = 4;
int seg_4 = 21;

// 定义位选线数组
int seg_array[4] = {seg_1, seg_2, seg_3, seg_4};

// 定义段选线引脚;
int a = 16;
int b = 23;
int c = 25;
int d = 12;
int e = 13;
int f = 18;
int g = 32;
int dp = 26;

// 定义位选线引脚
int led_array[8] = {a, b, c, d, e, f, g, dp};

// 定义共阴极数码管不同数字对应的逻辑电平的二维数组
int logic_array[10][8] = {
        //a, b, c, d, e, f, g, dp
        {1, 1, 1, 1, 1, 1, 0, 0}, // 0
        {0, 1, 1, 0, 0, 0, 0, 0}, // 1
        {1, 1, 0, 1, 1, 0, 1, 0}, // 2
        {1, 1, 1, 1, 0, 0, 1, 0}, // 3
        {0, 1, 1, 0, 0, 1, 1, 0}, // 4
        {1, 0, 1, 1, 0, 1, 1, 0}, // 5
        {1, 0, 1, 1, 1, 1, 1, 0}, // 6
        {1, 1, 1, 0, 0, 0, 0, 0}, // 7
        {1, 1, 1, 1, 1, 1, 1, 0}, // 8
        {1, 1, 1, 1, 0, 1, 1, 0}, // 9
};

// 延时时间
int count = 355;


// 清屏函数
void clear() {
    for (int i=0;i<4;i++) {
        digitalWrite(seg_array[i], HIGH);
    }

    for (int i=0;i<8;i++) {
        digitalWrite(led_array[i], LOW);
    }
}

// 显示数字的函数
void display_number(int order, int number) {

    // 清屏
    clear();

    // 把对应位选线的电平拉低
    digitalWrite(seg_array[order], LOW);

    // 显示数字
    for (int i=0;i<8;i++) {
        digitalWrite(led_array[i], logic_array[number][i]);
    }
}

// 4 位数码管显示函数
void display_4_number(int number, int dotPos) {
    // 把输入的数字格式化为 4 位数的数组
    if (number < 10000) {
        // 获取每一位对应的数字
//    // 获取个位
//    int seg_4_number = number % 10;
//    number /= 10;
//
//    // 获取十位
//    int seg_3_number = number % 10;
//    number /= 10;
//
//    // 获取百位
//    int seg_2_number = number % 10;
//    number /= 10;
//
//    // 获取千位
//    int seg_1_number = number % 10;

        // 定义格式化数组
        int number_array[4];

        // 使用循环获取格式化数组
        for (int i=3;i>=0;i--) {
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

            delay(5);
        }
    }
}

void setup() {
    // 设置所有位选线引脚为输出模式，初始化所有位选线引脚为高电平
    for (int i=0;i<4;i++) {
        pinMode(seg_array[i], OUTPUT);
        digitalWrite(seg_array[i], HIGH);
    }

    // 设置所有段选线引脚为输出模式，初始化所有段选线引脚为低电平
    for (int i=0;i<8;i++) {
        pinMode(led_array[i], OUTPUT);
        digitalWrite(led_array[i], LOW);
    }
    Serial.begin(115200);
}

void loop() {
//    display_number(0, 1);
//    delay(count);
//
//    display_number(1, 2);
//    delay(count);
//
//    display_number(2, 3);
//    delay(count);
//
//    display_number(3, 4);
//    delay(count);
//
//    if (count > 10) {
//        if (count > 110) {
//            count -= 50;
//        }else {
//            count -= 10;
//        }
//    }
    int x = analogRead(VRx); // 0~4095
    int y = analogRead(VRy);
    Serial.print("X:");

    Serial.print(x);

    Serial.print(" Y:");

    Serial.println(y);

    display_4_number((x/100)*100,1);


}
