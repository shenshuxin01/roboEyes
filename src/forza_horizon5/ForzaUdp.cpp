#include "ForzaUdp.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>


std::map<std::string, uint32_t> TYPE_SIZE = {
        {"f32", 4},
        {"u32", 4},
        {"s32", 4},
        {"u16", 2},
        {"u8", 1},
        {"s8", 1},
        {"hzn", 4}
};

std::vector<Field> loadDatFile(const std::string& path) {
    std::vector<Field> fields;

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open dat file");
    }

    std::string line;
    uint32_t offset = 0;

    while (std::getline(file, line)) {

        auto posSemi = line.find(';');
        if (posSemi != std::string::npos)
            line.erase(posSemi, 1);

        std::string comment;

        auto posComment = line.find("//");
        if (posComment != std::string::npos) {
            comment = line.substr(posComment + 2);
            line = line.substr(0, posComment);
        }

        if (line.empty())
            continue;

        std::stringstream ss(line);

        std::string typeName;
        std::string fieldName;

        ss >> typeName >> fieldName;

        if (typeName.empty() || fieldName.empty())
            continue;

        auto it = TYPE_SIZE.find(typeName);
        if (it == TYPE_SIZE.end()) {
            std::cout << "unknown type: " << typeName << std::endl;
            continue;
        }

        fields.push_back({
                                 comment,
                                 fieldName,
                                 typeName,
                                 it->second,
                                 offset
                         });

        offset += it->second;
    }

    return fields;
}

template<typename T>
T readValue(const uint8_t* data, uint32_t offset) {
    T value;
    memcpy(&value, data + offset, sizeof(T));
    return value;
}

//struct Telemetry {
//    std::map<std::string, double> values;
//};

Telemetry parsePacket(
        const std::vector<uint8_t>& data,
        const std::vector<Field>& fields) {

    Telemetry result;

    for (const auto& field : fields) {

        double value = 0;

        if (field.fmt == "f32")
            value = readValue<float>(data.data(), field.offset);

        else if (field.fmt == "u32")
            value = readValue<uint32_t>(data.data(), field.offset);

        else if (field.fmt == "s32")
            value = readValue<int32_t>(data.data(), field.offset);

        else if (field.fmt == "u16")
            value = readValue<uint16_t>(data.data(), field.offset);

        else if (field.fmt == "u8")
            value = readValue<uint8_t>(data.data(), field.offset);

        else if (field.fmt == "s8")
            value = readValue<int8_t>(data.data(), field.offset);

        result.values[field.name] = value;
    }

    return result;
}

void udpPacket() {

    constexpr int UDP_PORT = 5555;

    auto fields = loadDatFile("../src/forza_horizon5/FH4_packetformat.dat");

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(UDP_PORT);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    bind(
            sockfd,
            reinterpret_cast<sockaddr*>(&localAddr),
            sizeof(localAddr));

    // 非阻塞
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in relayAddr{};
    relayAddr.sin_family = AF_INET;
    relayAddr.sin_port = htons(5555);
    inet_pton(AF_INET, "192.168.0.103", &relayAddr.sin_addr);

    sockaddr_in displayAddr{};
    displayAddr.sin_family = AF_INET;
    displayAddr.sin_port = htons(9999);
    inet_pton(AF_INET, "192.168.0.107", &displayAddr.sin_addr);

    std::cout << "waiting..." << std::endl;

    constexpr int FPS = 10;
    constexpr int BUFFER_SIZE = 1500;

    std::vector<uint8_t> latestPacket;

    while (true) {

        try {

            std::this_thread::sleep_for(
                    std::chrono::milliseconds(1000 / FPS));

            while (true) {

                uint8_t buffer[BUFFER_SIZE];

                sockaddr_in sender{};
                socklen_t senderLen = sizeof(sender);

                ssize_t len = recvfrom(
                        sockfd,
                        buffer,
                        BUFFER_SIZE,
                        0,
                        reinterpret_cast<sockaddr*>(&sender),
                        &senderLen);

                if (len <= 0)
                    break;

                latestPacket.assign(buffer, buffer + len);
            }

            if (latestPacket.empty())
                continue;

            auto telemetry =
                    parsePacket(latestPacket, fields);

            double speed =
                    telemetry.values["Speed"];

            double rpm =
                    telemetry.values["CurrentEngineRpm"];

            double maxRpm =
                    telemetry.values["EngineMaxRpm"];

            double gear =
                    telemetry.values["Gear"];

            double carId =
                    telemetry.values["CarOrdinal"];

            if (carId == 0)
                gear = -1;

            double rpm16 =
                    maxRpm == 0 ? 0 : rpm / maxRpm * 16;

            int kmh =
                    static_cast<int>(speed * 3.6);

            std::string sendData =
                    std::to_string(kmh) + "," +
                    std::to_string((int)rpm16) + "," +
                    std::to_string((int)gear);

            sendto(
                    sockfd,
                    latestPacket.data(),
                    latestPacket.size(),
                    0,
                    reinterpret_cast<sockaddr*>(&relayAddr),
                    sizeof(relayAddr));

            sendto(
                    sockfd,
                    sendData.c_str(),
                    sendData.size(),
                    0,
                    reinterpret_cast<sockaddr*>(&displayAddr),
                    sizeof(displayAddr));
        }
        catch (const std::exception& e) {

            std::cout << e.what() << std::endl;

            std::this_thread::sleep_for(
                    std::chrono::seconds(10));
        }
    }
}
