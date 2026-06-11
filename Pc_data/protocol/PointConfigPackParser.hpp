#ifndef POINT_CONFIG_PACK_PARSER_HPP
#define POINT_CONFIG_PACK_PARSER_HPP

#include <string>
#include <vector>

#include "model/PointConfig.hpp"

class PointConfigPackParser
{
public:

    static bool parseJson(const std::string& payload,
                          std::vector<PointConfig>& outConfigs,
                          std::string& errorMessage);

private:
    static std::int64_t currentTimeMs();
};

#endif // POINT_CONFIG_PACK_PARSER_HPP
