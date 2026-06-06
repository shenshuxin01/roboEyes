#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct Field {
    std::string comment;
    std::string name;
    std::string fmt;
    uint32_t size;
    uint32_t offset;
};

struct Telemetry {
    std::map<std::string, double> values;
};

extern std::map<std::string, uint32_t> TYPE_SIZE;

/**
 * 读取 FH4_packetformat.dat
 */
std::vector<Field> loadDatFile(const std::string& path);

/**
 * 从字节流读取指定类型
 */
template<typename T>
T readValue(const uint8_t* data, uint32_t offset);

/**
 * 解析 Forza UDP 数据包
 */
Telemetry parsePacket(
        const std::vector<uint8_t>& data,
        const std::vector<Field>& fields);

/**
 * 主循环
 */
void udpPacket();