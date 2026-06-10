
#include "vncconsoledialog.h"
#include "vncwidget.h"
#include "vm_manager.h"

#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

VncConsoleDialog::VncConsoleDialog(const QString& vmName, QWidget* parent)
    : QDialog(parent)
    , vncWidget(new VncWidget(this))
{
    setWindowTitle("Console - " + vmName);
    resize(900, 600);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vncWidget);

    VncConsoleInfo info;
    std::string errorMessage;

    bool ok = getVncConsoleInfo(vmName.toStdString(), &info, &errorMessage);

    if (!ok){
        QMessageBox::warning(this, "Console", QString::fromStdString(errorMessage));

        QTimer::singleShot(0, this, &QDialog::reject);

        return;
    }

    vncWidget->connectToVnc(QString::fromStdString(info.host), info.port);
}


