NUMBER_OF_ANALISES = 4
NUMBER_OF_DOP_OBSLEDOVANIYA = 9
NUMBER_OF_CONSULTATIONS_AND_MANIPULATIONS = 5

# ==========================================Шаблоны==================================================================

PATH_TO_CONSERVATIVE_DIARY_TEMPLATE = (
    "шаблоны_основных_документов/шаблоны_дневников/Консервативно.docx"
)
PATH_TO_POSTOPERATION_DIARY_TEMPLATE = (
    "шаблоны_основных_документов/шаблоны_дневников/Послеоперационный.docx"
)
PATH_TO_PERVICHNIY_OSMOTR_TEMPLATE = "шаблоны_основных_документов/ПервичныйОсмотр.docx"
PATH_TO_ROUND_EXAMINATION_TEMPLATE = "шаблоны_основных_документов/ОбходЗаведующего.docx"
PATH_TO_OPERATION_TEMPLATE = "шаблоны_основных_документов/Операция.docx"
PATH_TO_OPERATION2_TEMPLATE = "шаблоны_основных_документов/Операция2.docx"
PATH_TO_VIPISNOY_EPICRIS = "шаблоны_основных_документов/ВыписнойЭпикриз.docx"
PATH_TO_APPOINTMENTS_LIST = "шаблоны_основных_документов/ЛистНазначений.docx"
PATH_TO_FORM045_HORSE = "шаблоны_основных_документов/Форма_045_АИГ_Лошадиный.docx"
PATH_TO_FORM045_HUMAN = "шаблоны_основных_документов/Форма_045_АИГ_Человеческий.docx"

# ==========================================Дневники==================================================================

# WORDS_TO_REMOVE_IN_LOCAL_STATUS_FOR_DIARIES = [
#             "выраженный", "выраженная", "выраженные", "умеренный", "умеренная", "умеренное"
#             "умеренно", "умерено", "деформация", "при поступлении", "небольшой", "незначительная"
#             "незначительный", "незначительное", "незначительно", "нерезко", "резко"
#         ]

WORDS_TO_REMOVE_IN_LOCAL_STATUS_FOR_DIARIES = [
    "выраженн",
    "умерен",
    "деформац",
    "больш",
    "значительн",
    "резко",
]

KEYWORDS_IN_LOCAL_STATUS_MALE = ["отек", "отёк", "болевой синдром"]
KEYWORDS_IN_LOCAL_STATUS_FEMALE = [
    "болезненность",
    "боль",
    "отечность",
    "отёчность",
]
KEYWORDS_IN_LOCAL_STATUS_MIDLE = ["болезненна", "болезненны", "болезнена"]

WORDS_TO_INSERT_IN_LOCAL_STATUS_MALE = ["умеренный", "умеренный", "незначительный"]
WORDS_TO_INSERT_IN_JALOBI_FEMALE = ["умеренную", "умеренную", "незначительную"]
WORDS_TO_INSERT_IN_LOCAL_STATUS_FEMALE = ["умеренная", "умеренная", "незначительная"]
WORDS_TO_INSERT_IN_LOCAL_SHORT_ADJECTIVE = ["умеренно", "умеренно", "незначительно"]
WORDS_TO_INSERT_IN_LOCAL_STATUS_MIDLE = ["умеренное", "умеренное", "незначительное"]

BLOUD_DEGRIES_MIDLE = ["серозно-геморрагическое", "серозно-геморрагическое", "серозное"]
BLOUD_DEGRIES_MALE = ["серозно-геморрагическим", "серозно-геморрагическим", "серозным"]


def get_max_index_for_words_to_insert(data):
    if "MaxIndexForWordsToInsert" not in data:
        data["MaxIndexForWordsToInsert"] = (
            min(
                len(WORDS_TO_INSERT_IN_LOCAL_STATUS_FEMALE),
                len(WORDS_TO_INSERT_IN_LOCAL_SHORT_ADJECTIVE),
                len(WORDS_TO_INSERT_IN_LOCAL_STATUS_MALE),
                len(WORDS_TO_INSERT_IN_LOCAL_STATUS_MIDLE),
                len(BLOUD_DEGRIES_MALE),
                len(BLOUD_DEGRIES_MIDLE),
            )
            - 1
        )
    return data["MaxIndexForWordsToInsert"]


# ==========================================Состояние пациента==========================================================

CONDITION_DEGRIES = ["средней степени тяжести", "удовлетворительное"]


def get_condition_degries(data):
    condition = data["PatientCondition"]
    if "крайне тяжелое" in condition:
        return ["крайне тяжелое", "тяжелое"] + CONDITION_DEGRIES
    if "тяжелое" in condition:
        return [condition] + CONDITION_DEGRIES
    return list(CONDITION_DEGRIES)


def get_condition_stage(data, index, max_diary_index):
    stages = get_condition_degries(data)
    if max_diary_index <= 0:
        return 0
    if stages[0] == "крайне тяжелое" and len(stages) > 3:
        if index == 0:
            return 0
        rest = stages[1:]
        return 1 + min(
            len(rest) - 1,
            (index - 1) * len(rest) // max_diary_index,
        )
    return min(
        len(stages) - 1,
        index * len(stages) // (max_diary_index + 1),
    )
