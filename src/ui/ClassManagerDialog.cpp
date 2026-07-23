#include "ClassManagerDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kMaxGroupsPerClass = 6;
}

ClassManagerDialog::ClassManagerDialog(AppSettings *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings), m_classes(settings->classes())
{
    setWindowTitle(QStringLiteral("Classes et groupes"));
    resize(560, 420);

    auto *root = new QVBoxLayout(this);
    auto *splitLayout = new QHBoxLayout();

    auto *classColumn = new QVBoxLayout();
    m_classList = new QListWidget(this);
    for (const SchoolClass &schoolClass : m_classes)
        m_classList->addItem(schoolClass.name);
    classColumn->addWidget(m_classList, 1);

    auto *classButtonsRow = new QHBoxLayout();
    auto *addClassButton = new QPushButton(QStringLiteral("+ Classe"), this);
    auto *removeClassButton = new QPushButton(QStringLiteral("- Classe"), this);
    classButtonsRow->addWidget(addClassButton);
    classButtonsRow->addWidget(removeClassButton);
    classColumn->addLayout(classButtonsRow);

    splitLayout->addLayout(classColumn, 1);

    auto *detailColumn = new QVBoxLayout();
    auto *form = new QFormLayout();
    m_nameEdit = new QLineEdit(this);
    m_teacherNameEdit = new QLineEdit(this);
    m_teacherNameEdit->setPlaceholderText(QStringLiteral("Prénom Nom"));
    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setPlaceholderText(QStringLiteral("courriel.enseignant@ecole.qc.ca"));
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(QStringLiteral("mot de passe partagé de la classe"));
    form->addRow(QStringLiteral("Nom de la classe"), m_nameEdit);
    form->addRow(QStringLiteral("Nom de l'enseignant"), m_teacherNameEdit);
    form->addRow(QStringLiteral("Courriel de l'enseignant"), m_emailEdit);
    form->addRow(QStringLiteral("Mot de passe de la classe"), m_passwordEdit);
    detailColumn->addLayout(form);

    auto *groupGroup = new QGroupBox(QStringLiteral("Groupes (6 maximum)"), this);
    auto *groupLayout = new QVBoxLayout(groupGroup);
    m_groupList = new QListWidget(groupGroup);
    groupLayout->addWidget(m_groupList, 1);
    auto *groupButtonsRow = new QHBoxLayout();
    auto *addGroupButton = new QPushButton(QStringLiteral("+ Groupe"), groupGroup);
    auto *removeGroupButton = new QPushButton(QStringLiteral("- Groupe"), groupGroup);
    groupButtonsRow->addWidget(addGroupButton);
    groupButtonsRow->addWidget(removeGroupButton);
    groupLayout->addLayout(groupButtonsRow);
    detailColumn->addWidget(groupGroup, 1);

    splitLayout->addLayout(detailColumn, 2);
    root->addLayout(splitLayout, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(addClassButton, &QPushButton::clicked, this, &ClassManagerDialog::onAddClass);
    connect(removeClassButton, &QPushButton::clicked, this, &ClassManagerDialog::onRemoveClass);
    connect(m_classList, &QListWidget::currentRowChanged, this, &ClassManagerDialog::onClassSelectionChanged);
    connect(addGroupButton, &QPushButton::clicked, this, &ClassManagerDialog::onAddGroup);
    connect(removeGroupButton, &QPushButton::clicked, this, &ClassManagerDialog::onRemoveGroup);
    connect(buttons, &QDialogButtonBox::accepted, this, &ClassManagerDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadClassIntoEditor(-1);
    if (!m_classes.isEmpty())
        m_classList->setCurrentRow(0);
}

void ClassManagerDialog::loadClassIntoEditor(int index)
{
    if (index < 0 || index >= m_classes.size()) {
        m_nameEdit->clear();
        m_teacherNameEdit->clear();
        m_emailEdit->clear();
        m_passwordEdit->clear();
        m_groupList->clear();
        m_nameEdit->setEnabled(false);
        m_teacherNameEdit->setEnabled(false);
        m_emailEdit->setEnabled(false);
        m_passwordEdit->setEnabled(false);
        m_groupList->setEnabled(false);
        return;
    }

    m_nameEdit->setEnabled(true);
    m_teacherNameEdit->setEnabled(true);
    m_emailEdit->setEnabled(true);
    m_passwordEdit->setEnabled(true);
    m_groupList->setEnabled(true);

    const SchoolClass &schoolClass = m_classes.at(index);
    m_nameEdit->setText(schoolClass.name);
    m_teacherNameEdit->setText(schoolClass.teacherName);
    m_emailEdit->setText(schoolClass.teacherEmail);
    m_passwordEdit->setText(schoolClass.password);
    m_groupList->clear();
    m_groupList->addItems(schoolClass.groups);
}

void ClassManagerDialog::storeEditorIntoClass(int index)
{
    if (index < 0 || index >= m_classes.size())
        return;

    SchoolClass &schoolClass = m_classes[index];
    schoolClass.name = m_nameEdit->text().trimmed();
    schoolClass.teacherName = m_teacherNameEdit->text().trimmed();
    schoolClass.teacherEmail = m_emailEdit->text().trimmed();
    schoolClass.password = m_passwordEdit->text();
    schoolClass.groups.clear();
    for (int i = 0; i < m_groupList->count(); ++i)
        schoolClass.groups.append(m_groupList->item(i)->text());

    if (m_classList->item(index))
        m_classList->item(index)->setText(schoolClass.name);
}

void ClassManagerDialog::onClassSelectionChanged()
{
    storeEditorIntoClass(m_currentIndex);
    m_currentIndex = m_classList->currentRow();
    loadClassIntoEditor(m_currentIndex);
}

void ClassManagerDialog::onAddClass()
{
    storeEditorIntoClass(m_currentIndex);

    SchoolClass newClass;
    newClass.name = QStringLiteral("Nouvelle classe");
    m_classes.append(newClass);
    m_classList->addItem(newClass.name);
    m_classList->setCurrentRow(m_classList->count() - 1);
}

void ClassManagerDialog::onRemoveClass()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_classes.size())
        return;

    const auto answer = QMessageBox::question(this, QStringLiteral("Supprimer"),
        QStringLiteral("Supprimer la classe « %1 » ?").arg(m_classes.at(m_currentIndex).name));
    if (answer != QMessageBox::Yes)
        return;

    m_classes.remove(m_currentIndex);
    delete m_classList->takeItem(m_currentIndex);
    m_currentIndex = -1;
    onClassSelectionChanged();
}

void ClassManagerDialog::onAddGroup()
{
    if (m_currentIndex < 0)
        return;
    if (m_groupList->count() >= kMaxGroupsPerClass) {
        QMessageBox::information(this, QStringLiteral("Groupes"),
            QStringLiteral("Maximum %1 groupes par classe.").arg(kMaxGroupsPerClass));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Nouveau groupe"),
                                                QStringLiteral("Nom du groupe :"), QLineEdit::Normal,
                                                QStringLiteral("Groupe %1").arg(m_groupList->count() + 1), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    m_groupList->addItem(name.trimmed());
}

void ClassManagerDialog::onRemoveGroup()
{
    const int row = m_groupList->currentRow();
    if (row < 0)
        return;
    delete m_groupList->takeItem(row);
}

void ClassManagerDialog::onSave()
{
    storeEditorIntoClass(m_currentIndex);
    m_settings->setClasses(m_classes);
    accept();
}
