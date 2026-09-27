#include "spectrumwidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QTimerEvent>

#include <algorithm>
#include <cmath>

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
    , m_target(kBandCount, 0.0f)
    , m_display(kBandCount, 0.0f)
    , m_hann(kFftSize, 1.0f)
    , m_timerId(0)
{
    for (int i = 0; i < kFftSize; ++i)
        m_hann[i] = 0.5f - 0.5f * std::cos(2.0f * float(M_PI) * i / float(kFftSize - 1));

    m_timerId = startTimer(33); // ~30fps 平滑刷新
}

void SpectrumWidget::pushAudio(const QByteArray &pcm, int sampleRate)
{
    if (sampleRate <= 16000)
        appendSamples(m_mic, pcm);
    else
        appendSamples(m_tts, pcm);
}

void SpectrumWidget::appendSamples(QVector<float> &buffer, const QByteArray &pcm)
{
    const int count = pcm.size() / 2;
    if (count <= 0)
        return;

    const qint16 *samples = reinterpret_cast<const qint16 *>(pcm.constData());
    buffer.reserve(buffer.size() + count);
    for (int i = 0; i < count; ++i)
        buffer.append(float(samples[i]));

    if (buffer.size() > 8192)
        buffer.remove(0, buffer.size() - 8192);
}

void SpectrumWidget::fft(float *re, float *im, int n)
{
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j |= bit;
        if (i < j) {
            qSwap(re[i], re[j]);
            qSwap(im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const float ang = -2.0f * float(M_PI) / float(len);
        const float wr = std::cos(ang);
        const float wi = std::sin(ang);
        const int half = len / 2;
        for (int start = 0; start < n; start += len) {
            float cr = 1.0f;
            float ci = 0.0f;
            for (int k = start; k < start + half; ++k) {
                const float pr = re[k + half] * cr - im[k + half] * ci;
                const float pi = re[k + half] * ci + im[k + half] * cr;
                const float ur = re[k];
                const float ui = im[k];
                re[k] = ur + pr;
                im[k] = ui + pi;
                re[k + half] = ur - pr;
                im[k + half] = ui - pi;

                const float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

QVector<float> SpectrumWidget::computeBands(const QVector<float> &buffer, int sampleRate) const
{
    QVector<float> levels(kBandCount, 0.0f);
    if (buffer.size() < kFftSize)
        return levels;

    const int offset = buffer.size() - kFftSize;
    float re[kFftSize];
    float im[kFftSize];
    float peak = 0.0f;
    for (int i = 0; i < kFftSize; ++i) {
        const float sample = buffer.at(offset + i);
        peak = std::max(peak, std::fabs(sample));
        re[i] = sample * m_hann.at(i);
        im[i] = 0.0f;
    }
    if (peak < 120.0f)
        return levels;

    fft(re, im, kFftSize);

    const float binHz = float(sampleRate) / float(kFftSize);
    const float baseDb = 20.0f * std::log10(0.5f * float(kFftSize) * 16384.0f);
    for (int band = 0; band < kBandCount; ++band) {
        const float lo = 80.0f * std::pow(7000.0f / 80.0f, float(band) / float(kBandCount));
        const float hi = 80.0f * std::pow(7000.0f / 80.0f, float(band + 1) / float(kBandCount));
        const int first = std::max(1, int(lo / binHz));
        const int last = std::min(kFftSize / 2 - 1, std::max(first, int(hi / binHz)));
        float mag = 0.0f;
        for (int i = first; i <= last; ++i)
            mag = std::max(mag, std::hypot(re[i], im[i]));
        const float db = 20.0f * std::log10(mag + 1e-6f) - baseDb;
        levels[band] = std::min(1.0f, std::max(0.0f, (db + 80.0f) / 65.0f));
    }
    return levels;
}

void SpectrumWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timerId) {
        QWidget::timerEvent(event);
        return;
    }

    const QVector<float> mic = computeBands(m_mic, 16000);
    const QVector<float> tts = computeBands(m_tts, 24000);
    for (int i = 0; i < kBandCount; ++i) {
        m_target[i] = std::max(mic.at(i), tts.at(i));
        const float rate = m_target[i] > m_display[i] ? 0.55f : 0.15f;
        m_display[i] += (m_target[i] - m_display[i]) * rate;
    }
    update();
}

void SpectrumWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(12, 8, -12, -8);
    const qreal gap = 6.0;
    const qreal barW = (r.width() - gap * (kBandCount - 1)) / qreal(kBandCount);
    const qreal baseY = r.bottom() - 8.0;
    const qreal maxH = r.height() - 26.0;

    QLinearGradient gradient(0, r.top(), 0, r.bottom());
    gradient.setColorAt(0.0, QColor(0x38, 0xbd, 0xf8));
    gradient.setColorAt(0.55, QColor(0x63, 0x66, 0xf1));
    gradient.setColorAt(1.0, QColor(0xa8, 0x55, 0xf7));

    for (int i = 0; i < kBandCount; ++i) {
        const qreal level = m_display.at(i);
        const qreal h = 6.0 + level * maxH;
        const qreal x = r.left() + i * (barW + gap);

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(QRectF(x, baseY - h, barW, h), barW / 2.0, barW / 2.0);

        // 倒影，增加一点"频谱仪"的质感
        QLinearGradient mirror(0, baseY, 0, baseY + h * 0.4);
        QColor color(0x63, 0x66, 0xf1);
        color.setAlpha(60);
        mirror.setColorAt(0.0, color);
        color.setAlpha(0);
        mirror.setColorAt(1.0, color);
        painter.setBrush(mirror);
        painter.drawRoundedRect(QRectF(x, baseY + 5, barW, h * 0.4), barW / 2.0, barW / 2.0);
    }
}
