#include "ComparisonDialog.h"

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "RevealCompareWidget.h"
#include "core/GalleryModel.h"

ComparisonDialog::ComparisonDialog(GalleryModel *model, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Comparer deux photos"));
    resize(900, 560);

    auto *root = new QVBoxLayout(this);

    auto *pickerRow = new QHBoxLayout();
    m_leftCombo = new QComboBox(this);
    m_rightCombo = new QComboBox(this);
    m_modeButton = new QPushButton(tr("Vue : curseur de révélation"), this);
    pickerRow->addWidget(m_leftCombo, 1);
    pickerRow->addWidget(m_rightCombo, 1);
    pickerRow->addWidget(m_modeButton);
    root->addLayout(pickerRow);

    m_stack = new QStackedWidget(this);

    auto *sideBySideContainer = new QWidget(this);
    auto *previewRow = new QHBoxLayout(sideBySideContainer);
    previewRow->setContentsMargins(0, 0, 0, 0);
    m_leftPreview = new QLabel(sideBySideContainer);
    m_rightPreview = new QLabel(sideBySideContainer);
    for (QLabel *preview : {m_leftPreview, m_rightPreview}) {
        preview->setAlignment(Qt::AlignCenter);
        preview->setMinimumSize(320, 240);
        preview->setStyleSheet(QStringLiteral("background-color: #05070a; border-radius: 8px;"));
    }
    previewRow->addWidget(m_leftPreview, 1);
    previewRow->addWidget(m_rightPreview, 1);

    m_revealWidget = new RevealCompareWidget(this);

    m_stack->addWidget(sideBySideContainer);
    m_stack->addWidget(m_revealWidget);
    root->addWidget(m_stack, 1);

    auto *closeButton = new QPushButton(tr("Fermer"), this);
    auto *bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(closeButton);
    root->addLayout(bottomRow);

    for (int row = 0; row < model->rowCount(); ++row) {
        const GalleryModel::Entry entry = model->entryAt(row);
        if (entry.isVideo)
            continue;
        const QString name = QFileInfo(entry.filePath).fileName();
        m_leftCombo->addItem(name, entry.filePath);
        m_rightCombo->addItem(name, entry.filePath);
    }
    if (m_leftCombo->count() > 0)
        m_leftCombo->setCurrentIndex(0);
    if (m_rightCombo->count() > 1)
        m_rightCombo->setCurrentIndex(1);

    connect(m_leftCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ComparisonDialog::updatePreviews);
    connect(m_rightCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ComparisonDialog::updatePreviews);
    connect(m_modeButton, &QPushButton::clicked, this, &ComparisonDialog::toggleMode);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    updatePreviews();
}

void ComparisonDialog::toggleMode()
{
    m_revealMode = !m_revealMode;
    m_stack->setCurrentIndex(m_revealMode ? 1 : 0);
    m_modeButton->setText(m_revealMode ? tr("Vue : côte à côte")
                                        : tr("Vue : curseur de révélation"));
}

void ComparisonDialog::updatePreviews()
{
    const QPixmap leftPixmap(m_leftCombo->currentData().toString());
    const QPixmap rightPixmap(m_rightCombo->currentData().toString());

    m_leftPreview->setPixmap(leftPixmap.isNull()
        ? QPixmap()
        : leftPixmap.scaled(m_leftPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_rightPreview->setPixmap(rightPixmap.isNull()
        ? QPixmap()
        : rightPixmap.scaled(m_rightPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_revealWidget->setImages(leftPixmap, rightPixmap);
}
