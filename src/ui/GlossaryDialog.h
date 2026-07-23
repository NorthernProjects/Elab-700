#pragma once

#include <QDialog>

// Reference dialog listing microscopy vocabulary (lame, lamelle, objectif,
// grossissement...) with simple definitions, so students can look up a word
// they don't know without leaving the app. Includes a live search field.
class GlossaryDialog : public QDialog {
    Q_OBJECT

public:
    explicit GlossaryDialog(QWidget *parent = nullptr);
};
