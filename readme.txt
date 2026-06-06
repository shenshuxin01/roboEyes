配置环境变量g++的
$env:PATH = "C:\Users\Shenshuxin\Downloads\w64devkit\w64devkit\bin\;" + $env:PATH


g++ -std=c++11 test_AnimationSequences.cpp -o test_AnimationSequences.exe  -lgdi32 -luser32


g++ -std=c++11 test_MyDisplay_simple.cpp -o test_MyDisplay_simple.exe  -lgdi32 -luser32

# 这个是最全的18个表情，支持自定义屏幕尺寸，windows浮窗展示。macOS发送udp数据给esp32
g++ -std=c++11 has18_kind_eye_all_in_one_esp32.cpp -o has18_kind_eye_all_in_one_esp32.exe  -lgdi32 -luser32
