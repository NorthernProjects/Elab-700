#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class GalleryModel;

// "Avant/après" viewer: pick two photos from the current group's gallery
// and see them side by side. Deliberately simple (no slider/overlay
// blending) — just enough to compare two observations at a glance.
class ComparisonDialog : public QDialog {
    Q_OBJECT

public:
    ComparisonDialog(GalleryModel *model, QWidget *parent = nullptr);

private slots:
    void updatePreviews();

private:
    QComboBox *m_leftCombo;
    QComboBox *m_rightCombo;
    QLabel *m_leftPreview;
    QLabel *m_rightPreview;
};
