#ifndef DATABASE_H
#define DATABASE_H

#include <QMainWindow>
#include <QMessageBox>
#include <QDate>

#include <SQLTable/sqltable.h>
#include "Common/common.h"
#include "Common/diariesparser.h"
#include "GUI/addanaliseswindow.h"
#include "GUI/settingswindow.h"
#include "qevent.h"
#include "searchwindow.h"
#include "printwindow.h"
#include "ui_database.h"


namespace Ui {
class DataBase;
}

class QTimer;

class DataBase : public QMainWindow
{
    Q_OBJECT

public:
    explicit DataBase(QWidget *parent = nullptr);
    friend class SearchWindow;
    friend class PrintWindow;
    ~DataBase();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    //кнопки===========================================================
    void PrintButtonClicked();
    void SaveButtonClicked();
    void SearchButtonClicked();
    void AddAnalisesButtonClicked();

    //слоты для коннекта===============================================
    void SetGystology();
    void TryToParseBaseDiagnosisAndMKB();
    void TryToParseVnesnyaPrichinaTravmi();
    void TryToParseSoputstvyushieZabolevanie();
    void TryToParseOsloznenie();
    void FillVitalIfKnownDateofBirth();
    void on_OperationBegin_editingFinished();
    void on_OperationBegin2_editingFinished();
    void ParseDictionaryDescription(const QString& setting_name,
                                    const QString& variant_prefix,
                                    const QString& text_prefix);
    void UpdateCheckboxIfEditingText();
    void IsDataChangedFromLastPrint();
    void ChangeNameToCorrect();
    void ChangeDopAnalisesBotton();
    void ParseDescriptionToDopAnalises();
    // Функция для расчета дозировки и заполнения препаратов===========
    void TryToParseMedicineAdministrationMethod();
    void on_IncomeDate_editingFinished();
    void on_GepatitiText1_textChanged(const QString &arg1);
    void on_KalText1_textChanged(const QString &arg1);
    void on_EKGText_textChanged(const QString &arg1);
    void OutComeDateEditingFinished();
    void on_Weight_editingFinished();
    void on_action_triggered();
    void on_copy_triggered();
    void OpenSettingsWindow();
    void AutosaveTimeout();

    // void on_ParseDiaries_button_clicked();

private:
    Ui::DataBase *ui;

    //Функции===========================================================
    void FillForms(std::unordered_map<QString, QVariant>&& widgets_data);
    void SetCompletters();
    void ConnectAll();
    void ConnectButtons();
    void ConnectMedicines();
    void ConnectQObjectToCheckbox();
    void ConnectDopAnalisesWidgetsToDopAnalisesButton();
    void ConnectWidgetsToCheckIfDataChanged();
    void ConnectDopAnalisesComboBoxesToDescriptionParser();
    bool IsAllLinesEmpty(QCheckBox *check_box);
    void ChangeLine(QLineEdit* line_to_change, QString&& text);
    void ChangeText(QTextEdit* line_to_change, QString&& text);
    void ChangeCombobox(QComboBox* combobox_to_change, QString&& text);
    void ChangeDiagnosisText(QTextEdit* diagnosis_text);
    void CheckDopAnalises();

    const QString GetDiagnosisContext(const QString& context) const;

    int GetAge();

    //Структуры для доп функций==========================================
    SQLTable data_base_;
    MKBMap mkb_map_;
    WidgetHandler widget_handler_;
    QCompleter* operation_list_completer_ = nullptr;
    SettingsSetter settings_setter_;
    MedicinesHandler medicines_handler_;
    DiariesParser diaries_parser_;

    //Дополнительные окна================================================
    // SearchWindow search_window_;
    AddAnalisesWindow dop_analises_window;
    PrintWindow* print_window = nullptr;
    SearchWindow* search_window = nullptr;

    //дополнительные переменные==========================================
    std::optional<int> patient_id_ = std::nullopt;
    bool is_saved = false;
    bool dates_is_filled = false;
    bool is_changed_from_last_print = false;

    //настройка базы
    void SetSettings();

    // автосохранение в PostgreSQL через минуту после последнего изменения
    void ScheduleAutosave();
    QTimer* autosave_timer_ = nullptr;
    static constexpr int kAutosaveDelayMs = 60 * 1000;

};

#endif // DATABASE_H
