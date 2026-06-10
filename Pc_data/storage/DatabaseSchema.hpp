#ifndef DATABASE_SCHEMA_HPP
#define DATABASE_SCHEMA_HPP

#include <string>
#include <vector>

class DatabaseSchema
{
public:
    static std::vector<std::string> tableSqlList();
    static std::vector<std::string> indexSqlList();
};

#endif // DATABASE_SCHEMA_HPP