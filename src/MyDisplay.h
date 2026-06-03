
#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#endif


class MyDisplay {
private:
    int _width;
    int _height;
    uint8_t* _buffer;
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

    // 脏区域跟踪 - 本次刷新区域 (x1, y1)左上角, (x2, y2)右下角
    int _dirtyX1 = 0, _dirtyY1 = 0, _dirtyX2 = 0, _dirtyY2 = 0;
    // 上次刷新区域（用于比较）
    int _lastDirtyX1 = 0, _lastDirtyY1 = 0, _lastDirtyX2 = 240, _lastDirtyY2 = 120;

    void updateDirtyRegion(int x1, int y1, int x2, int y2) {
        if (x2 <= x1 || y2 <= y1) return;
        if (_dirtyX2 == 0 && _dirtyY2 == 0) {
            _dirtyX1 = x1;
            _dirtyY1 = y1;
            _dirtyX2 = x2;
            _dirtyY2 = y2;
        } else {
            _dirtyX1 = std::min(_dirtyX1, x1);
            _dirtyY1 = std::min(_dirtyY1, y1);
            _dirtyX2 = std::max(_dirtyX2, x2);
            _dirtyY2 = std::max(_dirtyY2, y2);
        }
    }

public:
    MyDisplay(int width, int height) : _width(width), _height(height) {
        _buffer = new uint8_t[width * height]();
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
        // 重置本次脏区域为初始值（全屏）
        // _dirtyX1 = 0;
        // _dirtyY1 = 0;
        // _dirtyX2 = _width;
        // _dirtyY2 = _height;
    }

    void display() {

        // 刷新区域逻辑：直接取本次和上次区域的并集
        int refreshX1, refreshY1, refreshX2, refreshY2;

        if (_dirtyX2 > 0 && _dirtyY2 > 0 && _lastDirtyX2 > 0 && _lastDirtyY2 > 0) {
            // 上次和本次都有脏区域，取并集
            refreshX1 = std::min(_dirtyX1, _lastDirtyX1);
            refreshY1 = std::min(_dirtyY1, _lastDirtyY1);
            refreshX2 = std::max(_dirtyX2, _lastDirtyX2);
            refreshY2 = std::max(_dirtyY2, _lastDirtyY2);
        } else if (_dirtyX2 > 0 && _dirtyY2 > 0) {
            // 只有本次有脏区域
            refreshX1 = _dirtyX1;
            refreshY1 = _dirtyY1;
            refreshX2 = _dirtyX2;
            refreshY2 = _dirtyY2;
        } else if (_lastDirtyX2 > 0 && _lastDirtyY2) {
            // 只有上次有脏区域
            refreshX1 = _lastDirtyX1;
            refreshY1 = _lastDirtyY1;
            refreshX2 = _lastDirtyX2;
            refreshY2 = _lastDirtyY2;
        } else {
            // 默认使用全屏
            refreshX1 = 0;
            refreshY1 = 0;
            refreshX2 = _width;
            refreshY2 = _height;
        }

        int refreshW = refreshX2 - refreshX1;
        int refreshH = refreshY2 - refreshY1;
        const int scale = 1;
        int displayX = refreshX1 * scale;
        int displayY = refreshY1 * scale;
        int displayWidth = refreshW * scale;
        int displayHeight = refreshH * scale;
#ifdef _WIN32
        SYSTEMTIME st;
        GetLocalTime(&st);
        printf("\n[%02d:%02d:%02d.%03d] Display (%dx%d) - Dirty: (%d,%d,%d,%d), Last: (%d,%d,%d,%d), Refresh: (%d,%d,%d,%d)\n",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, _width, _height,
               _dirtyX1, _dirtyY1, _dirtyX2, _dirtyY2,
               _lastDirtyX1, _lastDirtyY1, _lastDirtyX2, _lastDirtyY2,
               refreshX1, refreshY1, refreshX2, refreshY2);

        if (_hwnd && _hdc) {

            HDC memDC = CreateCompatibleDC(_hdc);
            HBITMAP tempBmp = CreateCompatibleBitmap(_hdc, displayWidth, displayHeight);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, tempBmp);

            RECT rect = {0, 0, displayWidth, displayHeight};
            FillRect(memDC, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

            for (int y = refreshY1; y < refreshY2; y++) {
                for (int x = refreshX1; x < refreshX2; x++) {
                    if (y >= 0 && y < _height && x >= 0 && x < _width && _buffer[y * _width + x]) {
                        RECT pixelRect = {
                            (x - refreshX1) * scale, (y - refreshY1) * scale,
                            (x - refreshX1 + 1) * scale, (y - refreshY1 + 1) * scale
                        };
                        FillRect(memDC, &pixelRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
                    }
                }
            }

            BitBlt(_hdc, displayX, displayY, displayWidth, displayHeight, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(tempBmp);
            DeleteDC(memDC);

            RECT updateRect = {displayX, displayY, displayX + displayWidth, displayY + displayHeight};
            InvalidateRect(_hwnd, &updateRect, FALSE);
            UpdateWindow(_hwnd);

            MSG msg;
            while (PeekMessage(&msg, _hwnd, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
#elif __APPLE__
        // =============================
        // macOS UDP显示逻辑
        // 与 Win32 保持一致：
        // 1. 使用 refreshX1/refreshY1/refreshW/refreshH
        // 2. 只发送脏区域
        // 3. 位图采用 1bit BitPacked 格式
        // =============================

        uint16_t x = refreshX1;
        uint16_t y = refreshY1;

        // ESP32 解码要求宽度必须是 8 的倍数
        uint16_t w = (refreshW + 7) & ~7;
        uint16_t h = refreshH;

        // 没有脏区域时直接返回
        if (w > 0 && h > 0)
        {
            const int bytesPerRow = w / 8;

            // BitPacked 数据
            std::vector<uint8_t> packed(bytesPerRow * h, 0);

            // 将脏区域压缩为 1bit 位图
            for (int row = 0; row < h; row++)
            {
                for (int byteIndex = 0;
                     byteIndex < bytesPerRow;
                     byteIndex++)
                {
                    uint8_t value = 0;

                    for (int bit = 0; bit < 8; bit++)
                    {
                        int srcX = refreshX1 + byteIndex * 8 + bit;
                        int srcY = refreshY1 + row;

                        if (srcX >= _width || srcY >= _height)
                        {
                            continue;
                        }

                        if (_buffer[srcY * _width + srcX])
                        {
                            value |= (0x80 >> bit);
                        }
                    }

                    packed[row * bytesPerRow + byteIndex] = value;
                }
            }

            // UDP最大负载
            constexpr int HEADER_SIZE = 9;
            constexpr int MAX_PACKET_SIZE = 1440;

            // 每个UDP包最多容纳多少行
            int rowsPerPacket =
                (MAX_PACKET_SIZE - HEADER_SIZE)
                / bytesPerRow;

            if (rowsPerPacket < 1)
            {
                rowsPerPacket = 1;
            }

            auto put16 = [](std::vector<uint8_t>& buf,
                            uint16_t value)
            {
                buf.push_back(value >> 8);
                buf.push_back(value & 0xFF);
            };

            // 分包发送
            for (int startRow = 0;
                 startRow < h;
                 startRow += rowsPerPacket)
            {
                int packetRows =
                    std::min(rowsPerPacket,
                             (int)h - startRow);

                int bitmapOffset =
                    startRow * bytesPerRow;

                int bitmapSize =
                    packetRows * bytesPerRow;

                std::vector<uint8_t> packet;

                packet.reserve(
                    HEADER_SIZE + bitmapSize
                );

                // 协议号
                packet.push_back(0x42);

                // ESP32协议头
                put16(packet, x);
                put16(packet, y + startRow);
                put16(packet, w);
                put16(packet, packetRows);

                packet.insert(
                    packet.end(),
                    packed.begin() + bitmapOffset,
                    packed.begin() + bitmapOffset + bitmapSize
                );
                struct timeval tv;
                gettimeofday(&tv, nullptr);

                struct tm* tm_info = localtime(&tv.tv_sec);

                int milliseconds = (int)(tv.tv_usec / 1000);

                printf(
                    "\n[%02d:%02d:%02d.%03d] UDP Packet - "
                    "Dirty:(%d,%d,%d,%d) "
                    "Refresh:(%d,%d,%d,%d) "
                    "PacketRow:%d "
                    "PacketHeight:%d "
                    "PacketSize:%zu",
                    tm_info->tm_hour,
                    tm_info->tm_min,
                    tm_info->tm_sec,
                    milliseconds,
                    _dirtyX1,
                    _dirtyY1,
                    _dirtyX2,
                    _dirtyY2,
                    refreshX1,
                    refreshY1,
                    refreshX2,
                    refreshY2,
                    startRow,
                    packetRows,
                    packet.size()
                );

                sendto(
                    _udpSock,
                    packet.data(),
                    packet.size(),
                    0,
                    (sockaddr*)&_udpAddr,
                    sizeof(_udpAddr)
                );

                // 给 ESP32 一点处理时间
                usleep(1000);
            }
        }

#else
        printf("\nDisplay (%dx%d):\n", _width, _height);
        for (int y = 0; y < _height; y++) {
            for (int x = 0; x < _width; x++) {
                printf("%c", _buffer[y * _width + x] ? '#' : ' ');
            }
            printf("\n");
        }
#endif
        // 保存本次脏区域到上次
        _lastDirtyX1 = _dirtyX1;
        _lastDirtyY1 = _dirtyY1;
        _lastDirtyX2 = _dirtyX2;
        _lastDirtyY2 = _dirtyY2;

        // 重置本次脏区域（等待下次绘制更新）
        _dirtyX1 = 0;
        _dirtyY1 = 0;
        _dirtyX2 = 0;
        _dirtyY2 = 0;
    }
    void fillRoundRect(int x, int y, int w, int h, int r, uint8_t color) {
        // printf("fillRoundRect: x=%d, y=%d, w=%d, h=%d, r=%d, color=%d\n", x, y, w, h, r, color);
        int r2 = r * 2;
        if (w <= r2) { r = w / 2; r2 = w; }
        if (h <= r2) { r = h / 2; r2 = h; }

        for (int py = y; py < y + h; py++) {
            for (int px = x; px < x + w; px++) {
                int dx = 0, dy = 0;
                if (px < x + r) dx = x + r - px;
                else if (px >= x + w - r) dx = px - (x + w - r - 1);
                if (py < y + r) dy = y + r - py;
                else if (py >= y + h - r) dy = py - (y + h - r - 1);

                if (dx * dx + dy * dy <= r * r) {
                    if (px >= 0 && px < _width && py >= 0 && py < _height) {
                        _buffer[py * _width + px] = color;
                    }
                }
            }
        }
        // 更新脏区域
        updateDirtyRegion(x, y, x + w, y + h);
    }

    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {
        // printf("fillTriangle: x0=%d, y0=%d, x1=%d, y1=%d, x2=%d, y2=%d, color=%d\n", x0, y0, x1, y1, x2, y2, color);

        if (y0 > y1) { int t; t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
        if (y1 > y2) { int t; t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
        if (y0 > y1) { int t; t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }

        if (y0 == y2) {
            int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
            int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
            drawHLine(minx, y0, maxx - minx + 1, color);
            return;
        }

        int yDiff1 = y1 - y0;
        int yDiff2 = y2 - y0;
        int yDiff3 = y2 - y1;

        int xDiff1 = x1 - x0;
        int xDiff2 = x2 - x0;
        int xDiff3 = x2 - x1;

        for (int y = y0; y <= y2; y++) {
            int xa, xb;
            if (y <= y1) {
                int dy = y - y0;
                // 当 y0 == y1 时，顶部是水平线，直接使用 x0 和 x1
                if (y0 == y1) {
                    xa = x0 < x1 ? x0 : x1;
                    xb = x0 > x1 ? x0 : x1;
                } else {
                    xa = x0 + (dy * xDiff1 + yDiff1 / 2) / yDiff1;
                    xb = x0 + (dy * xDiff2 + yDiff2 / 2) / (yDiff2 > 0 ? yDiff2 : 1);
                }
            } else {
                int dy = y - y1;
                xa = x1 + (dy * xDiff3 + yDiff3 / 2) / (yDiff3 > 0 ? yDiff3 : 1);
                dy = y - y0;
                xb = x0 + (dy * xDiff2 + yDiff2 / 2) / (yDiff2 > 0 ? yDiff2 : 1);
            }
            if (xa > xb) { int t = xa; xa = xb; xb = t; }
            drawHLine(xa, y, xb - xa + 1, color);
        }
    }

private:
#ifdef _WIN32
    void initWindow() {
        const int scale = 1;
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
        // 更新脏区域
        updateDirtyRegion(x, y, x + w, y + 1);
    }

    void fillCircleHelper(int x0, int y0, int r, uint8_t cornername, uint8_t color) {
        int f = 1 - r;
        int ddF_x = 1;
        int ddF_y = -2 * r;
        int x = 0;
        int y = r;

        while (x <= y) {
            if (cornername & 0x1) {
                drawHLine(x0 - x, y0 - y, 2 * x + 1, color);
                if (x != y) drawHLine(x0 - y, y0 - x, 2 * y + 1, color);
            }
            if (cornername & 0x2) {
                drawHLine(x0 - y + 1, y0 - y, 2 * y + 1, color);
                if (x != y) drawHLine(x0 - x + 1, y0 - x, 2 * x + 1, color);
            }
            if (cornername & 0x4) {
                drawHLine(x0 - x, y0 + y - r * 2, 2 * x + 1, color);
                if (x != y) drawHLine(x0 - y, y0 + x - r * 2, 2 * y + 1, color);
            }
            if (cornername & 0x8) {
                drawHLine(x0 - y + 1, y0 + y - r * 2, 2 * y + 1, color);
                if (x != y) drawHLine(x0 - x + 1, y0 + x - r * 2, 2 * x + 1, color);
            }

            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;
        }
    }


    uint8_t _drawColor = 1;

public:
    void setDrawColor(uint8_t color) {
        _drawColor = color;
    }

    void clearBuffer() {
        clearDisplay();
    }

    void sendBuffer() {
        display();
    }

    void drawHLine(int x, int y, int w) {
        drawHLine(x, y, w, _drawColor);
    }

    void drawBox(int x, int y, int w, int h) {
        fillRect(x, y, w, h, _drawColor);
    }

    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2) {
        fillTriangle(x0, y0, x1, y1, x2, y2, _drawColor);
    }
};


    