#include "diariesparser.h"
#include <QVector>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QRandomGenerator>


DiariesParser::DiariesParser(WidgetHandler& widget_handler) : widget_handler_(widget_handler) {
}

QVector<Diarie> DiariesParser::ParseDiearies() {
    QVector<Diarie> diaries;
    widget_handler_.FillWidgetsMap();
    QDateTime income_date = qobject_cast<QDateTimeEdit*>(widget_handler_.GetWidget("IncomeDate"))->dateTime();
    QDateTime outcome_date = qobject_cast<QDateTimeEdit*>(widget_handler_.GetWidget("OutComeDate"))->dateTime();
    int heart_rate = std::stoi(widget_handler_.GetWidgetValue("Puls"));
    int breath_rate = std::stoi(widget_handler_.GetWidgetValue("BreathRate"));
    int sistolic_ad = std::stoi(widget_handler_.GetWidgetValue("SistolicAD"));
    int diastolic_ad = std::stoi(widget_handler_.GetWidgetValue("DiastolicAD"));
    QRandomGenerator random_generator;
    for (QDateTime current_diary_day = income_date; current_diary_day <= outcome_date;) {
        Diarie current_diary;
        current_diary.date_ = RandomizeTime(current_diary_day);
        current_diary.adds_ = "нет";
        current_diary.breath_rate_ = breath_rate + random_generator.bounded(-1,1);
        current_diary.heart_rate_ = heart_rate + random_generator.bounded(-3,4);
        current_diary.sistolic_ad_ = sistolic_ad + random_generator.bounded(-3,4);
        current_diary.diastolic_ad_ = diastolic_ad + random_generator.bounded(-2,4);
        diaries.push_back(std::move(current_diary));
        current_diary_day = current_diary_day.addDays(1);
    }
    return diaries;
}

QDateTime DiariesParser::RandomizeTime(const QDateTime &originalDateTime) {
    // Оставляем исходную дату
    QDate date = originalDateTime.date();

    // Возможные минуты (с шагом 5 минут от 10 до 30)
    QList<int> possibleMinutes = {10, 15, 20, 25, 30};

    // Выбираем случайный индекс
    int randomIndex = QRandomGenerator::global()->bounded(possibleMinutes.size());

    // Устанавливаем время: 8 часов + случайные минуты
    QTime newTime(8, possibleMinutes[randomIndex]);

    // Возвращаем новую дату-время
    return QDateTime(date, newTime);
}
