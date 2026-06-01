```md
$env:PATH = "C:\Users\Shenshuxin\Downloads\w64devkit\w64devkit\bin\;" + $env:PATH

g++ -std=c++11 -o test.exe test_RoboEyes.cpp -lgdi32 -luser32

g++ -std=c++11 test_MyDisplay_simple.cpp -o test_simple.exe  -lgdi32 -luser32
 
```


