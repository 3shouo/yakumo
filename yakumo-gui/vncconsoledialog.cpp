
#include "vncconsoledialog.h"
#include "vncwidget.h"
#include "vm_service.h"

#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

VncConsoleDialog::VncConsoleDialog(IVMService& service, const QString& vmName, QWidget* parent)
    : QDialog(parent)
    , vncWidget(new VncWidget(this))
{
    setWindowTitle("Console - " + vmName);
    resize(900, 600);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vncWidget);

    VncConsoleInfo info;
    VMResult result = service.getVncConsoleInfo(vmName.toStdString(), &info);

    if (!result.ok) {
        QMessageBox::warning(this, "Console", QString::fromStdString(result.message));

        QTimer::singleShot(0, this, &QDialog::reject);

        return;
    }

    /*
    std::string errorMessage;

    bool ok = getVncConsoleInfo(vmName.toStdString(), &info, &errorMessage);

    if (!ok){
        QMessageBox::warning(this, "Console", QString::fromStdString(errorMessage));

        QTimer::singleShot(0, this, &QDialog::reject);

        return;
    }
        */

    vncWidget->connectToVnc(QString::fromStdString(info.host), info.port);
}


