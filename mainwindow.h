#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QString>
#include <QWidget>

class AssistantProcess;
class SpectrumWidget;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void toggleAssistant();
    void onRunningChanged(bool running);
    void onLogLine(const QString &line);
    void onVolumeChanged(int value);
    void applyVolume();

private:
    void detectPulseServer();
    int querySinkVolume() const;
    bool runPactl(const QStringList &arguments, QByteArray *output = nullptr) const;

    AssistantProcess *m_assistant;
    SpectrumWidget *m_spectrum;
    QPushButton *m_toggleButton;
    QSlider *m_volumeSlider;
    QLabel *m_volumeLabel;
    QLabel *m_statusLabel;
    QLabel *m_userLabel;
    QLabel *m_aiLabel;
    QTimer *m_volumeTimer;
    QString m_pulseServer;
};

#endif // MAINWINDOW_H
