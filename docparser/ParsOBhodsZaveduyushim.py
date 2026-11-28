import common
from datetime import timedelta
import datetime
import Constants

from docxtpl import DocxTemplate


def ParseObhodSZaveduyushim(data):
    ShablonObhoda = DocxTemplate(Constants.PATH_TO_ROUND_EXAMINATION_TEMPLATE)

    common.parse_random_vital(data)
    common.parse_obsledovaniya_do_postupleniya(data)
    common.parse_date_of_obhod_szaveduyusim_examination(data)

    round_examination_with_time = common.parse_date_of_obhod_szaveduyusim_examination(
        data
    )
    round_examination = round_examination_with_time.date()

    common.parse_manipulation_to_current_date(
        data, round_examination_with_time, ShablonObhoda
    )

    common.parse_analises_to_current_date(data, round_examination)
    outcome_date = common.parse_outcome_date(data)
    days_to_outcome = (outcome_date.date() - common.parse_income_date(data).date()).days
    is_early_refusal = (
        data.get("OtkazOtGospitalizacii") and 0 <= days_to_outcome <= 1
    )
    if is_early_refusal:
        # AllExaminationsToCurrentDay и AllConsultationsToCurrentDay уже заполнены
        # выписным эпикризом, т.к. print_hystory рендерит документы в обратном порядке.
        data["AllMedicalInveromentsToCurrentDay"] = (
            "Стационарное лечение прекращено по инициативе пациента. "
            "Отказ от дальнейшего стационарного лечения оформлен в письменном виде, "
            "последствия отказа разъяснены."
        )
        data["RecomendationsExaminationSec"] = "нет"
        data["CurrentNaznacheniya"] = "нет"
    else:
        common.parse_all_obsledovaniya_vstacionare_to_current_day(
            data, round_examination
        )
        common.parse_all_medical_inveroments_to_current_day(data, round_examination)
        common.parse_all_consultations_to_current_day(data, round_examination)
        common.parse_current_naznacheniya(data, round_examination_with_time)

    # 	return True
    return ShablonObhoda


# parse_random_vital(data)
# parse_current_naznacheniya(data, current_diary_day)
