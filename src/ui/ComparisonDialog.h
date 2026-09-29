#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class GalleryModel;
class RevealCompareWidget;

// "Avant/après" viewer: pick two photos from the current group's gallery.
// Two modes, toggled by m_modeButton: side by side (the original, simplest
// view) and a reveal slider (RevealCompareWidget) for a more visual
// comparison — drag the divider to wipe between the two images.
class ComparisonDialog : public QDialog {
    Q_OBJECT

public:
    ComparisonDialog(GalleryModel *model, QWidget *parent = nullptr);

private slots:
    void updatePreviews();
    void toggleMode();

private:
    QComboBox *m_leftCombo;
    QComboBox *m_rightCombo;
    QLabel *m_leftPreview;
    QLabel *m_rightPreview;
    QStackedWidget *m_stack;
    RevealCompareWidget *m_revealWidget;
    QPushButton *m_modeButton;
    bool m_revealMode = false;
};
