

#include "vncwidget.h"

#include <QColor>
#include <QPainter>
#include <QKeyEvent>

VncWidget::VncWidget(QWidget* parent)
    : QWidget(parent)
{
        setMinimumSize(640, 480);
        setFocusPolicy(Qt::StrongFocus);
        
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
            appendU16(encodings, 2);
            appendU32(encodings, 0);
            appendU32(encodings, 0xFFFFFF21u); 
            socket.write(encodings);
            /*
            QByteArray encodings;
            encodings.append(static_cast<char>(2));
            encodings.append(static_cast<char>(0));
            appendU16(encodings, 1);
            appendU32(encodings, 0);
            socket.write(encodings);
            */

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

                if (encoding == 0xFFFFFF21u){
                    framebufferWidth = w;
                    framebufferHeight = h;
                    QImage resized(w, h, QImage::Format_RGB32);
                    resized.fill(Qt::black);
                    framebuffer = resized;
                    needFullUpdate = true;
                    continue;
                }
                
                if (encoding != 0){
                    statusText = "Unsupported VNS encoding";
                    update();
                    return;
                }

                int bytes = w * h * 4;
                if (buffer.size() < offset + bytes){
                    return;
                }

                if (x < 0 || y < 0 || x + w > framebuffer.width() || y + h > framebuffer.height()){
                    offset += bytes;
                    continue;
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
            requestFramebufferUpdate(!needFullUpdate);
            needFullUpdate = false;
            /*
            buffer.remove(0, offset);
            update();
            requestFramebufferUpdate(true);
            */
        }
    }
}

// キーボードイベント（種別4）をサーバへ送る
void VncWidget::sendKeyEvent(quint32 keysym, bool down)
{
    if (state != State::WaitFramebufferUpdate){
        return;
    }
    if (keysym == 0){
        return;
    }

    QByteArray message;
    message.append(static_cast<char>(4));
    message.append(static_cast<char>(down ? 1 : 0));
    message.append(static_cast<char>(0));
    message.append(static_cast<char>(0));
    message.append(static_cast<char>((keysym >> 24) & 0xff));
    message.append(static_cast<char>((keysym >> 16) & 0xff));
    message.append(static_cast<char>((keysym >> 8) & 0xff));
    message.append(static_cast<char>(keysym & 0xff));

    socket.write(message);
}

// Qtのキー情報をX11のkeysymへ変換する
quint32 VncWidget::mapQtKeyToKeysym(QKeyEvent* event) const
{
    switch (event->key())
    {
    case Qt::Key_Backspace: return 0xff08;
    case Qt::Key_Tab:       return 0xFF09;
    case Qt::Key_Return:
    case Qt::Key_Enter:     return 0xFF0D;
    case Qt::Key_Escape:    return 0xFF1B;
    case Qt::Key_Delete:    return 0xFFFF;
    case Qt::Key_Home:      return 0xFF50;
    case Qt::Key_Left:      return 0xFF51;
    case Qt::Key_Up:        return 0xFF52;
    case Qt::Key_Right:     return 0xFF53;
    case Qt::Key_Down:      return 0xFF54;
    case Qt::Key_PageUp:    return 0xFF55;
    case Qt::Key_PageDown:  return 0xFF56;
    case Qt::Key_End:       return 0xFF57;
    /*case Qt::Key_Shift:     return 0xFFE1;*/
    case Qt::Key_Control:   return 0xFFE3;
    case Qt::Key_Alt:       return 0xFFE9;
    default: break;
    }

    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12){
        return 0xFFBE + (event->key() - Qt::Key_F1);
    }

    QString text = event->text();

    if (!text.isEmpty() && text.at(0).unicode() >= 0x20 && text.at(0).unicode() != 0x7f){
        return text.at(0).unicode();
    }

    if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z){
        return 0x61 + (event->key() - Qt::Key_A);
    }

    return 0;
}

// US配列で、その文字を出すのに Shift が要るか判定する
static bool keysymNeedsShift(quint32 keysym)
{
    // 大文字 A~Z は Shift が必要
    if (keysym >= 'A' && keysym <= 'Z') {
        return true;
    }

    // US配列で Shift を要する記号
    switch (keysym) {
        case '~': case '!': case '@': case '#': case '$':
        case '%': case '^': case '&': case '*': case '(':
        case ')': case '_': case '+': case '{': case '}':
        case '|': case ':': case '"': case '<': case '>':
        case '?':
            return true;
        default:
            return false;
    }
}

// キーが押されたときの処理
void VncWidget::keyPressEvent(QKeyEvent* event)
{
    int key = event->key();

    // Ctrl/Alt は押下状態を保持する必要があるのでdownだけ送って戻る
    if (key == Qt::Key_Control || key == Qt::Key_Alt){
        sendKeyEvent(mapQtKeyToKeysym(event), true);
        return;
    }

    quint32 keysym = mapQtKeyToKeysym(event);
    if (keysym == 0) {
        return;
    }
    
    // Shiftを押している間は、対象キーをshiftで挟んで送る（記号対象）
    bool  shift = keysymNeedsShift(keysym);
    if (shift) {
        sendKeyEvent(0xFFE1, true);
    }
    sendKeyEvent(keysym, true);
    sendKeyEvent(keysym, false);

    if (shift) {
        sendKeyEvent(0xFFE1, false);
    }
}

// キーが離されたときの処理
void VncWidget::keyReleaseEvent(QKeyEvent* event)
{
    int key = event->key();

    // Ctrl/Alt の離しだけ送る（他のキーは押下時に down/up を送りきっている）
    if (key == Qt::Key_Control || key == Qt::Key_Alt) {
        sendKeyEvent(mapQtKeyToKeysym(event), false);
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




