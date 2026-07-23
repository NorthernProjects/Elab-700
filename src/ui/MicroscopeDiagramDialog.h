#pragma once

#include <QDialog>
#include <QWidget>

// Simplified, labeled side-view schematic of a trinocular compound
// microscope (original line-drawing illustration, not a photo of the real
// OMAX 83S — drawn to teach the vocabulary of the parts a student is
// actually touching, not to be a precise rendering of this one product).
class MicroscopeDiagramWidget : public QWidget {
    Q_OBJECT

public:
    explicit MicroscopeDiagramWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

class MicroscopeDiagramDialog : public QDialog {
    Q_OBJECT

public:
    explicit MicroscopeDiagramDialog(QWidget *parent = nullptr);
};
