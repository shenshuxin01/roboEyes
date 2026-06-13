//
// Created by Shen Shuxin on 2026/6/13.
//

//
// Created by Shen Shuxin on 2026/6/13.
//

#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>
#include <chrono>

// UDP监听端口，需要与ESP32保持一致
constexpr int UDP_PORT = 5556;

// Home Assistant配置
constexpr const char *HA_URL = "http://192.168.0.102:8123";
constexpr const char *HA_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI2NjA0YWUxZDdmYjA0MzQ4YjQyOTJjMTUwODMwNzYzNSIsImlhdCI6MTc4MTMzOTY2OCwiZXhwIjoyMDk2Njk5NjY4fQ.qSx7ouEN2gT7Rza0k90xiKDHGcUaHSy_ENiiYXJq2sQ";
constexpr const char *MEDIA_PLAYER = "media_player.xiaomi_lx06_62d6_play_control_2";

int cachedVolume = -1;
time_t cachedVolumeTime = 0;

std::atomic<uint64_t> volumeDisplayVersion{0};

#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << " errno=" << errno << " (" << strerror(errno) << ")" << std::endl

// 获取当前音量（0~100），失败返回-1
int getCurrentVolume() {
    time_t now = time(nullptr);
    if (cachedVolume >= 0 && now - cachedVolumeTime < 10) {
        return cachedVolume;
    }

    LOG_INFO("获取Home Assistant当前音量");
    std::string cmd =
            "curl -s -H 'Authorization: Bearer " +
            std::string(HA_TOKEN) +
            "' " +
            std::string(HA_URL) +
            "/api/states/" +
            MEDIA_PLAYER;

    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        LOG_ERROR("执行curl失败");
        return -1;
    }

    std::string response;
    char buf[1024];

    while (fgets(buf, sizeof(buf), fp)) {
        response += buf;
    }

    pclose(fp);

    auto pos = response.find("\"volume_level\":");
    if (pos == std::string::npos) {
        LOG_ERROR("未找到volume_level字段，响应=" << response);
        return -1;
    }

    pos += strlen("\"volume_level\":");

    double volume = atof(response.c_str() + pos);

    cachedVolume = static_cast<int>(volume*100);
    cachedVolumeTime = now;
    return cachedVolume;
}


//当前音量显示2秒
void showCurVolumeTo4num() {
    int volume = getCurrentVolume();
    if (volume < 0) {
        return;
    }

    uint64_t version = ++volumeDisplayVersion;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("创建ESP32 TCP Socket失败");
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(82);
    inet_pton(AF_INET, "192.168.0.108", &serverAddr.sin_addr);

    if (connect(sockfd,
                reinterpret_cast<sockaddr *>(&serverAddr),
                sizeof(serverAddr)) != 0) {
        LOG_ERROR("连接ESP32失败");
        close(sockfd);
        return;
    }

    std::string request =
            "{\"cmd\":\"setNumber\","
            "\"value\":" +
            std::to_string(volume) +
            ",\"dotPos\":-1}\n";

    if (send(sockfd,
             request.c_str(),
             request.size(),
             0) <= 0) {
        LOG_ERROR("发送setNumber失败");
        close(sockfd);
        return;
    }
    LOG_INFO("ESP32显示音量: " << volume);
    close(sockfd);

    std::thread([version]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (version != volumeDisplayVersion.load()) {
            return;
        }

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            LOG_ERROR("创建ESP32 TCP Socket失败");
            return;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(82);
        inet_pton(AF_INET, "192.168.0.108", &serverAddr.sin_addr);

        if (connect(sockfd,
                    reinterpret_cast<sockaddr *>(&serverAddr),
                    sizeof(serverAddr)) != 0) {
            LOG_ERROR("连接ESP32失败");
            close(sockfd);
            return;
        }

        std::string request =
                "{\"cmd\":\"displayTime\"}\n";

        if (send(sockfd,
                 request.c_str(),
                 request.size(),
                 0) <= 0) {
            LOG_ERROR("发送displayTime失败");
            close(sockfd);
        } else {
            LOG_INFO("ESP32恢复时间显示");
            close(sockfd);
        }
    }).detach();
}


// 调用Home Assistant音量服务
void changeVolume(int delta) {
    int currentVolume = getCurrentVolume();
    if (currentVolume < 0) {
        return;
    }

    currentVolume += delta*2;

    if (currentVolume < 10) currentVolume = 10;
    if (currentVolume > 80) currentVolume = 80;

    double volumeLevel = currentVolume / 100.0;

    LOG_INFO("设置音量: " << currentVolume << "%");
    std::string cmd =
            "curl -s -X POST " +
            std::string(HA_URL) +
            "/api/services/media_player/volume_set "
            "-H 'Authorization: Bearer " + HA_TOKEN +
            "' -H 'Content-Type: application/json' "
            "-d '{\"entity_id\":\"" +
            MEDIA_PLAYER +
            "\",\"volume_level\":" +
            std::to_string(volumeLevel) +
            "}'";

    int ret = system(cmd.c_str());
    if (ret != 0) {
        LOG_ERROR("调用Home Assistant volume_set失败");
    } else {
        LOG_INFO("Home Assistant volume_set成功");
    }

    cachedVolume = currentVolume;
    cachedVolumeTime = time(nullptr);
}

// Apple Music下一首
void nextTrack() {
    if (system("osascript -e 'tell application \"Music\" to next track'") != 0) {
        LOG_ERROR("Apple Music下一首失败");
    } else {
        LOG_INFO("Apple Music下一首成功");
    }
}

// Apple Music上一首
void prevTrack() {
    if (system("osascript -e 'tell application \"Music\" to previous track'") != 0) {
        LOG_ERROR("Apple Music上一首失败");
    } else {
        LOG_INFO("Apple Music上一首成功");
    }
}

// 输出当前Apple Music播放信息
void printCurrentTrackInfo() {
    const char *script = R"(
osascript <<'EOF'
tell application "Music"
    if player state is playing then
        set trackName to name of current track
        set artistName to artist of current track
        set albumName to album of current track
        set durationSec to duration of current track
        set currentPos to player position
        return trackName & "|" & artistName & "|" & albumName & "|" & currentPos & "|" & durationSec
    else
        return "NOT_PLAYING"
    end if
end tell
EOF
)";

    FILE *fp = popen(script, "r");
    if (!fp) {
        return;
    }

    char buffer[1024];
    std::string result;

    while (fgets(buffer, sizeof(buffer), fp)) {
        result += buffer;
    }

    pclose(fp);

    if (result.find("NOT_PLAYING") != std::string::npos) {
        std::cout << "Apple Music未播放" << std::endl;
        return;
    }

    result.erase(result.find_last_not_of("\r\n") + 1);

    std::string parts[5];
    size_t start = 0;

    for (int i = 0; i < 4; ++i) {
        size_t pos = result.find('|', start);
        if (pos == std::string::npos) {
            return;
        }
        parts[i] = result.substr(start, pos - start);
        start = pos + 1;
    }

    parts[4] = result.substr(start);

    double currentPos = atof(parts[3].c_str());
    double duration = atof(parts[4].c_str());

    std::cout << "当前歌曲: " << parts[0] << std::endl;
    std::cout << "歌手: " << parts[1] << std::endl;
    std::cout << "专辑: " << parts[2] << std::endl;
    std::cout << "进度: " << static_cast<int>(currentPos)
              << " / " << static_cast<int>(duration)
              << " 秒" << std::endl;
}

// 处理ESP32发送的指令
void handleCommand(const std::string &msg) {
    std::cout << "收到: " << msg << std::endl;

    if (msg == "volume,get") {
        int volume = getCurrentVolume();
        std::cout << "当前音量: " << volume << "%" << std::endl;
        return;
    }

    if (msg.rfind("volume,", 0) == 0) {
        int delta = std::stoi(msg.substr(7));
        changeVolume(delta);
        showCurVolumeTo4num();
        std::cout << "当前音量: " << cachedVolume << std::endl;
    } else if (msg == "track,next") {
        nextTrack();
    } else if (msg == "track,prev") {
        prevTrack();
    }

//    printCurrentTrackInfo();
}

int main() {
    // 创建UDP Socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("UDP socket创建失败");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 绑定端口
    if (bind(sockfd,
             reinterpret_cast<sockaddr *>(&addr),
             sizeof(addr)) < 0) {
        LOG_ERROR("UDP bind失败");
        close(sockfd);
        return -1;
    }

    LOG_INFO("UDP服务启动，监听端口: " << UDP_PORT);

    char buffer[1024];

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);

        ssize_t n = recvfrom(sockfd,
                             buffer,
                             sizeof(buffer) - 1,
                             0,
                             reinterpret_cast<sockaddr *>(&clientAddr),
                             &len);

        if (n <= 0) {
            continue;
        }

        buffer[n] = '\0';

        // 丢弃积压的UDP数据，只保留最新的一条指令
        while (true) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);

            timeval tv{};
            tv.tv_sec = 0;
            tv.tv_usec = 0;

            int ret = select(sockfd + 1, &readfds, nullptr, nullptr, &tv);
            if (ret <= 0) {
                break;
            }

            n = recvfrom(sockfd,
                         buffer,
                         sizeof(buffer) - 1,
                         0,
                         reinterpret_cast<sockaddr *>(&clientAddr),
                         &len);

            if (n <= 0) {
                break;
            }

            buffer[n] = '\0';
        }

        handleCommand(buffer);
    }

    close(sockfd);
    return 0;
}