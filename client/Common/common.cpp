#include "common.h"
#include "qmessagebox.h"


#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <qrandom.h>


WidgetHandler::WidgetHandler() = default;

WidgetHandler::~WidgetHandler() = default;



const std::unordered_map<QCheckBox *, std::vector<QObject *> > &WidgetHandler::GetCheckboxesAndWidgets() {
    return checkboxes_and_their_widgets_;
}

std::unordered_map<QObject *, QCheckBox *> &WidgetHandler::GetWidgetsAndCheckboxes() {
    return widgets_and_their_checkbox;
}



void WidgetHandler::CollectWidgetFromTab(QTabWidget* tabWidget) {
    // Очищаем контейнеры перед сбором данных
    all_widgets_.clear();
    checkboxes_and_their_widgets_.clear();
    all_widgets_unbound_.clear();

    for (int i = 0; i < tabWidget->count(); ++i) {
        QString tabName = tabWidget->tabText(i);
        QWidget* tabContent = tabWidget->widget(i);
        // Сбор виджетов из вкладки
        auto tab_widgets = CollectWidgets(tabContent);
        all_widgets_[tabName] = tab_widgets;
        all_widgets_unbound_.insert(all_widgets_unbound_.end(), tab_widgets.begin(), tab_widgets.end());
    }

}

void WidgetHandler::FillWidgetsMap() {
    all_widgets_map_.clear();
    for (const auto& [tab_name, widgets_vector] : all_widgets_) {
        for (const auto& widget : widgets_vector) {
            all_widgets_map_[widget->objectName()] = widget;
            // qDebug() << widget->objectName() << '\n';
        }
    }
}

void WidgetHandler::AddWidgetsFromWindow(QObject* parent, QString name_of_tab) {
    auto result = CollectWidgets(parent);
    all_widgets_[name_of_tab].insert(all_widgets_[name_of_tab].end(), result.begin(), result.end());
    all_widgets_unbound_.insert(all_widgets_unbound_.end(), result.begin(), result.end());
}

const std::vector<QObject *> &WidgetHandler::GetAllDates() {
    return all_dates;
}


std::vector<QObject*> WidgetHandler::CollectWidgets(const QObject *object) {
    std::vector<QObject*> result;

    // Получаем список всех дочерних виджетов
    const auto children = object->children();

    // Проходим по всем дочерним виджетам
    for (const auto widget : children) {
        // Проверяем тип виджета и добавляем в список
        if (qobject_cast<QLineEdit*>(widget) ||
            qobject_cast<QTextEdit*>(widget) ||
            qobject_cast<QComboBox*>(widget) ||
            qobject_cast<QCheckBox*>(widget)) {
            result.push_back(widget);
        } else if (qobject_cast<QDateEdit*>(widget) ||
                   qobject_cast<QDateTimeEdit*>(widget)) {
            result.push_back(widget);
            all_dates.push_back(widget);
        } else if (qobject_cast<QLayout*>(widget)) {
            auto recursion_result = CollectWidgets(widget);
            result.insert(result.end(), recursion_result.begin(), recursion_result.end());
            CollectWidgetsAndCheckboxFromLayout(qobject_cast<QLayout*>(widget));
        }
    }

    return result;

}


void WidgetHandler::CollectWidgetsAndCheckboxFromLayout(QLayout* layout){

    QCheckBox* checkbox = nullptr;
    std::vector<QObject*> q_objects;
    for (int i = 0; i < layout->count(); i++) {
        if (auto widget = layout->itemAt(i)->widget()) {
            if (auto check_box = qobject_cast<QCheckBox*>(widget)) {
                checkbox = check_box;
            } else if (qobject_cast<QLineEdit*>(widget) ||
                qobject_cast<QTextEdit*>(widget) ||
                qobject_cast<QComboBox*>(widget) ||
                qobject_cast<QDateEdit*>(widget) ||
                qobject_cast<QDateTimeEdit*>(widget)) {
                q_objects.push_back(widget);

                if(checkbox) {
                    widgets_and_their_checkbox[widget] = checkbox;
                }
            }
        } else if (auto child_layout = layout->itemAt(i)->layout()){
            CollectWidgetsAndCheckboxFromLayout(child_layout);
        }
    }

    if (checkbox && !q_objects.empty()) {
        checkboxes_and_their_widgets_[checkbox] = q_objects;
    }
}

// void WidgetHandler::AddCheckBoxAndItsWidgets(QCheckBox* checkbox) {
    // auto layout = checkbox->layout();
    // std::vector<QObject*> widgets;
    // //начинаем с 1 т.к. первый индекс это чекбокс
    // for (int i = 0; i < layout->count();i++ ) {
    //     auto widget = layout->itemAt(i)->widget();
    //     if (qobject_cast<QLineEdit*>(widget) ||
    //         qobject_cast<QTextEdit*>(widget) ||
    //         qobject_cast<QComboBox*>(widget) ||
    //         qobject_cast<QDateEdit*>(widget) ||
    //         qobject_cast<QDateTimeEdit*>(widget)) {
    //         widgets.push_back(widget);
    //     }
    // }
    // checkboxes_and_their_widgets_[checkbox] = widgets;
// }


void WidgetHandler::FillForms(const std::unordered_map<QString, QVariant>& widgets_data) {
    auto widgets = GetAllWidgets();
    for (auto& widget : widgets) {
        widget->blockSignals(true);
        if (widgets_data.count(widget->objectName())) {
            if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
                lineEdit->setText(widgets_data.at(lineEdit->objectName()).toString());
            } else if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
                textEdit->setText(widgets_data.at(textEdit->objectName()).toString());
            } else if (auto dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget)) {
                dateTimeEdit->setDateTime(widgets_data.at(dateTimeEdit->objectName()).toDateTime());
            } else if (auto dateEdit = qobject_cast<QDateEdit*>(widget)) {
                dateEdit->setDate(widgets_data.at(dateEdit->objectName()).toDate());
            } else if (auto checkBox = qobject_cast<QCheckBox*>(widget)) {
                checkBox->setChecked(widgets_data.at(checkBox->objectName()).toBool());
            } else if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
                comboBox->setCurrentText(widgets_data.at(comboBox->objectName()).toString());
            }
        }
        widget->blockSignals(false);
    }
}

const std::vector<QObject *> &WidgetHandler::GetAllWidgets() const {
    return all_widgets_unbound_;
}

std::string WidgetHandler::GetWidgetValue(const QString& key) const {

    QObject* widget = all_widgets_map_.at(key);
    QString result;
    // std::string result;
    if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
        result = lineEdit->text();
    } else if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
        result = textEdit->toPlainText();
    } else if (auto dateEdit = qobject_cast<QDateEdit*>(widget)) {
        result = dateEdit->date().toString("dd.MM.yyyy");
    } else if (auto dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget)) {
        result = dateTimeEdit->dateTime().toString("dd.MM.yyyy hh:mm");
    } else if (auto checkBox = qobject_cast<QCheckBox*>(widget)) {
        if (checkBox->isChecked()){
            result = "true";
        } else {
            result = "false";
        }

    } else if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
        result = comboBox->currentText();
    }
    return result.trimmed().toStdString();
}

bool WidgetHandler::HasWidget(const QString& key) const
{
    return all_widgets_map_.find(key) != all_widgets_map_.end();
}

QObject *WidgetHandler::GetWidget(const QString &key) const
{
    QObject* result = all_widgets_map_.at(key);
    return result;
}

const std::vector<QObject *> WidgetHandler::GetWidgetsFromTabName(const QString &tab_name)
{
    return all_widgets_.at(tab_name);
}

MKBMap::MKBMap () {
    FillMKBMap();
    FillMKBOperationsMap();
}

MKBMap::~MKBMap() = default;

const QStringList& MKBMap::GetDiagnosisList() const {
    return diagnosis_list_;
}
const std::unordered_map<QString, QString>& MKBMap::GetMKBMap() {
    return MKB_map_;
}


void MKBMap::FillMKBMap() {
    QDir dir(QCoreApplication::applicationDirPath());
    QString diagnosisFilePath = dir.filePath("Sources_files/DiagnosesMKB.txt");
    QFile file(diagnosisFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Не удалось открыть файл:" << diagnosisFilePath;
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        // Разделяем строку на код и расшифровку по первому пробелу
        int spaceIndex = line.indexOf(' ');
        if (spaceIndex != -1) {
            QString code = line.left(spaceIndex).trimmed();
            QString description = line.mid(spaceIndex + 1).trimmed();
            MKB_map_[code] = std::move(description); // Добавляем в unordered_map
            diagnosis_list_.push_back(std::move(code)); // Добавляем коды диагнозов в список
        }
    }

    file.close();
}

const QStringList& MKBMap::GetOperationsList() const  {
    return operations_list_;
}


void MKBMap::FillMKBOperationsMap() {
    QDir dir(QCoreApplication::applicationDirPath());
    QString operationsFilePath = dir.filePath("Sources_files/OperationsMKB.txt");
    QFile file(operationsFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Не удалось открыть файл:" << operationsFilePath;
        return;
    }

    QString operation_name;
    QString lastMKB;

    QTextStream in (&file);
    while (!in.atEnd()) {
        lastMKB = in.readLine();
        in.readLine();
        operation_name = in.readLine();
        in.readLine();
        QString operation_name_with_mkb_reverse = lastMKB + ' ' + operation_name;
        QString operation_name_with_mkb = std::move(operation_name) + ' ' + std::move(lastMKB);
        operations_list_.push_back(std::move(operation_name_with_mkb));
        operations_list_.push_back(std::move(operation_name_with_mkb_reverse));
        lastMKB.clear();
        operation_name.clear();
    }


    file.close();
}

void WidgetHandler::Foo() {
    for (const auto widget : all_widgets_unbound_) {

        if (QComboBox *comboBox = qobject_cast<QComboBox*>(widget)) {
            qDebug() << "{" << comboBox->objectName() << ", \"TEXT\" }," /*<< "значение:" << comboBox->currentText()*/;
        } else if (QTextEdit *textEdit = qobject_cast<QTextEdit*>(widget)) {
            qDebug() << "{" << textEdit->objectName() << ", \"TEXT\" },"/*<< "значение:" << textEdit->toPlainText()*/;
        } else if  (QDateEdit *dateEdit =  qobject_cast<QDateEdit*>(widget)) {
            qDebug() << "{" << dateEdit->objectName()<< ", \"DATE\" },";
        } else if (QDateTimeEdit *dateTimeEdit =  qobject_cast<QDateTimeEdit*>(widget)) {
            qDebug() << "{" << dateTimeEdit->objectName()<< ", \"DATETIME\" },";
            /*qDebug() << "Значение:" << dateTimeEdit->dateTime().toString("yyyy-MM-dd hh:mm:ss")*/;
        } else if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(widget)) {
            qDebug() << "{" << lineEdit->objectName() << ", \"TEXT\" },";
            // qDebug() << " Значение:" << lineEdit->text();
        } else if (QCheckBox *checkBox = qobject_cast<QCheckBox*>(widget)) {
            qDebug() << "{" << checkBox->objectName() << ", \"BOOLEAN\" },";
            // qDebug() << " Значение:" << checkBox->isChecked();
        }

    }
}


void SettingsSetter::SetSettingsByName(const QJsonObject& json, const QString& setting_name) {
    if (json.contains(setting_name)) {
        QStringList result;
        if (json[setting_name].isArray()) {
            QJsonArray json_array = json[setting_name].toArray();
            for (const QJsonValue& value : json_array) {
                result.push_back(value.toString());
            }
        } else if (json[setting_name].isObject()) {
            QJsonObject json_dict = json[setting_name].toObject();
            for (const QString &key : json_dict.keys()) {
                result.push_back(key);
            }
        }

        result.sort();
        settings_map[setting_name] = std::move(result);
    }
}

void SettingsSetter::SetDictionaryByName(const QJsonObject& json, const QString& setting_name)
{
    if (!json.contains(setting_name) || !json[setting_name].isObject()) {
        return;
    }

    const QJsonObject dictionary = json[setting_name].toObject();
    std::unordered_map<QString, QString> values;
    for (const QString& key : dictionary.keys()) {
        values[key] = dictionary.value(key).toString();
    }
    dictionary_map_[setting_name] = std::move(values);
}

QString SettingsSetter::GetDictionaryValue(const QString& setting_name, const QString& key) const
{
    const auto setting_it = dictionary_map_.find(setting_name);
    if (setting_it == dictionary_map_.end()) {
        return QString();
    }

    const auto value_it = setting_it->second.find(key);
    if (value_it == setting_it->second.end()) {
        return QString();
    }
    return value_it->second;
}

SettingsSetter::SettingsSetter() {
    QDir dir(QCoreApplication::applicationDirPath());
    QString settings_dir = dir.filePath("Sources_files/DepartmentInfo.json");

    QFile file(settings_dir);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Не удалось открыть файл:" << file.errorString();
        return; // Завершаем выполнение, если файл не удалось открыть
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDoc((QJsonDocument::fromJson(jsonData)));
    if (jsonDoc.isNull()) {
        qDebug() << "Ошибка при парсинге JSON";
        return; // Завершаем выполнение, если парсинг не удался
    }

    QJsonObject jsonObj = jsonDoc.object();



    SetSettingsByName(jsonObj, "доктора");
    SetSettingsByName(jsonObj, "лекарственные препараты");
    SetSettingsByName(jsonObj, "лекарственные препараты дополнительно");
    SetSettingsByName(jsonObj, "медицинские операционные сестры");
    SetSettingsByName(jsonObj, "медицинские анестезиологические сестры");
    SetSettingsByName(jsonObj, "анестезиологи");
    SetSettingsByName(jsonObj, "импланты");
    SetSettingsByName(jsonObj, "медицинское оборудование");
    SetSettingsByName(jsonObj, "обследования");
    SetSettingsByName(jsonObj, "физиопроцедуры");
    SetSettingsByName(jsonObj, "консультации");
    SetSettingsByName(jsonObj, "манипуляции после поступления");

    SetDictionaryByName(jsonObj, "лекарственные препараты дополнительно");
    SetDictionaryByName(jsonObj, "обследования");
    SetDictionaryByName(jsonObj, "консультации");
    SetDictionaryByName(jsonObj, "манипуляции после поступления");
}


SettingsSetter::~SettingsSetter() = default;

void SettingsSetter::SetDoctorsNames(QComboBox* doctors_names) {
    for (const auto& doctor_name : settings_map["доктора"])
        doctors_names->addItem(doctor_name);
}

void SettingsSetter::SetSettings(const std::initializer_list<std::pair<QComboBox*, QString>>& args) {

    for (const auto& pair : args) {
        QComboBox* combobox_name = pair.first;
        QString setting_name = pair.second;

        // Добавляем элементы в combobox из settings_map
        for (const auto& item_name : settings_map[setting_name]) {
            combobox_name->addItem(item_name);
        }
    }
}

MedicinesHandler::MedicinesHandler() {
    QDir dir(QCoreApplication::applicationDirPath());
    QString settings_dir = dir.filePath("Sources_files/DepartmentInfo.json");

    QFile file(settings_dir);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Не удалось открыть файл:" << file.errorString();
        return; // Завершаем выполнение, если файл не удалось открыть
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDoc((QJsonDocument::fromJson(jsonData)));
    if (jsonDoc.isNull()) {
        qDebug() << "Ошибка при парсинге JSON";
        return; // Завершаем выполнение, если парсинг не удался
    }

    QJsonObject jsonObj = jsonDoc.object();

    if (!jsonObj.contains("лекарственные препараты дополнительно")) {
        qDebug() << "нет дополнительных лекарственных препаратов";
        return;
    }

    QJsonObject additional_medications = jsonObj["лекарственные препараты дополнительно"].toObject();

    for (const QString& key : additional_medications.keys()) {
        medicine_to_description[key] = additional_medications[key].toString();
    }

}

MedicinesHandler::~MedicinesHandler() = default;

QString MedicinesHandler::TryToParseDescription(const QString& medicine_name, const int weight, const int age) const {
    QString result;
    if (medicine_name == "цефтриаксон") {
        double dosage_in_gram = CalculateDosage(weight, {30, 50});
        if (weight > 90) {
            result.append("2,0 г внутримышечно, 1 раз в сутки из расчёта 20-50 мг на кг массы тела");
        } else if (age >= 12 || weight >= 50) {
            result.append("1,5 г внутримышечно, 1 раз в сутки из расчёта 20-50 мг на кг массы тела");
        } else if (age < 1) {
            result.append(std::to_string(dosage_in_gram).substr(0, 3) + " г внутримышечно, 1 раз в сутки из расчёта 20-50 мг на кг массы тела");
        } else if (age >= 1 && age < 12) {
            dosage_in_gram = CalculateDosage(weight, {30, 50});
            if (dosage_in_gram > 1.5) {
                result.append("1,5 г внутримышечно, 1 раз в сутки из расчёта 20-50 мг на кг массы тела");
            } else {
                result.append(std::to_string(dosage_in_gram).substr(0, 3) + " г внутримышечно, 1 раз в сутки из расчёта 20-50 мг на кг массы тела");
            }
        }

    } else if (medicine_name == "трамадол") {
        result.append( " 5% - ");
        double dosage_in_gram = CalculateDosage(weight, {1, 2});
        if (dosage_in_gram > 0.100) {
            result.append("2.0 мл внутримышечно до 4 раз в сутки, из расчёта 1-2 мг на кг массы тела");
        } else {
            result.append(std::to_string(dosage_in_gram/0.05).substr(0, 3) + " мл внутримышечно до 4 раз в сутки, из расчёта 1-2 мг на кг массы тела");
        }
        result.append(", по показаниям");
    } else if (medicine_name == "этамзилат натрия") {
        result += " 12,5 % - ";
        double dosage_in_gram = CalculateDosage(weight, {5, 8});
        if (dosage_in_gram > 0.375) {
            dosage_in_gram = 0.375;
        }
        result.append(std::to_string(dosage_in_gram/0.125).substr(0, 3) + " мл внутримышечно, 2 раза в сутки из расчёта 10-15 мг на кг массы тела");
        // change_date();
    } else if (medicine_name.contains("аспаркам")) {
        double dosage_in_gram = CalculateDosage(weight, {10, 10});
        if (dosage_in_gram >= 0.35) {
            result.append("по 1 таблетке перорально, 2 раза в сутки из расчёта 10 мг на кг массы тела");
        } else if (dosage_in_gram < 0.35 && dosage_in_gram >= 0.175) {
            result.append("по 1/2 таблетки перорально, 2 раза в сутки из расчёта 10 мг на кг массы тела");
        } else {
            result.append("по 1/4 таблетки перорально, 2 раза в сутки из расчёта 10 мг на кг массы тела");
        }
        // change_date();
    } else if (medicine_name == "ацетазоламид") {
        double dosage_in_gram = CalculateDosage(weight, {5, 10});
        if (dosage_in_gram > 0.25) {
            result.append("по 1 таблетке перорально, 1 раз в сутки из расчёта 5-10 мг на кг массы тела");
        } else if (dosage_in_gram <= 0.25 && dosage_in_gram >= 0.125){
            result.append("по 1/2 таблетки перорально, 1 раз в сутки из расчёта 5-10 мг на кг массы тела");
        } else {
            result.append("по 1/4 таблетки перорально, 1 раз в сутки из расчёта 5-10 мг на кг массы тела");
        }
        // change_date();
    } else if (medicine_name == "линкомицин") {
        result += "300 мг на 1 мл: ";
        if (age > 14) {
            result.append("по 2 мл внутримышечно, 2 раза в сутки из расчёта 10-20 мг на кг массы тела");
        } else {
            double dosage_in_gram = CalculateDosage(weight, {10, 15});
            if (dosage_in_gram > 0.6) {
                result.append("по 2 мл внутримышечно, 2 раза в сутки из расчёта 10-20 мг на кг массы тела");
            } else if (dosage_in_gram <= 0.6 && dosage_in_gram >= 0.4) {
                result.append("по 1.5 мл внутримышечно, 2 раза в сутки из расчёта 10-20 мг на кг массы тела");
            } else if (dosage_in_gram < 0.4 && dosage_in_gram >= 0.2) {
                result.append("по 1.0 мл внутримышечно, 2 раза в сутки из расчёта 10-20 мг на кг массы тела");
            } else {
                result.append(std::to_string(dosage_in_gram/0.3).substr(0, 3) + " мл внутримышечно, 2 раза в сутки из расчёта 10-20 мг на кг массы тела");
            }
        }
    } else if (medicine_name == "парацетамол") {
        double dosage_in_gram = CalculateDosage(weight, {10, 15});
        if (dosage_in_gram > 0.5) {
            result.append("по 1 таблетке 500 мг перорально, до 4 раз в сутки из расчёта 10-15 мг на кг массы тела");
        } else {
            result.append(std::to_string(dosage_in_gram).substr(0, 3) + " г перорально, до 4 раз в сутки из расчёта 10-15 мг на кг массы тела");
        }
        result.append(", по показаниям");
    } else if (medicine_name == "ибупрофен") {
        double dosage_in_gram = CalculateDosage(weight, {5, 10});
        if (dosage_in_gram <= 0.3) {
            result.append("по 1 таблетке 200 мг перорально, до 3х раз в сутки из расчёта 5-10 мг на кг массы тела");
        } else {
            result.append("по 1 таблетке 400 мг перорально, до 3х раз в сутки из расчёта 5-10 мг на кг массы тела");
        }
        result.append(", по показаниям");
    } else if (medicine_name == "цефотаксим") {
        if (age > 12 && weight > 50) {
            result +="1.0 мл внутримышечно, 2 раза в сутки для детей старше 12 лет и массой тела более 50 кг";
        } else {
            double dosage_in_gram = CalculateDosage(weight, {20, 30});
            result.append(std::to_string(dosage_in_gram).substr(0, 3) + " г внутримышечно, 2 раза в сутки из расчёта 50-100 мг на кг массы тела");
        }
    } else if (medicine_name == "гентамицин") {
        if (age < 18) {
            double dosage_in_gram = CalculateDosage(weight, {1, 2}) / 0.4 * 10;
            result.append(std::to_string(dosage_in_gram).substr(0, 3) + " мл внутримышечно, 2 раза в сутки из расчёта 2-5 мг на кг массы тела в сутки");
        } else {
            double dosage_in_gram = CalculateDosage(weight, {1, 1}) / 0.4 * 10;
            result.append(std::to_string(dosage_in_gram).substr(0, 3) + " мл внутримышечно, 2 раза в сутки из расчёта 3 мг на кг массы тела в сутки");
        }
    } else if (medicine_name == "ванкомицин") {
        result.append("в\\в капельно не более 10 мг в минуту, продолжительность введения не менее 60 минут");

    } else if (medicine_name == "метамизол натрия") {
        if (age >= 15) {
            result.append(" 2.0 мл внутримышечно по показаниям до 4 раз в сутки");
        } else {
            if (weight < 8.) {
                result.append("0.1 мл внутримышечно по показаниям до 4 раз в сутки");
            } else if (weight < 15.) {
                result.append("0.3 мл внутримышечно по показаниям до 4 раз в сутки");
            } else if (weight < 23.) {
                result.append("0.5 мл внутримышечно по показаниям до 4 раз в сутки");
            } else if (weight < 30.) {
                result.append("0.7 мл внутримышечно по показаниям до 4 раз в сутки");
            } else if (weight < 45.) {
                result.append("1.1 мл внутримышечно по показаниям до 4 раз в сутки");
            } else if (weight < 53.) {
                result.append(" 1.4 мл внутримышечно по показаниям до 4 раз в сутки");
            } else {
                result.append("2.0 мл внутримышечно по показаниям до 4 раз в сутки");
            }
        }

    } else if (medicine_name == "дифенгидрамин") {
        if (age > 14) {
            result.append("3.0 мл внутримышечно по показаниям до 3 раз в сутки");
        } else if (age <= 1) {
            result.append("0.4 мл внутримышечно по показаниям до 3 раз в сутки");
        } else if (age <= 3) {
            result.append("0.7 мл внутримышечно по показаниям до 3 раз в сутки");
        } else if (age <= 6) {
            result.append("1.2 мл внутримышечно по показаниям до 3 раз в сутки");
        } else if (age <= 14) {
            result.append("2.0 мл внутримышечно по показаниям до 3 раз в сутки");
        } else {
            result.append("3.0 мл внутримышечно по показаниям до 3 раз в сутки");
        }
    } else if (medicine_name == "раствор nacl 0.9%"){
        result.append("мл внутривенно капельно 30-40 капель в минуту");
    } else if (medicine_name == "раствор глюкозы 10%"){
        result.append("мл внутривенно капельно 30-40 капель в минуту");
    } else if (medicine_to_description.count(medicine_name)) {
        result.append(medicine_to_description.at(medicine_name));
    }

    return result;
}

double MedicinesHandler::CalculateDosage(double weight, std::pair<int, int> range) {
    // Пример расчета дозировки: берем среднее значение из диапазона
    double result = (range.first + range.second) / 2.0 * weight / 1000.0;
    if (result > 0.25) {
        result = std::round(result / 0.25) * 0.25;
    }
    return result; // в граммах
}


