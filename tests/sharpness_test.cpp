// Checks the focus meter on synthetic "cell" pictures: more defocus must mean
// a lower score, brightness must not matter, dark noise and featureless
// frames must never read as sharp. Build with -DE_LAB_BUILD_TESTS=ON, run
// with ctest.
#include <QGuiApplication>
#include <QPainter>
#include <QRandomGenerator>

#include <cstdio>

#include "core/SharpnessMeter.h"

namespace {

int g_failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++g_failures;
}

QImage cellScene()
{
    QImage image(2592, 1944, QImage::Format_RGB888);
    image.fill(QColor(150, 190, 170));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QRandomGenerator rng(42);
    for (int i = 0; i < 400; ++i) {
        painter.setPen(QPen(QColor(30, 40, 40), 3));
        painter.setBrush(QColor(rng.bounded(60, 200), rng.bounded(100, 220), rng.bounded(100, 220)));
        painter.drawEllipse(QPointF(rng.bounded(2592), rng.bounded(1944)), rng.bounded(20, 90), rng.bounded(20, 90));
    }
    return image;
}

// Defocus approximation: shrink, then enlarge smoothly.
QImage defocused(const QImage &source, int factor)
{
    if (factor <= 1)
        return source;
    return source.scaled(source.width() / factor, source.height() / factor, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage dimmed(const QImage &source, double factor)
{
    QImage out = source.convertToFormat(QImage::Format_RGB888);
    for (int y = 0; y < out.height(); ++y) {
        uchar *line = out.scanLine(y);
        for (int x = 0; x < out.width() * 3; ++x)
            line[x] = static_cast<uchar>(line[x] * factor);
    }
    return out;
}

double settle(SharpnessMeter &meter, const QImage &frame)
{
    double score = 0.0;
    for (int i = 0; i < 24; ++i)
        score = meter.update(frame);
    return score;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QImage scene = cellScene();

    double previous = 1e9;
    bool monotonic = true;
    for (int factor : {1, 2, 4, 8, 16, 32}) {
        const double detail = SharpnessMeter::detailMeasure(defocused(scene, factor));
        monotonic = monotonic && detail < previous;
        previous = detail;
    }
    check(monotonic, "detail decreases steadily with defocus");

    const double sharp = SharpnessMeter::detailMeasure(scene);
    const double half = SharpnessMeter::detailMeasure(dimmed(scene, 0.5));
    check(std::abs(sharp - half) < 0.05 * sharp, "halving the brightness barely changes the measure");

    SharpnessMeter sweep;
    const double soft = settle(sweep, defocused(scene, 32));
    const double best = settle(sweep, scene);
    const double blurred = settle(sweep, defocused(scene, 8));
    check(best > 95.0, "best focus seen reads ~100");
    check(blurred < 40.0, "clearly defocused reads low after having seen the best focus");
    check(soft < best, "starting soft scores below the later sharp frame");

    SharpnessMeter flatMeter;
    QImage flat(640, 480, QImage::Format_RGB888);
    flat.fill(QColor(128, 128, 128));
    check(settle(flatMeter, flat) < 5.0, "a featureless frame never reads sharp");

    SharpnessMeter darkMeter;
    QImage dark(640, 480, QImage::Format_RGB888);
    QRandomGenerator noise(7);
    for (int y = 0; y < dark.height(); ++y) {
        uchar *line = dark.scanLine(y);
        for (int x = 0; x < dark.width() * 3; ++x)
            line[x] = static_cast<uchar>(noise.bounded(0, 9));
    }
    check(settle(darkMeter, dark) < 5.0, "a dark noisy frame never reads sharp");

    std::printf(g_failures ? "%d check(s) failed\n" : "all checks passed\n", g_failures);
    return g_failures ? 1 : 0;
}
