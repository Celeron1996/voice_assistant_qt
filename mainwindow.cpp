#include "mainwindow.h"

#include "assistantprocess.h"
#include "spectrumwidget.h"

#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_assistant(new AssistantProcess(this))
    , m_spectrum(new SpectrumWidget(this))
    , m_toggleButton(new QPushButton(QStringLiteral("开始对话"), this))
    , m_volumeSlider(new QSlider(Qt::Horizontal, this))
    , m_volumeLabel(new QLabel(QStringLiteral("--%"), this))
    , m_statusLabel(new QLabel(QStringLiteral("已暂停"), this))
    , m_userLabel(new QLabel(QStringLiteral("你："), this))
    , m_aiLabel(new QLabel(QStringLiteral("AI："), this))
    , m_volumeTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(QStringLiteral("语音助手"));

    // ---------- 布局 ----------
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 18, 26, 20);
    root->setSpacing(8);

    QHBoxLayout *header = new QHBoxLayout;
    QLabel *title = new QLabel(QStringLiteral("语音助手"), this);
    title->setObjectName(QStringLiteral("title"));
    m_statusLabel->setObjectName(QStringLiteral("status"));
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_statusLabel);
    root->addLayout(header);

    m_userLabel->setObjectName(QStringLiteral("hint"));
    m_aiLabel->setObjectName(QStringLiteral("hint"));
    m_userLabel->setWordWrap(true);
    m_aiLabel->setWordWrap(true);

    root->addWidget(m_spectrum, 1);

    root->addWidget(m_userLabel);
    root->addWidget(m_aiLabel);

    QHBoxLayout *bottom = new QHBoxLayout;
    bottom->setSpacing(14);

    m_toggleButton->setObjectName(QStringLiteral("toggle"));
    m_toggleButton->setFixedSize(190, 62);
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    m_toggleButton->setProperty("running", false);
    bottom->addWidget(m_toggleButton);

    bottom->addStretch();

    QLabel *volumeTitle = new QLabel(QStringLiteral("音量"), this);
    volumeTitle->setObjectName(QStringLiteral("hint"));
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setFixedWidth(340);
    m_volumeLabel->setObjectName(QStringLiteral("volumeValue"));
    m_volumeLabel->setFixedWidth(66);
    m_volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bottom->addWidget(volumeTitle);
    bottom->addWidget(m_volumeSlider);
    bottom->addWidget(m_volumeLabel);
    root->addLayout(bottom);

    // ---------- 样式（深色扁平风格） ----------
    setStyleSheet(QStringLiteral(R"(
#mainWindow { background-color: #0f1115; }
#title { font-size: 26px; font-weight: 600; color: #f2f4f8; }
#status { font-size: 16px; color: #8b93a7; }
#hint { font-size: 15px; color: #8b93a7; }
#volumeValue { font-size: 16px; color: #cbd5e1; }
QPushButton#toggle {
    font-size: 20px; font-weight: 600;
    border: none; border-radius: 18px;
    background-color: #22c55e; color: #08131f;
}
QPushButton#toggle[running="true"] { background-color: #f59e0b; color: #1c1204; }
QPushButton#toggle:pressed { background-color: #16a34a; }
QPushButton#toggle[running="true"]:pressed { background-color: #d97706; }
QSlider::groove:horizontal { height: 8px; border-radius: 4px; background: #232838; }
QSlider::sub-page:horizontal { border-radius: 4px; background: #38bdf8; }
QSlider::add-page:horizontal { border-radius: 4px; background: #232838; }
QSlider::handle:horizontal {
    width: 26px; margin: -10px 0; border-radius: 13px; background: #e5e9f0;
}
QSlider::handle:horizontal:pressed { background: #ffffff; }
)"));

    // ---------- 信号 ----------
    connect(m_toggleButton, &QPushButton::clicked, this, &MainWindow::toggleAssistant);
    connect(m_assistant, &AssistantProcess::audioData, m_spectrum, &SpectrumWidget::pushAudio);
    connect(m_assistant, &AssistantProcess::logLine, this, &MainWindow::onLogLine);
    connect(m_assistant, &AssistantProcess::runningChanged, this, &MainWindow::onRunningChanged);

    m_volumeTimer->setSingleShot(true);
    m_volumeTimer->setInterval(150);
    connect(m_volumeTimer, &QTimer::timeout, this, &MainWindow::applyVolume);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    // ---------- 初始化音量 ----------
    detectPulseServer();
    if (!m_pulseServer.isEmpty()) {
        const int volume = querySinkVolume();
        if (volume >= 0) {
            m_volumeSlider->blockSignals(true);
            m_volumeSlider->setValue(volume);
            m_volumeSlider->blockSignals(false);
            m_volumeLabel->setText(QStringLiteral("%1%").arg(volume));
        }
    }

    // 调试用：设置 VOICE_QT_AUTOSTART=1 启动即自动开始对话（方便无人值守验证）
    if (qEnvironmentVariable("VOICE_QT_AUTOSTART") == QLatin1String("1"))
        QTimer::singleShot(600, this, &MainWindow::toggleAssistant);
}

MainWindow::~MainWindow()
{
    m_assistant->stop();
}

void MainWindow::toggleAssistant()
{
    if (m_assistant->isRunning()) {
        m_statusLabel->setText(QStringLiteral("正在停止…"));
        m_assistant->stop();
        m_statusLabel->setText(QStringLiteral("已暂停"));
    } else {
        m_userLabel->setText(QStringLiteral("你："));
        m_aiLabel->setText(QStringLiteral("AI："));
        m_statusLabel->setText(QStringLiteral("启动中…"));
        m_assistant->start();
    }
}

void MainWindow::onRunningChanged(bool running)
{
    m_toggleButton->setText(running ? QStringLiteral("暂停对话") : QStringLiteral("开始对话"));
    m_toggleButton->setProperty("running", running);
    m_toggleButton->style()->unpolish(m_toggleButton);
    m_toggleButton->style()->polish(m_toggleButton);
    if (!running)
        m_statusLabel->setText(QStringLiteral("已暂停"));
}

void MainWindow::onLogLine(const QString &line)
{
    if (line.contains(QStringLiteral("实时会话已建立")))
        m_statusLabel->setText(QStringLiteral("对话中"));
    else if (line.contains(QStringLiteral("麦克风预热")))
        m_statusLabel->setText(QStringLiteral("启动中…"));
    else if (line.contains(QStringLiteral("空闲超过")))
        m_statusLabel->setText(QStringLiteral("会话刷新中…"));
    else if (line.contains(QStringLiteral("实时对话错误"))
             || line.contains(QStringLiteral("建立实时会话失败"))
             || line.contains(QStringLiteral("出错")))
        m_statusLabel->setText(QStringLiteral("网络异常，重连中…"));

    const QString partial = QStringLiteral("识别中… ");
    const QString finalText = QStringLiteral("你说: ");
    if (line.contains(partial))
        m_userLabel->setText(QStringLiteral("你：%1").arg(line.mid(line.indexOf(partial) + partial.size())));
    else if (line.contains(finalText))
        m_userLabel->setText(QStringLiteral("你：%1").arg(line.mid(line.indexOf(finalText) + finalText.size())));

    const QString aiText = QStringLiteral("AI  : ");
    if (line.contains(aiText))
        m_aiLabel->setText(QStringLiteral("AI：%1").arg(line.mid(line.indexOf(aiText) + aiText.size())));
}

void MainWindow::onVolumeChanged(int value)
{
    m_volumeLabel->setText(QStringLiteral("%1%").arg(value));
    m_volumeTimer->start(); // 拖动时防抖，停顿 150ms 后真正下发
}

void MainWindow::applyVolume()
{
    if (m_pulseServer.isEmpty())
        detectPulseServer();
    if (m_pulseServer.isEmpty())
        return;
    const QString volume = QStringLiteral("%1%").arg(m_volumeSlider->value());
    runPactl(QStringList() << QStringLiteral("set-sink-mute") << QStringLiteral("@DEFAULT_SINK@")
                           << QStringLiteral("0"));
    runPactl(QStringList() << QStringLiteral("set-sink-volume") << QStringLiteral("@DEFAULT_SINK@")
                           << volume);
}

void MainWindow::detectPulseServer()
{
    const QStringList dirs = QDir(QStringLiteral("/tmp"))
        .entryList(QStringList() << QStringLiteral("pulse-*"),
                   QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i = 0; i < dirs.size(); ++i) {
        const QString socketPath = QStringLiteral("/tmp/%1/native").arg(dirs.at(i));
        if (!QFile::exists(socketPath))
            continue;

        QProcess probe;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PULSE_SERVER"), QStringLiteral("unix:%1").arg(socketPath));
        probe.setProcessEnvironment(env);
        probe.start(QStringLiteral("pactl"), QStringList() << QStringLiteral("info"));
        if (probe.waitForFinished(1500) && probe.exitCode() == 0) {
            m_pulseServer = QStringLiteral("unix:%1").arg(socketPath);
            return;
        }
        probe.kill();
        probe.waitForFinished(200);
    }
}

int MainWindow::querySinkVolume() const
{
    // 注意：板子上的 pactl 较老，没有 get-sink-volume 子命令，只能从 list sinks 解析
    QByteArray output;
    if (!runPactl(QStringList() << QStringLiteral("list") << QStringLiteral("sinks"), &output))
        return -1;

    const QRegularExpression regex(QStringLiteral("Volume:[^\\n]*?(\\d+)%"));
    const QRegularExpressionMatch match = regex.match(QString::fromLatin1(output));
    if (!match.hasMatch())
        return -1;
    return match.captured(1).toInt();
}

bool MainWindow::runPactl(const QStringList &arguments, QByteArray *output) const
{
    if (m_pulseServer.isEmpty())
        return false;

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PULSE_SERVER"), m_pulseServer);
    process.setProcessEnvironment(env);
    process.start(QStringLiteral("pactl"), arguments);
    if (!process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(200);
        return false;
    }
    if (output)
        *output = process.readAllStandardOutput();
    return process.exitCode() == 0;
}
