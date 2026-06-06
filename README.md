```md
$env:PATH = "C:\Users\Shenshuxin\Downloads\w64devkit\w64devkit\bin\;" + $env:PATH

g++ -std=c++11 -o test.exe test_RoboEyes.cpp -lgdi32 -luser32

g++ -std=c++11 test_MyDisplay_simple.cpp -o test_simple.exe  -lgdi32 -luser32
 
```

## 项目结构
- `version1`目录是简单实现，可忽略
- `version2`是oled动画眼实现逻辑
- `horza_forizon5`是地平线5脚本

