#ifndef MY_DISPLAY_H
#define MY_DISPLAY_H

#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <math.h>
#endif

class MyDisplay {
private:
    int _width;
    int _height;
    uint8_t* _buffer;
    uint8_t* _lastSentBuffer;
#ifdef __APPLE__
    int _udpSock;
    sockaddr_in _udpAddr;
#endif
#ifdef _WIN32
    HWND _hwnd;
    HDC _hdc;
    HBITMAP _hbitmap;
    BITMAPINFO _bmi;
#endif

public:
    MyDisplay(int width, int height) : _width(width), _height(height) {
        _buffer = new uint8_t[width * height]();
        _lastSentBuffer = new uint8_t[width * height]();
#ifdef __APPLE__
        _udpSock = socket(AF_INET, SOCK_DGRAM, 0);

        memset(&_udpAddr, 0, sizeof(_udpAddr));
        _udpAddr.sin_family = AF_INET;
        _udpAddr.sin_port = htons(9998);
        inet_pton(AF_INET, "192.168.0.107", &_udpAddr.sin_addr);

#endif
#ifdef _WIN32
        _hwnd = NULL;
        _hdc = NULL;
        _hbitmap = NULL;
        initWindow();
#endif
    }

    ~MyDisplay() {
        delete[] _buffer;
        delete[] _lastSentBuffer;
#ifdef __APPLE__
        if (_udpSock >= 0) close(_udpSock);
#endif
#ifdef _WIN32
        if (_hbitmap) DeleteObject(_hbitmap);
        if (_hdc) ReleaseDC(_hwnd, _hdc);
        if (_hwnd) DestroyWindow(_hwnd);
#endif
    }

    int width() const { return _width; }
    int height() const { return _height; }
    uint8_t getPixel(int x, int y) const {
        if (x < 0 || x >= _width || y < 0 || y >= _height) return 0;
        return _buffer[y * _width + x];
    }

    void clearDisplay() {
        for (int i = 0; i < _width * _height; i++) {
            _buffer[i] = 0;
        }
    }

    void display() {
#ifdef _WIN32
        if (_hwnd && _hdc) {
            // 创建显示缓冲区（放大显示，便于观察）
            const int scale = 4;
            int displayWidth = _width * scale;
            int displayHeight = _height * scale;

            // 创建临时位图
            HDC memDC = CreateCompatibleDC(_hdc);
            HBITMAP tempBmp = CreateCompatibleBitmap(_hdc, displayWidth, displayHeight);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, tempBmp);

            // 绘制背景（黑色）
            RECT rect = {0, 0, displayWidth, displayHeight};
            FillRect(memDC, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

            // 绘制像素（白色）
            for (int y = 0; y < _height; y++) {
                for (int x = 0; x < _width; x++) {
                    if (_buffer[y * _width + x]) {
                        // 放大像素
                        RECT pixelRect = {
                            x * scale, y * scale,
                            (x + 1) * scale, (y + 1) * scale
                        };
                        FillRect(memDC, &pixelRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
                    }
                }
            }

            // 复制到窗口
            BitBlt(_hdc, 0, 0, displayWidth, displayHeight, memDC, 0, 0, SRCCOPY);

            // 清理
            SelectObject(memDC, oldBmp);
            DeleteObject(tempBmp);
            DeleteDC(memDC);

            // 更新窗口
            UpdateWindow(_hwnd);

            // 处理消息，防止窗口卡死
            MSG msg;
            while (PeekMessage(&msg, _hwnd, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
#elif defined(__APPLE__)
        sendRGB565Frame();
#else
        printf("\nDisplay (%dx%d):\n", _width, _height);
        for (int y = 0; y < _height; y++) {
            for (int x = 0; x < _width; x++) {
                printf("%c", _buffer[y * _width + x] ? '#' : ' ');
            }
            printf("\n");
        }
#endif
    }

    void fillRoundRect(int x, int y, int w, int h, int r, uint8_t color) {
        if (r <= 0) {
            fillRect(x, y, w, h, color);
            return;
        }

        if (r > w / 2) r = w / 2;
        if (r > h / 2) r = h / 2;

        for (int yy = 0; yy < h; yy++) {

            int dx = 0;

            if (yy < r) {
                int dy = r - yy - 1;
                dx = (int)(r - sqrt((double)r * r - (double)dy * dy));
            }
            else if (yy >= h - r) {
                int dy = yy - (h - r);
                dx = (int)(r - sqrt((double)r * r - (double)dy * dy));
            }

            drawHLine(x + dx,
                      y + yy,
                      w - dx * 2,
                      color);
        }
    }

    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {

        int minY = y0;
        if (y1 < minY) minY = y1;
        if (y2 < minY) minY = y2;

        int maxY = y0;
        if (y1 > maxY) maxY = y1;
        if (y2 > maxY) maxY = y2;

        for (int y = minY; y <= maxY; y++) {

            int nodes[3];
            int count = 0;

            auto intersect = [&](int xa, int ya, int xb, int yb) {
                if ((ya < y && yb >= y) ||
                    (yb < y && ya >= y)) {

                    nodes[count++] = xa +
                        (y - ya) * (xb - xa) / (yb - ya);
                }
            };

            intersect(x0, y0, x1, y1);
            intersect(x1, y1, x2, y2);
            intersect(x2, y2, x0, y0);

            if (count < 2)
                continue;

            if (nodes[0] > nodes[1]) {
                int t = nodes[0];
                nodes[0] = nodes[1];
                nodes[1] = t;
            }

            drawHLine(nodes[0],
                      y,
                      nodes[1] - nodes[0] + 1,
                      color);
        }
    }

private:
#ifdef _WIN32
    void initWindow() {
        const int scale = 4;
        int windowWidth = _width * scale;
        int windowHeight = _height * scale;

        // 注册窗口类
        WNDCLASSA wc = {0};
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (msg == WM_DESTROY) {
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        };
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "MyDisplayWindow";
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

        RegisterClassA(&wc);

        // 创建窗口
        _hwnd = CreateWindowExA(
            0, "MyDisplayWindow", "MyDisplay",
            WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT,
            windowWidth + GetSystemMetrics(SM_CXFRAME) * 2,
            windowHeight + GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
            NULL, NULL, GetModuleHandleA(NULL), NULL
        );

        if (_hwnd) {
            ShowWindow(_hwnd, SW_SHOW);
            _hdc = GetDC(_hwnd);
        }
    }
#endif

    void fillRect(int x, int y, int w, int h, uint8_t color) {
        for (int i = 0; i < h; i++) {
            drawHLine(x, y + i, w, color);
        }
    }

    void drawHLine(int x, int y, int w, uint8_t color) {
        if (y < 0 || y >= _height) return;
        for (int i = 0; i < w; i++) {
            int px = x + i;
            if (px >= 0 && px < _width) {
                _buffer[y * _width + px] = color;
            }
        }
    }

#ifdef __APPLE__
    void sendRGB565Frame() {
        const uint16_t x = 0;
        const uint16_t y = 0;
        const uint16_t w = _width;
        const uint16_t h = _height;

        const int frameSize = _width * _height;

        if (memcmp(_buffer,
                   _lastSentBuffer,
                   frameSize) == 0) {
            return;
        }

        memcpy(_lastSentBuffer,
               _buffer,
               frameSize);

        const int pixelCount = w * h;
        const int packedSize = (pixelCount + 7) / 8;

        printf("\n\n======\nw:%d, h:%d, packedSize:%d",w,h,packedSize);

        uint8_t* packed = new uint8_t[packedSize];
        memset(packed, 0, packedSize);

        for (int i = 0; i < pixelCount; i++) {
            if (_buffer[i]) {
                packed[i >> 3] |= (1 << (7 - (i & 7)));
            }
        }

        const int MAX_PACKET_BYTES = 1440;
        const int bytesPerLine = w * 2;
        const int maxLines = MAX_PACKET_BYTES / bytesPerLine;

        if (packedSize <= MAX_PACKET_BYTES) {

            uint8_t packet[9 + packedSize];

            packet[0] = 0x42;

            packet[1] = x >> 8;
            packet[2] = x & 0xFF;
            packet[3] = y >> 8;
            packet[4] = y & 0xFF;
            packet[5] = w >> 8;
            packet[6] = w & 0xFF;
            packet[7] = h >> 8;
            packet[8] = h & 0xFF;

            memcpy(packet + 9, packed, packedSize);
            printf("%d*%d,%lu\n",w,h,sizeof(packet));
            sendto(_udpSock,
                   packet,
                   sizeof(packet),
                   0,
                   (sockaddr*)&_udpAddr,
                   sizeof(_udpAddr));
        } else {
            // 位压缩后通常单包即可发送
            // 如果未来尺寸增大，再重新设计按位拆包协议
            printf("bit-packed frame too large\n");
        }

        delete[] packed;
    }
#endif
};

#endif
