# Сборка под Windows

Проверено с Qt 6.7.3 (MinGW 64-bit), Windows 10/11 64-bit.

## 1. Установить Qt

1. Скачать Qt Online Installer: https://www.qt.io/download-qt-installer
2. При установке выбрать **Qt 6.7.3 → MinGW 64-bit**.
3. Qt Creator устанавливается вместе с Qt.
4. Нужен именно MinGW, не MSVC.

## 2. Собрать программу

1. Открыть файл `PatientBase.pro` в Qt Creator.
2. Слева выбрать набор **Desktop Qt 6.7.3 MinGW 64-bit**.
3. Внизу слева выбрать **Release** (не Debug).
4. Нажать кнопку сборки (молоток).
5. Готовый `PatientBase.exe` появится в папке сборки, внутри `release`.

Путь к папке сборки показан в Qt Creator внизу, на вкладке сборки.

## 3. Подготовить папку для запуска

Создать папку, например `C:\PatientBase`, и положить в неё:

- `PatientBase.exe` — из папки сборки (`release\PatientBase.exe`);
- `DocParser.exe` — из корня проекта;
- `settings.ini`;
- папку `Sources_files`;
- папку `шаблоны_основных_документов`;
- пустую папку `Output`.

Все файлы должны лежать рядом с `PatientBase.exe`: программа ищет
`settings.ini` и `Sources_files` в своей папке.

## 4. Скопировать библиотеки Qt

Без этого программа не запустится на другом компьютере.

1. Пуск → **Qt 6.7.3 (MinGW 64-bit)** — откроется консоль Qt.
2. Выполнить:

   ```bat
   cd C:\PatientBase
   windeployqt --release PatientBase.exe
   ```

Рядом с программой появятся DLL-файлы и папка `sqldrivers`.

## 5. Настроить базу и запустить

1. Открыть `settings.ini` блокнотом и указать `host`, `port`, `name`,
   `user`, `password` сервера PostgreSQL.
   `name` — база именно этого отделения (на одном сервере их несколько,
   список: `sudo bash server/add_database.sh --list` на сервере).
2. Запустить `PatientBase.exe` двойным щелчком.
3. При печати документы сохраняются в папку `Output`.

Сервер PostgreSQL должен быть уже настроен администратором
(см. `server/README.md`).

## Если что-то не работает

- **«Драйвер не загружен» / «driver not loaded»** — не хватает
  `sqldrivers\qsqlpsql.dll` или `libpq.dll`. Проверить, что команда
  `windeployqt` выполнена и папка `sqldrivers` лежит рядом с `.exe`.
- **Не подключается к базе** — проверить `settings.ini` и доступность
  сервера PostgreSQL.
- **Не печатает документы** — проверить, что `DocParser.exe` лежит
  рядом с `PatientBase.exe`.
- **SmartScreen или антивирус ругается на `DocParser.exe`** — разрешить
  запуск файла.
