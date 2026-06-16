#include "MqttConfig.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "common/JsonUtils.hpp"

namespace {
const char* kMqttConfigPath = "config/mqtt_config.json";
}

MqttConfig loadMqttConfig()
{
    MqttConfig config;
    std::ifstream file(kMqttConfigPath, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "MQTT config file not found: " << kMqttConfigPath
                  << ", use defaults host=" << config.host
                  << ", port=" << config.port
                  << ", clientId=" << config.clientId
                  << ", commandGatewayId=" << config.commandGatewayId
                  << ", topicCount=" << config.topics.size()
                  << std::endl;
        return config;
    }

    std::cout << "MQTT config file opened: " << kMqttConfigPath << std::endl;

    std::stringstream buffer;
    buffer << file.rdbuf();

    rapidjson::Document root;
    root.Parse(buffer.str().c_str());

    if (root.HasParseError() || !root.IsObject()) {
        std::cout << "MQTT config parse failed, use defaults host=" << config.host
                  << ", port=" << config.port
                  << ", clientId=" << config.clientId
                  << ", commandGatewayId=" << config.commandGatewayId
                  << ", topicCount=" << config.topics.size()
                  << std::endl;
        return config;
    }

    const std::string host = getJsonString(root, "host");
    const int port = getJsonInt(root, "port", config.port);
    const std::string commandGatewayId = getJsonString(root, "commandGatewayId");

    if (!host.empty()) {
        config.host = host;
    }
    if (port >= 1 && port <= 65535) {
        config.port = port;
    }
    if (!commandGatewayId.empty()) {
        config.commandGatewayId = commandGatewayId;
    }

    std::cout << "MQTT config loaded from file: host=" << config.host
              << ", port=" << config.port
              << ", clientId=" << config.clientId
              << ", commandGatewayId=" << config.commandGatewayId
              << ", topicCount=" << config.topics.size()
              << std::endl;
    for (const std::string& topic : config.topics) {
        std::cout << "MQTT config topic: " << topic << std::endl;
    }

    return config;
}

bool saveMqttConfigFile(const MqttConfig& config)
{
    try {
        std::filesystem::path path(kMqttConfigPath);
        std::filesystem::path dir = path.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    } catch (const std::exception& e) {
        std::cout << "MQTT config directory create failed: " << e.what() << std::endl;
        return false;
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("host");
    writer.String(config.host.c_str());
    writer.Key("port");
    writer.Int(config.port);
    writer.Key("commandGatewayId");
    writer.String(config.commandGatewayId.c_str());
    writer.EndObject();

    std::ofstream file(kMqttConfigPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << buffer.GetString();
    return file.good();
}
