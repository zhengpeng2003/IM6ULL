#include "data_parser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

DataParser::DataParser()
{

}

bool DataParser::parseJson(const QByteArray &json, DataPack &outPack)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return false;
    }

    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    outPack.seq  = root.value("seq").toInt();
    outPack.time = QDateTime::fromSecsSinceEpoch(
        root.value("time").toVariant().toLongLong());

    QJsonArray devs = root.value("devices").toArray();
    outPack.devices.clear();

    for (const auto &v : devs) {
        QJsonObject o = v.toObject();

        DeviceData dev;
        dev.deviceId = o.value("id").toInt();
        dev.valid    = o.value("valid").toInt() == 1;

        QString typeStr = o.value("type").toString();

        /* ===== 传感器 ===== */
        if (typeStr == "sensor_th") {
            dev.type = DEV_SENSOR_TH;

            if (dev.valid) {
                dev.temperature = o.value("temp").toDouble();
                dev.humidity    = o.value("humi").toDouble();
            }
        }
        /* ===== 继电器 ===== */
        else if (typeStr == "relay") {
            dev.type = DEV_RELAY;

            if (dev.valid) {
                dev.relayStates =
                    static_cast<quint16>(o.value("states").toInt());
            }
        }
        /* ===== 系统信息 ===== */
        else if (typeStr == "sysinfo") {
            dev.type = DEV_SYSINFO;

            if (dev.valid) {
                QJsonObject sysObj = o;

                dev.sys.kernel    = sysObj.value("kernel").toString();
                dev.sys.arch      = sysObj.value("arch").toString();
                dev.sys.os        = sysObj.value("os").toString();
                dev.sys.screenW   = sysObj.value("screen_w").toInt();
                dev.sys.screenH   = sysObj.value("screen_h").toInt();
            }
        }
        else {
            dev.type = DEV_UNKNOWN; // 可选
        }

        outPack.devices.append(dev);
    }

    return true;
}

