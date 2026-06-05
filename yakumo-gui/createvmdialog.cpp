
#include "createvmdialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDir>

CreateVMDialog::CreateVMDialog(QWidget *parent)
    : QDialog(parent)
    , nameEdit(new QLineEdit(this))
    , memorySpinBox(new QSpinBox(this))
    , vcpuSpinBox(new QSpinBox(this))
    , diskPathEdit(new QLineEdit(this))
{
    setWindowTitle("Create VM");

    memorySpinBox->setRange(256, 32768);
    memorySpinBox->setSingleStep(256);
    memorySpinBox->setValue(2048);

    vcpuSpinBox->setRange(1, 16);
    vcpuSpinBox->setValue(2);

    QPushButton* browseButton = new QPushButton("Browse", this);

    QHBoxLayout* diskLayout = new QHBoxLayout;
    diskLayout->addWidget(diskPathEdit);
    diskLayout->addWidget(browseButton);

    QFormLayout* formLayout = new QFormLayout;
    formLayout->addRow("VM name:", nameEdit);
    formLayout->addRow("Memory MB:", memorySpinBox);
    formLayout->addRow("vCPUs:", vcpuSpinBox);
    formLayout->addRow("Diskimage:", diskLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);

    connect(browseButton, &QPushButton::clicked,
            this, &CreateVMDialog::onBrowseButtonClicked);

    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);

    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

QString CreateVMDialog::vmName() const
{
    return nameEdit->text();
}

int CreateVMDialog::memoryMB() const
{
    return memorySpinBox->value();
}

int CreateVMDialog::vcpus() const
{
    return vcpuSpinBox->value();
}

QString CreateVMDialog::diskPath() const
{
    return diskPathEdit->text();
}

void CreateVMDialog::onBrowseButtonClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select qcow2 Disk Image",
        QDir::homePath(),
        "QCOW2 Images (*.qcow2)"
    );

    if(!fileName.isEmpty()){
        diskPathEdit->setText(fileName);
    }
}


