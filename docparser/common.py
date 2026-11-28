import json
import random
import datetime
from datetime import timedelta
import os
from docxtpl import DocxTemplate
from pathlib import Path
import ParsPervichniyOsmotr
import ParsOBhodsZaveduyushim
import ParsOperation
import ParsVipisnoyEpicris
import ParseDiaries
import ParseListNaznacheniy
import ParseVaccinationDocs
import locale
from pathlib import Path
import Constants

# locale.setlocale(locale.LC_TIME, 'Russian_Russia.1251')


# ========================================================================


def parse_treatment_in_stacionare(data):

    if f"TreatmentInStacionare" not in data:
        for index in range(1, Constants.NUMBER_OF_ANALISES):
            # Общий анализ крови
            parse_oak_analises(data, index)
            # Общий анализ мочи
            parse_oam_analises(data, index)
            # Биохимический анализ крови
            parse_bh_analises(data, index)
        if not parse_dop_obsledovaniya_json(data):
            for index in range(1, Constants.NUMBER_OF_DOP_OBSLEDOVANIYA):
                # Дополнительные обследования
                parse_dop_obsledovaniya(data, index)
        # Консультации
        for index in range(1, Constants.NUMBER_OF_CONSULTATIONS_AND_MANIPULATIONS):
            parse_consultations(data, index)
            # Манипуляции
            parse_manipulations(data, index)

        parse_EKG(data)
        parse_kal_analises(data)

        age = int(data["Age"])
        if "ВМП" in data["AnamnesDisease"] or age < 4:
            data["Diaries_every_day"] = True
        else:
            data["Diaries_every_day"] = False

        data[f"TreatmentInStacionare"] = True
        # обрезаем все переносы и пробелы с конца строк
    for key in data.keys():
        if isinstance(data[key], str):
            data[key] = data[key].strip()


# ========================================================================


def observation_date_key(date):
    if hasattr(date, "strftime"):
        return date.strftime("%d.%m.%Y")
    return str(date)


def add_observation_to_obseravation_plane_in_current_day(
    data, date, observation_variant
):
    date_key = observation_date_key(date)
    if f"ObservationToCurrentDay{date_key}" not in data:
        data[f"ObservationToCurrentDay{date_key}"] = observation_variant
    else:
        data[f"ObservationToCurrentDay{date_key}"] += ", " + observation_variant


def add_obsledovaniya_to_date(data, date, variant_obsledovaniya):
    date_key = observation_date_key(date)
    if f"Analises{date_key}" in data:
        data[f"Analises{date_key}"] += ", " + variant_obsledovaniya
    else:
        data[f"Analises{date_key}"] = (
            "Лабораторные и инструментальные обследования от "
            + date_key
            + ": "
            + variant_obsledovaniya
        )


def add_analises_result_to_date(data, date, analis_result):
    date_key = observation_date_key(date)
    if f"AnalisesResult{date_key}" in data:
        data[f"AnalisesResult{date_key}"] += analis_result
    else:
        data[f"AnalisesResult{date_key}"] = (
            "Лабораторные и инструментальные обследования от "
            + date_key
            + ": "
            + analis_result
        )


def parse_analises_to_current_date(data, current_date):
    result = ""
    for test_type in ["OAM", "OAK", "BH", "DopObsledovanie"]:
        if f"{test_type}Result{current_date}" in data:
            result += "".join(data[f"{test_type}Result{current_date}"])
    data["AnalisesToCurrentDate"] = result if result else "нет"


def parse_oak_analises(data, index):
    if data[f"OAKCheck{index}"]:
        hemoglobin = data[f"hemoglobin{index}"]
        eritrocit = data[f"eritrocit{index}"]
        leykocit = data[f"leykocit{index}"]
        trombocit = data[f"trombocit{index}"]
        gematocrit = data[f"gematocrit{index}"]
        soe = data[f"SOE{index}"]
        date_oak = str_to_date(data[f"OAKDate{index}"])
        result_oak = []
        if hemoglobin:
            result_oak.append(f"Гемоглобин: {hemoglobin} г/л")
        if eritrocit:
            result_oak.append(f"Эритроциты: {eritrocit} млн/мкл")
        if leykocit:
            result_oak.append(f"Лейкоциты: {leykocit} тыс/мкл")
        if trombocit:
            result_oak.append(f"Тромбоциты: {trombocit} тыс/мкл")
        if gematocrit:
            result_oak.append(f"Гематокрит: {gematocrit} %")
        if soe:
            result_oak.append(f"СОЭ: {soe}  мм/ч")

        add_obsledovaniya_to_date(data, date_oak, "ОАК")
        data[f"OAKResult{date_oak}"] = "ОАК: " + ", ".join(result_oak) + "; "
        add_analises_result_to_date(data, date_oak, data[f"OAKResult{date_oak}"])
        add_observation_to_obseravation_plane_in_current_day(data, date_oak, "ОАК")


def parse_oam_analises(data, index):
    if data[f"OAMCheck{index}"]:
        belok_mochi = data[f"BelokMochi{index}"]
        plotnost_mochi = data[f"PlotnostMochi{index}"]
        eritrocity_mochi = data[f"EritrocityMochi{index}"]
        leycocity_mochi = data[f"LeycocityMochi{index}"]
        date_oam = str_to_date(data[f"OAMDate{index}"])
        result_oam = []
        if belok_mochi:
            result_oam.append(f"Белок: {belok_mochi} г/л")
        if plotnost_mochi:
            result_oam.append(f"Удельный вес: {plotnost_mochi}")
        if eritrocity_mochi:
            result_oam.append(f"Эритроциты: {eritrocity_mochi} /в поле зрения")
        if leycocity_mochi:
            result_oam.append(f"Лейкоциты: {leycocity_mochi} /в поле зрения")

        add_obsledovaniya_to_date(data, date_oam, "ОАМ")
        data[f"OAMResult{date_oam}"] = "ОАМ: " + ", ".join(result_oam) + "; "
        add_analises_result_to_date(data, date_oam, data[f"OAMResult{date_oam}"])
        add_observation_to_obseravation_plane_in_current_day(data, date_oam, "ОАМ")


def parse_bh_analises(data, index):
    if data[f"BHCheck{index}"]:
        ast = data[f"AST{index}"]
        alt = data[f"ALT{index}"]
        mochevina = data[f"Mochevina{index}"]
        kreatinin = data[f"Kreatinin{index}"]
        shelochnaya = data[f"ShelochnayaFosf{index}"]
        billirubin = data[f"ObshiyBillir{index}"]
        belok = data[f"ObshiyBelok{index}"]
        glyukosa = data[f"Glyukosa{index}"]
        pti = data[f"PTI{index}"]
        date_bh = str_to_date(data[f"BHDate{index}"])

        result_bh = []
        if kreatinin:
            result_bh.append(f"Креатинин: {kreatinin} мкмоль/л")
        if glyukosa:
            result_bh.append(f"Глюкоза: {glyukosa} ммоль/л")
        if ast:
            result_bh.append(f"АСТ: {ast} Ед/л")
        if alt:
            result_bh.append(f"АЛТ: {alt} Ед/л")
        if mochevina:
            result_bh.append(f"Мочевина: {mochevina} ммоль/л")
        if shelochnaya:
            result_bh.append(f"Щелочная фосфотаза: {shelochnaya} Ед/л")
        if billirubin:
            result_bh.append(f"Общий биллирубин: {billirubin} мкмоль/л")
        if belok:
            result_bh.append(f"Общий белок: {belok} г/л")
        if pti:
            result_bh.append(f"ПТИ: {pti} %")

        add_obsledovaniya_to_date(data, date_bh, "БХ анализ крови")
        data[f"BHResult{date_bh}"] = "БХ анализ крови: " + ", ".join(result_bh) + "; "
        add_analises_result_to_date(data, date_bh, data[f"BHResult{date_bh}"])
        add_observation_to_obseravation_plane_in_current_day(
            data, date_bh, "БХ анализ крови"
        )


def parse_kal_analises(data):
    if data["KalCheck1"]:
        date = str_to_date(data[f"KalDate1"]).date()
        result = "анализы кала: " + data["KalText1"] + ";"
        add_analises_result_to_date(data, date, result)
        add_obsledovaniya_to_date(
            data, date, "анализы кала на энтеробиоз, яйца глистов"
        )
        add_observation_to_obseravation_plane_in_current_day(
            data, date, "анализы кала на энтеробиоз, яйца глистов"
        )


# ========================================================================


def parse_all_obsledovaniya_vstacionare_to_current_day(data, current_diary_day):
    dop_text = data.get("TextAfterIncome")
    previous_date = current_diary_day - timedelta(days=1)
    income_date = parse_income_date(data).date()
    # переберем циклом с даты поступления до даты предыдущего дневника,
    # чтобы получить все проведенные вмешательства
    while income_date <= previous_date:
        key = f"AnalisesResult{income_date.strftime('%d.%m.%Y')}"
        if key in data:
            dop_text += data[key] + "\n"
        income_date += timedelta(days=1)
    if dop_text:
        data["AllExaminationsToCurrentDay"] = dop_text.rstrip()
        return True
    else:
        data["AllExaminationsToCurrentDay"] = "нет"
        return False


def parse_dop_obsledovaniya_json(data):
    raw_json = str(data.get("DopObsledovaniyaJson", "")).strip()
    if not raw_json:
        return False

    try:
        rows = json.loads(raw_json)
    except (TypeError, ValueError):
        return False

    if not isinstance(rows, list):
        return False

    for row in rows:
        if not isinstance(row, dict):
            continue

        variant = str(row.get("variant", "")).strip()
        text = str(row.get("text", "")).strip()
        date_raw = str(row.get("date", "")).strip()

        if not variant and not text:
            continue

        if date_raw:
            date = str_to_date(date_raw)
        else:
            date = parse_income_date(data).date()

        add_obsledovaniya_to_date(data, date, variant)
        add_analises_result_to_date(data, date, f"{variant} {text}; ")
        add_observation_to_obseravation_plane_in_current_day(data, date, variant)

    return True


def parse_dop_obsledovaniya(data, index):
    if (
        f"DopObsledovanieText{index}" in data
        and str(data[f"DopObsledovanieText{index}"]).strip()
    ):
        date = str_to_date(data[f"DopObsledovanieDate{index}"])
        variant = data[f"DopObsledovanieVariant{index}"]
        text = data[f"DopObsledovanieText{index}"]
        add_obsledovaniya_to_date(data, date, variant)
        add_analises_result_to_date(data, date, f"{variant} {text}; ")
        add_observation_to_obseravation_plane_in_current_day(data, date, variant)


def parse_EKG(data):
    if data["EKGCheck"]:
        date = str_to_date(data[f"EKGDate"])
        result = "ЭКГ: " + data["EKGText"] + ";"
        add_analises_result_to_date(data, date, result)
        add_obsledovaniya_to_date(data, date, "ЭКГ")
        add_observation_to_obseravation_plane_in_current_day(data, date, "ЭКГ")


# ========================================================================


def add_consulation_result_to_date(data, date, consultation_result):
    if f"ConsultationsResult{date}" in data:
        data[f"ConsultationsResult{date}"] += consultation_result
    else:
        data[f"ConsultationsResult{date}"] = (
            "Консультации от " + date.strftime("%d.%m.%Y") + ": " + consultation_result
        )


def add_consultation_to_date(data, date, consultation_variant):
    if f"Consultation{date}" in data:
        data[f"Consultations{date}"] += consultation_variant
    else:
        data[f"Consultations{date}"] = (
            "Консультации от " + date.strftime("%d.%m.%Y") + ": " + consultation_variant
        )


def parse_all_consultations_to_current_day(data, current_diary_day):
    income_date = parse_income_date(data).date()
    result = []
    while income_date <= current_diary_day:
        key = f"ConsultationsResult{income_date.strftime('%d.%m.%Y')}"
        if key in data:
            result.append(data[key])
        income_date += timedelta(days=1)
    data["AllConsultationsToCurrentDay"] = "\n".join(result) if result else "нет"


def parse_consultations(data, index):

    if (data[f"ConsultationVariant{index}"]).replace(" ", "") != "" and (
        data[f"ConsultationText{index}"]
    ).replace(" ", "") != "":
        variant = data[f"ConsultationVariant{index}"]
        text = data[f"ConsultationText{index}"]
        date = str_to_date(data[f"ConsultationDate{index}"])
        consultation_result = variant + ": " + text + "; "

        add_consulation_result_to_date(data, date, consultation_result)
        add_consultation_to_date(data, date, variant)
        add_observation_to_obseravation_plane_in_current_day(data, date, variant)


# ========================================================================


def parse_all_medical_inveroments_to_current_day(data, current_date):
    previous_date = current_date - timedelta(days=1)
    income_date_without_time = parse_income_date(data).date()

    result = ""
    # переберем циклом с даты поступления до даты предыдущего дневника,
    # что получить все проводимые вмешательства
    while income_date_without_time <= previous_date:
        current_day = income_date_without_time.strftime("%d.%m.%Y")
        if f"Analises{current_day}" in data:
            result += data[f"Analises{current_day}"] + "\n"
        if f"Manipulation{current_day}" in data:
            result += data[f"Manipulation{current_day}"] + "\n"
        income_date_without_time += timedelta(days=1)
    if result == "":
        data["AllMedicalInveromentsToCurrentDay"] = "не проводились"
        return False
    else:
        data["AllMedicalInveromentsToCurrentDay"] = result.rstrip()
        return True


def parse_all_manipulations_to_current_date(data, current_date):
    previous_date = current_date - timedelta(days=1)
    income_date = parse_income_date(data).date()
    result = ""
    while income_date <= previous_date:
        current_day = income_date.strftime("%d.%m.%Y")
        if f"Manipulation{current_day}" in data:
            result += data[f"Manipulation{current_day}"] + "\n"
        income_date += timedelta(days=1)
    if result == "":
        data["AllManipulationsToCurrentDay"] = "не проводились"
        return False
    else:
        data["AllManipulationsToCurrentDay"] = result.rstrip()
        return True


def clear_manipulation_data(data):
    data["CurrentManipulationDate"] = ""
    data["CurrentManipulationHeading"] = ""
    data["CurrentManipulationText"] = ""
    data["CurrentDoctorName"] = ""
    data["CurrentManipulation"] = ""


def parse_manipulation_to_current_date(data, current_date, diary_tmpl: DocxTemplate):
    current_date_without_time = current_date.date().strftime("%d.%m.%Y")
    next_date_without_time = (
        (current_date + timedelta(days=1)).date().strftime("%d.%m.%Y")
    )
    if (
        f"ManipulationDate{current_date_without_time}" in data
        and f"ManipulationText{current_date_without_time}" in data
    ):
        manipulation_date_with_time = str_to_date_time(
            data[f"ManipulationDate{current_date_without_time}"]
        )
    elif (
        f"ManipulationDate{next_date_without_time}" in data
        and f"ManipulationText{next_date_without_time}" in data
    ):
        manipulation_date_with_time = str_to_date_time(
            data[f"ManipulationDate{next_date_without_time}"]
        )
        current_date_without_time = next_date_without_time
    else:
        clear_manipulation_data(data)
        return
    if (
        manipulation_date_with_time >= current_date
        and manipulation_date_with_time < current_date + timedelta(days=1)
    ):
        data["CurrentManipulationDate_rus"] = str_to_date_time(
            data[f"ManipulationDate{current_date_without_time}"]
        ).strftime("%d.%m.%Y %H:%M")
        data["CurrentManipulationHeading"] = data[
            f"ManipulationHeading{current_date_without_time}"
        ]
        data["CurrentManipulationText"] = data[
            f"ManipulationText{current_date_without_time}"
        ]
        data["CurrentDoctorName"] = data["DoctorName"]
        data["NextDiary"] = ""
        data["NextPage"] = str("\f{{p NextDiary}}")
        sd = diary_tmpl.new_subdoc("шаблоны_основных_документов/Манипуляция.docx")
        data["CurrentManipulation"] = sd
    else:
        clear_manipulation_data(data)


def add_manipulation_result_to_date(data, date, variant):
    if f"Manipulation{date}" in data:
        data[f"Manipulation{date}"] += variant + ";"
    else:
        data[f"Manipulation{date}"] = (
            "Манипуляции от " + date.strftime("%d.%m.%Y") + ": " + variant
        )


def parse_manipulations(data, index):
    if (
        data.get(f"ManipulationVariant{index}") != ""
        and data.get(f"ManipulationText{index}") != ""
    ):
        date = str_to_date(data[f"ManipulationDate{index}"])
        data[f"ManipulationDate{date}"] = data[f"ManipulationDate{index}"]
        data[f"ManipulationHeading{date}"] = (
            data[f"ManipulationVariant{index}"]
            + " под "
            + data[f"ManipulationAnestesiya{index}"]
        )
        data[f"ManipulationText{date}"] = (
            "\t Под "
            + data[f"ManipulationAnestesiya{index}"]
            + " "
            + data[f"ManipulationText{index}"].lower()
        )
        add_manipulation_result_to_date(data, date, data[f"ManipulationHeading{date}"])


# ========================================================================


def parse_current_naznacheniya(data, date):
    date_without_time = date.date().strftime("%d.%m.%Y")
    result = ""
    if f"ManipulationDate{date_without_time}" in data:
        result += data[f"ManipulationHeading{date_without_time}"] + "\n"
    if f"MedicinesPurposeOnDate{date_without_time}" in data:
        result += data[f"MedicinesPurposeOnDate{date_without_time}"] + "\n"
    if f"MedicinesCancelOnDate{date_without_time}" in data:
        result += data[f"MedicinesCancelOnDate{date_without_time}"] + "\n"
    if result:
        data["CurrentNaznacheniya"] = result.rstrip()
    else:
        data["CurrentNaznacheniya"] = "без дополнений"


# =============================================================ФУНКЦИИ ПАР


def parse_random_vital(data):
    ranges = {
        "Puls": 4,
        "SistolicAD": 3,
        "DiastolicAD": 3,
        "BreathRate": 2,
        "Saturation": 1,
    }
    if data["Saturation"] == 100:
        data["Saturation"] -= 1

    for key, range_value in ranges.items():
        original_value = int(data[key])
        data[f"Current{key}"] = random.randint(
            original_value - range_value, original_value + range_value
        )
    original_temperature = float(str(data["Termometria"]).replace(",", "."))
    data["CurrentTermometria"] = round(
        random.uniform(original_temperature - 0.2, original_temperature + 0.2), 1
    )


def parse_obsledovaniya_do_postupleniya(data):
    if "ObsledovaniyaDoPostupleniya" not in data:
        results = []
        if data.get("MRTBeforeIncomeCheck"):
            results.append(
                f"МРТ от {str_to_date(data['MRTBeforeIncomeDate1']).strftime('%d.%m.%Y')}: {data['MRTBeforeIncome']}"
            )
        if data.get("RKTBeforeIncomeCheck"):
            results.append(
                f"РКТ от {str_to_date(data['RKTBeforeIncomeDate1']).strftime('%d.%m.%Y')}: {data['RKTBeforeIncome']}"
            )
        if data.get("RgrBeforeIncomeCheck"):
            results.append(
                f"Рентгенография от {str_to_date(data['RgrBeforeIncomeDate']).strftime('%d.%m.%Y')}: {data['RgrBeforeIncome']}"
            )
        if data.get("DopBeforeIncomeCheck"):
            results.append(data["TextBeforeIncome"])
        data["ObsledovaniyaDoPostupleniya"] = ", ".join(results) if results else "нет"


# ================================================================ПАРСИНГ


def str_to_date(date_str):
    return datetime.datetime.strptime(date_str, "%d.%m.%Y")


def str_to_date_time(date_str):
    return datetime.datetime.strptime(date_str, "%d.%m.%Y %H:%M")


def parse_birthday_date(data):
    if "BirthdayDate_rus" not in data:
        data["BirthdayDate_rus"] = str_to_date(data["BirthdayDate"]).strftime(
            "%d.%m.%Y"
        )


def parse_date_of_first_examination(data):
    if "DateOfFirstExamination_rus" not in data:
        income_date = parse_income_date(data)
        data["DateOfFirstExamination1_rus"] = income_date.strftime("%d.%m.%Y %H:%M")
        data["DateOfFirstExamination_rus"] = (
            income_date + timedelta(minutes=10)
        ).strftime("%d.%m.%Y %H:%M")
        data["DateOfFirstExamination"] = income_date + timedelta(minutes=10)
    return data["DateOfFirstExamination"]


def days_to_outcome(data):
    income_date = parse_income_date(data)
    outcome_date = parse_outcome_date(data)
    return (outcome_date.date() - income_date.date()).days


def round_minutes_to_ten(value):
    """Округлить минуты до ближайшего десятка (30, 40, 50 и т.д.)."""
    minute = value.minute
    rounded = value.replace(minute=(minute // 10) * 10, second=0, microsecond=0)
    if minute % 10 >= 5:
        rounded += timedelta(minutes=10)
    return rounded


def parse_date_of_obhod_szaveduyusim_examination(data):
    if "ObhodDate_rus" not in data:
        income_date = parse_income_date(data)
        outcome_date = parse_outcome_date(data)
        days = days_to_outcome(data)
        refusal = bool(data.get("OtkazOtGospitalizacii"))

        if refusal and days == 0:
            # Отказ в день поступления: обход между первичным осмотром и выпиской.
            first_examination = parse_date_of_first_examination(data)
            if outcome_date <= first_examination:
                result_date = outcome_date
            else:
                middle = first_examination + (outcome_date - first_examination) / 2
                result_date = round_minutes_to_ten(middle)
                if result_date <= first_examination:
                    result_date = first_examination + timedelta(minutes=1)
                if result_date >= outcome_date:
                    result_date = outcome_date - timedelta(minutes=1)
        elif refusal and days == 1:
            # Отказ на следующий день: обычные 08:30, но не позже выписки.
            result_date = (income_date + timedelta(days=1)).replace(
                hour=8, minute=30, second=0, microsecond=0
            )
            if outcome_date < result_date:
                result_date = max(
                    outcome_date - timedelta(minutes=10),
                    (income_date + timedelta(days=1)).replace(
                        hour=0, minute=0, second=0, microsecond=0
                    ),
                )
        else:
            start_time = income_date.replace(hour=0, minute=0)
            end_time = income_date.replace(hour=8, minute=0)
            result_date = (
                end_time
                if start_time <= income_date <= end_time
                else end_time + timedelta(days=1)
            )
            result_date = result_date.replace(minute=30, second=0, microsecond=0)

        data["ObhodDate_rus"] = result_date.strftime("%d.%m.%Y %H:%M")
        data["ObhodDate"] = result_date
    return data["ObhodDate"]


def parse_preoperation_epicris_date(data, index):
    if is_operation_exist(data, index):
        if f"PreOperationEpicrisDate{index}_rus" not in data:
            preoperation_epicris_date = str_to_date_time(
                data.get(f"OperationBegin{index}")
            )
            preoperation_epicris_date -= timedelta(minutes=15)
            data[f"PreOperationEpicrisDate{index}_rus"] = (
                preoperation_epicris_date.strftime("%d.%m.%Y %H:%M")
            )
            data[f"PreOperationEpicrisDate{index}"] = preoperation_epicris_date
        else:
            return data[f"PreOperationEpicrisDate{index}"]
    else:
        return None


def parse_operation_begin_date(data, index):
    if is_operation_exist(data, index):
        if f"OperationBegin{index}_rus" not in data:
            data[f"OperationBegin{index}_rus"] = str_to_date_time(
                data[f"OperationBegin{index}"]
            ).strftime("%d.%m.%Y %H:%M")
            data[f"OperationBegin{index}"] = str_to_date_time(
                data[f"OperationBegin{index}"]
            )
        else:
            return data[f"OperationBegin{index}"]
    else:
        return None


def parse_operation_end_date(data, index):
    if is_operation_exist(data, index):
        if f"OperationEnd{index}_rus" not in data:
            data[f"OperationEnd{index}_rus"] = str_to_date_time(
                data[f"OperationEnd{index}"]
            ).strftime("%d.%m.%Y %H:%M")
            data[f"OperationEnd{index}"] = str_to_date_time(
                data[f"OperationEnd{index}"]
            )
        else:
            return data[f"OperationEnd{index}"]
    else:
        return None


def parse_operations_dates(data, index):

    parse_preoperation_epicris_date(data, index)
    parse_operation_begin_date(data, index)
    parse_operation_end_date(data, index)


def parse_income_date(data):
    if "IncomeDate_rus" not in data:
        income_date = str_to_date_time(data["IncomeDate"])
        data["IncomeDate_rus"] = income_date.strftime("%d.%m.%Y %H:%M")
        data["IncomeDate"] = income_date
    return data["IncomeDate"]


def parse_outcome_date(data):
    if "OutComeDate_rus" not in data:
        outcome_date = str_to_date_time(data["OutComeDate"])
        data["OutComeDate_rus"] = outcome_date.strftime("%d.%m.%Y %H:%M")
        data["OutComeDate"] = outcome_date
    return data["OutComeDate"]


def parse_income_and_outcome_date(data):
    parse_income_date(data)
    parse_outcome_date(data)


def parse_all_dates_in_rus(data):
    parse_date_of_first_examination(data)
    parse_birthday_date(data)
    parse_date_of_obhod_szaveduyusim_examination(data)
    parse_operations_dates(data, "")
    parse_operations_dates(data, 2)
    parse_income_and_outcome_date(data)


# =====================================================ВЫВОД ИСТОРИИ В DOC


def delete_docx_file(file_path):
    try:
        os.remove(file_path)
        # print(f'Файл {file_path} успешно удален.')
    except FileNotFoundError:
        print(f"Файл {file_path} не найден.")
    except PermissionError:
        print(f"Нет прав для удаления файла {file_path}.")
    except Exception as e:
        print(f"Произошла ошибка при удалении файла {file_path}: {e}")


def delete_docx_files(files):
    for doc in files:
        delete_docx_file(doc)


def print_hystory(data, output_file):
    date_of_round_examination = parse_date_of_obhod_szaveduyusim_examination(data)
    date_of_first_operation = parse_operation_begin_date(data, "")
    date_of_second_operation = parse_operation_begin_date(data, 2)
    date_vipisnoy_epicris = parse_outcome_date(data)
    patient_fio = data["PatientFIO"]
    doc_name = ""

    # словарь с функциями
    function_map = {
        "FirstExamination": lambda data: ParsPervichniyOsmotr.ParsePervichniyOsmotr(
            data
        ),
        "RoundExamination": lambda data: ParsOBhodsZaveduyushim.ParseObhodSZaveduyushim(
            data
        ),
        "Operation1": lambda data: ParsOperation.ParseOperationProtocol(data, ""),
        "Operation2": lambda data: ParsOperation.ParseOperationProtocol(data, 2),
        "Epicrisis": lambda data: ParsVipisnoyEpicris.ParseVipisnoyEpicris(data),
        "Diaries": lambda data: ParseDiaries.ParseDiaries(data),
        "Form045": lambda data: ParseVaccinationDocs.ParseForm045(data),
        "AppointmentList": lambda data: ParseListNaznacheniy.ParseAppointmentList(data),
        "PreoperationDiaries": lambda data: ParseDiaries.ParseConservativeDiaries(
            data,
            date_of_round_examination + timedelta(days=1),
            date_of_first_operation,
            doc_to_delete,
            doc_name,
        ),
        "DiariesFromFirstOperationToSecondOperation": lambda data: ParseDiaries.ParsePostOperationDiaries(
            data,
            parse_operation_end_date(data, ""),
            date_of_second_operation,
            doc_to_delete,
            doc_name,
        ),
        "DiariesFromSecondOperationToEpicrisis": lambda data: ParseDiaries.ParsePostOperationDiaries(
            data,
            parse_operation_end_date(data, 2),
            date_vipisnoy_epicris,
            doc_to_delete,
            doc_name,
        ),
        "DiariesFromFirstOperationToEpicrisis": lambda data: ParseDiaries.ParsePostOperationDiaries(
            data,
            parse_operation_end_date(data, ""),
            date_vipisnoy_epicris,
            doc_to_delete,
            doc_name,
        ),
        "DiariesFromRoundExaminationToEpicrisis": lambda data: ParseDiaries.ParseConservativeDiaries(
            data,
            date_of_round_examination + timedelta(days=1),
            date_vipisnoy_epicris,
            doc_to_delete,
            doc_name,
        ),
        "PostOperationDiary": lambda data: ParseDiaries.ParsePostOperationDiaries(
            data,
            parse_operation_end_date(data, ""),
            parse_operation_end_date(data, ""),
            doc_to_delete,
            doc_name,
        ),
        "DiariesFromRoundToSecondOperation2": lambda data: ParseDiaries.ParsePostOperationDiaries(
            data,
            date_of_round_examination + timedelta(days=1),
            date_of_second_operation,
            doc_to_delete,
            doc_name,
        ),
        "DiariesFromRoundExaminationToEpicrisisAfterOperation": lambda data: ParseDiaries.ParsePostOperationDiaries(
            data,
            date_of_round_examination + timedelta(days=1),
            date_vipisnoy_epicris,
            doc_to_delete,
            doc_name,
        ),
    }

    doc_to_delete = []
    is_last_doc = True
    documents_to_print = data["print_settings"]
    if documents_to_print[0] == "ALL_HYSTORY":
        documents_to_print = parse_all_history(data)

    total_documents = len(documents_to_print)

    previous_diary_path = Path("Output")
    for index in reversed(range(total_documents)):
        key = documents_to_print[index]
        func = function_map.get(documents_to_print[index])

        doc_name = f"{index}{patient_fio}"
        if func:
            doc_tmpl = func(data)
            if doc_tmpl:
                if "CurrentManipulation" in data:
                    if data["CurrentManipulation"]:
                        doc_tmpl.render(data, autoescape=True)
                        doc_tmpl.save(f"Output/Diary_with_manipulation{doc_name}.docx")
                        doc_tmpl = DocxTemplate(
                            f"Output/Diary_with_manipulation{doc_name}.docx"
                        )
                        doc_to_delete.append(
                            f"Output/Diary_with_manipulation{doc_name}.docx"
                        )
                if is_last_doc:
                    is_last_doc = False
                else:
                    previos_diary = doc_tmpl.new_subdoc(previous_diary_path)
                    data["NextDiary"] = previos_diary
                    data["NextPage"] = "\f"
                doc_tmpl.render(data, autoescape=True)

                if index == 0:
                    doc_tmpl.save(output_file)
                    break
                previous_diary_path = Path("Output") / f"{doc_name}.docx"
                doc_tmpl.save(previous_diary_path)
                doc_to_delete.append(previous_diary_path)

            else:

                print(f"Ошибка: функция для '{key}' вернула False или None.")

    delete_docx_files(doc_to_delete)


def parse_all_history(data):
    date_of_round_examination = parse_date_of_obhod_szaveduyusim_examination(data)
    date_of_first_operation = parse_operation_begin_date(data, "")
    date_of_second_operation = parse_operation_begin_date(data, 2)
    diaries = []
    # добавляем первичный осмотр
    diaries.append("FirstExamination")

    # если есть операция
    if date_of_first_operation:
        # если дата первой операции меньше даты обхода
        if date_of_first_operation < date_of_round_examination:
            # добавляем сначала операционный протокол, затем обход
            diaries.append("Operation1")
            diaries.append("PostOperationDiary")
            diaries.append("RoundExamination")
            if date_of_second_operation:
                diaries.append("DiariesFromRoundToSecondOperation2")
                diaries.append("Operation2")
                diaries.append("DiariesFromSecondOperationToEpicrisis")
            else:
                diaries.append("DiariesFromRoundExaminationToEpicrisisAfterOperation")
        # если дата первой операции больше или равна дате обхода
        elif date_of_first_operation >= date_of_round_examination:
            # добавляем обход
            diaries.append("RoundExamination")
            # добавляем предоперационные дневники начиная с следующего дня
            # после обхода и заканчивая днем операции
            diaries.append("PreoperationDiaries")
            # добавляем первую операцию
            diaries.append("Operation1")
            # если есть вторая операция
            if date_of_second_operation:
                # добавляем дневники после первой операции до второй операции
                diaries.append("DiariesFromFirstOperationToSecondOperation")
                # добавляем вторую операцию
                diaries.append("Operation2")
                # добавляем дневники после второй операции до выписки
                diaries.append("DiariesFromSecondOperationToEpicrisis")
            # если второй операции нет добавляем после операции и до выписки
            else:
                diaries.append("DiariesFromFirstOperationToEpicrisis")
    # если нет операций, то добавляем обход, дневники с обхода по выписку и
    # выписку
    else:
        diaries.append("RoundExamination")
        diaries.append("DiariesFromRoundExaminationToEpicrisis")

    diaries.append("Epicrisis")
    if diaries:
        return diaries
    else:
        return False


# ============================================ДОПОЛНИТЕЛЬНО==================================================================================


def is_operation_exist(data, index):
    if f"OperationExist{index}" not in data:
        if (
            data.get(f"OperationName{index}") != ""
            and data.get(f"OperationProtocol{index}") != ""
        ):
            data[f"OperationExist{index}"] = True
        else:
            data[f"OperationExist{index}"] = False
        return data[f"OperationExist{index}"]
    else:
        return data[f"OperationExist{index}"]
