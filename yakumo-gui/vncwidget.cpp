

#include "vncwidget.h"

#include <QColor>
#include <QPainter>

VncWidget::VncWidget(QWidget* parent)
    : QWidget(parent)
{
        setMinimumSize(640, 480);
        
        connect(&socket, &QTcpSocket::connected, this, &VncWidget::onConnected);

        connect(&socket, &QTcpSocket::readyRead, this, &VncWidget::onReadyRead);

        connect(&socket, &QTcpSocket::errorOccurred, this, &VncWidget::onSocketError);
}

void VncWidget::connectToVnc(const QString& host, int port)
{
    statusText = "Connecting to " + host + ":" + QString::number(port);
    update();

    state = State::WaitProtocolVersion;
    buffer.clear();
    socket.connectToHost(host, port);
}

void VncWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (!framebuffer.isNull()){
        painter.drawImage(rect(), framebuffer);
    } else {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, statusText);
    }
}

void VncWidget::onConnected()
{
    statusText = "Connected. Waiting for RFB version...";
    update();
}

void VncWidget::onReadyRead()
{
    buffer += socket.readAll();
    processBuffer();
}

void VncWidget::onSocketError(QAbstractSocket::SocketError)
{
    statusText = socket.errorString();
    update();
}

// VNCサーバへ画面更新を要求する
void VncWidget::requestFramebufferUpdate(bool incremental)
{
    if (framebufferWidth <= 0 || framebufferHeight <= 0){
        return;
    }

    auto appendU16 = [](QByteArray& out, quint16 value){
        out.append(static_cast<char> ((value >> 8) & 0xff));
        out.append(static_cast<char> ((value & 0xff)));
    };

    QByteArray message;
    message.append(static_cast<char>(3));
    message.append(static_cast<char>(incremental ? 1 : 0));
    appendU16(message, 0);
    appendU16(message, 0);
    appendU16(message, framebufferWidth);
    appendU16(message, framebufferHeight);

    socket.write(message);
}

void VncWidget::processBuffer()
{
    while (true) {
        if(state == State::WaitProtocolVersion) {
            if (buffer.size() < 12) {
                return;
            }

            buffer.remove(0, 12);
            socket.write("RFB 003.008\n", 12);
            state = State::WaitSecurityTypes;
        }

        else if (state == State::WaitSecurityTypes) {
            if (buffer.size() < 1){
                return;
            }

            quint8 count = static_cast<quint8>(buffer[0]);

            if (buffer.size() < 1 + count){
                return;
            }

            bool hasNone = false;

            for (int i = 0; i < count; i++) {
                if (static_cast<quint8>(buffer[1 + i]) == 1){
                    hasNone = true;
                }
            }

            buffer.remove(0, 1 + count);

            if (!hasNone) {
                statusText = "VNC security failed";
                update();
                return;
            }

            socket.write(QByteArray(1, static_cast<char>(1)));
            state = State::WaitSecurityResult;
        } else if (state == State::WaitSecurityResult){
            if (buffer.size() < 4){
                return;
            }
            quint32 result = readU32(buffer.constData());
            buffer.remove(0, 4);

            if (result != 0){
                statusText = "VNC security failed";
                update();
                return;
            }
            socket.write(QByteArray(1, static_cast<char>(1)));
            state = State::WaitServerInit;
        }else if (state == State::WaitServerInit) {
            if (buffer.size() < 24) {
                return;
            }

            framebufferWidth = readU16(buffer.constData());
            framebufferHeight = readU16(buffer.constData() + 2);
            quint32 nameLength = readU32(buffer.constData() +20);

            if (buffer.size() < 24 + static_cast<int>(nameLength)){
                return;
            }

            buffer.remove(0, 24 + static_cast<int>(nameLength));
        
            framebuffer = QImage(framebufferWidth, framebufferHeight, QImage::Format_RGB32);

            framebuffer.fill(Qt::black);

            auto appendU16 = [](QByteArray& out, quint16 value){
                out.append(static_cast<char>((value >> 8) & 0xff));
                out.append(static_cast<char>(value & 0xff));
            };

             auto appendU32 = [](QByteArray& out, quint32 value){
                out.append(static_cast<char>((value >> 24) & 0xff));
                out.append(static_cast<char>((value >> 16) & 0xff));
                out.append(static_cast<char>((value >> 8) & 0xff));
                out.append(static_cast<char>(value & 0xff));
            };

            QByteArray pixelFormat;
            pixelFormat.append(static_cast<char>(0));
            pixelFormat.append(3, static_cast<char>(0));
            pixelFormat.append(static_cast<char>(32));
            pixelFormat.append(static_cast<char>(24));
            pixelFormat.append(static_cast<char>(0));
            pixelFormat.append(static_cast<char>(1));
            appendU16(pixelFormat, 255);
            appendU16(pixelFormat, 255);
            appendU16(pixelFormat, 255);
            pixelFormat.append(static_cast<char>(16));
            pixelFormat.append(static_cast<char>(8));
            pixelFormat.append(static_cast<char>(0));
            pixelFormat.append(3, static_cast<char>(0));
            socket.write(pixelFormat);

            QByteArray encodings;
            encodings.append(static_cast<char>(2));
            encodings.append(static_cast<char>(0));
            appendU16(encodings, 1);
            appendU32(encodings, 0);
            socket.write(encodings);

            state = State::WaitFramebufferUpdate;
            requestFramebufferUpdate(false);
            update();
        }
        else if (state == State::WaitFramebufferUpdate) {
            if (buffer.size() < 4) {
                return;
            }

            quint8 messageType = static_cast<quint8>(buffer[0]);

            if (messageType != 0){
                buffer.remove(0, 1);
                continue;
            }

            quint16 rectCount = readU16(buffer.constData() + 2);
            int offset = 4;

            for (int r = 0; r < rectCount; ++r) {
                if (buffer.size() < offset + 12){
                    return;
                }

                int x = readU16(buffer.constData() + offset);
                int y = readU16(buffer.constData() + offset + 2);
                int w = readU16(buffer.constData() + offset + 4);
                int h = readU16(buffer.constData() + offset + 6);
                quint32 encoding = readU32(buffer.constData() + offset + 8);
                offset += 12;

                if (encoding != 0){
                    statusText = "Unsupported VNS encoding";
                    update();
                    return;
                }

                int bytes = w * h * 4;
                if (buffer.size() < offset + bytes){
                    return;
                }

                const uchar* src = reinterpret_cast<const uchar*>(buffer.constData() + offset);

                for (int row = 0; row < h; ++row){
                    QRgb* dst = reinterpret_cast<QRgb*>(framebuffer.scanLine(y + row)) + x;
                    const uchar* line = src + row * w * 4;

                    for (int col = 0; col < w; ++col) {
                        int b = line[col * 4 + 0];
                        int g = line[col * 4 + 1];
                        int rValue = line[col * 4 + 2];
                        dst[col] = qRgb(rValue, g, b);
                    }
                }

                offset += bytes;
            }

            buffer.remove(0, offset);
            update();
            requestFramebufferUpdate(true);
        }
    }
}

quint16 VncWidget::readU16(const char* data) const
{
    return (static_cast<unsigned char>(data[0]) << 8) | static_cast<unsigned char>(data[1]);
}

quint32 VncWidget::readU32(const char* data) const
{
    return (static_cast<quint32>(static_cast<unsigned char>(data[0])) << 24) |
           (static_cast<quint32>(static_cast<unsigned char>(data[1])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data[2])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data[3]))); 
}




