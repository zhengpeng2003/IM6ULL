#ifndef JSON_UTILS_HPP
#define JSON_UTILS_HPP

#include <cstdint>
#include <string>

#include <rapidjson/document.h>

std::string jsonEscape(const std::string& input);
std::int64_t currentTimeMs();
std::string extractJsonStringValue(const std::string& json, const std::string& key);

std::int64_t getJsonInt64(const rapidjson::Value& obj,
                          const char* key,
                          std::int64_t defaultValue);
int getJsonInt(const rapidjson::Value& obj, const char* key, int defaultValue);
bool getJsonBool(const rapidjson::Value& obj, const char* key, bool defaultValue);
std::string getJsonString(const rapidjson::Value& obj, const char* key);

#endif // JSON_UTILS_HPP
