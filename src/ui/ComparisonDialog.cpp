#include "ComparisonDialog.h"

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/GalleryModel.h"

ComparisonDialog::ComparisonDialog(GalleryModel *model, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Comparer deux photos"));
    resize(900, 560);

    auto *root = new QVBoxLayout(this);

    auto *pickerRow = new QHBoxLayout();
    m_leftCombo = new QComboBox(this);
    m_rightCombo = new QComboBox(this);
    pickerRow->addWidget(m_leftCombo, 1);
    pickerRow->addWidget(m_rightCombo, 1);
    root->addLayout(pickerRow);

    auto *previewRow = new QHBoxLayout();
    m_leftPreview = new QLabel(this);
    m_rightPreview = new QLabel(this);
    for (QLabel *preview : {m_leftPreview, m_rightPreview}) {
        preview->setAlignment(Qt::AlignCenter);
        preview->setMinimumSize(320, 240);
        preview->setStyleSheet(QStringLiteral("background-color: #05070a; border-radius: 8px;"));
    }
    previewRow->addWidget(m_leftPreview, 1);
    previewRow->addWidget(m_rightPreview, 1);
    root->addLayout(previewRow, 1);

    auto *closeButton = new QPushButton(QStringLiteral("Fermer"), this);
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
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    updatePreviews();
}

void ComparisonDialog::updatePreviews()
{
    auto loadInto = [](QComboBox *combo, QLabel *label) {
        const QString path = combo->currentData().toString();
        const QPixmap pixmap(path);
        label->setPixmap(pixmap.isNull() ? QPixmap()
                                          : pixmap.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    };
    loadInto(m_leftCombo, m_leftPreview);
    loadInto(m_rightCombo, m_rightPreview);
}
