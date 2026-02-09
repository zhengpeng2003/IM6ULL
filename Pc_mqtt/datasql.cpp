#include "datasql.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

datasql::datasql()
{
}

datasql::~datasql()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool datasql::init()
{
    // 1. 数据库路径：程序（exe）所在目录
    QString dbPath =
        QCoreApplication::applicationDirPath()
        + "/mqtt_data.db";

    qDebug() << "SQLite path:" << dbPath;

    // 2. 创建 SQLite 连接
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    // 3. 打开数据库
    if (!m_db.open()) {
        qCritical() << "Open SQLite failed:"
                    << m_db.lastError().text();
        return false;
    }

    // 4. 建表
    QSqlQuery query;
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS sensor_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            topic TEXT NOT NULL,
            payload TEXT NOT NULL,
            timestamp INTEGER NOT NULL
        )
    )";

    if (!query.exec(createTableSql)) {
        qCritical() << "Create table failed:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool datasql::insertData(const QString &topic,
                         const QByteArray &payload)
{
    if (!m_db.isOpen())
        return false;

    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO sensor_data(topic, payload, timestamp)
        VALUES (?, ?, ?)
    )");

    query.addBindValue(topic);
    query.addBindValue(QString::fromUtf8(payload));
    query.addBindValue(QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        qWarning() << "Insert failed:"
                   << query.lastError().text();
        return false;
    }

    return true;
}
