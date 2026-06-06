

#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <Shellapi.h>
#pragma comment(lib, "Shell32.lib")
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
    uint8_t* _lastBuffer;
#ifdef __APPLE__
    int _udpSock;
    sockaddr_in _udpAddr;
#endif
#ifdef _WIN32
    HWND _hwnd;
    HDC _hdc;
    HBITMAP _hbitmap;
    BITMAPINFO _bmi;
    bool _isDragging;
    POINT _dragStart;
    POINT _windowStart;
    bool _isInTaskbar;
    HWND _originalParent;
#endif

    // 脏区域跟踪 - 本次刷新区域 (x1, y1)左上角, (x2, y2)右下角
    int _dirtyX1 = 0, _dirtyY1 = 0, _dirtyX2 = 0, _dirtyY2 = 0;
    // 上次刷新区域（用于比较）
    int _lastDirtyX1 = 0, _lastDirtyY1 = 0, _lastDirtyX2 = 0, _lastDirtyY2 = 0;

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
        _lastBuffer = new uint8_t[width * height]();
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
        _isDragging = false;
        _isInTaskbar = false;
        _originalParent = NULL;
        initFloatWindow();
#endif
    }

    ~MyDisplay() {
        delete[] _buffer;
        delete[] _lastBuffer;
#ifdef __APPLE__
        if (_udpSock >= 0) close(_udpSock);
#endif
#ifdef _WIN32
        if (_hbitmap) DeleteObject(_hbitmap);
        if (_hdc) ReleaseDC(_hwnd, _hdc);
        if (_hwnd) DestroyWindow(_hwnd);
#endif
    }

    bool hasBufferChanged() const {
        for (int i = 0; i < _width * _height; i++) {
            if (_buffer[i] != _lastBuffer[i]) {
                return true;
            }
        }
        return false;
    }

    void copyBufferToLast() {
        for (int i = 0; i < _width * _height; i++) {
            _lastBuffer[i] = _buffer[i];
        }
    }

    bool needsRefresh() {
        if (!hasBufferChanged()) {
            return false;
        }
        copyBufferToLast();
        return true;
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
        if (!needsRefresh()) {
            return;
        }

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
// 全屏刷新
// SH1106 Page 格式
// 单包发送
// =============================

        uint16_t x = 0;
        uint16_t y = 0;

        uint16_t w = 128;
        uint16_t h = 64;

        int pageCount = 8;

        std::vector<uint8_t> packed(w * pageCount, 0);

        for (int page = 0; page < 8; page++)
        {
            int pageOffset = page * w;

            for (int col = 0; col < w; col++)
            {
                uint8_t value = 0;

                for (int bit = 0; bit < 8; bit++)
                {
//                    int srcY = page * 8 + bit;
//
//                    if (_buffer[srcY * _width + col])
//                    {
//                        value |= (1 << bit);
//                    }
//旋转180度
                    int srcY = page * 8 + bit;

                    int srcX = (_width - 1) - col;
                    int rotatedY = (_height - 1) - srcY;

                    if (_buffer[rotatedY * _width + srcX])
                    {
                        value |= (1 << bit);
                    }

                }

                packed[pageOffset + col] = value;
            }
        }

        constexpr int HEADER_SIZE = 9;

        auto put16 = [](std::vector<uint8_t>& buf,
                        uint16_t value)
        {
            buf.push_back(value >> 8);
            buf.push_back(value & 0xFF);
        };

        std::vector<uint8_t> packet;

        packet.reserve(
                HEADER_SIZE + packed.size()
        );

        packet.push_back(0x42);

        put16(packet, x);
        put16(packet, y);
        put16(packet, w);
        put16(packet, h);

        packet.insert(
                packet.end(),
                packed.begin(),
                packed.end()
        );

//        struct timeval tv;
//        gettimeofday(&tv, nullptr);
//
//        struct tm* tm_info = localtime(&tv.tv_sec);
//
//        int milliseconds = (int)(tv.tv_usec / 1000);
//
//        printf(
//                "\n[%02d:%02d:%02d.%03d] "
//                "UDP FullScreen "
//                "PacketSize:%zu",
//                tm_info->tm_hour,
//                tm_info->tm_min,
//                tm_info->tm_sec,
//                milliseconds,
//                packet.size()
//        );

        sendto(
                _udpSock,
                packet.data(),
                packet.size(),
                0,
                (sockaddr*)&_udpAddr,
                sizeof(_udpAddr)
        );

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
    // void initWindow() {
    //     const int scale = 1;
    //     int windowWidth = _width * scale;
    //     int windowHeight = _height * scale;

    //     // 注册窗口类
    //     WNDCLASSA wc = {0};
    //     wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
    //         if (msg == WM_DESTROY) {
    //             PostQuitMessage(0);
    //             return 0;
    //         }
    //         return DefWindowProcA(hwnd, msg, wParam, lParam);
    //     };
    //     wc.hInstance = GetModuleHandleA(NULL);
    //     wc.lpszClassName = "MyDisplayWindow";
    //     wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    //     RegisterClassA(&wc);

    //     // 创建窗口
    //     _hwnd = CreateWindowExA(
    //         0, "MyDisplayWindow", "MyDisplay",
    //         WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
    //         CW_USEDEFAULT, CW_USEDEFAULT,
    //         windowWidth + GetSystemMetrics(SM_CXFRAME) * 2,
    //         windowHeight + GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
    //         NULL, NULL, GetModuleHandleA(NULL), NULL
    //     );

    //     if (_hwnd) {
    //         ShowWindow(_hwnd, SW_SHOW);
    //         _hdc = GetDC(_hwnd);
    //     }
    // }


void initFloatWindow() {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        MyDisplay* pThis = reinterpret_cast<MyDisplay*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_LBUTTONDOWN: {
                if (!pThis->_isInTaskbar) {
                    pThis->_isDragging = true;
                    pThis->_dragStart = {LOWORD(lParam), HIWORD(lParam)};
                    GetWindowRect(hwnd, (LPRECT)&pThis->_windowStart);
                    SetCapture(hwnd);
                }
                return 0;
            }

            case WM_LBUTTONUP: {
                if (pThis->_isDragging) {
                    pThis->_isDragging = false;
                    ReleaseCapture();

                    POINT cursorPos;
                    GetCursorPos(&cursorPos);

                    HWND hTaskbar = FindWindowA("Shell_TrayWnd", NULL);
                    if (hTaskbar) {
                        RECT taskbarRect;
                        GetWindowRect(hTaskbar, &taskbarRect);

                        if (cursorPos.x >= taskbarRect.left && cursorPos.x <= taskbarRect.right &&
                            cursorPos.y >= taskbarRect.top && cursorPos.y <= taskbarRect.bottom) {
                            pThis->_originalParent = GetParent(hwnd);
                            SetParent(hwnd, hTaskbar);

                            int xPos = (taskbarRect.right - taskbarRect.left) - pThis->_width - 100;

                            SetWindowPos(hwnd, NULL, xPos, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                            SetWindowLongPtrA(hwnd, GWL_STYLE, WS_CHILD | WS_VISIBLE);
                            SetWindowLongPtrA(hwnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
                            pThis->_isInTaskbar = true;
                        }
                    }
                }
                return 0;
            }

            case WM_MOUSEMOVE: {
                if (pThis->_isDragging && !pThis->_isInTaskbar) {
                    POINT cursorPos;
                    GetCursorPos(&cursorPos);

                    int newX = pThis->_windowStart.x + cursorPos.x - pThis->_dragStart.x;
                    int newY = pThis->_windowStart.y + cursorPos.y - pThis->_dragStart.y;

                    SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }
                return 0;
            }

            case WM_RBUTTONDOWN: {
                if (pThis->_isInTaskbar) {
                    pThis->_isInTaskbar = false;

                    POINT cursorPos;
                    GetCursorPos(&cursorPos);

                    SetParent(hwnd, pThis->_originalParent);
                    SetWindowLongPtrA(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                    SetWindowLongPtrA(hwnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
                    SetWindowPos(hwnd, HWND_TOPMOST, cursorPos.x - pThis->_width/2, cursorPos.y - pThis->_height/2, 0, 0, SWP_NOSIZE);
                }
                return 0;
            }

            case WM_NCHITTEST: {
                if (pThis->_isInTaskbar) {
                    return HTTRANSPARENT;
                }
                return HTCAPTION;
            }
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    };
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "MyFloatWindow";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_DBLCLKS;

    RegisterClassA(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int xPos = (screenWidth - _width) / 2;
    int yPos = (screenHeight - _height) / 2;

    _hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "MyFloatWindow",
        "",
        WS_POPUP | WS_VISIBLE,
        xPos, yPos,
        _width,
        _height,
        NULL,
        NULL,
        GetModuleHandleA(NULL),
        NULL
    );

    if (_hwnd) {
        SetWindowLongPtrA(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
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




