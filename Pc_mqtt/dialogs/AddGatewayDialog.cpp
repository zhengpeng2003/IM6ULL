#include "AddGatewayDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

AddGatewayDialog::AddGatewayDialog(QWidget *parent) : QDialog(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("AddGatewayDialog 表单待完善"), this));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);
}
