#ifndef DATA_PROTOCOL_H
#define DATA_PROTOCOL_H

#include <QString>
#include <QVector>
#include <QDateTime>

/* 设备类型（和 C 端保持一致） */
enum DeviceType {
    DEV_SENSOR_TH = 1,
    DEV_RELAY     = 2,
    DEV_METER     = 3,
    DEV_UNKNOWN   =0,
    DEV_SYSINFO   =100,
};
struct SysInfo {
    QString kernel;
    QString arch;
    QString os;
    int screenW;
    int screenH;
};

/* 单个设备数据 */
struct DeviceData
{
    int deviceId = 0;
    DeviceType type = DEV_SENSOR_TH;
    bool valid = false;
    QString errorMessage;
    qint64 timestampMs = 0;

    // 温湿度
    double temperature = 0.0;
    double humidity    = 0.0;
     /* ===== 继电器 ===== */
    quint16 relayStates = 0;
    //板子信息
    SysInfo sys;
};

/* 一包数据 */
struct DataPack
{
    quint32 seq = 0;
    QDateTime time;
    int masterSlot = -1;
    QVector<DeviceData> devices;
};

#endif // DATA_PROTOCOL_H
