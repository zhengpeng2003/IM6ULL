#include "data_parser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

namespace {

bool jsonBoolValue(const QJsonValue &value, bool defaultValue = false)
{
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        return value.toInt() != 0;
    }
    if (value.isString()) {
        const QString text = value.toString().trimmed().toLower();
        return text == "true" || text == "1" || text == "ok";
    }
    return defaultValue;
}

int slotFromPortId(const QString &portId)
{
    if (portId == "port_001")
        return 0;
    if (portId == "port_002")
        return 1;
    return -1;
}

} // namespace

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
    const qint64 rootTimestampMs = root.value("timestampMs").toVariant().toLongLong();
    outPack.time = rootTimestampMs > 0
        ? QDateTime::fromMSecsSinceEpoch(rootTimestampMs)
        : QDateTime::fromSecsSinceEpoch(root.value("time").toVariant().toLongLong());
    outPack.masterSlot = -1;

    const QJsonObject site = root.value("site").toObject();
    if (!site.isEmpty())
        outPack.masterSlot = slotFromPortId(site.value("portId").toString());

    QJsonArray devs = root.value("devices").toArray();
    outPack.devices.clear();

    for (const auto &v : devs) {
        QJsonObject o = v.toObject();

        DeviceData dev;
        dev.deviceId = o.value("id").toInt();
        dev.valid    = jsonBoolValue(o.value("valid"));
        dev.errorMessage = o.value("errorMessage").toString();
        dev.timestampMs = o.value("timestampMs").toVariant().toLongLong();
        if (dev.timestampMs <= 0) {
            dev.timestampMs = rootTimestampMs;
        }
        if (!dev.valid && dev.errorMessage.isEmpty()) {
            dev.errorMessage = QStringLiteral("数据无效");
        }

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
