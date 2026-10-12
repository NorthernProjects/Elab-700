#include "SharpnessMeter.h"

#include <algorithm>
#include <cmath>

namespace {
// Analysis runs on a fixed-size copy of the frame's center, so a 640x480 and
// a 2592x1944 stream of the same view score alike, and the cost per frame
// stays small whatever the camera resolution.
constexpr int kAnalysisWidth = 320;
// Only the middle of the picture decides focus (edges often hold dust or
// vignetting that stays soft while the subject is sharp).
constexpr double kCenterFraction = 0.6;
constexpr int kAnalyzeEveryNthFrame = 3;
// The remembered "best" fades slowly (per analysis, ~7-10/s: half-life a
// few tens of seconds) so changing sample/objective eventually re-bases it.
constexpr double kPeakDecayPerAnalysis = 0.995;
// A featureless frame must never read as "perfectly sharp" just because it
// is the best of nothing.
constexpr double kMinPeak = 0.02;
constexpr double kSmoothing = 0.35;
// Below this mean brightness (0-255) the picture is mostly sensor noise and
// "sharpness" is meaningless — report no detail instead of a confident score.
constexpr double kMinUsableBrightness = 16.0;
}

double SharpnessMeter::detailMeasure(const QImage &frame)
{
    if (frame.isNull() || frame.width() < 16 || frame.height() < 16)
        return 0.0;

    const int cropW = static_cast<int>(frame.width() * kCenterFraction);
    const int cropH = static_cast<int>(frame.height() * kCenterFraction);
    const QRect center((frame.width() - cropW) / 2, (frame.height() - cropH) / 2, cropW, cropH);

    const QImage small = frame.copy(center)
                             .scaledToWidth(kAnalysisWidth, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_Grayscale8);
    const int w = small.width();
    const int h = small.height();
    if (w < 3 || h < 3)
        return 0.0;

    double lapEnergy = 0.0;
    double intensity = 0.0;
    int count = 0;
    for (int y = 1; y < h - 1; ++y) {
        const uchar *row = small.constScanLine(y);
        const uchar *up = small.constScanLine(y - 1);
        const uchar *down = small.constScanLine(y + 1);
        for (int x = 1; x < w - 1; ++x) {
            const int lap = 4 * row[x] - row[x - 1] - row[x + 1] - up[x] - down[x];
            lapEnergy += static_cast<double>(lap) * lap;
            intensity += row[x];
            ++count;
        }
    }
    if (count == 0)
        return 0.0;

    if (intensity / count < kMinUsableBrightness)
        return 0.0;

    // Dividing by the mean brightness keeps an exposure change from looking
    // like a focus change (a brighter frame has proportionally larger edges).
    const double meanIntensity = std::max(intensity / count, 24.0);
    return std::sqrt(lapEnergy / count) / meanIntensity;
}

void SharpnessMeter::reset()
{
    m_peak = 0.0;
    m_score = 0.0;
    m_frameCounter = 0;
}

double SharpnessMeter::update(const QImage &frame)
{
    if (m_frameCounter++ % kAnalyzeEveryNthFrame != 0)
        return m_score;

    const double detail = detailMeasure(frame);
    m_peak = std::max({m_peak * kPeakDecayPerAnalysis, detail, kMinPeak});
    const double relative = std::min(100.0, 100.0 * detail / m_peak);
    m_score = kSmoothing * relative + (1.0 - kSmoothing) * m_score;
    return m_score;
}
