#ifndef ASSISTANTPROCESS_H
#define ASSISTANTPROCESS_H

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>

// 启动/停止 Python 语音助手进程，解析其 stdout：
//   @AUDIO M <base64>  麦克风 PCM（16k）
//   @AUDIO T <base64>  TTS PCM（24k）
//   其他行              日志（用于界面状态展示）
class AssistantProcess : public QObject
{
    Q_OBJECT

public:
    explicit AssistantProcess(QObject *parent = nullptr);

    bool isRunning() const;
    void start();
    void stop();

signals:
    void audioData(const QByteArray &pcm, int sampleRate);
    void logLine(const QString &line);
    void runningChanged(bool running);

private slots:
    void handleReadyRead();
    void handleStateChanged(QProcess::ProcessState state);

private:
    QProcess m_proc;
    QByteArray m_buffer;
};

#endif // ASSISTANTPROCESS_H
