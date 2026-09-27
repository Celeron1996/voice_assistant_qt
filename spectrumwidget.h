#ifndef SPECTRUMWIDGET_H
#define SPECTRUMWIDGET_H

#include <QByteArray>
#include <QVector>
#include <QWidget>

// 音频频谱动效：接收麦克风/TTS PCM，在 C++ 侧做 FFT，绘制对数频段柱状图。
// 板子单核跑不动 Python FFT（256 点约 39ms），所以 DSP 放在这里（微秒级）。
class SpectrumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

    // sampleRate: 16000 = 麦克风，24000 = TTS
    void pushAudio(const QByteArray &pcm, int sampleRate);

protected:
    void timerEvent(QTimerEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    static const int kFftSize = 256;
    static const int kBandCount = 24;

    void appendSamples(QVector<float> &buffer, const QByteArray &pcm);
    QVector<float> computeBands(const QVector<float> &buffer, int sampleRate) const;
    static void fft(float *re, float *im, int n);

    QVector<float> m_mic;
    QVector<float> m_tts;
    QVector<float> m_target;
    QVector<float> m_display;
    QVector<float> m_hann;
    int m_timerId;
};

#endif // SPECTRUMWIDGET_H
