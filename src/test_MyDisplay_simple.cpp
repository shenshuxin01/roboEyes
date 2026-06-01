#include "MyDisplay.h"
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <chrono>

// 保存为 PBM 格式图像文件
void savePBM(MyDisplay& display, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("Error: Cannot open %s for writing\n", filename);
        return;
    }
    
    // PBM 格式头部
    fprintf(fp, "P4\n");
    fprintf(fp, "%d %d\n", display.width(), display.height());
    
    // 写入像素数据（每字节8像素）
    int bytesPerRow = (display.width() + 7) / 8;
    unsigned char* rowBuffer = (unsigned char*)malloc(bytesPerRow);
    
    for (int y = 0; y < display.height(); y++) {
        memset(rowBuffer, 0, bytesPerRow);
        for (int x = 0; x < display.width(); x++) {
            if (display.getPixel(x, y)) {
                int byteIdx = x / 8;
                int bitIdx = 7 - (x % 8);
                rowBuffer[byteIdx] |= (1 << bitIdx);
            }
        }
        fwrite(rowBuffer, 1, bytesPerRow, fp);
    }
    
    free(rowBuffer);
    fclose(fp);
    printf("Saved: %s\n", filename);
}

int main() {
    printf("=== MyDisplay Function Test ===\n\n");
    
    // 创建显示缓冲区
    MyDisplay display(128, 64);
    
    // 测试 clearDisplay
    printf("1. Testing clearDisplay()...\n");
    display.clearDisplay();
    
    // 测试 fillRoundRect - 绘制左眼
    printf("2. Testing fillRoundRect() - Left Eye...\n");
    display.fillRoundRect(10, 14, 36, 36, 8, 1);
    display.display();
    savePBM(display, "test_output1.pbm");
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // 测试 fillRoundRect - 绘制右眼
    printf("3. Testing fillRoundRect() - Right Eye...\n");
    display.fillRoundRect(82, 14, 36, 36, 8, 1);
    display.display();
    savePBM(display, "test_output2.pbm");
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    
    // 测试 fillTriangle - 左眼疲劳眼皮
    printf("4. Testing fillTriangle() - Left Tired Eyelid...\n");
    display.fillTriangle(10, 14, 46, 14, 10, 32, 0);
    display.display();
    savePBM(display, "test_output3.pbm");
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    
    // 测试 fillTriangle - 右眼疲劳眼皮
    printf("5. Testing fillTriangle() - Right Tired Eyelid...\n");
    display.fillTriangle(82, 14, 118, 14, 118, 32, 0);
    display.display();
    savePBM(display, "test_output4.pbm");
    std::this_thread::sleep_for(std::chrono::milliseconds(30000));
    
    // 保存测试结果
    printf("\n6. Saving test image...\n");
    savePBM(display, "test_output.pbm");
    
    printf("\n=== Test Complete ===\n");
    printf("Open 'test_output.pbm' with an image viewer (e.g., IrfanView, GIMP)\n");
    
    return 0;
}
