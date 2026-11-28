#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QDialog>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "Common/departmentinfostorage.h"
#include "Common/departmentinforepository.h"

class QLineEdit;
class QTableWidget;

namespace Ui {
class SettingsWindow;
}

class SettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);
    ~SettingsWindow();

private slots:
    void SaveButtonClicked();
    void CancelButtonClicked();
    void CurrentListKeyChanged(const QString& key);
    void AddListValue();
    void RemoveListValue();
    void MoveListValueUp();
    void MoveListValueDown();
    void AddMedicine();
    void RemoveMedicine();
    void AddExamination();
    void RemoveExamination();
    void AddConsultation();
    void RemoveConsultation();
    void AddManipulation();
    void RemoveManipulation();

private:
    void ConnectAll();
    bool LoadSettings(QString* error);
    void BuildStringEditors();
    void BuildListEditors();
    void BuildDictionaryTable(const QString& key, QTableWidget* table);
    void AddDictionaryItem(const QString& key, QTableWidget* table, const QString& title);
    void RemoveDictionaryItem(QTableWidget* table);
    void ShowCurrentList();
    void StoreCurrentList();
    QJsonObject CollectDictionary(const QString& key, QTableWidget* table, QString* error) const;
    bool CollectSettings(QJsonObject* data, QString* error) const;
    void SetStatus(const QString& message, bool is_error = false);

    Ui::SettingsWindow* ui;
    DepartmentInfoStorage storage_;
    DepartmentInfoRepository repository_;
    QJsonObject source_data_;
    QHash<QString, QLineEdit*> string_editors_;
    QHash<QString, QStringList> list_values_;
    QString current_list_key_;
    bool loaded_ = false;
};

#endif // SETTINGSWINDOW_H
