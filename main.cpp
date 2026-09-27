#include "mainwindow.h"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QList>
#include <QPalette>
#include <QString>

#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

// 以下触摸/音频环境适配沿用 photo_album 的成熟方案（同一块板子、同样的 udev 问题）。

// 扫描 /dev/input/event*，按设备能力（是否支持 ABS_MT_POSITION_X/Y）找到触摸屏。
// 本板 goodix 触摸屏被 udev 误标为 tablet，linuxfb 默认使用的 libinput 会忽略它。
static QString findTouchScreen()
{
    unsigned char absBits[(ABS_CNT + 7) / 8];

    for (int i = 0; i < 32; ++i) {
        const QString path = QStringLiteral("/dev/input/event%1").arg(i);
        const QByteArray nativePath = path.toLocal8Bit();
        const int fd = ::open(nativePath.constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        std::memset(absBits, 0, sizeof(absBits));
        const bool hasMtAxes =
            ::ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) >= 0
            && (absBits[ABS_MT_POSITION_X / 8] & (1 << (ABS_MT_POSITION_X % 8)))
            && (absBits[ABS_MT_POSITION_Y / 8] & (1 << (ABS_MT_POSITION_Y % 8)));
        ::close(fd);

        if (hasMtAxes)
            return path;
    }
    return QString();
}

static void setupInputEnvironment()
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_FB_NO_LIBINPUT"))
        qputenv("QT_QPA_FB_NO_LIBINPUT", "1");

    const QString touchDevice = findTouchScreen();
    if (touchDevice.isEmpty())
        return;

    // 修正触摸上报范围：DTS 声明 800x480，驱动实际输出 1024x600
    int screenW = 0;
    int screenH = 0;
    QFile sizeFile(QStringLiteral("/sys/class/graphics/fb0/virtual_size"));
    if (sizeFile.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> parts = sizeFile.readAll().trimmed().split(',');
        if (parts.size() == 2) {
            screenW = parts.at(0).toInt();
            screenH = parts.at(1).toInt();
        }
    }

    const int fd = ::open(touchDevice.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
        struct input_absinfo info;
        if (screenW > 1 && ::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &info) == 0
            && info.maximum != screenW - 1) {
            info.maximum = screenW - 1;
            ::ioctl(fd, EVIOCSABS(ABS_MT_POSITION_X), &info);
        }
        if (screenH > 1 && ::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &info) == 0
            && info.maximum != screenH - 1) {
            info.maximum = screenH - 1;
            ::ioctl(fd, EVIOCSABS(ABS_MT_POSITION_Y), &info);
        }
        ::close(fd);
    }

    if (qEnvironmentVariableIsEmpty("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS"))
        qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", touchDevice.toLocal8Bit());
}

// PulseAudio socket 路径带随机后缀，探测后设置 PULSE_SERVER（Qt 自身与子进程都受益）
static void setupAudioEnvironment()
{
    if (!qEnvironmentVariableIsEmpty("PULSE_SERVER"))
        return;

    const QStringList candidates = QDir(QStringLiteral("/tmp"))
        .entryList(QStringList() << QStringLiteral("pulse-*"),
                   QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i = 0; i < candidates.size(); ++i) {
        const QString socket = QStringLiteral("/tmp/%1/native").arg(candidates.at(i));
        if (QFile::exists(socket)) {
            qputenv("PULSE_SERVER", QStringLiteral("unix:%1").arg(socket).toLocal8Bit());
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    setupInputEnvironment();
    setupAudioEnvironment();

    QApplication a(argc, argv);
    a.setOrganizationName(QStringLiteral("100ask"));
    a.setApplicationName(QStringLiteral("VoiceAssistantQt"));

    QApplication::setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x0f, 0x11, 0x15));
    palette.setColor(QPalette::WindowText, QColor(0xea, 0xea, 0xf0));
    palette.setColor(QPalette::Base, QColor(0x1a, 0x1a, 0x20));
    palette.setColor(QPalette::Text, QColor(0xea, 0xea, 0xf0));
    palette.setColor(QPalette::Button, QColor(0x1a, 0x1a, 0x20));
    palette.setColor(QPalette::ButtonText, QColor(0xea, 0xea, 0xf0));
    palette.setColor(QPalette::Highlight, QColor(0x38, 0xbd, 0xf8));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    a.setPalette(palette);

    const QString fontPath = QStringLiteral("/usr/lib/fonts/msyh.ttc");
    if (QFileInfo::exists(fontPath)) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            a.setFont(QFont(families.first(), 12));
    }

    MainWindow w;
    w.showFullScreen();
    return a.exec();
}
