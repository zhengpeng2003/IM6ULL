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

QJsonValue jsonValueAny(const QJsonObject &obj, const QStringList &keys)
{
    for (const QString &key : keys) {
        if (obj.contains(key) && !obj.value(key).isUndefined() && !obj.value(key).isNull()) {
            return obj.value(key);
        }
    }
    return QJsonValue();
}

QString jsonStringAny(const QJsonObject &obj, const QStringList &keys, const QString &defaultValue = QString())
{
    const QJsonValue value = jsonValueAny(obj, keys);
    return value.isString() ? value.toString() : defaultValue;
}

int jsonIntAny(const QJsonObject &obj, const QStringList &keys, int defaultValue = 0)
{
    const QJsonValue value = jsonValueAny(obj, keys);
    if (value.isDouble()) {
        return value.toInt();
    }
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        return ok ? parsed : defaultValue;
    }
    return defaultValue;
}

qint64 jsonInt64Any(const QJsonObject &obj, const QStringList &keys, qint64 defaultValue = 0)
{
    const QJsonValue value = jsonValueAny(obj, keys);
    if (value.isDouble() || value.isString()) {
        const qint64 parsed = value.toVariant().toLongLong();
        return parsed != 0 ? parsed : defaultValue;
    }
    return defaultValue;
}

double jsonDoubleAny(const QJsonObject &obj, const QStringList &keys, double defaultValue = 0.0)
{
    const QJsonValue value = jsonValueAny(obj, keys);
    if (value.isDouble() || value.isString()) {
        return value.toVariant().toDouble();
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
    outPack.seq = static_cast<quint32>(jsonIntAny(root, {"seq", "sequence"}));
    const qint64 rootTimestampMs = jsonInt64Any(root, {"timestampMs", "timestamp"});
    outPack.time = rootTimestampMs > 0
        ? QDateTime::fromMSecsSinceEpoch(rootTimestampMs)
        : QDateTime::fromSecsSinceEpoch(jsonInt64Any(root, {"time"}));
    outPack.masterSlot = -1;

    const QJsonObject site = root.value("site").toObject();
    if (!site.isEmpty())
        outPack.masterSlot = slotFromPortId(site.value("portId").toString());

    QJsonArray devs = root.value("devices").toArray();
    outPack.devices.clear();

    for (const auto &v : devs) {
        QJsonObject o = v.toObject();

        DeviceData dev;
        dev.deviceId = jsonIntAny(o, {"deviceId", "slave_id", "slaveAddress", "id"});
        dev.valid    = jsonBoolValue(o.value("valid"));
        dev.errorMessage = o.value("errorMessage").toString();
        dev.timestampMs = jsonInt64Any(o, {"timestampMs", "timestamp"});
        if (dev.timestampMs <= 0) {
            dev.timestampMs = rootTimestampMs;
        }
        if (!dev.valid && dev.errorMessage.isEmpty()) {
            dev.errorMessage = QStringLiteral("数据无效");
        }

        QString typeStr = jsonStringAny(o, {"deviceType", "device_type"});
        if (typeStr.isEmpty()) {
            typeStr = o.value("type").toString();
        }

        /* ===== 传感器 ===== */
        if (typeStr == "sensor_th") {
            dev.type = DEV_SENSOR_TH;

            if (dev.valid) {
                const QJsonObject thObj = o.value("th").toObject();
                dev.temperature = jsonDoubleAny(o, {"temperature", "temp"},
                                                jsonDoubleAny(thObj, {"temperature", "temp"}));
                dev.humidity = jsonDoubleAny(o, {"humidity", "humi"},
                                             jsonDoubleAny(thObj, {"humidity", "humi"}));
            }
        }
        /* ===== 继电器 ===== */
        else if (typeStr == "relay") {
            dev.type = DEV_RELAY;

            if (dev.valid) {
                const QJsonObject relayObj = o.value("relay").toObject();
                dev.relayStates =
                    static_cast<quint16>(jsonIntAny(o,
                                                    {"relayStates", "states"},
                                                    jsonIntAny(relayObj, {"relayStates", "states"})));
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
