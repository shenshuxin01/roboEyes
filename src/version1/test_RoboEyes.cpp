#include "MyDisplay.h"
#include "FluxGarage_RoboEyes.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== MyDisplay with RoboEyes Test ===\n";
    std::cout << "Press Ctrl+C to exit\n\n";

    // 创建显示和机器人眼睛
    MyDisplay display(128, 64);
    RoboEyes<MyDisplay> roboEyes(display);

    // 初始化
    roboEyes.begin(128, 64, 30);
    roboEyes.setAutoblinker(ON, 3, 2);
    roboEyes.setIdleMode(ON, 2, 2);
    roboEyes.setMood(HAPPY);

    // 主循环
    while (true) {
        roboEyes.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    return 0;
}
