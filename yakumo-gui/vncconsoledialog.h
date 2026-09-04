
#pragma once

#include <QDialog>
#include <QString>

class VncWidget;
class IVMService;

class VncConsoleDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit VncConsoleDialog(IVMService& service, const QString& vmName, QWidget* parent = nullptr);
    
    private:
        VncWidget* vncWidget;
};

