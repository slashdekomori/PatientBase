#ifndef DIARIESPARSER_H
#define DIARIESPARSER_H

#include "Common/common.h"

struct Diarie {
    QDateTime date_;
    bool is_depatment_round = false;
    QString complaints_;
    QString comon_status_;
    QString nevrologic_status_;
    QString temperature_;
    QString fisical_status_;
    QString local_status_;
    QString adds_;
    QString therapy_plan_;

    int heart_rate_;
    int breath_rate_;
    int sistolic_ad_;
    int diastolic_ad_;
};

class DiariesParser {
public:
    DiariesParser(WidgetHandler &widget_handler);
    QVector<Diarie> ParseDiearies();
private:
    QDateTime RandomizeTime(const QDateTime &originalDateTime);
    WidgetHandler& widget_handler_;
};


#endif // DIARIESPARSER_H
