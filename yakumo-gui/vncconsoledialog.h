
#pragma once

#include <QDialog>
#include <QString>

class VncWidget;

class VncConsoleDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit VncConsoleDialog(const QString& vmName, QWidget* parent = nullptr);
    
    private:
        VncWidget* vncWidget;
};

