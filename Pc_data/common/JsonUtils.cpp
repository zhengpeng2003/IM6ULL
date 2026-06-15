#include "JsonUtils.hpp"

#include <chrono>

std::string jsonEscape(const std::string& input)
{
    std::string output;
    output.reserve(input.size());

    for (char ch : input) {
        switch (ch) {
        case '\"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += ch;
            break;
        }
    }

    return output;
}

std::int64_t currentTimeMs()
{
    using namespace std::chrono;

    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()
               ).count();
}

std::string extractJsonStringValue(const std::string& json, const std::string& key)
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return "";
    }

    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) {
        return "";
    }

    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) {
        return "";
    }

    std::string result;
    bool escaped = false;
    for (size_t i = pos + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaped) {
            result.push_back(ch);
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }

        if (ch == '"') {
            break;
        }

        result.push_back(ch);
    }

    return result;
}

std::int64_t getJsonInt64(const rapidjson::Value& obj,
                          const char* key,
                          std::int64_t defaultValue)
{
    if (!obj.IsObject() || !obj.HasMember(key)) {
        return defaultValue;
    }

    const rapidjson::Value& value = obj[key];
    if (value.IsInt64()) {
        return value.GetInt64();
    }

    if (value.IsNumber()) {
        return static_cast<std::int64_t>(value.GetDouble());
    }

    return defaultValue;
}

int getJsonInt(const rapidjson::Value& obj, const char* key, int defaultValue)
{
    if (!obj.IsObject() || !obj.HasMember(key)) {
        return defaultValue;
    }

    const rapidjson::Value& value = obj[key];
    if (value.IsInt()) {
        return value.GetInt();
    }

    if (value.IsNumber()) {
        return static_cast<int>(value.GetDouble());
    }

    return defaultValue;
}

bool getJsonBool(const rapidjson::Value& obj, const char* key, bool defaultValue)
{
    if (!obj.IsObject() || !obj.HasMember(key)) {
        return defaultValue;
    }

    const rapidjson::Value& value = obj[key];
    if (value.IsBool()) {
        return value.GetBool();
    }
    if (value.IsInt()) {
        return value.GetInt() != 0;
    }

    return defaultValue;
}

std::string getJsonString(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject() || !obj.HasMember(key) || !obj[key].IsString()) {
        return "";
    }

    return obj[key].GetString();
}

std::int64_t getJsonInt64Any(const rapidjson::Value& obj,
                             std::initializer_list<const char*> keys,
                             std::int64_t defaultValue)
{
    if (!obj.IsObject()) {
        return defaultValue;
    }
    for (const char* key : keys) {
        if (key && obj.HasMember(key)) {
            return getJsonInt64(obj, key, defaultValue);
        }
    }
    return defaultValue;
}

int getJsonIntAny(const rapidjson::Value& obj,
                  std::initializer_list<const char*> keys,
                  int defaultValue)
{
    if (!obj.IsObject()) {
        return defaultValue;
    }
    for (const char* key : keys) {
        if (key && obj.HasMember(key)) {
            return getJsonInt(obj, key, defaultValue);
        }
    }
    return defaultValue;
}

bool getJsonBoolAny(const rapidjson::Value& obj,
                    std::initializer_list<const char*> keys,
                    bool defaultValue)
{
    if (!obj.IsObject()) {
        return defaultValue;
    }
    for (const char* key : keys) {
        if (key && obj.HasMember(key)) {
            return getJsonBool(obj, key, defaultValue);
        }
    }
    return defaultValue;
}

std::string getJsonStringAny(const rapidjson::Value& obj,
                             std::initializer_list<const char*> keys)
{
    if (!obj.IsObject()) {
        return "";
    }
    for (const char* key : keys) {
        const std::string value = key ? getJsonString(obj, key) : std::string();
        if (!value.empty()) {
            return value;
        }
    }
    return "";
}
