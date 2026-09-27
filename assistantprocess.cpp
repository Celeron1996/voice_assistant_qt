#include "assistantprocess.h"

namespace {
const char *const kDefaultScript = "/root/voice_assistant/voice_assistant.py";
const char *const kDefaultDir = "/root/voice_assistant";
}

AssistantProcess::AssistantProcess(QObject *parent)
    : QObject(parent)
{
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_proc, &QProcess::readyReadStandardOutput,
            this, &AssistantProcess::handleReadyRead);
    connect(&m_proc, &QProcess::stateChanged,
            this, &AssistantProcess::handleStateChanged);
}

bool AssistantProcess::isRunning() const
{
    return m_proc.state() != QProcess::NotRunning;
}

void AssistantProcess::start()
{
    if (isRunning())
        return;

    const QString script = qEnvironmentVariable("VOICE_ASSISTANT_SCRIPT", kDefaultScript);
    const QString workDir = qEnvironmentVariable("VOICE_ASSISTANT_DIR", kDefaultDir);
    m_proc.setWorkingDirectory(workDir);
    m_proc.start(QStringLiteral("python3"),
                 QStringList() << script << QStringLiteral("--spectrum"));
}

void AssistantProcess::stop()
{
    if (!isRunning())
        return;
    m_proc.terminate();
    if (!m_proc.waitForFinished(5000)) {
        m_proc.kill();
        m_proc.waitForFinished(2000);
    }
}

void AssistantProcess::handleReadyRead()
{
    m_buffer += m_proc.readAllStandardOutput();

    int index;
    while ((index = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(index).trimmed();
        m_buffer.remove(0, index + 1);

        // Python 的流式文本 print(end="") 可能与 @AUDIO 行粘在一起，
        // 因此在整个行里找协议标记，而不是只看行首。
        const int micIndex = line.indexOf("@AUDIO M ");
        const int ttsIndex = line.indexOf("@AUDIO T ");
        if (micIndex >= 0 || ttsIndex >= 0) {
            const bool isMic = micIndex >= 0;
            const int index = isMic ? micIndex : ttsIndex;
            emit audioData(QByteArray::fromBase64(line.mid(index + 9)), isMic ? 16000 : 24000);
            const QByteArray prefix = line.left(index).trimmed();
            if (!prefix.isEmpty())
                emit logLine(QString::fromUtf8(prefix));
        } else if (!line.isEmpty()) {
            emit logLine(QString::fromUtf8(line));
        }
    }
}

void AssistantProcess::handleStateChanged(QProcess::ProcessState state)
{
    emit runningChanged(state != QProcess::NotRunning);
    if (state == QProcess::NotRunning)
        m_buffer.clear();
}
