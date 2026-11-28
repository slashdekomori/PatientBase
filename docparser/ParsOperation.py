import common
from docxtpl import DocxTemplate
import datetime
from datetime import timedelta
import Constants


def parse_antibiotic_profilaction(data, index, operation_begin_date):
    antibiotic_key = f"AntibioticProfilaction{index}"
    # operation_begin_key = f"OperationBegin{index}"

    if data.get(antibiotic_key):
        # operation_date = common.str_to_date_time(data[operation_begin_key])
        weight = 0.0
        try:
            weight = float(data["Weight"])
        except Exception as e:
            antibiotic_profilaction_time = operation_begin_date - timedelta(minutes=30)
            data[antibiotic_key] = (
                f"{antibiotic_profilaction_time.strftime('%d.%m.%Y %H:%M')} проведена антибиотикопрофилактика"
            )
            return

        antibiotic_profilaction_time = operation_begin_date - timedelta(minutes=30)
        result = f"{antibiotic_profilaction_time.strftime('%d.%m.%Y %H:%M')} проведена антибиотикопрофилактика"
        # if data["Cefrtriakson"] and weight:

        #     ceftriaxon_dose = common.calculate_dosage(weight, (20, 50))
        #     if ceftriaxon_dose > 1.5:
        #         result += f" цефтриаксон 1.5 в\м"
        #     else:
        #         result += f" цефтриаксон {ceftriaxon_dose} в\м"

        data[antibiotic_key] = result
    else:
        data[antibiotic_key] = "не проводилась"


def parse_operation_duration(
    data, index, operation_begin_with_time, operation_end_with_time
):
    if f"OperationDuration{index}" not in data:
        # operation_begin = common.str_to_date_time(data[f"OperationBegin{index}"])
        # operation_end = common.str_to_date_time(data[f"OperationEnd{index}"])
        delta = operation_end_with_time - operation_begin_with_time
        total_minutes = delta.total_seconds() / 60
        hours = int(total_minutes // 60)
        minutes = int(total_minutes % 60)
        data[f"OperationDuration{index}"] = str(hours) + "час(а) " + str(minutes)


def ParseOperationProtocol(data, index):
    if common.is_operation_exist(data, index):
        operation_begin_with_time = common.parse_operation_begin_date(data, index)
        operation_date_without_time = operation_begin_with_time.date()
        operation_end_with_time = common.parse_operation_end_date(data, index)
        # parse_preoperation_epicris_date(data, index)
        common.parse_random_vital(data)
        common.parse_obsledovaniya_do_postupleniya(data)
        # operation_date = common.str_to_date(data[f"OperationBegin{index}"])
        common.parse_all_obsledovaniya_vstacionare_to_current_day(
            data, operation_date_without_time
        )
        parse_antibiotic_profilaction(data, index, operation_begin_with_time)
        parse_operation_duration(
            data, index, operation_begin_with_time, operation_end_with_time
        )
        common.parse_current_naznacheniya(data, operation_begin_with_time)
        return (
            DocxTemplate(Constants.PATH_TO_OPERATION_TEMPLATE)
            if index == ""
            else DocxTemplate(Constants.PATH_TO_OPERATION2_TEMPLATE)
        )
    else:
        return False
