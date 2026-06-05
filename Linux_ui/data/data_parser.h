#ifndef DATA_PARSER_H
#define DATA_PARSER_H

#include <QByteArray>
#include "data_protocol.h"

class DataParser
{
public:
    DataParser();
    static bool parseJson(const QByteArray &json, DataPack &outPack);

};

#endif // DATA_PARSER_H
