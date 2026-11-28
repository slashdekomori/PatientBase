import common
from docxtpl import DocxTemplate
import datetime
from datetime import timedelta
import Constants


def parse_recomendations(data):
    if "Recommendations" not in data:
        result = data.get("RecomendationsText", "")
        recommendations = []
        for variant in [
            "Sutures",
            "Perevyazki",
            "GipsovayaImmobilizaciya",
            "KontrolnayaRgr",
            "UdalenieMetalloconstrukciy",
            "UdalenieSpic",
            "OsvobojdenieOtFisri",
            "DomashniyRejim",
            "PrivivochniyAnamnes",
        ]:
            if data.get(f"{variant}Check"):
                recommendations.append(data.get(f"{variant}Text", ""))
        data["Recommendations"] = result + "\n" + "\n".join(recommendations)


def parse_operations_in_stacionare(data):
    if "OperationsInStacionare" not in data:
        operations = []
        if common.is_operation_exist(data, ""):
            operations.append(
                f"{data['OperationName']}, анестезия: {data['Anestesiya']}, дата операции: {data['OperationBegin_rus']}, послеоперационный период: {data['PostOperationPeriod']}."
            )
        if common.is_operation_exist(data, 2):
            operations.append(
                f"{data['OperationName2']}, анестезия: {data['Anestesiya2']}, дата операции: {data['OperationBegin2_rus']}, послеоперационный период: {data['PostOperationPeriod2']}."
            )
        data["OperationsInStacionare"] = (
            "\n".join(operations) if operations else "не проводились"
        )


def parse_treatment_duration(data):
    if "TreatmentDuration" not in data:
        income_date = common.parse_income_date(data).date()
        outcome_date = common.parse_outcome_date(data).date()
        data["TreatmentDuration"] = (outcome_date - income_date).days


def parse_gender(data):
    if "Gender" not in data:
        patient_name = data.get("PatientFIO", "")
        if patient_name:
            parts = patient_name.split()
            if len(parts) <= 1:
                data["Gender"] = ""
                return
            patronymic = parts[2] if len(parts) > 2 else ""
            if patronymic.endswith("ович") or patronymic.endswith("евич"):
                gender = "мужской"
            elif patronymic.endswith("овна") or patronymic.endswith("евна"):
                gender = "женский"
            elif parts[1].endswith("а") or parts[1].endswith("я"):
                gender = "женский"
            else:
                gender = "мужской"
            data["Gender"] = gender


def parse_all_implants_after_operation(data):
    if "AllImplantsAfterOperations" not in data:
        result = ""
        if common.is_operation_exist(data, "") and (
            data["ImplantsAfterOperation1"] != "нет"
            and data["ImplantsAfterOperation1"] != ""
        ):
            result += (
                data["OperationBegin_rus"]
                + " "
                + data["NumberOfImplants"]
                + " "
                + data["ImplantsAfterOperation1"]
                + "\n"
            )
        if common.is_operation_exist(data, 2) and (
            data["ImplantsAfterOperation2"] != "нет"
            and data["ImplantsAfterOperation2"] != ""
        ):
            result += (
                data["OperationBegin2_rus"]
                + " "
                + data["NumberOfImplants2"]
                + " "
                + data["ImplantsAfterOperation2"]
            )
        if result:
            data["AllImplantsAfterOperations"] = result
        else:
            data["AllImplantsAfterOperations"] = "нет"


def parse_otkaz(data):
    if data["OtkazOtGospitalizacii"]:
        data["Otkaz"] = "отказываются"
    else:
        data["Otkaz"] = "не нуждается"


def ParseVipisnoyEpicris(data):
    outcome_date = common.parse_outcome_date(data).date() + timedelta(days=1)
    parse_gender(data)
    parse_treatment_duration(data)
    common.parse_obsledovaniya_do_postupleniya(data)
    common.parse_all_obsledovaniya_vstacionare_to_current_day(data, outcome_date)
    parse_operations_in_stacionare(data)
    common.parse_all_medical_inveroments_to_current_day(data, outcome_date)
    common.parse_all_consultations_to_current_day(data, outcome_date)
    parse_recomendations(data)
    parse_otkaz(data)
    parse_all_implants_after_operation(data)

    return DocxTemplate(Constants.PATH_TO_VIPISNOY_EPICRIS)
