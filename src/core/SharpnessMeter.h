#pragma once

#include <QImage>

// Focus aid for the live view. Absolute sharpness numbers mean nothing
// across objectives (a 40x view of smooth cells never has the raw contrast
// of a 4x view of a leaf, even in perfect focus), so the meter reports each
// frame RELATIVE to the best focus seen recently: 100 = as sharp as it has
// been lately, lower = softer than that. Turn the focus knob through the
// optimum and the bar peaks there, whatever the objective.
class SharpnessMeter {
public:
    // Feed every displayed frame; returns the smoothed 0-100 score. Does the
    // (cheap but not free) analysis only on every few frames and returns the
    // last value in between.
    double update(const QImage &frame);

    // New scene/camera: forget the remembered peak.
    void reset();

    double score() const { return m_score; }

    // Resolution-independent, brightness-normalized detail measure of one
    // frame (higher = more fine detail). Exposed for testing.
    static double detailMeasure(const QImage &frame);

private:
    double m_peak = 0.0;
    double m_score = 0.0;
    int m_frameCounter = 0;
};
