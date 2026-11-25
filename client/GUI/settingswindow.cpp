#include "settingswindow.h"
#include "ui_settingswindow.h"

#include <QAbstractItemView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidgetItem>

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::SettingsWindow)
{
    ui->setupUi(this);
    ConnectAll();

    QString error;
    if (!LoadSettings(&error)) {
        SetStatus(error, true);
        ui->SaveButton->setEnabled(false);
    }
}

SettingsWindow::~SettingsWindow()
{
    delete ui;
}

void SettingsWindow::ConnectAll()
{
    connect(ui->SaveButton, &QPushButton::clicked, this, &SettingsWindow::SaveButtonClicked);
    connect(ui->CancelButton, &QPushButton::clicked, this, &SettingsWindow::CancelButtonClicked);

    connect(ui->listKeyCombo, &QComboBox::currentTextChanged,
            this, &SettingsWindow::CurrentListKeyChanged);
    connect(ui->AddListButton, &QPushButton::clicked, this, &SettingsWindow::AddListValue);
    connect(ui->RemoveListButton, &QPushButton::clicked, this, &SettingsWindow::RemoveListValue);
    connect(ui->MoveUpButton, &QPushButton::clicked, this, &SettingsWindow::MoveListValueUp);
    connect(ui->MoveDownButton, &QPushButton::clicked, this, &SettingsWindow::MoveListValueDown);

    connect(ui->AddMedicineButton, &QPushButton::clicked, this, &SettingsWindow::AddMedicine);
    connect(ui->RemoveMedicineButton, &QPushButton::clicked, this, &SettingsWindow::RemoveMedicine);
    connect(ui->AddExaminationButton, &QPushButton::clicked, this, &SettingsWindow::AddExamination);
    connect(ui->RemoveExaminationButton, &QPushButton::clicked, this, &SettingsWindow::RemoveExamination);
    connect(ui->AddConsultationButton, &QPushButton::clicked, this, &SettingsWindow::AddConsultation);
    connect(ui->RemoveConsultationButton, &QPushButton::clicked, this, &SettingsWindow::RemoveConsultation);
    connect(ui->AddManipulationButton, &QPushButton::clicked, this, &SettingsWindow::AddManipulation);
    connect(ui->RemoveManipulationButton, &QPushButton::clicked, this, &SettingsWindow::RemoveManipulation);

    for (QTableWidget* table : {ui->medicineTable, ui->examinationTable,
                                ui->consultationTable, ui->manipulationTable}) {
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
    }
}

bool SettingsWindow::LoadSettings(QString* error)
{
    if (!storage_.Load(error)) {
        source_data_ = QJsonObject();
        return false;
    }

    source_data_ = storage_.Data();

    string_editors_.clear();
    list_values_.clear();
    current_list_key_.clear();

    BuildStringEditors();
    BuildListEditors();
    BuildDictionaryTable(DepartmentInfoStorage::AdditionalMedicinesKey(), ui->medicineTable);
    BuildDictionaryTable(DepartmentInfoStorage::ExaminationsKey(), ui->examinationTable);
    BuildDictionaryTable(DepartmentInfoStorage::ConsultationsKey(), ui->consultationTable);
    BuildDictionaryTable(DepartmentInfoStorage::ManipulationsKey(), ui->manipulationTable);

    loaded_ = true;
    SetStatus(QString("Файл настроек загружен: %1").arg(DepartmentInfoStorage::DefaultFilePath()));
    return true;
}

void SettingsWindow::BuildStringEditors()
{
    auto add_editor = [this](const QString& key) {
        auto* label = new QLabel(key, ui->commonTab);
        auto* editor = new QLineEdit(ui->commonTab);
        editor->setText(source_data_.value(key).toString());
        ui->commonLayout->addRow(label, editor);
        string_editors_.insert(key, editor);
    };

    for (const QString& key : DepartmentInfoStorage::KnownStringKeys()) {
        add_editor(key);
    }

    QStringList handled = DepartmentInfoStorage::KnownStringKeys();
    handled.append(DepartmentInfoStorage::KnownArrayKeys());
    handled.append(DepartmentInfoStorage::KnownDictionaryKeys());

    for (const QString& key : source_data_.keys()) {
        if (handled.contains(key)) {
            continue;
        }
        if (source_data_.value(key).isString()) {
            add_editor(key);
        }
    }
}

void SettingsWindow::BuildListEditors()
{
    QStringList keys = DepartmentInfoStorage::KnownArrayKeys();
    for (const QString& key : source_data_.keys()) {
        if (!keys.contains(key) && source_data_.value(key).isArray()) {
            keys.append(key);
        }
    }

    for (const QString& key : keys) {
        QStringList values;
        if (source_data_.value(key).isArray()) {
            const QJsonArray array = source_data_.value(key).toArray();
            for (const QJsonValue& value : array) {
                values.append(value.toString());
            }
        }
        list_values_.insert(key, values);
    }

    QSignalBlocker blocker(ui->listKeyCombo);
    ui->listKeyCombo->clear();
    ui->listKeyCombo->addItems(keys);

    if (!keys.isEmpty()) {
        current_list_key_ = keys.first();
    }
    ShowCurrentList();
}

void SettingsWindow::BuildDictionaryTable(const QString& key, QTableWidget* table)
{
    table->setRowCount(0);

    if (!source_data_.value(key).isObject()) {
        return;
    }

    const QJsonObject dictionary = source_data_.value(key).toObject();
    for (const QString& name : dictionary.keys()) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(name));
        table->setItem(row, 1, new QTableWidgetItem(dictionary.value(name).toString()));
    }

    table->resizeColumnsToContents();
}

void SettingsWindow::ShowCurrentList()
{
    ui->listWidget->clear();

    const QStringList values = list_values_.value(current_list_key_);
    for (const QString& value : values) {
        auto* item = new QListWidgetItem(value);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->listWidget->addItem(item);
    }
}

void SettingsWindow::StoreCurrentList()
{
    if (current_list_key_.isEmpty()) {
        return;
    }

    QStringList values;
    for (int i = 0; i < ui->listWidget->count(); ++i) {
        const QString value = ui->listWidget->item(i)->text().trimmed();
        if (!value.isEmpty()) {
            values.append(value);
        }
    }
    list_values_[current_list_key_] = values;
}

void SettingsWindow::CurrentListKeyChanged(const QString& key)
{
    if (key == current_list_key_) {
        return;
    }

    StoreCurrentList();
    current_list_key_ = key;
    ShowCurrentList();
}

void SettingsWindow::AddListValue()
{
    bool ok = false;
    const QString value = QInputDialog::getText(this, "Добавить значение",
                                                "Значение:", QLineEdit::Normal,
                                                QString(), &ok);
    if (!ok || value.trimmed().isEmpty()) {
        return;
    }

    auto* item = new QListWidgetItem(value.trimmed());
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    ui->listWidget->addItem(item);
}

void SettingsWindow::RemoveListValue()
{
    const int row = ui->listWidget->currentRow();
    if (row >= 0) {
        delete ui->listWidget->takeItem(row);
    }
}

void SettingsWindow::MoveListValueUp()
{
    const int row = ui->listWidget->currentRow();
    if (row <= 0) {
        return;
    }

    QListWidgetItem* item = ui->listWidget->takeItem(row);
    ui->listWidget->insertItem(row - 1, item);
    ui->listWidget->setCurrentRow(row - 1);
}

void SettingsWindow::MoveListValueDown()
{
    const int row = ui->listWidget->currentRow();
    if (row < 0 || row >= ui->listWidget->count() - 1) {
        return;
    }

    QListWidgetItem* item = ui->listWidget->takeItem(row);
    ui->listWidget->insertItem(row + 1, item);
    ui->listWidget->setCurrentRow(row + 1);
}

void SettingsWindow::AddDictionaryItem(const QString& key, QTableWidget* table, const QString& title)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, title,
                                               "Название:", QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    const auto name_matches = table->findItems(name.trimmed(), Qt::MatchExactly);
    for (QTableWidgetItem* match : name_matches) {
        if (match->column() == 0) {
            QMessageBox::warning(this, title, QString("\"%1\" уже есть в словаре \"%2\".")
                                                   .arg(name.trimmed(), key));
            return;
        }
    }

    const QString description = QInputDialog::getText(this, title,
                                                      "Типичное описание:",
                                                      QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }

    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(name.trimmed()));
    table->setItem(row, 1, new QTableWidgetItem(description.trimmed()));
}

void SettingsWindow::RemoveDictionaryItem(QTableWidget* table)
{
    const int row = table->currentRow();
    if (row >= 0) {
        table->removeRow(row);
    }
}

void SettingsWindow::AddMedicine()
{
    AddDictionaryItem(DepartmentInfoStorage::AdditionalMedicinesKey(),
                      ui->medicineTable, "Добавить препарат");
}

void SettingsWindow::RemoveMedicine()
{
    RemoveDictionaryItem(ui->medicineTable);
}

void SettingsWindow::AddExamination()
{
    AddDictionaryItem(DepartmentInfoStorage::ExaminationsKey(),
                      ui->examinationTable, "Добавить обследование");
}

void SettingsWindow::RemoveExamination()
{
    RemoveDictionaryItem(ui->examinationTable);
}

void SettingsWindow::AddConsultation()
{
    AddDictionaryItem(DepartmentInfoStorage::ConsultationsKey(),
                      ui->consultationTable, "Добавить консультацию");
}

void SettingsWindow::RemoveConsultation()
{
    RemoveDictionaryItem(ui->consultationTable);
}

void SettingsWindow::AddManipulation()
{
    AddDictionaryItem(DepartmentInfoStorage::ManipulationsKey(),
                      ui->manipulationTable, "Добавить манипуляцию");
}

void SettingsWindow::RemoveManipulation()
{
    RemoveDictionaryItem(ui->manipulationTable);
}

QJsonObject SettingsWindow::CollectDictionary(const QString& key, QTableWidget* table, QString* error) const
{
    QJsonObject result;

    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* name_item = table->item(row, 0);
        QTableWidgetItem* description_item = table->item(row, 1);
        const QString name = name_item ? name_item->text().trimmed() : QString();
        const QString description = description_item ? description_item->text().trimmed() : QString();

        if (name.isEmpty()) {
            if (!description.isEmpty()) {
                if (error) {
                    *error = QString("В словаре \"%1\" строка %2 без названия.").arg(key).arg(row + 1);
                }
            }
            continue;
        }
        if (result.contains(name)) {
            if (error) {
                *error = QString("\"%1\" добавлен дважды в словаре \"%2\".").arg(name, key);
            }
            return QJsonObject();
        }
        result.insert(name, description);
    }

    return result;
}

bool SettingsWindow::CollectSettings(QJsonObject* data, QString* error) const
{
    QJsonObject result = source_data_;

    for (auto it = string_editors_.constBegin(); it != string_editors_.constEnd(); ++it) {
        if (it.value()) {
            result.insert(it.key(), it.value()->text().trimmed());
        }
    }

    for (auto it = list_values_.constBegin(); it != list_values_.constEnd(); ++it) {
        QJsonArray array;
        for (const QString& value : it.value()) {
            const QString trimmed = value.trimmed();
            if (!trimmed.isEmpty()) {
                array.append(trimmed);
            }
        }
        result.insert(it.key(), array);
    }

    QString dictionary_error;
    QJsonObject medicines = CollectDictionary(DepartmentInfoStorage::AdditionalMedicinesKey(),
                                               ui->medicineTable, &dictionary_error);
    if (!dictionary_error.isEmpty()) {
        if (error) {
            *error = dictionary_error;
        }
        return false;
    }
    QJsonObject examinations = CollectDictionary(DepartmentInfoStorage::ExaminationsKey(),
                                                  ui->examinationTable, &dictionary_error);
    if (!dictionary_error.isEmpty()) {
        if (error) {
            *error = dictionary_error;
        }
        return false;
    }
    QJsonObject consultations = CollectDictionary(DepartmentInfoStorage::ConsultationsKey(),
                                                   ui->consultationTable, &dictionary_error);
    if (!dictionary_error.isEmpty()) {
        if (error) {
            *error = dictionary_error;
        }
        return false;
    }
    QJsonObject manipulations = CollectDictionary(DepartmentInfoStorage::ManipulationsKey(),
                                                   ui->manipulationTable, &dictionary_error);
    if (!dictionary_error.isEmpty()) {
        if (error) {
            *error = dictionary_error;
        }
        return false;
    }
    result.insert(DepartmentInfoStorage::AdditionalMedicinesKey(), medicines);
    result.insert(DepartmentInfoStorage::ExaminationsKey(), examinations);
    result.insert(DepartmentInfoStorage::ConsultationsKey(), consultations);
    result.insert(DepartmentInfoStorage::ManipulationsKey(), manipulations);

    *data = result;
    return DepartmentInfoStorage::Validate(result, error);
}

void SettingsWindow::SetStatus(const QString& message, bool is_error)
{
    ui->statusLabel->setText(message);
    ui->statusLabel->setStyleSheet(is_error ? QStringLiteral("color: #b00020;")
                                            : QStringLiteral("color: #2e7d32;"));
}

void SettingsWindow::SaveButtonClicked()
{
    if (!loaded_) {
        return;
    }

    StoreCurrentList();

    QJsonObject data;
    QString error;
    if (!CollectSettings(&data, &error)) {
        SetStatus(error, true);
        QMessageBox::warning(this, "Настройки", error);
        return;
    }

    if (!storage_.Save(data, &error)) {
        SetStatus(error, true);
        QMessageBox::warning(this, "Настройки", error);
        return;
    }

    source_data_ = data;
    SetStatus("Настройки сохранены.");
    QMessageBox::information(this, "Настройки",
                             "Настройки сохранены.\n\n"
                             "Перезапустите программу, чтобы изменения списков "
                             "появились в полях ввода.");
    accept();
}

void SettingsWindow::CancelButtonClicked()
{
    reject();
}
