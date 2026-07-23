#pragma once

#include <QDialog>

// Static reference dialog explaining every icon/badge/button on the main
// screen — the interface has grown a fair number of small controls (grid,
// netteté, échelle, caméra, résolution...) that aren't all self-explanatory
// to a student seeing them for the first time.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);
};
