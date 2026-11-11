#include "json_printer.h"


#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>

JSONPrinter::JSONPrinter(WidgetHandler& widget_handler) : widget_handler_(widget_handler) {

}

void JSONPrinter::MakeJSONFromData(const std::vector<std::string>& doc_to_print) {

    JSON::Builder builder;
    builder.StartDict();
    for (const auto& widget : widget_handler_.GetAllWidgets()) {
        builder.Key(widget->objectName().toStdString());
        if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
            builder.Value(lineEdit->text().toStdString());
        } else if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
            builder.Value(textEdit->toPlainText().toStdString());
        } else if (auto dateEdit = qobject_cast<QDateEdit*>(widget)) {
            // builder.Value(dateEdit->date().toString("yyyy-MM-dd").toStdString());
            builder.Value(dateEdit->date().toString("dd.MM.yyyy").toStdString());
        } else if (auto dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget)) {
            // builder.Value(dateTimeEdit->dateTime().toString("yyyy-MM-dd hh:mm").toStdString());
            builder.Value(dateTimeEdit->dateTime().toString("dd.MM.yyyy hh:mm").toStdString());
        } else if (auto checkBox = qobject_cast<QCheckBox*>(widget)) {
            builder.Value(checkBox->isChecked());
        } else if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
            builder.Value(comboBox->currentText().toStdString());
        }
    }
    builder.Key("print_settings").StartArray();

    for (const auto& doc_name : doc_to_print) {
        builder.Value(doc_name);
    }
    builder.EndArray();

    widgets_data data;
    widget_handler_.FillWidgetsMap();

    ParseAllTreatmentInStacionare(data, builder);

    if (std::count(doc_to_print.begin(), doc_to_print.end(), "AppointmentList")) {
        ParseAppointmentListData(data);
    }

    if (std::count(doc_to_print.begin(), doc_to_print.end(), "Form045")) {
        ParseForm045(data);
    }


    for (auto&& [key, value] : data) {
        std::string key_ = key;
        builder.Key(std::move(key_)).Value(std::move(value));
    }

    JSON::Document document(std::move(builder.EndDict().Build()));
    std::ofstream out_file ("patient_data.json");
    JSON::Print(document, out_file);
    out_file.close();
}

bool JSONPrinter::IsNeedToParseDiariesEveryDay() {
    std::string age_s = widget_handler_.GetWidgetValue("Age");
    int age_i = 0;
    try {
        age_i = std::stoi(age_s);
    } catch (...) {

    }
    if (widget_handler_.GetWidgetValue("AnamnesDisease").find("ВМП") != std::string::npos || age_i < 4){
        return true;
    }
    auto patient_condition = widget_handler_.GetWidgetValue("PatientCondition");
    std::transform(patient_condition.begin(), patient_condition.end(), patient_condition.begin(), tolower);
    if (patient_condition.find("тяжелое") != std::string::npos){
        return true;
    }
    return false;
}

void JSONPrinter::ParseAllTreatmentInStacionare(widgets_data& data, JSON::Builder &builder) {
    for (int i = 1; i <= Constants::NUMBER_OF_ANALISES; i++) {
        QString index = QString::number(i);
        ParseOAKAnalises(index, data);
        ParseBHAnalises(index, data);
        ParseOAMAnalises(index, data);
    }
    for (int i = 1; i <= Constants::NUMBER_OF_DOP_OBSLEDOVANIYA; i++) {
        ParseDopObservations(QString::number(i), data);
    }

    for (int i = 1; i <= Constants::NUMBER_OF_CONSULTATIONS_AND_MANIPULATIONS; i++) {
        QString index = QString::number(i);
        ParseConsultations(index, data);
        ParseManipulations(index, data);
    }

    ParseMedicines(data);
    ParseFisioProcedures(data);
    ParseVaccinations(data);

    ParseEKG(data);
    ParseKalAnalises(data);

    if (data["MedicinesText"].empty()) {
        data["MedicinesText"] = "Лекарственные препараты: нет";
    }
    if (IsNeedToParseDiariesEveryDay()) {
        builder.Key("Diaries_every_day").Value(true);
    } else {
        builder.Key("Diaries_every_day").Value(false);
    }
    builder.Key("TreatmentInStacionare").Value(true);
}

void JSONPrinter::ParseMedicines(widgets_data &data) {
    std::string medicines_text;
    for (int i = 1; i <= Constants::NUMBER_OF_MEDICINES; i++) {
        QString index = QString::number(i);
        std::string medicines_variant = widget_handler_.GetWidgetValue("MedicineVariant" + index);
        if (!medicines_variant.empty()) {
            std::string medicine_purpose_date = widget_handler_.GetWidgetValue("MedicinePurposeDate" + index);
            std::string medicine_cancel_date = widget_handler_.GetWidgetValue("MedicineCancelDate" + index);
            std::string medicine_administation_method =  widget_handler_.GetWidgetValue("MethodOfAdministrationMedicine" + index);
            std::string outcome_date = widget_handler_.GetWidgetValue("OutComeDate");
            std::string income_date = widget_handler_.GetWidgetValue("IncomeDate");
            auto result = (medicines_variant + ' ' + medicine_administation_method);
            if (medicine_purpose_date != medicine_cancel_date) {
                result.append(", дата назначения " + medicine_purpose_date +
                              ", дата отмены " + medicine_cancel_date + '\n');
            } else if (medicine_purpose_date == medicine_cancel_date && income_date != outcome_date) {
                result.append(", однократно, дата назначения " + medicine_purpose_date + '\n');
                medicine_administation_method.append(", однократно");
            } else {
                result.append(", дата назначения " + medicine_purpose_date);
            }

            medicines_text.append(result);
            data["MedicinesPurposeOnDate" + medicine_purpose_date].append(medicines_variant+ ' ' + std::move(medicine_administation_method) + '\n');
            // data["MedicinesCancelOnDate" + medicine_cancel_date].append(std::move(medicines_variant) + ";");

            std::string& cancel_on_date = data["MedicinesCancelOnDate" + medicine_cancel_date];
            if (medicine_purpose_date != medicine_cancel_date) {
                if (cancel_on_date.empty()) {
                    cancel_on_date.append("отменено: " + std::move(medicines_variant));
                } else {
                    cancel_on_date.append(", " + std::move(medicines_variant));
                }
            }

        }
    }
    if (!medicines_text.empty()) {
        data["MedicinesText"].append("Лекарственные препараты: " + std::move(medicines_text));
    }
}
void JSONPrinter::ParseFisioProcedures(widgets_data &data) {
    std::string medicines_text;
    for (int i = 1; i <= Constants::NUMBER_OF_FISIOPROCEDURES; i++) {
        QString index = QString::number(i);
        std::string fisio_variant = widget_handler_.GetWidgetValue("FisioVariant" + index);
        if (!fisio_variant.empty()) {
            std::string fisio_purpose_date = widget_handler_.GetWidgetValue("FisioAdministationDate" + index);
            std::string fisio_cancel_date = widget_handler_.GetWidgetValue("FisioCancelDate" + index);
            std::string fisio_text =  widget_handler_.GetWidgetValue("FisioText" + index);
            auto result = fisio_variant + ' ' + fisio_text;
            if (fisio_purpose_date != fisio_cancel_date) {
                result.append(", дата назначения " + fisio_purpose_date +
                              ", дата отмены " + fisio_cancel_date + '\n');
            } else {
                result.append(", дата назначения " + fisio_purpose_date + '\n');
            }

            medicines_text.append(result);
            data["MedicinesPurposeOnDate" + fisio_purpose_date].append(fisio_variant + ' ' + std::move(fisio_text) + '\n');

            std::string& cancel_on_date = data["MedicinesCancelOnDate" + fisio_cancel_date];

            if (fisio_purpose_date != fisio_cancel_date) {
                if (cancel_on_date.empty()) {
                    cancel_on_date.append("отменено: " + std::move(fisio_variant));
                } else {
                    cancel_on_date.append(", " + std::move(fisio_variant));
                }
            }


        }
    }
    if (!medicines_text.empty()) {
        data["MedicinesText"].append("Физиопроцедуры: " + std::move(medicines_text));
    }

}

template <typename Data>
void AddVaccineToData(Data& data, std::string&& vaccine_text,
                      std::string&& vaccination_purpose_date,
                      std::string&& vaccination_cancel_date) {
    data["MedicinesPurposeOnDate" + vaccination_purpose_date].append(vaccine_text + '\n');
    if (vaccine_text.find("КОКАВ 1.0") != std::string::npos) {
        auto vaccination_purpose_qstring = QString::fromStdString(vaccination_purpose_date);
        QDate purpose_date = QDate::fromString(vaccination_purpose_qstring, "dd.MM.yyyy");
        auto purpose_date_sdt_string = purpose_date.addDays(3).toString("dd.MM.yyyy").toStdString();
        auto new_vaccine_text = vaccine_text.substr(0, vaccine_text.length() - 15);
        data["MedicinesPurposeOnDate" + purpose_date_sdt_string].append(new_vaccine_text + '\n');
        purpose_date_sdt_string = purpose_date.addDays(7).toString("dd.MM.yyyy").toStdString();
        data["MedicinesPurposeOnDate" + purpose_date_sdt_string].append(new_vaccine_text + '\n');
    }
    // data["MedicinesCancelOnDate" + vaccination_cancel_date].append(vaccine_text);
    data["MedicinesText"].append(std::move(vaccine_text) + ", дата назначения " + std::move(vaccination_purpose_date) +
                                 ", дата отмены " + std::move(vaccination_cancel_date) + '\n');
}

void JSONPrinter::ParseVaccinations(widgets_data &data) {
    auto pgs = widget_handler_.GetWidgetValue("PGS");
    auto pss = widget_handler_.GetWidgetValue("PSSCheck");
    auto anatoxine = widget_handler_.GetWidgetValue("AnatoxinStolb");
    auto aig = widget_handler_.GetWidgetValue("AIGcheck");
    auto kokav = widget_handler_.GetWidgetValue("Kokav");
    if (pgs == "true" || pss == "true" || anatoxine == "true" || aig == "true" || kokav == "true") {
        data["MedicinesText"].append("Вакцины: ");
    } else {
        return;
    }

    if (pgs == "true") {
        AddVaccineToData(data, "ПГС " + widget_handler_.GetWidgetValue("PGSText"),
                         widget_handler_.GetWidgetValue("VaccineAdministationDate1"),
                         widget_handler_.GetWidgetValue("VaccineCancelDate1"));
    }

    if (pss == "true") {
        AddVaccineToData(data, "ПСС " + widget_handler_.GetWidgetValue("PSSText"),
                         widget_handler_.GetWidgetValue("VaccineAdministationDate2"),
                         widget_handler_.GetWidgetValue("VaccineCancelDate2"));
    }

    if (anatoxine == "true") {
        AddVaccineToData(data, "АС " + widget_handler_.GetWidgetValue("AnatoxinStolText"),
                         widget_handler_.GetWidgetValue("VaccineAdministationDate3"),
                         widget_handler_.GetWidgetValue("VaccineCancelDate3"));
    }

    if (aig == "true") {
        std::string aig_text = "АИГ";
        std::string aig_type = widget_handler_.GetWidgetValue("AIGHumanOrAnimal") == "человеческий" ? "человеческий" : "лошадиный";
        std::string aig_titr = widget_handler_.GetWidgetValue("AIGtitr");

        double aig_dosage = GetAigDosage();
        aig_text.append(' ' + aig_type +
                        ", титр: " + aig_titr +
                        ", серия: " + widget_handler_.GetWidgetValue("AIGSerion") +
                        ", годен до " + widget_handler_.GetWidgetValue("AIGDate") + ", " +
                        QString::number(aig_dosage, 'f', 2).toStdString() + " внутримышечно по схеме ");
        AddVaccineToData(data, std::move(aig_text),
                         widget_handler_.GetWidgetValue("VaccineAdministationDate4"),
                         widget_handler_.GetWidgetValue("VaccineCancelDate4"));
    }

    if (kokav == "true") {
        std::string kokav_text = "КОКАВ 1.0 ";
        std::string kokav_serion = widget_handler_.GetWidgetValue("KokavSerion");
        std::string kokav_purpose_date = widget_handler_.GetWidgetValue("VaccineAdministationDate5");
        std::string kokav_cancel_date = widget_handler_.GetWidgetValue("VaccineCancelDate5");
        std::string kokav_srok_godnosti = widget_handler_.GetWidgetValue("KokavDate");
        kokav_text.append(", серия " + kokav_serion + ", годен до " + kokav_srok_godnosti +
                          ", внутримышечно по схеме");
        AddVaccineToData(data, std::move(kokav_text),
                         std::move(kokav_purpose_date),
                         std::move(kokav_cancel_date));
    }

}

void JSONPrinter::ParseAppointmentListData(widgets_data& data) {
    int counter = 1;

    auto AddMedicine = [&](std::string&& medicine_text
                           ,std::string&& administration_date
                           ,std::string&& cancel_date){
        std::string index = std::to_string(counter++);
        data["_MedicineVariant" + index] = std::move(medicine_text);
        if (administration_date != cancel_date) {
            data["_MedicineCancelDate" + index] = std::move(cancel_date);
        } /*else {
            data["_MedicineVariant" + index] += ", однократно";
        }*/
        data["_MedicineAdministrationDate" + index] = std::move(administration_date);

    };

    for (int i = 1; i <= Constants::NUMBER_OF_MEDICINES; i++) {
        std::string medicine_variant = widget_handler_.GetWidgetValue(QString("MedicineVariant%1").arg(i));
        if (!medicine_variant.empty()) {
            auto administration_date = widget_handler_.GetWidgetValue(QString("MedicinePurposeDate%1").arg(i));
            auto cancel_date = widget_handler_.GetWidgetValue(QString("MedicineCancelDate%1").arg(i));
            AddMedicine(std::move(medicine_variant) + ' ' + widget_handler_.GetWidgetValue(QString("MethodOfAdministrationMedicine%1").arg(i)),
                        std::move(administration_date),
                        std::move(cancel_date));
        }
    }

    for (int i = 1; i <= Constants::NUMBER_OF_FISIOPROCEDURES; i++) {
        std::string fisio_variant = widget_handler_.GetWidgetValue(QString("FisioVariant%1").arg(i));
        if (!fisio_variant.empty()) {
            AddMedicine(std::move(fisio_variant) + ' ' + widget_handler_.GetWidgetValue(QString("FisioText%1").arg(i)),
                        widget_handler_.GetWidgetValue(QString("FisioAdministationDate%1").arg(i)),
                        widget_handler_.GetWidgetValue(QString("FisioCancelDate%1").arg(i)));
        }
    }

    if (widget_handler_.GetWidgetValue("PGS") == "true") {
        AddMedicine("ПГС " + widget_handler_.GetWidgetValue("PGSText"),
                    widget_handler_.GetWidgetValue("VaccineAdministationDate1"),
                    widget_handler_.GetWidgetValue("VaccineCancelDate1"));
    }

    if (widget_handler_.GetWidgetValue("PSSCheck") == "true") {
        AddMedicine("ПСС " + widget_handler_.GetWidgetValue("PSSText"),
                    widget_handler_.GetWidgetValue("VaccineAdministationDate2"),
                    widget_handler_.GetWidgetValue("VaccineCancelDate2"));
    }

    if (widget_handler_.GetWidgetValue("AnatoxinStolb") == "true") {
        AddMedicine("АС " + widget_handler_.GetWidgetValue("AnatoxinStolText"),
                    widget_handler_.GetWidgetValue("VaccineAdministationDate3"),
                    widget_handler_.GetWidgetValue("VaccineCancelDate3"));
    }

    if (widget_handler_.GetWidgetValue("AIGcheck") == "true") {
        std::string aig_text = "АИГ";
        std::string aig_type = widget_handler_.GetWidgetValue("AIGHumanOrAnimal") == "человеческий" ? "человеческий" : "лошадиный";
        std::string aig_titr = widget_handler_.GetWidgetValue("AIGtitr");

        double aig_dosage = GetAigDosage();
        aig_text.append(' ' + aig_type +
                        ", титр: " + aig_titr +
                        ", серия: " + widget_handler_.GetWidgetValue("AIGSerion") +
                        ", годен до " + widget_handler_.GetWidgetValue("AIGDate") + ", " +
                        QString::number(aig_dosage, 'f', 2).toStdString() + " внутримышечно по схеме ");

        AddMedicine(std::move(aig_text),
                    widget_handler_.GetWidgetValue("VaccineAdministationDate4"),
                    widget_handler_.GetWidgetValue("VaccineCancelDate4"));
    }

    if (widget_handler_.GetWidgetValue("Kokav") == "true") {
        std::string kokav_text = "КОКАВ 1.0 ";
        std::string kokav_serion = widget_handler_.GetWidgetValue("KokavSerion");
        std::string kokav_purpose_date = widget_handler_.GetWidgetValue("VaccineAdministationDate5");
        std::string kokav_cancel_date = widget_handler_.GetWidgetValue("VaccineCancelDate5");
        std::string kokav_srok_godnosti = widget_handler_.GetWidgetValue("KokavDate");
        kokav_text.append(", серия " + kokav_serion + ", годен до " + kokav_srok_godnosti +
                          ", внутримышечно по схеме");
        AddMedicine(std::move(kokav_text),
                    widget_handler_.GetWidgetValue("VaccineAdministationDate5"),
                    widget_handler_.GetWidgetValue("VaccineCancelDate5"));
    }


    auto obsledovaniya_text = widget_handler_.GetWidgetValue("RecomendationsExaminationPerv");
    if (!obsledovaniya_text.empty()) {
        data["_ObsledovaniaIncome"] = std::move(obsledovaniya_text);
    }

    auto date_string = QString::fromStdString(widget_handler_.GetWidgetValue("IncomeDate"));
    QString format = "dd.MM.yyyy hh:mm";
    QDate date = QDateTime::fromString(date_string, format).date();
    data["_IncomeDate"] = date.toString("dd.MM.yyyy").toStdString();
    for (int i = 1; i <= Constants::NUMBER_OF_DATES_IN_APPOINTMENT_LIST; i++) {
        data["_Date" + std::to_string(i)] = date.toString("dd/MM").toStdString();
        date = date.addDays(1);
    }
}

void JSONPrinter::ParseForm045(widgets_data &data) {
    double aig_dosage = GetAigDosage();
    double aig_dosage_ = aig_dosage / 3;
    data["AIGDose"] = QString::number(aig_dosage, 'f', 2).toStdString();
    data["AIGDose_"] = QString::number(aig_dosage_, 'f', 2).toStdString();

    auto date_string = QString::fromStdString(widget_handler_.GetWidgetValue("VaccineAdministationDate5"));
    QString format = "dd.MM.yyyy";
    QDate date = QDate::fromString(date_string, format);
    std::vector<int> add_days({0, 3, 7, 14, 30, 90});
    for (int i = 0; i < Constants::NUMBER_OF_KOKAV_DATES; i++) {
        auto kokav_date = date.addDays(add_days[i]);
        data["KokavDate" + std::to_string(i)] = kokav_date.toString("dd.MM.yyyy").toStdString();
    }

    date_string = QString::fromStdString(widget_handler_.GetWidgetValue("IncomeDate"));
    format = "dd.MM.yyyy hh:mm";
    date = QDateTime::fromString(date_string, format).date();
    data["_IncomeDate"] = date.toString("dd.MM.yyyy").toStdString();
}

double JSONPrinter::GetAigDosage() {
    std::string aig_type = widget_handler_.GetWidgetValue("AIGHumanOrAnimal") == "человеческий" ? "человеческий" : "лошадиный";
    std::string aig_titr = widget_handler_.GetWidgetValue("AIGtitr");
    const double HUMAN_AIG_MULTIPLIER = 20.;
    const double HORSE_AIG_MULTIPLIER = 40.;
    double weight = 0.;
    double titr = 0.;
    try {
        weight = std::stod(widget_handler_.GetWidgetValue("Weight"));
        titr = std::stod(aig_titr);
    } catch (...) {

    }
    return aig_type == "человеческий" ? weight * HUMAN_AIG_MULTIPLIER / titr : weight * HORSE_AIG_MULTIPLIER / titr;

}

void JSONPrinter::ParseOAKAnalises(const QString& index, widgets_data &data) {
    std::string result;
    if (widget_handler_.GetWidgetValue("OAKCheck" + index) != "true") {
        return;
    }
    std::vector<std::string> result_oak;

    if (!widget_handler_.GetWidgetValue("hemoglobin" + index).empty()) {
        result_oak.push_back("Гемоглобин: " + widget_handler_.GetWidgetValue("hemoglobin" + index) + " г/л");
    }
    if (!widget_handler_.GetWidgetValue("eritrocit" + index).empty()) {
        result_oak.push_back("Эритроциты: " + widget_handler_.GetWidgetValue("eritrocit" + index) + " млн/мкл");
    }
    if (!widget_handler_.GetWidgetValue("leykocit" + index).empty()) {
        result_oak.push_back("Лейкоциты: " + widget_handler_.GetWidgetValue("leykocit" + index) + " тыс/мкл");
    }
    if (!widget_handler_.GetWidgetValue("trombocit" + index).empty()) {
        result_oak.push_back("Тромбоциты: " + widget_handler_.GetWidgetValue("trombocit" + index) + " тыс/мкл");
    }
    if (!widget_handler_.GetWidgetValue("gematocrit" + index).empty()) {
        result_oak.push_back("Гематокрит: " + widget_handler_.GetWidgetValue("gematocrit" + index) + " %");
    }
    if (!widget_handler_.GetWidgetValue("SOE" + index).empty()) {
        result_oak.push_back("СОЭ: " + widget_handler_.GetWidgetValue("SOE" + index) + " мм/ч");
    }

    std::string OAK_date = widget_handler_.GetWidgetValue("OAKDate" + index);

    if (!result_oak.empty()) {
        result.append("ОАК: ");
        for (auto&& analis : result_oak) {
            result.append(std::move(analis) + ", ");
        }
        result.resize(result.size() - 2);
        result.append("; ");
    }
    AddObservationResultToDate(data, OAK_date, std::move(result));
    AddObservationToDate(data, OAK_date, "ОАК");
    AddPlainObservationToDate(data, std::move(OAK_date), "ОАК");

}

void JSONPrinter::ParseDopObservations(const QString& index, widgets_data& data) {
    if (widget_handler_.GetWidgetValue("DopObsledovanieText" + index).empty()) {
        return; // Если текст обследования пустой, выходим из функции
    }

    std::string variant = widget_handler_.GetWidgetValue("DopObsledovanieVariant" + index);
    std::string text = widget_handler_.GetWidgetValue("DopObsledovanieText" + index);
    std::string dop_obsledovanie_date = widget_handler_.GetWidgetValue("DopObsledovanieDate" + index);

    // Добавляем обследование в дату
    AddObservationResultToDate(data, dop_obsledovanie_date, variant + " " + std::move(text) + "; ");
    AddObservationToDate(data, dop_obsledovanie_date, variant);
    AddPlainObservationToDate(data, std::move(dop_obsledovanie_date), variant);
}

void JSONPrinter::ParseEKG(widgets_data& data) {
    if (widget_handler_.GetWidgetValue("EKGCheck") != "true") {
        return; // Если проверка ЭКГ не пройдена, выходим из функции
    }

    std::string ekg_text = widget_handler_.GetWidgetValue("EKGText");
    std::string ekg_date = widget_handler_.GetWidgetValue("EKGDate");

    // Формируем результат
    std::string result = "ЭКГ: " + ekg_text + ';';

    // Добавляем результат анализа в дату
    AddObservationResultToDate(data, ekg_date, std::move(result));
    AddObservationToDate(data, ekg_date, "ЭКГ");
    AddPlainObservationToDate(data, std::move(ekg_date), "ЭКГ");
}

void JSONPrinter::ParseOAMAnalises(const QString& index, widgets_data &data) {
    std::string result;
    if (widget_handler_.GetWidgetValue("OAMCheck" + index) != "true") {
        return;
    }

    std::vector<std::string> result_oam;

    if (!widget_handler_.GetWidgetValue("BelokMochi" + index).empty()) {
        result_oam.push_back("Белок: " + widget_handler_.GetWidgetValue("BelokMochi" + index) + " г/л");
    }
    if (!widget_handler_.GetWidgetValue("PlotnostMochi" + index).empty()) {
        result_oam.push_back("Удельный вес: " + widget_handler_.GetWidgetValue("PlotnostMochi" + index));
    }
    if (!widget_handler_.GetWidgetValue("EritrocityMochi" + index).empty()) {
        result_oam.push_back("Эритроциты: " + widget_handler_.GetWidgetValue("EritrocityMochi" + index) + " /в поле зрения");
    }
    if (!widget_handler_.GetWidgetValue("LeycocityMochi" + index).empty()) {
        result_oam.push_back("Лейкоциты: " + widget_handler_.GetWidgetValue("LeycocityMochi" + index) + " /в поле зрения");
    }

    std::string OAM_date = widget_handler_.GetWidgetValue("OAMDate" + index);

    if (!result_oam.empty()) {
        result.append("ОАМ: ");
        for (const auto& analis : result_oam) {
            result.append(analis + ", ");
        }
        result.resize(result.size() - 2); // Удаляем последнюю запятую и пробел
        result.append("; ");
    }

    AddObservationResultToDate(data, OAM_date, std::move(result));
    AddObservationToDate(data, OAM_date, "ОАМ");
    AddPlainObservationToDate(data, std::move(OAM_date), "ОАМ");
}

void JSONPrinter::ParseBHAnalises(const QString& index, widgets_data &data) {
    std::string result;
    if (widget_handler_.GetWidgetValue("BHCheck" + index) != "true") {
        return;
    }

    std::vector<std::string> result_bh;

    if (!widget_handler_.GetWidgetValue("Kreatinin" + index).empty()) {
        result_bh.push_back("Креатинин: " + widget_handler_.GetWidgetValue("Kreatinin" + index) + " мкмоль/л");
    }
    if (!widget_handler_.GetWidgetValue("Glyukosa" + index).empty()) {
        result_bh.push_back("Глюкоза: " + widget_handler_.GetWidgetValue("Glyukosa" + index) + " ммоль/л");
    }
    if (!widget_handler_.GetWidgetValue("AST" + index).empty()) {
        result_bh.push_back("АСТ: " + widget_handler_.GetWidgetValue("AST" + index) + " Ед/л");
    }
    if (!widget_handler_.GetWidgetValue("ALT" + index).empty()) {
        result_bh.push_back("АЛТ: " + widget_handler_.GetWidgetValue("ALT" + index) + " Ед/л");
    }
    if (!widget_handler_.GetWidgetValue("Mochevina" + index).empty()) {
        result_bh.push_back("Мочевина: " + widget_handler_.GetWidgetValue("Mochevina" + index) + " ммоль/л");
    }
    if (!widget_handler_.GetWidgetValue("ShelochnayaFosf" + index).empty()) {
        result_bh.push_back("Щелочная фосфотаза: " + widget_handler_.GetWidgetValue("ShelochnayaFosf" + index) + " Ед/л");
    }
    if (!widget_handler_.GetWidgetValue("ObshiyBillir" + index).empty()) {
        result_bh.push_back("Общий биллирубин: " + widget_handler_.GetWidgetValue("ObshiyBillir" + index) + " мкмоль/л");
    }
    if (!widget_handler_.GetWidgetValue("ObshiyBelok" + index).empty()) {
        result_bh.push_back("Общий белок: " + widget_handler_.GetWidgetValue("ObshiyBelok" + index) + " г/л");
    }
    if (!widget_handler_.GetWidgetValue("PTI" + index).empty()) {
        result_bh.push_back("ПТИ: " + widget_handler_.GetWidgetValue("PTI" + index) + " %");
    }

    std::string BH_date = widget_handler_.GetWidgetValue("BHDate" + index);

    if (!result_bh.empty()) {
        result.append("БХ анализ крови: ");
        for (const auto& analis : result_bh) {
            result.append(analis + ", ");
        }
        result.resize(result.size() - 2); // Удаляем последнюю запятую и пробел
        result.append("; ");
    }

    AddObservationResultToDate(data, BH_date, std::move(result));
    AddObservationToDate(data, BH_date, "БХ анализ крови");
    AddPlainObservationToDate(data, std::move(BH_date), "БХ анализ крови");
}

void JSONPrinter::ParseKalAnalises(widgets_data &data) {
    if (widget_handler_.GetWidgetValue("KalCheck1") != "true") {
        return;
    }

    std::string kal_text = widget_handler_.GetWidgetValue("KalText1");
    std::string kal_date_str = widget_handler_.GetWidgetValue("KalDate1");

    std::string result = "анализы кала: " + kal_text + ';';
    AddObservationResultToDate(data, kal_date_str, std::move(result));
    AddObservationToDate(data, kal_date_str, "анализы кала на энтеробиоз, яйца глистов");
    AddPlainObservationToDate(data, kal_date_str, "анализы кала на энтеробиоз, яйца глистов");

}

void JSONPrinter::ParseConsultations(const QString& index, widgets_data &data) {
    std::string variant = widget_handler_.GetWidgetValue("ConsultationVariant" + index);
    std::string text = widget_handler_.GetWidgetValue("ConsultationText" + index);

    // Проверяем, что вариант и текст не пустые
    if (variant.empty() || text.empty()) {
        return;
    }
    std::string consultation_date = widget_handler_.GetWidgetValue("ConsultationDate" + index);

    // Формируем результат консультации
    std::string consultation_result = variant + ": " + text + "; ";

    AddConsultationResultToDate(data, consultation_date, std::move(consultation_result));
    AddConsultationsToDate(data, consultation_date, variant);
    AddPlainObservationToDate(data, consultation_date, variant);

}
void JSONPrinter::ParseManipulations(const QString& index, widgets_data &data) {
    std::string variant = widget_handler_.GetWidgetValue("ManipulationVariant" + index);
    std::string text = widget_handler_.GetWidgetValue("ManipulationText" + index);

    // Проверяем, что вариант и текст не пустые
    if (variant.empty() || text.empty()) {
        return;
    }

    std::transform(text.begin(), text.end(), text.begin(), tolower);
    std::string manipulation_date = widget_handler_.GetWidgetValue("ManipulationDate" + index);
    std::string manipulation_date_without_time = manipulation_date.substr(0, manipulation_date.find(' '));
    std::string anestesiya = widget_handler_.GetWidgetValue("ManipulationAnestesiya" + index);

    data["ManipulationDate" + manipulation_date_without_time] = manipulation_date;
    data["ManipulationHeading" + manipulation_date_without_time] = variant + " " + anestesiya;
    data["ManipulationText" + manipulation_date_without_time] = "\t" + text;
    AddManipulationResultToDate(data, manipulation_date_without_time, ' ' +
                                data["ManipulationHeading" + manipulation_date_without_time]);
}


void JSONPrinter::AddManipulationResultToDate(widgets_data& data,
                                 const std::string& manipulation_date,
                                 const std::string& manipulation_variant) {
    auto key = "Manipulation" + manipulation_date;
    if (data.count(key)) {
        data[key].append(manipulation_variant + ';');
    } else {
        data[key] = "Манипуляции от " + manipulation_date + ": " + manipulation_variant;
    }
}

void JSONPrinter::AddObservationToDate(widgets_data &data,
                                       const std::string& observation_date,
                                       const std::string& observation_variant) {
    auto key = "Analises" + observation_date;
    if (data.count(key)) {
        data[key].append(", " + observation_variant);
    } else {
        data[key] = "Лабораторные и инструментальные обследования от "
                    + observation_date + ": " + observation_variant;
    }

}

void JSONPrinter::AddObservationResultToDate(widgets_data& data,
                                             const std::string& observation_date,
                                             std::string &&observation_result) {
    auto key = "AnalisesResult" + observation_date;
    if (data.count(key)) {
        data[key].append(" " + std::move(observation_result));
    } else {
        data[key] = "Лабораторные и инструментальные обследования от "
                    + observation_date + ": " + std::move(observation_result);
    }

}

void JSONPrinter::AddPlainObservationToDate(widgets_data& data,
                                            const std::string& observation_date,
                                            const std::string& observation_variant) {
    auto key = "ObservationToCurrentDay" + observation_date;
    if (data.count(key)) {
        data[key].append(", " + observation_variant);
    } else {
        data[key] = observation_variant;
    }

}

void JSONPrinter::AddConsultationsToDate(widgets_data& data,
                            const std::string& consultation_date,
                            const std::string& consultation_variant) {
    auto key = "Consultations" + consultation_date;
    if (data.count(key)) {
        data[key].append(", " + consultation_variant);
    } else {
        data[key] =  "Консультации от " + consultation_date + ": " + consultation_variant;
    }

}
void JSONPrinter::AddConsultationResultToDate(widgets_data& data,
                                 const std::string& consultation_date,
                                 std::string&& consultation_result) {
    auto key = "ConsultationsResult" + consultation_date;
    if (data.count(key)) {
        data[key].append(consultation_result);
    } else {
        data[key] = "Консультации от " + consultation_date + ": " + consultation_result;
    }
}
