/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "MqttWidget.h"
#include <QtCore/QDateTime>
#include <QtMqtt/QMqttClient>
#include <QtWidgets/QMessageBox>
#include <qtimer.h>
#include "ui_MqttWidget.h"
MqttWidget::MqttWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MqttWidget)
{
    ui->setupUi(this);

    m_client = new QMqttClient(this);
    m_client->setHostname(ui->lineEditHost->text()  );
    m_client->setPort(ui->spinBoxPort->value());

    connect(m_client, &QMqttClient::stateChanged, this, &MqttWidget::updateLogStateChange);
    connect(m_client, &QMqttClient::disconnected, this, &MqttWidget::brokerDisconnected);
    connect(m_client, &QMqttClient::messageReceived, this,&MqttWidget::updatemessageReceived);

    connect(m_client, &QMqttClient::pingResponseReceived, this, [this]() {
        const QString content = QDateTime::currentDateTime().toString()
        + QLatin1String(" PingResponse")
            + QLatin1Char('\n');

    });

    connect(ui->lineEditHost, &QLineEdit::textChanged, m_client, &QMqttClient::setHostname);
    connect(ui->spinBoxPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &MqttWidget::setClientPort);
    updateLogStateChange();
}

MqttWidget::~MqttWidget()
{
    delete ui;
}

void MqttWidget::on_buttonConnect_clicked()
{
    if (m_client->state() == QMqttClient::Disconnected) {
        ui->lineEditHost->setEnabled(false);
        ui->spinBoxPort->setEnabled(false);
        ui->buttonConnect->setText(tr("Disconnect"));
        m_client->connectToHost();
    } else {
        ui->lineEditHost->setEnabled(true);
        ui->spinBoxPort->setEnabled(true);
        ui->buttonConnect->setText(tr("Connect"));
        m_client->disconnectFromHost();
    }
}

void MqttWidget::on_buttonQuit_clicked()
{
    QApplication::quit();
}

void MqttWidget::updateLogStateChange()
{
    const QString content = QDateTime::currentDateTime().toString()
    + QLatin1String(": State Change")
        + QString::number(m_client->state())
        + QLatin1Char('\n');

}

void MqttWidget::updatemessageReceived(const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return;
    }

    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();

    // devices 数组
    if (!root.contains("devices") || !root.value("devices").isArray())
        return;

    QJsonArray devices = root.value("devices").toArray();
    if (devices.isEmpty())
        return;

    QJsonObject dev = devices.first().toObject();

    // 判断类型
    if (dev.value("type").toString() != "sensor_th")
        return;

    // 有效性
    if (dev.value("valid").toInt() != 1)
        return;

    double temp = dev.value("temp").toDouble();
    double humi = dev.value("humi").toDouble();

    qDebug() << "TEMP =" << temp << "HUMI =" << humi;

    // 👉 通知 GPIO 页面
    emit S_thDataUpdated(temp, humi);
}


void MqttWidget::brokerDisconnected()
{
    ui->lineEditHost->setEnabled(true);
    ui->spinBoxPort->setEnabled(true);
    ui->buttonConnect->setText(tr("Connect"));
}

void MqttWidget::setClientPort(int p)
{
    m_client->setPort(p);
}

void MqttWidget::on_buttonPublish_clicked()
{
    if (m_client->publish(ui->lineEditTopic->text(), ui->lineEditMessage->text().toUtf8()) == -1)
        QMessageBox::critical(this, QLatin1String("Error"), QLatin1String("Could not publish message"));
}

void MqttWidget::on_buttonSubscribe_clicked()
{
    auto subscription = m_client->subscribe(ui->lineEditTopic->text());
    if (!subscription) {
        QMessageBox::critical(this, QLatin1String("Error"), QLatin1String("Could not subscribe. Is there a valid connection?"));
        return;
    }
}
void MqttWidget::onLedButtonClicked(bool targetState)
{
    publishWithAck("imx6ull/gpio/led/set", targetState, true);
}

void MqttWidget::onBuzzerButtonClicked(bool targetState)
{
    publishWithAck("imx6ull/gpio/buzzer/set", targetState, false);
}

void MqttWidget::publishWithAck(const QString &topic, bool value, bool isLed)
{
    if (!m_client)
        return;

    QByteArray payload = value ? "1" : "0";
    auto res = m_client->publish(topic, payload);

    if (res == -1) {
        QMessageBox::critical(this, "MQTT Error", "Publish failed!");
        if (isLed)
            emit ledAckReceived(false);
        else
            emit buzzerAckReceived(false);
        return;
    }

    // 假设对面会 ACK，demo 用定时器模拟 ACK
    // 实际项目里根据 messageReceived 来解析 JSON
    QTimer::singleShot(500, this, [=](){
        bool success = true; // 假设收到 ACK 成功
        if (!success)
            QMessageBox::warning(this, "GPIO Error", "Set GPIO failed!");
        if (isLed)
            emit ledAckReceived(success);
        else
            emit buzzerAckReceived(success);
    });
}
