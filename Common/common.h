#ifndef COMMON_H
#define COMMON_H

#include <QCheckBox>
#include <QComboBox>
#include <QTabWidget>
#include <QObject>
#include <QWidget>
#include <QBoxLayout>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QTextEdit>


// #include "JSON/json.h"
// #include <sstream>

#include <unordered_map>
#include <vector>


class WidgetHandler {
public:
    WidgetHandler();
    ~WidgetHandler();
    //работа с базой данных
    void FillForms(const std::unordered_map<QString, QVariant>& widgets_data);

    //получение и выдача информации
    const std::vector<QObject*>& GetAllDates();
    const std::unordered_map<QCheckBox*, std::vector<QObject*>>& GetCheckboxesAndWidgets();
    std::unordered_map<QObject*, QCheckBox*>& GetWidgetsAndCheckboxes();
    std::string GetWidgetValue(const QString &key) const;
    QObject* GetWidget(const QString& key) const;
    const std::vector<QObject*> GetWidgetsFromTabName(const QString& tab);
    const std::vector<QObject*>& GetAllWidgets() const;

    //сборщики

    void CollectWidgetFromTab(QTabWidget* tabWidget);
    void AddWidgetsFromWindow(QObject* parent, QString name_of_tab);
    void CollectWidgetsAndCheckboxFromLayout(QLayout* layout);
    void FillWidgetsMap();

    // вспомогательная функция для вывода имен столбцов и их типа для заполнения таблицы
    void Foo();

private:
    std::unordered_map<QString, std::vector<QObject*>> all_widgets_;
    std::unordered_map<QCheckBox*, std::vector<QObject*>> checkboxes_and_their_widgets_;
    std::unordered_map<QObject*, QCheckBox*> widgets_and_their_checkbox;
    std::vector<QObject*> all_widgets_unbound_;
    std::vector<QObject*> all_dates;
    std::vector<QObject*> CollectWidgets(const QObject* object);
    std::unordered_map<QString, QObject*> all_widgets_map_;
};



class MKBMap {
public:
    MKBMap();
    ~MKBMap();

    const QStringList& GetDiagnosisList() const ;
    const std::unordered_map<QString, QString>& GetMKBMap();
    const QStringList& GetOperationsList() const;

    void FillMKBMap();
    void FillMKBOperationsMap();

private:
    std::unordered_map<QString, QString> MKB_map_;
    QStringList diagnosis_list_;
    QStringList operations_list_;
};


class SettingsSetter {

public:
    SettingsSetter();
    ~SettingsSetter();

    void SetSettings(const std::initializer_list<std::pair<QComboBox *, QString> > &args);


private:
    std::unordered_map<QString, QStringList> settings_map;
    void SetSettingsByName(const QJsonObject& json, const QString& setting_name);
    void SetDoctorsNames(QComboBox* doctors_names);
};

class MedicinesHandler {
public:
    MedicinesHandler();
    ~MedicinesHandler();
    QString TryToParseDescription(const QString& medicine_name, const int weight, const int age) const;



private:
    static double CalculateDosage(double weight, std::pair<int, int> range);
    std::unordered_map<QString, QString> medicine_to_description;
};

namespace Constants {
    const int NUMBER_OF_ANALISES = 3;
    const int NUMBER_OF_DOP_OBSLEDOVANIYA = 8;
    const int NUMBER_OF_CONSULTATIONS_AND_MANIPULATIONS = 4;
    const int NUMBER_OF_MEDICINES = 9;
    const int NUMBER_OF_FISIOPROCEDURES = 5;
    const int NUMBER_OF_VACCINATIONS = 5;
    const int NUMBER_OF_DATES_IN_APPOINTMENT_LIST = 11;
    const int NUMBER_OF_KOKAV_DATES = 6;
}





#endif // COMMON_H
