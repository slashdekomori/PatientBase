# DocParser (исходники)

Этот каталог — источник истины для Python-части DocParser. Раньше проект жил вне репозитория (`/home/ilya/DocParser`) и переносился вручную, теперь исходники версионируются вместе с клиентом.

## Что здесь есть

- `.py`-файлы парсера и генерации DOCX;
- `self_test.py` и `test_data.py` — встроенный self-test с **синтетическими, деперсонализированными** данными.

## Что здесь пока НЕТ

`Sources_files/` и `шаблоны_основных_документов/` в этот коммит не включены. Пока используются копии ассетов в `client/` и в dist-папках. Запускать DocParser нужно из каталога, где рядом лежат эти каталоги (например, из текущей рабочей папки клиента или `/home/ilya/DocParser`). Перенос ассетов в репозиторий — отдельный шаг.

Не коммитьте `patient_data.json`, `Output/`, `.venv/`, `__pycache__/` и каталоги сборки — они содержат сгенерированные или персональные данные.

## Окружение

Поддерживается Python 3.11+ (проверено на 3.11.14).

Linux:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

Windows (PowerShell):

```powershell
py -3 -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
```

## Запуск

DocParser общается по stdin/stdout: клиент пишет `read_json\n` для генерации и `exit\n` при закрытии. `patient_data.json` читается и `Output/` пишутся в **текущий рабочий каталог**, поэтому запускать нужно из папки с ассетами.

```bash
python DocParser.py        # обычный режим, ждёт команд на stdin
python DocParser.py /test  # встроенный self-test
```

## Сборка бинарников

Клиент запускает не `.py`, а `DocParser.exe` (Windows) или `DocParser.bin` (Linux) — см. `client/GUI/printwindow.cpp`. Поэтому после изменения исходников бинарники нужно пересобирать из этого каталога и класть рядом с клиентом.

Linux (из этого каталога, `patchelf` должен быть в `PATH`):

```bash
python -m nuitka --onefile --standalone --remove-output DocParser.py
cp DocParser.bin ../client/DocParser.bin
```

Windows (PowerShell, требуется Python 3.12):

```powershell
.\build_windows.ps1
```

Скрипт сам создаст `.venv-win`, поставит зависимости и обновит `client/DocParser.exe`.
