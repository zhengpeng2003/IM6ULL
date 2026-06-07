#include "TrendPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>

TrendPage::TrendPage(DatabaseManager *database, QWidget *parent)
    : QWidget(parent), m_database(database)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("趋势分析"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *filters = new QHBoxLayout;
    QStringList items = {QStringLiteral("工厂"), QStringLiteral("厂房"), QStringLiteral("网关"), QStringLiteral("主站"), QStringLiteral("从站"), QStringLiteral("数据类型"), QStringLiteral("最近30分钟")};
    for (const auto &it : items) {
        auto *cb = new QComboBox(this);
        cb->addItem(it);
        filters->addWidget(cb);
    }
    filters->addWidget(new QPushButton(QStringLiteral("查询"), this));
    filters->addWidget(new QPushButton(QStringLiteral("暂停刷新"), this));
    filters->addWidget(new QPushButton(QStringLiteral("导出CSV"), this));
    layout->addLayout(filters);

    auto *series = new QLineSeries(this);
    series->append(0, 26);
    series->append(1, 27);
    series->append(2, 26.5);
    auto *chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->setTitle(QStringLiteral("实时趋势图"));
    auto *view = new QChartView(chart, this);
    layout->addWidget(view, 1);

    auto *stats = new QLabel(QStringLiteral("当前值: 26.5    最大值: 28.1    最小值: 24.9    平均值: 26.2"), this);
    stats->setObjectName("StatsLabel");
    layout->addWidget(stats);
}
