
#pragma once

#include <QDialog>

class QSpinBox;
class QLineEdit;
class QDialogButtonBox;

class CreateVMDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit CreateVMDialog(QWidget *parent = nullptr);

        QString vmName() const;
        int memoryMB() const;
        int vcpus() const;
        QString diskPath() const;

    private slots:
        void onBrowseButtonClicked();
        void onAccepted();

    private:
        QLineEdit* nameEdit;
        QSpinBox* memorySpinBox;
        QSpinBox* vcpuSpinBox;
        QLineEdit* diskPathEdit;
        QDialogButtonBox* buttonBox;
};

