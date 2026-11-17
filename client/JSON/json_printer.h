#ifndef JSON_PRINTER_H
#define JSON_PRINTER_H

#include "json_builder.h"
#include <Common/common.h>
// #include "json_builder.h"

class JSONPrinter
{
    using widgets_data = std::unordered_map<std::string, std::string>;
public:
    explicit JSONPrinter(WidgetHandler& widget_handler);
    void MakeJSONFromData(const std::vector<std::string> &doc_to_print);

private:

    //Здесь находится часть функционала перенесенная из docparser
    //Планируется перенести оттуда всю практически всю логику и оставить только парсинг документов

    void ParseAllTreatmentInStacionare(widgets_data& data, JSON::Builder& builder);
    void ParseOAKAnalises(const QString& index, widgets_data& data);
    void ParseOAMAnalises(const QString& index, widgets_data& data);
    void ParseBHAnalises(const QString& index, widgets_data &data);
    void ParseKalAnalises(widgets_data& data);
    void ParseDopObservations(const QString& index, widgets_data &data);
    void ParseEKG(widgets_data& data);
    bool IsNeedToParseDiariesEveryDay();

    void AddObservationToDate(widgets_data& data,
                              const std::string& observation_date,
                              const std::string& observation_variant);
    void AddObservationResultToDate(widgets_data& data,
                                 const std::string& observation_date,
                                 std::string&& observation_result);
    void AddPlainObservationToDate(widgets_data& data,
                                    const std::string& observation_date,
                                    const std::string& observation_variant);

    void ParseConsultations(const QString& index, widgets_data &data);
    void ParseManipulations(const QString& index, widgets_data &data);

    void AddConsultationsToDate(widgets_data& data,
                                const std::string& consultation_date,
                                const std::string& consultation_variant);
    void AddConsultationResultToDate(widgets_data& data,
                                    const std::string& consultation_date,
                                    std::string&& consultation_result);

    void AddManipulationResultToDate(widgets_data& data,
                                    const std::string& manipulation_date,
                                    const std::string& manipulation_variant);

    void ParseMedicines(widgets_data &data);
    void ParseFisioProcedures(widgets_data &data);
    void ParseVaccinations(widgets_data &data);
    void ParseAppointmentListData(widgets_data& data);
    void ParseForm045(widgets_data& data);

    double GetAigDosage();

    WidgetHandler& widget_handler_;

};

#endif // JSON_PRINTER_H
