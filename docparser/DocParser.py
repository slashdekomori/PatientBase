import json
import common
import subprocess
import sys
import os
from pathlib import Path
import locale
from plyer import notification
import self_test


def load_info_about_department(data):
    department_info_path = Path("Sources_files") / "DepartmentInfo.json"
    with open(department_info_path, "r", encoding="utf-8") as file:
        department_info = json.load(file)
        data["DoctorName"] = department_info["лечащий врач"] + " " + data["DoctorName"]
        data["HeadOfDepartment"] = department_info["заведующий отделения"]
        data["DepartmentName"] = department_info["отделение"]


def show_error(error):
    notification.notify(title="Ошибка", message=str(error), app_name="PatientBase")


try:
    if sys.platform == "win32":
        locale.setlocale(locale.LC_TIME, "Russian_Russia.1251")
    else:
        locale.setlocale(locale.LC_TIME, "ru_RU.UTF-8")
except locale.Error:
    print("Не удалось установить локаль. Продолжаем выполнение.")
    locale.setlocale(locale.LC_TIME, "")


def process_json_data(data):
    print("output hystory")
    load_info_about_department(data)
    common.parse_birthday_date(data)
    common.parse_treatment_in_stacionare(data)
    common.parse_all_dates_in_rus(data)
    patientFIO = data.get("PatientFIO", "Пациент")
    income_date = data.get("IncomeDate").strftime("%d.%m.%Y")
    if "ALL_HYSTORY" in data["print_settings"]:
        output_file_path = (
            Path("Output/ALL_HYSTORY") / f"{patientFIO} {income_date}.docx"
        )
        os.makedirs("Output/ALL_HYSTORY", exist_ok=True)
    else:
        output_file_path = Path("Output") / f"{patientFIO} {income_date}.docx"
    common.print_hystory(data, output_file_path)
    print(f"file complite")
    # Открытие файла
    try:
        if sys.platform == "win32":  # Windows
            os.startfile(output_file_path)
        elif sys.platform == "darwin":  # macOS
            subprocess.run(["open", output_file_path])
        else:  # Linux
            subprocess.run(["xdg-open", output_file_path])
    except Exception as e:
        show_error(f"Ошибка при открытии файла: {e}")


def main():
    if len(sys.argv) > 1 and sys.argv[1].lower() == "/test":
        self_test.run()
        return

    print("Ожидание ввода JSON данных (команды: exit, read_json, /test):")
    while True:
        input_data = input().strip()
        command = input_data.lower()
        if command == "exit":
            print("Завершение программы.")
            break
        elif command == "/test":
            self_test.run()
        elif command == "read_json":
            if not Path("patient_data.json").is_file():
                print("Файл 'patient_data.json' не найден.")
                continue
            try:
                with open("patient_data.json", "r", encoding="utf-8") as file:
                    data = json.load(file)
                    process_json_data(data)
            except json.JSONDecodeError:
                show_error("Ошибка при декодировании JSON данных.")
            except Exception as e:
                show_error(f"Ошибка при обработке данных.  {e}")


if __name__ == "__main__":
    main()
