
#pragma once

#include <QByteArray>
#include <QImage>
#include <QTcpSocket>
#include <QWidget>

class VncWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit VncWidget(QWidget* parent = nullptr);

        void connectToVnc(const QString& host, int port);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private slots:
        void onConnected();
        void onReadyRead();
        void onSocketError(QAbstractSocket::SocketError error);

    private:
        enum class State {
            WaitProtocolVersion,
            WaitSecurityTypes,
            WaitSecurityResult,
            WaitServerInit,
            WaitFramebufferUpdate
        };

        void processBuffer();
        void requestFramebufferUpdate(bool incremental);
        quint16 readU16(const char* data) const;
        quint32 readU32(const char* data) const;

        QTcpSocket socket;
        QByteArray buffer;
        QImage framebuffer;
        State state = State::WaitProtocolVersion;
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        QString statusText = "Not connected";

};

