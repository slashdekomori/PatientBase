"""Встроенный self-test DocParser.

Запуск: наберите /test в консоли работающего DocParser или запустите
DocParser.exe /test.

Тесты не пишут DOCX, не открывают документы и не показывают plyer-уведомления.
При падении показывают переменную JSON, функцию, файл и строку, где возникла
ошибка.
"""

import contextlib
import datetime
import json
import traceback
from pathlib import Path

import common
import Constants
import DocParser
import ParseDiaries
import ParsPervichniyOsmotr
import ParsOBhodsZaveduyushim
import ParsOperation
import ParsVipisnoyEpicris
import ParseListNaznacheniy
import ParseVaccinationDocs
import test_data


class TrackedDict(dict):
    """Словарь, который помнит последний прочитанный/записанный ключ JSON."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.last_key = None

    def __getitem__(self, key):
        self.last_key = key
        return super().__getitem__(key)

    def __setitem__(self, key, value):
        self.last_key = key
        super().__setitem__(key, value)

    def get(self, key, default=None):
        self.last_key = key
        return super().get(key, default)

    def setdefault(self, key, default=None):
        self.last_key = key
        return super().setdefault(key, default)

    def __contains__(self, key):
        self.last_key = key
        return super().__contains__(key)


class FakeDocxTemplate:
    """Заглушка вместо docxtpl.DocxTemplate для сухого прогона."""

    def __init__(self, *args, **kwargs):
        pass

    def render(self, *args, **kwargs):
        pass

    def save(self, *args, **kwargs):
        pass

    def new_subdoc(self, *args, **kwargs):
        return FakeDocxTemplate()


@contextlib.contextmanager
def dry_render():
    """Временно подменить рендеринг DOCX заглушкой."""
    modules = [
        common,
        ParseDiaries,
        ParsPervichniyOsmotr,
        ParsOBhodsZaveduyushim,
        ParsOperation,
        ParsVipisnoyEpicris,
        ParseListNaznacheniy,
        ParseVaccinationDocs,
    ]
    originals = {}
    for module in modules:
        if hasattr(module, "DocxTemplate"):
            originals[module] = module.DocxTemplate
            module.DocxTemplate = FakeDocxTemplate

    original_delete_files = common.delete_docx_files
    original_delete_file = common.delete_docx_file
    common.delete_docx_files = lambda files: None
    common.delete_docx_file = lambda file_path: None

    try:
        yield
    finally:
        for module, value in originals.items():
            module.DocxTemplate = value
        common.delete_docx_files = original_delete_files
        common.delete_docx_file = original_delete_file


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


def expect_equal(actual, expected, name):
    if actual != expected:
        raise AssertionError(
            f"{name}: ожидалось {expected!r}, получено {actual!r}"
        )


def run_pipeline(data):
    """Тот же порядок обработки, что и в DocParser.process_json_data()."""
    DocParser.load_info_about_department(data)
    common.parse_birthday_date(data)
    common.parse_treatment_in_stacionare(data)
    common.parse_all_dates_in_rus(data)
    common.print_hystory(data, Path("dummy.docx"))


class Report:
    def __init__(self):
        self.total = 0
        self.passed = 0
        self.failed = 0
        self.skipped = 0

    def ok(self, name, detail=""):
        self.total += 1
        self.passed += 1
        line = f"[ OK ] {name}"
        if detail:
            line += f" — {detail}"
        print(line)

    def skip(self, name, reason):
        self.total += 1
        self.skipped += 1
        print(f"[SKIP] {name} — {reason}")

    def fail(self, name, error, data=None):
        self.total += 1
        self.failed += 1
        print(f"[FAIL] {name}")

        key = getattr(data, "last_key", None)
        print(f"       переменная: {key if key is not None else 'не определена'}")
        print(f"       ошибка: {type(error).__name__}: {error}")

        frames = traceback.extract_tb(error.__traceback__)
        production_frames = [
            frame
            for frame in frames
            if Path(frame.filename).name not in ("self_test.py", "test_data.py")
        ]
        if not production_frames:
            production_frames = frames
        for frame in reversed(production_frames[-2:]):
            print(f"       -> {frame.filename}:{frame.lineno} в {frame.name}")


def run_check(name, function, report, data=None):
    try:
        detail = function() or ""
        report.ok(name, detail)
    except Exception as error:
        report.fail(name, error, data)


def check_constants():
    expect_equal(
        Constants.CONDITION_DEGRIES,
        ["средней степени тяжести", "удовлетворительное"],
        "CONDITION_DEGRIES",
    )

    cases = {
        "тяжелое": ["тяжелое", "средней степени тяжести", "удовлетворительное"],
        "крайне тяжелое": [
            "крайне тяжелое",
            "тяжелое",
            "средней степени тяжести",
            "удовлетворительное",
        ],
        "средней степени тяжести": [
            "средней степени тяжести",
            "удовлетворительное",
        ],
    }
    for condition, expected in cases.items():
        actual = Constants.get_condition_degries(
            {"PatientCondition": condition}
        )
        expect_equal(actual, expected, f"этапы для {condition!r}")
        expect_equal(len(actual), len(set(actual)), f"дубликаты для {condition!r}")
    return "дубликатов нет"


def check_stage_mapping():
    def stages(condition, max_index, count):
        return [
            Constants.get_condition_stage(
                {"PatientCondition": condition}, index, max_index
            )
            for index in range(count)
        ]

    expect_equal(stages("тяжелое", 23, 24), [0] * 8 + [1] * 8 + [2] * 8, "тяжелое")
    expect_equal(
        stages("крайне тяжелое", 23, 24),
        [0] + [1] * 8 + [2] * 8 + [3] * 7,
        "крайне тяжелое",
    )
    expect_equal(
        stages("средней степени тяжести", 23, 24),
        [0] * 12 + [1] * 12,
        "средней степени тяжести",
    )
    expect_equal(
        stages("крайне тяжелое", 1, 2), [0, 1], "крайне тяжелое, срок 1 день"
    )
    expect_equal(
        Constants.get_condition_stage({"PatientCondition": "тяжелое"}, 0, 0),
        0,
        "нулевой срок",
    )
    return "границы 8/8/8, 1+8/8/7, 12/12"


def check_dates():
    data = TrackedDict(test_data.make_case(test_data.CASES[0]))
    expect(isinstance(data["IncomeDate"], str), "IncomeDate должен быть строкой до разбора")
    common.parse_birthday_date(data)
    common.parse_treatment_in_stacionare(data)
    common.parse_all_dates_in_rus(data)

    expect_equal(data["BirthdayDate_rus"], "11.01.2017", "BirthdayDate_rus")
    expect_equal(data["IncomeDate_rus"], "02.11.2026 15:55", "IncomeDate_rus")
    expect_equal(data["OutComeDate_rus"], "27.11.2026 12:00", "OutComeDate_rus")
    expect(
        isinstance(data["IncomeDate"], datetime.datetime),
        "IncomeDate не преобразован в datetime",
    )
    expect(
        isinstance(data["OutComeDate"], datetime.datetime),
        "OutComeDate не преобразован в datetime",
    )
    return "русские даты преобразованы"


def check_round_examination_dates():
    def obhod_date(income, outcome, refusal=True):
        data = TrackedDict(
            {
                "IncomeDate": income,
                "OutComeDate": outcome,
                "OtkazOtGospitalizacii": refusal,
            }
        )
        return common.parse_date_of_obhod_szaveduyusim_examination(data).strftime(
            "%d.%m.%Y %H:%M"
        )

    expect_equal(
        obhod_date("02.11.2026 15:55", "02.11.2026 17:25"),
        "02.11.2026 16:50",
        "отказ в тот же день, середина интервала",
    )
    expect_equal(
        obhod_date("02.11.2026 15:55", "02.11.2026 16:20"),
        "02.11.2026 16:10",
        "отказ в тот же день, короткий интервал",
    )
    expect_equal(
        obhod_date("02.11.2026 15:55", "03.11.2026 12:00"),
        "03.11.2026 08:30",
        "отказ на следующий день",
    )
    expect_equal(
        obhod_date("02.11.2026 15:55", "03.11.2026 08:20"),
        "03.11.2026 08:10",
        "отказ на следующий день, выписка до 08:30",
    )
    expect_equal(
        obhod_date("02.11.2026 15:55", "03.11.2026 12:00", refusal=False),
        "03.11.2026 08:30",
        "обычный обход без отказа",
    )
    return "даты обхода для отказа и обычного режима"


def check_refusal_fields():
    data = TrackedDict(test_data.make_case(test_data.CASES[0]))
    data["OtkazOtGospitalizacii"] = True
    data["IncomeDate"] = "02.11.2026 15:55"
    data["OutComeDate"] = "02.11.2026 17:25"
    common.parse_all_dates_in_rus(data)

    with dry_render():
        ParsOBhodsZaveduyushim.ParseObhodSZaveduyushim(data)

    interventions = data["AllMedicalInveromentsToCurrentDay"]
    expect(
        "Стационарное лечение прекращено" not in interventions,
        "текст отказа попал в выполненные медицинские вмешательства",
    )
    expect(
        data.get("Analises02.11.2026", "") in interventions,
        "выполненные медицинские вмешательства не подгрузились",
    )
    expect(
        "Стационарное лечение прекращено" in data["RecomendationsTreatmentSec"],
        "текст отказа не попал в план лечения",
    )
    expect_equal(
        data["RecomendationsExaminationSec"], "нет", "план обследования при отказе"
    )
    expect_equal(data["CurrentNaznacheniya"], "нет", "назначения при отказе")
    return "вмешательства и план лечения разделены"


def check_condition_dynamics():
    data = TrackedDict(
        {
            "PatientCondition": "тяжелое",
            "NeedToChangePatientCondition": True,
        }
    )
    values = []
    for index in (0, 8, 16, 23):
        ParseDiaries.parse_current_patient_condition(data, index, 23)
        values.append(data["CurrentPatientCondition"])

    expect_equal(
        values,
        ["тяжелое", "средней степени тяжести", "удовлетворительное", "удовлетворительное"],
        "динамика состояния",
    )
    expect_equal(data["PatientCondition"], "тяжелое", "PatientCondition изменён")
    return "PatientCondition не тронут"


def verify_case(data):
    original_condition = data["PatientCondition"]
    with dry_render():
        run_pipeline(data)

    expect_equal(
        data["PatientCondition"],
        original_condition,
        "PatientCondition изменён в результате прогона",
    )
    expect("BirthdayDate_rus" in data, "BirthdayDate_rus не создан")
    expect(
        isinstance(data["IncomeDate"], datetime.datetime),
        "IncomeDate не преобразован в datetime",
    )
    expect(
        isinstance(data["OutComeDate"], datetime.datetime),
        "OutComeDate не преобразован в datetime",
    )

    if "ALL_HYSTORY" in data["print_settings"]:
        expect("CurrentPatientCondition" in data, "CurrentPatientCondition не создан")
        return f"состояние: {data['CurrentPatientCondition']}"
    return "отдельные документы"


def run():
    report = Report()
    print("=== DocParser self-test ===")

    run_check("constants: состояния пациента", check_constants, report)
    run_check("constants: распределение по дням", check_stage_mapping, report)
    run_check("dates: разбор русских дат", check_dates, report)
    run_check("dates: дата обхода при отказе", check_round_examination_dates, report)
    run_check("отказ: поля обхода", check_refusal_fields, report)
    run_check("diaries: динамика состояния", check_condition_dynamics, report)

    for case in test_data.CASES:
        data = TrackedDict(test_data.make_case(case))
        run_check(
            "фикстура: " + case["name"],
            lambda case_data=data: verify_case(case_data),
            report,
            data,
        )

    current_path = Path("patient_data.json")
    if current_path.is_file():
        try:
            current_data = TrackedDict(
                json.loads(current_path.read_text(encoding="utf-8"))
            )
            run_check(
                "текущий patient_data.json",
                lambda case_data=current_data: verify_case(case_data),
                report,
                current_data,
            )
        except Exception as error:
            report.fail("текущий patient_data.json: чтение", error)
    else:
        report.skip("текущий patient_data.json", "файл не найден")

    print()
    if report.failed:
        verdict = f"Result: {report.passed}/{report.total} passed, {report.failed} FAILED"
    else:
        verdict = f"Result: {report.passed}/{report.total} passed — OK"
    if report.skipped:
        verdict += f", {report.skipped} skipped"
    print(verdict)
    return report.failed == 0
