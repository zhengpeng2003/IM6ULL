#pragma once
#include <QWidget>
class DatabaseManager;
class QChartView;

class TrendPage : public QWidget
{
    Q_OBJECT
public:
    explicit TrendPage(DatabaseManager *database, QWidget *parent = nullptr);

private:
    DatabaseManager *m_database = nullptr;
};
