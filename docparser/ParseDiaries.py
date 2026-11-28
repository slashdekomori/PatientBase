import random
import datetime
from datetime import timedelta
import re
from docx import Document
from docxtpl import DocxTemplate
import common
import Constants


NEGATION_BEFORE_KEYWORD = re.compile(r"(?:без|не)(?:\s|-)?$", re.IGNORECASE)


def replace_keywords(text, keywords, replacements):
    """Менять ключевые слова только как отдельные слова и не после 'без'/'не'."""
    for keyword, replacement in zip(keywords, replacements):
        current = text
        pattern = re.compile(
            rf"(?<!\w){re.escape(keyword)}(?!\w)",
            re.IGNORECASE,
        )

        def replace(match):
            prefix = current[: match.start()]
            if NEGATION_BEFORE_KEYWORD.search(prefix):
                return match.group(0)
            if match.group(0)[0].isupper():
                return replacement[:1].upper() + replacement[1:]
            return replacement

        text = pattern.sub(replace, text)
    return text


def is_next_page(data, counter_for_new_page):
    if counter_for_new_page[0] == 0:
        data["NextPage"] = "\f"
        counter_for_new_page[0] = 2
    else:
        data["NextPage"] = ""


def _get_manipulation_dates(data, date_begin, date_end):
    manipulation_dates = set()
    key_pattern = re.compile(r"^ManipulationDate(\d{2}\.\d{2}\.\d{4})$")
    for key in data:
        match = key_pattern.match(key)
        if not match:
            continue
        try:
            manipulation_date = datetime.datetime.strptime(
                match.group(1), "%d.%m.%Y"
            ).date()
        except ValueError:
            continue
        if date_begin <= manipulation_date <= date_end:
            manipulation_dates.add(manipulation_date)
    return manipulation_dates


def _build_diary_dates(data, date_begin, date_end, daily_tail):
    begin = date_begin.date()
    end = date_end.date()
    current = end
    index = (current - begin).days
    diary_dates = set()
    while index >= 0:
        diary_dates.add(current)
        step = 1 if data["Diaries_every_day"] or index <= daily_tail else 2
        current -= timedelta(days=step)
        index -= step
    diary_dates.update(_get_manipulation_dates(data, begin, end))
    return sorted(diary_dates, reverse=True)


def parse_etap_epicrisis(data, current_diary_date):
    etap_epicrisis = []
    current_diary_date_without_time = current_diary_date.date()

    if common.parse_all_obsledovaniya_vstacionare_to_current_day(
        data, current_diary_date_without_time
    ):
        etap_epicrisis.append(
            f"Были проведены следующие обследования:\n{data['AllExaminationsToCurrentDay']}"
        )
    if common.parse_all_manipulations_to_current_date(
        data, current_diary_date_without_time
    ):
        etap_epicrisis.append(
            f"Были выполнены следующие манипуляции:\n{data['AllManipulationsToCurrentDay']}"
        )
    if common.is_operation_exist(data, ""):
        etap_epicrisis.append(
            f"Оперативные вмешательства:\n{data['OperationName']} от {data['OperationBegin_rus']}"
        )
        if common.is_operation_exist(data, 2):
            etap_epicrisis.append(
                f"{data['OperationName2']} от {data['OperationBegin2_rus']}"
            )

    all_consultations_to_current_date = common.parse_all_consultations_to_current_day(
        data, current_diary_date_without_time
    )
    if all_consultations_to_current_date:
        etap_epicrisis.append(
            f"Пациент проконсультирован следующими специалистами:\n{all_consultations_to_current_date}"
        )
    etap_epicrisis.append(
        "За время нахождения пациента в стационаре, наблюдается положительная динамика."
    )

    data["EtapEpicrisis"] = "\nЭтапный эпикриз:\n"
    data["EtapEpicrisisText"] = (
        f"Пациент находится на стационарном лечение в отделении {data['DepartmentName']} с {data['IncomeDate_rus']}.\n"
        + ".\n".join(etap_epicrisis)
    )


def if_round_parse_obhod_for_diary(data, date, current_diary_index):
    current_date = date.date()
    obhod_date = common.parse_date_of_obhod_szaveduyusim_examination(data).date()

    # Лямбда-функция для обновления данных
    update_data = lambda data: (
        data.update(
            {
                "RoundExamination": "совместно с заведующим отделением",
                "Zaveduyushiy": data["HeadOfDepartment"],
                "IsRoundExamination": "лечение согласовано, замечаний нет.",
            }
        )
    )

    index = (current_date - obhod_date).days
    # дневник с обходом каждые 7 или 8 дней в зависимости от ежедневности дневников
    if (
        (index % 7 == 0 or index % 8 == 0)
        and index > 0
        and not data["Diaries_every_day"]
        and current_diary_index > 4
    ):
        update_data(data)
    elif (
        index % 7 == 0
        and index > 0
        and not data["Diaries_every_day"]
        and current_diary_index <= 4
    ):
        update_data(data)
    elif index % 7 == 0 and index > 0 and data["Diaries_every_day"]:
        update_data(data)
    else:
        data["RoundExamination"] = ""
        data["Zaveduyushiy"] = ""
        data["IsRoundExamination"] = "без изменений."

    if (
        ((index + 1) % 10 == 0 or (index + 1) % 9 == 0)
        and index > 0
        and not data["Diaries_every_day"]
    ):
        parse_etap_epicrisis(data, date)
    elif (index + 1) % 10 == 0 and index > 0 and data["Diaries_every_day"]:
        parse_etap_epicrisis(data, date)
    else:
        data["EtapEpicrisis"] = ""
        data["EtapEpicrisisText"] = ""


def parse_jalobi_for_conservative_diaries(data):
    if "JalobiForDiaries" not in data:
        jalobi = str(data.get("Jalobi", "")).strip()
        jalobi = re.sub(r"\s+", " ", jalobi)
        data["JalobiForDiaries"] = jalobi

    return data["JalobiForDiaries"]


def parse_current_jalobi_for_conservative_diary(data, index, max_diary_index):
    jalobi = parse_jalobi_for_conservative_diaries(data)
    normalized = jalobi.strip().lower()
    if not normalized or normalized == "не предъявляет":
        data["CurrentJalobi"] = "не предъявляет"
        return

    stages = [jalobi, "жалобы сохраняются, в меньшей степени", "не предъявляет"]
    if max_diary_index > 0:
        stage = min(
            len(stages) - 1,
            index * len(stages) // (max_diary_index + 1),
        )
    else:
        stage = 0

    data["CurrentJalobi"] = stages[stage]


def parse_current_jalobi_for_postoperative_diaries(data, index, max_diary_index):
    stages = [
        "на умеренную боль в области оперативного вмешательства",
        "на незначительную боль в области оперативного вмешательства",
        "не предъявляет",
    ]
    if max_diary_index > 0:
        stage = min(
            len(stages) - 1,
            index * len(stages) // (max_diary_index + 1),
        )
    else:
        stage = 0

    data["CurrentJalobi"] = stages[stage]


def is_need_to_change_patient_condition(data):
    if "NeedToChangePatientCondition" not in data:
        patient_condition_income = data["PatientCondition"]
        if (
            "тяжелое" in patient_condition_income
            or "средней" in patient_condition_income
        ):
            data["NeedToChangePatientCondition"] = True
        else:
            data["NeedToChangePatientCondition"] = False
    return data["NeedToChangePatientCondition"]


def parse_current_patient_condition(data, index, max_diary_index):
    if is_need_to_change_patient_condition(data):
        stages = Constants.get_condition_degries(data)
        stage = Constants.get_condition_stage(data, index, max_diary_index)
        data["CurrentPatientCondition"] = stages[stage]
    else:
        data["CurrentPatientCondition"] = data["PatientCondition"]


def parse_current_nevrologic_status(data):
    # возможно расширение логики работы для этого здесь эта функция
    nevrologic_status = data["NevrologicStatus"]
    meningial_status = data["MeningialStatus"]

    data["CurrentNevrologicStatus"] = nevrologic_status
    data["CurrentMeningialStatus"] = meningial_status


def parse_current_date_for_conservative_diary(data, date):
    valid_minutes = list(range(15, 46, 5))
    random_minutes = random.choice(valid_minutes)
    operation1_date = common.parse_operation_begin_date(data, "")

    if operation1_date and date.date() == operation1_date.date():
        new_date = operation1_date - timedelta(minutes=30)
        data["IsOperationDate"] = (
            "Пациент взят на оперативное вмешательство, согласия получены"
        )
    else:
        new_date = date.replace(hour=8, minute=random_minutes)
        data["IsOperationDate"] = ""

    data["CurrentDiaryDate_rus"] = new_date.strftime("%d.%m.%Y %H:%M")


def parse_current_date_for_postoperation_diary(data, date, index):
    valid_minutes = list(range(15, 46, 5))
    random_minutes = random.choice(valid_minutes)
    new_date = date.replace(hour=8, minute=random_minutes)
    date_without_time = date.date()
    operation1_end_date = common.parse_operation_end_date(data, "")
    operation2_end_date = common.parse_operation_end_date(data, 2)
    operation1_begin_date = common.parse_operation_begin_date(data, "")
    operation2_begin_date = common.parse_operation_begin_date(data, 2)

    # если есть только первая операция
    if operation1_begin_date and not operation2_begin_date:
        # первая ситуация индекс больше нуля убираем строку isoperationdate
        if index > 0:
            data["IsOperationDate"] = ""
        # вторая  ситуация послеоперационный дневник если индекс = 0 и дата первой операции равна дате дневника то это после операционный дневник
        elif date_without_time == operation1_end_date.date() and index == 0:
            new_date = operation1_end_date + timedelta(hours=2)
    # если есть первая и вторая операции
    elif operation1_end_date and operation2_end_date:
        # первая ситуация когда индекс = 0 и дата равна дате первой операции, приплюсовываем к дате первой операции 2 часа
        if index == 0 and date_without_time == operation1_end_date.date():
            new_date = operation1_end_date + timedelta(hours=2)
        # вторая ситуация когда индекс = 0 и дата равна дате второй операции, приплюсовываем к дате второй операции 2 часа
        elif index == 0 and date_without_time == operation2_end_date.date():
            new_date = operation2_end_date + timedelta(hours=2)
        # третья ситуация когда текущая дата равна дате первой операции и индекс больше нуля, предоперационный дневник первой операции
        elif index > 0 and date_without_time == operation1_begin_date.date():
            new_date = operation1_begin_date - timedelta(minutes=30)
            data["IsOperationDate"] = (
                "Пациент взят на оперативное вмешательство, согласия получены"
            )
        # четвертая ситуация когда текущая дата равна дате второй операции и индекс больше нуля, предоперационный дневник второй операции
        elif index > 0 and date_without_time == operation2_begin_date.date():
            new_date = operation2_begin_date - timedelta(minutes=30)
            data["IsOperationDate"] = (
                "Пациент взят на оперативное вмешательство, согласия получены"
            )
        # пятая ситуация - любой другой дневник
        else:
            data["IsOperationDate"] = ""
    else:
        data["IsOperationDate"] = ""

    data["CurrentDiaryDate_rus"] = new_date.strftime("%d.%m.%Y %H:%M")


def parse_local_status_for_conservative_diaries(data):
    if "LocalStatusForDiaries" not in data:
        # Убираем слова
        local_status_new = data["LocalStatusRound"]
        pattern = (
            r"\w*("
            + "|".join(Constants.WORDS_TO_REMOVE_IN_LOCAL_STATUS_FOR_DIARIES)
            + r")\w*\b"
        )
        # Удаляем однокоренные слова из строки
        cleaned_status = re.sub(pattern, "", local_status_new)

        # Удаляем лишние пробелы
        cleaned_status = re.sub(r"\s+", " ", cleaned_status).strip()
        data["LocalStatusForDiaries"] = cleaned_status
    return data["LocalStatusForDiaries"]


def parse_current_local_status_for_conservative_diary(data, index):
    if data["AsisstanForm"] == "плановая":
        data["CurrentLocalStatus"] = data["LocalStatusRound"]
        return

    max_index = Constants.get_max_index_for_words_to_insert(data)
    local_status = parse_local_status_for_conservative_diaries(data)

    # Определяем слова для подстановки в зависимости от индекса дневник (чем больше индекс тем он ближе к концу)
    if index > max_index and index <= 6:
        word_to_insert_female = "незначительная"
        word_to_insert_midle = "незначительно"
        word_to_insert_male = "незначительный"
    elif index > 6:
        word_to_insert_female = "не определяется"
        word_to_insert_midle = "без"
        word_to_insert_male = "не определяется"
    else:
        word_to_insert_midle = Constants.WORDS_TO_INSERT_IN_LOCAL_SHORT_ADJECTIVE[index]
        word_to_insert_male = Constants.WORDS_TO_INSERT_IN_LOCAL_STATUS_MALE[index]
        word_to_insert_female = Constants.WORDS_TO_INSERT_IN_LOCAL_STATUS_FEMALE[index]

    if index <= 6:
        # добавим немного рандома в дневники.
        word_to_insert_male_options = [
            [
                word_to_insert_male + " " + keyword
                for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_MALE
            ],
            [keyword for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_MALE],
            [
                keyword + " " + word_to_insert_male
                for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_MALE
            ],
        ]
        word_to_insert_male = random.choice(word_to_insert_male_options)

        word_to_insert_female_options = [
            [
                keyword + " " + word_to_insert_female
                for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_FEMALE
            ],
            [
                word_to_insert_female + " " + keyword
                for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_FEMALE
            ],
            [keyword for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_FEMALE],
        ]
        word_to_insert_female = random.choice(word_to_insert_female_options)

        word_to_insert_midle_options = [
            [
                word_to_insert_midle + " " + keyword
                for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_MIDLE
            ],
            [keyword for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_MIDLE],
        ]
        word_to_insert_midle = random.choice(word_to_insert_midle_options)

    else:
        word_to_insert_male = [
            keyword + " " + word_to_insert_male
            for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_MALE
        ]
        word_to_insert_female = [
            keyword + " " + word_to_insert_female
            for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_FEMALE
        ]
        word_to_insert_midle = [
            word_to_insert_midle + keyword
            for keyword in Constants.KEYWORDS_IN_LOCAL_STATUS_MIDLE
        ]

    local_status = replace_keywords(
        local_status,
        Constants.KEYWORDS_IN_LOCAL_STATUS_MALE,
        word_to_insert_male,
    )
    local_status = replace_keywords(
        local_status,
        Constants.KEYWORDS_IN_LOCAL_STATUS_FEMALE,
        word_to_insert_female,
    )
    local_status = replace_keywords(
        local_status,
        Constants.KEYWORDS_IN_LOCAL_STATUS_MIDLE,
        word_to_insert_midle,
    )

    data["CurrentLocalStatus"] = local_status


def parse_wound_type(data, current_diary_date, index):
    # Проверяем дату операции на наличие разрезов кожи
    skin_cut_date = is_cut_in_operation_protocol(data)
    # Проверяем дату имплантации спиц
    wires_date = is_wires_implanted(data)
    wound_type = ""  # Инициализируем переменную для типа раны

    # Если есть разрез кожи или имплантированные спицы и индекс равен 0
    # if (skin_cut_date or wires_date) and index == 0:
    #     wound_type = "послеоперационная повязка незначительно пропитана геморрагическим отделяемым, нейрососудистых нарушений нет." + add_immobilization(data['LocalStatusAtExamination'])

    # Если есть и разрез кожи, и спицы
    if skin_cut_date and wires_date:
        if skin_cut_date == wires_date and skin_cut_date < current_diary_date:
            wound_type = "послеоперационная рана, кожа в местах прохождения чрескостных элементов без признаков воспаления, швы состоятельны,"
        elif skin_cut_date < wires_date and skin_cut_date < current_diary_date:
            wound_type = (
                "послеоперационная рана без признаков воспаления, швы состоятельны,"
            )
        elif (
            skin_cut_date > wires_date
            and wires_date < current_diary_date
            and skin_cut_date > current_diary_date
        ):
            wound_type = "кожа в местах прохождения чрескостных элементов без признаков воспаления,"
        elif skin_cut_date > wires_date and skin_cut_date < current_diary_date:
            wound_type = "послеоперационная рана, кожа в местах прохождения чрескостных элементов без признаков воспаления, швы состоятельны,"

    # Если есть только разрез кожи и он произошел до текущей даты
    elif skin_cut_date and skin_cut_date < current_diary_date:
        wound_type = (
            "послеоперационная рана без признаков воспаления, швы состоятельны,"
        )

    # Если есть только спицы и они были установлены до текущей даты
    elif wires_date and wires_date < current_diary_date:
        wound_type = (
            "кожа в местах выхода чрескостных элементов без признаков воспаления,"
        )

    # Возвращаем тип раны или False, если он не определен
    return wound_type if wound_type else False


def add_immobilization(data):
    words_to_find = [
        "иммобилизация", "гипсовая", "скелетное вытяжение", "Гипсовая", "Иммобилизация", "Скелетное вытяжение"
    ]
    local_status_round = data["LocalStatusRound"]
    for word in words_to_find:
        if word in local_status_round:
            return " Иммобилизация в порядке, восстановлена."
    return ""


def create_status(data, index, wound_type):
    local_status_after_operation = (
        f"Послеоперационная повязка {Constants.WORDS_TO_INSERT_IN_LOCAL_SHORT_ADJECTIVE[index]} "
        f"пропитана {Constants.BLOUD_DEGRIES_MALE[index]} отделяемым, по снятию повязки {wound_type} "
        f"отделяемое из раны {Constants.WORDS_TO_INSERT_IN_LOCAL_STATUS_MIDLE[index]} "
        f"{Constants.BLOUD_DEGRIES_MIDLE[index]}, отек {Constants.WORDS_TO_INSERT_IN_LOCAL_STATUS_MALE[index]} "
        f"уменьшается, перевязка с растворами антисептиков, ас. повязки. "
        "Нейрососудистые нарушения отсутствуют."
    )
    return local_status_after_operation + add_immobilization(data)


def parse_current_local_status_for_postoperation_diary(data, current_diary_date, index):
    max_index = Constants.get_max_index_for_words_to_insert(data)
    # # для хирургов плановых
    # max_index = 1
    wound_type = parse_wound_type(data, current_diary_date, index)

    if index == 0:
        operation1_date = common.parse_operation_begin_date(data, "")
        operation2_date = common.parse_operation_begin_date(data, 2)
        local_status = (
            "послеоперационная повязка сухая, фиксирована, нейрососудистых нарушений нет."
            + add_immobilization(data)
        )
        if operation1_date and (current_diary_date.date() == operation1_date.date()):
            data["CurrentLocalStatus"] = local_status
            return
        elif operation2_date and (current_diary_date.date() == operation2_date.date()):
            data["CurrentLocalStatus"] = local_status
            return
    if not wound_type:
        parse_current_local_status_for_conservative_diary(data, index)
        return

    if index <= max_index:
        data["CurrentLocalStatus"] = create_status(data, index, wound_type)
    else:
        data["CurrentLocalStatus"] = (
            f"Послеоперационная повязка чистая, сухая, по снятию {wound_type} заживает, "
            "перевязка с растворами антисептиков, асептические повязки. "
            "Нейрососудистые нарушения отсутствуют."
        ) + add_immobilization(data)


def is_cut_in_operation_protocol(data):
    if "SkinCutDate" not in data:
        operation_begin1 = common.parse_operation_begin_date(data, "")
        operation_begin2 = common.parse_operation_begin_date(data, 2)

        operation_protocol_keywords = ["разрез", "доступ", "рана", "швы", "ушита"]
        operation_implants_keywords = [
            "пластина", "тэн", "ten", "титановый эластичный стержень", "биос", "стержень"
        ]

        if operation_begin1:
            if any(
                keyword in data["OperationProtocol"].lower()
                for keyword in operation_protocol_keywords
            ) or any(
                keyword in data["ImplantsAfterOperation1"].lower()
                for keyword in operation_implants_keywords
            ):
                data["SkinCutDate"] = operation_begin1
        if operation_begin2:
            if any(
                keyword in data["OperationProtocol2"].lower()
                for keyword in operation_protocol_keywords
            ) or any(
                keyword in data["ImplantsAfterOperation2"].lower()
                for keyword in operation_implants_keywords
            ):
                data["SkinCutDate"] = operation_begin2

        if "SkinCutDate" not in data:
            data["SkinCutDate"] = False
            return False
        else:
            return data["SkinCutDate"]
    else:
        return data["SkinCutDate"]


def is_wires_implanted(data):
    if "WiresDate" not in data:
        operation_begin1 = common.parse_operation_begin_date(data, "")
        operation_begin2 = common.parse_operation_begin_date(data, 2)
        operation_implants_keywords = ["спица", "киршнер"]

        if operation_begin1:
            if any(
                keyword in data["ImplantsAfterOperation1"].lower()
                for keyword in operation_implants_keywords
            ):
                data["WiresDate"] = operation_begin1
        if operation_begin2:
            if any(
                keyword in data["ImplantsAfterOperation2"].lower()
                for keyword in operation_implants_keywords
            ):
                data["WiresDate"] = operation_begin2

        if "WiresDate" not in data:
            data["WiresDate"] = False
            return False
        else:
            return data["WiresDate"]
    else:
        return data["WiresDate"]


def parse_observation_plane(data, date):
    date_without_time = date.date().strftime("%d.%m.%Y")
    if f"ObservationToCurrentDay{date_without_time}" in data:
        data["CurrentObservationPlane"] = data[
            f"ObservationToCurrentDay{date_without_time}"
        ]
    else:
        data["CurrentObservationPlane"] = "без изменений"


def parse_manipulation_to_current_day(data, date, index):
    previous_date_without_time = date.date() - timedelta(days=1)
    result = ""

    if common.is_operation_exist(data, "") and index == 0:
        if date.date() == common.parse_operation_end_date(data, "").date():
            data["CurrentMedicalInveroments"] = "не проводились"
            return
    if common.is_operation_exist(data, 2) and index == 0:
        if date.date() == common.parse_operation_end_date(data, 2).date():
            data["CurrentMedicalInveroments"] = "не проводились"
            return

    current_day = date.date().strftime("%d.%m.%Y")
    manipulation_day = (
        previous_date_without_time.strftime("%d.%m.%Y")
        if index == 999
        else current_day
    )

    if index >= 3 and data["Diaries_every_day"] == False:
        previous_diary_date = (previous_date_without_time - timedelta(days=1)).strftime(
            "%d.%m.%Y"
        )
        if f"Analises{previous_diary_date}" in data:
            result += data[f"AnalisesResult{previous_diary_date}"] + "\n"
        if f"Consultations{previous_diary_date}" in data:
            result += data[f"ConsultationsResult{previous_diary_date}"] + "\n"
        if index == 999:
            previous_diary_date = (
                previous_date_without_time - timedelta(days=2)
            ).strftime("%d.%m.%Y")
            if f"Analises{previous_diary_date}" in data:
                result += data[f"AnalisesResult{previous_diary_date}"] + "\n"
            if f"Consultations{previous_diary_date}" in data:
                result += data[f"ConsultationsResult{previous_diary_date}"] + "\n"
    if index == 999 and data["Diaries_every_day"] == True:
        previous_diary_date = (previous_date_without_time - timedelta(days=1)).strftime(
            "%d.%m.%Y"
        )
        if f"Analises{previous_diary_date}" in data:
            result += data[f"AnalisesResult{previous_diary_date}"] + "\n"
        if f"Consultations{previous_diary_date}" in data:
            result += data[f"ConsultationsResult{previous_diary_date}"] + "\n"

    previous_date_without_time = previous_date_without_time.strftime("%d.%m.%Y")
    if f"Analises{previous_date_without_time}" in data:
        result += data[f"AnalisesResult{previous_date_without_time}"] + "\n"
    if f"Manipulation{manipulation_day}" in data:
        result += data[f"ManipulationText{manipulation_day}"] + "\n"
    if f"Consultations{previous_date_without_time}" in data:
        result += data[f"ConsultationsResult{previous_date_without_time}"] + "\n"

    if result:
        data["CurrentMedicalInveroments"] = result.rstrip()
    else:
        data["CurrentMedicalInveroments"] = "не проводились"


def parse_vipisnoy_text(data, current_diary_date):
    if current_diary_date.date() == common.parse_outcome_date(data).date():
        if data["OtkazOtGospitalizacii"]:
            data["IsLastDay"] = (
                "Пациент выписывается на амбулаторное лечение в поликлинику по м\ж в связи с отказом от дальнейшего стационарного лечения. Рекомендации даны."
            )
        else:
            data["IsLastDay"] = (
                "Учитывая данные осмотра, динамики, клинической картины, анамнеза, инструментальных исследований показаний для дальнейшего стационарного лечения нет. Пациент выписывается на амбулаторное лечение в поликлинику по м\ж. Рекомендации даны."
            )

        list_netrudosposobnosti_text = data["ListNetrudosposobnosti"]

        if (
            "ЛН" in list_netrudosposobnosti_text
            or "№" in list_netrudosposobnosti_text
            or "выдан" in list_netrudosposobnosti_text
        ):
            data["IsLastDay"] += (
                data["IsLastDay"] + "\nЛН: " + list_netrudosposobnosti_text
            )

        data["IsRoundExamination"] = ""
        data["IsOperationDate"] = ""

        data["CurrentDiaryDate_rus"] = (
            common.parse_outcome_date(data) - timedelta(minutes=30)
        ).strftime("%d.%m.%Y %H:%M")
        # т.к. идет парсинг последнего дневника берем большой индекс, например 999, и меняем дату для парсинга обследований, чтобы все подтянулось
        index = 999
        last_diary_day = current_diary_date + timedelta(days=1)

        parse_manipulation_to_current_day(data, last_diary_day, index)
    else:
        data["IsLastDay"] = ""


def is_last_diary(data, is_last, new_diary, previous_diary_path, current_diary_date):
    if is_last[0] == True:
        is_last[0] = False
        parse_vipisnoy_text(data, current_diary_date)
        data["NextPage"] = str("\f{{p NextDiary}}")
        data["NextDiary"] = ""
    else:
        sd = new_diary.new_subdoc(previous_diary_path)
        data["NextDiary"] = sd
        data["IsLastDay"] = ""


def common_functions_for_parsing_diaries(
    data, index, current_diaty_date, max_diary_index
):
    common.parse_random_vital(data)
    parse_current_patient_condition(data, index, max_diary_index)
    parse_current_nevrologic_status(data)
    common.parse_current_naznacheniya(data, current_diaty_date)
    parse_observation_plane(data, current_diaty_date)
    parse_manipulation_to_current_day(data, current_diaty_date, index)
    if_round_parse_obhod_for_diary(data, current_diaty_date, index)


def parse_conservative_diary(data, current_diaty_date, index, max_diary_index):
    common_functions_for_parsing_diaries(
        data, index, current_diaty_date, max_diary_index
    )
    parse_current_jalobi_for_conservative_diary(data, index, max_diary_index)
    parse_current_local_status_for_conservative_diary(data, index)
    parse_current_date_for_conservative_diary(data, current_diaty_date)


def parse_postoperation_diary(data, current_diaty_date, index, max_diary_index):
    common_functions_for_parsing_diaries(
        data, index, current_diaty_date, max_diary_index
    )
    parse_current_jalobi_for_postoperative_diaries(data, index, max_diary_index)
    parse_current_local_status_for_postoperation_diary(data, current_diaty_date, index)
    parse_current_date_for_postoperation_diary(data, current_diaty_date, index)


# не стал объединять функции т.к. немного разная логика работы для консервативных и послеоперативных дневников.
def ParseConservativeDiaries(
    data, date_begin, date_end, doc_to_delete, output_file_path
):
    max_index = (date_end.date() - date_begin.date()).days
    diary_dates = _build_diary_dates(data, date_begin, date_end, daily_tail=2)
    is_last = [True]
    patient_fio = data["PatientFIO"].replace(" ", "")
    current_doc_to_delete = []
    previous_diary_path = ""
    counter_for_new_page = [2]
    for diary_date in diary_dates:
        current_diary_date = date_end.replace(
            year=diary_date.year, month=diary_date.month, day=diary_date.day
        )
        index = (diary_date - date_begin.date()).days
        is_next_page(data, counter_for_new_page)
        new_diary = DocxTemplate(Constants.PATH_TO_CONSERVATIVE_DIARY_TEMPLATE)
        parse_conservative_diary(data, current_diary_date, index, max_index)
        is_last_diary(data, is_last, new_diary, previous_diary_path, current_diary_date)
        new_diary.render(data, autoescape=True)
        previous_diary_path = f"Output/Diary{patient_fio}{index}.docx"
        if index > 0:
            new_diary.save(previous_diary_path)
            current_doc_to_delete.append(previous_diary_path)
        else:
            new_diary.save(output_file_path)
            current_doc_to_delete.append(output_file_path)
        counter_for_new_page[0] -= 1

    if current_doc_to_delete:
        doc_to_delete.extend(current_doc_to_delete)
        return DocxTemplate(output_file_path)
    else:
        return False


def ParsePostOperationDiaries(
    data, date_begin, date_end, doc_to_delete, output_file_path
):
    max_index = (date_end.date() - date_begin.date()).days
    diary_dates = _build_diary_dates(data, date_begin, date_end, daily_tail=4)
    is_last = [True]
    patient_fio = data["PatientFIO"].replace(" ", "")
    current_doc_to_delete = []
    previous_diary_path = ""
    counter_for_new_page = [2]
    for diary_date in diary_dates:
        current_diary_date = date_end.replace(
            year=diary_date.year, month=diary_date.month, day=diary_date.day
        )
        index = (diary_date - date_begin.date()).days
        is_next_page(data, counter_for_new_page)
        new_diary = DocxTemplate(Constants.PATH_TO_POSTOPERATION_DIARY_TEMPLATE)
        parse_postoperation_diary(data, current_diary_date, index, max_index)
        is_last_diary(data, is_last, new_diary, previous_diary_path, current_diary_date)
        new_diary.render(data, autoescape=True)
        previous_diary_path = f"Output/Diary{patient_fio}{index}.docx"
        if index > 0:
            new_diary.save(previous_diary_path)
            current_doc_to_delete.append(previous_diary_path)
        else:
            new_diary.save(output_file_path)
            current_doc_to_delete.append(output_file_path)
        counter_for_new_page[0] -= 1

    if current_doc_to_delete:
        doc_to_delete.extend(current_doc_to_delete)
        return DocxTemplate(output_file_path)
    else:
        return False


def ParseDiaries(data):
    return False
