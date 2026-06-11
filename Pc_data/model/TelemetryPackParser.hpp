#ifndef TELEMETRY_PACK_PARSER_HPP
#define TELEMETRY_PACK_PARSER_HPP

#include <cstdint>
#include <string>

#include "TelemetryPack.hpp"

class TelemetryPackParser
{
public:

    static bool parseJson(const std::string& payload,
                          TelemetryPack& outPack,
                          std::string& errorMessage);

private:
    static std::int64_t currentTimeMs();
};

#endif // TELEMETRY_PACK_PARSER_HPP
