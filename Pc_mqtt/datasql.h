#ifndef DATASQL_H
#define DATASQL_H

#include <QSqlDatabase>
#include <QString>
#include <QByteArray>

class datasql
{
public:
    datasql();
    ~datasql();

    bool init();   // 初始化数据库
    bool insertData(const QString &topic,
                    const QByteArray &payload);

private:
    QSqlDatabase m_db;
};

#endif // DATASQL_H
