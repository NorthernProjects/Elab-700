#include "StartupSelectionDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

StartupSelectionDialog::StartupSelectionDialog(const QVector<SchoolClass> &classes, QWidget *parent)
    : QDialog(parent), m_classes(classes)
{
    setWindowTitle(QStringLiteral("Se connecter"));
    setModal(true);
    resize(420, 300);

    auto *root = new QVBoxLayout(this);

    auto *logoLabel = new QLabel(this);
    logoLabel->setAlignment(Qt::AlignCenter);
    const QPixmap logo(QStringLiteral(":/branding/logo.png"));
    if (!logo.isNull())
        logoLabel->setPixmap(logo.scaledToWidth(200, Qt::SmoothTransformation));
    root->addWidget(logoLabel);

    auto *introLabel = new QLabel(QStringLiteral("Choisis ta classe et ton groupe, puis entre le mot de passe de ta classe."), this);
    introLabel->setAlignment(Qt::AlignCenter);
    introLabel->setWordWrap(true);
    root->addWidget(introLabel);

    auto *form = new QFormLayout();
    m_classCombo = new QComboBox(this);
    m_groupCombo = new QComboBox(this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    for (const SchoolClass &schoolClass : m_classes)
        m_classCombo->addItem(schoolClass.name);
    form->addRow(QStringLiteral("Classe"), m_classCombo);
    form->addRow(QStringLiteral("Groupe"), m_groupCombo);
    form->addRow(QStringLiteral("Mot de passe"), m_passwordEdit);
    root->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #ff5c6c;"));
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    auto *buttons = new QDialogButtonBox(this);
    auto *startButton = buttons->addButton(QStringLiteral("Se connecter"), QDialogButtonBox::AcceptRole);
    root->addWidget(buttons);

    connect(m_classCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StartupSelectionDialog::onClassChanged);
    connect(startButton, &QPushButton::clicked, this, &StartupSelectionDialog::onConnectClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &StartupSelectionDialog::onConnectClicked);

    onClassChanged(m_classCombo->currentIndex());
}

void StartupSelectionDialog::onClassChanged(int index)
{
    m_groupCombo->clear();
    if (index < 0 || index >= m_classes.size())
        return;
    m_groupCombo->addItems(m_classes.at(index).groups);
}

void StartupSelectionDialog::onConnectClicked()
{
    const int index = m_classCombo->currentIndex();
    if (index < 0 || index >= m_classes.size())
        return;

    const SchoolClass &schoolClass = m_classes.at(index);
    if (!schoolClass.password.isEmpty() && m_passwordEdit->text() != schoolClass.password) {
        m_errorLabel->setText(QStringLiteral("Mot de passe incorrect."));
        m_errorLabel->show();
        return;
    }

    accept();
}

QString StartupSelectionDialog::selectedClassName() const
{
    return m_classCombo->currentText();
}

QString StartupSelectionDialog::selectedGroupName() const
{
    return m_groupCombo->currentText();
}
