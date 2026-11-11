# PatientBase — agent notes

Qt 6 Widgets/SQL desktop registry for trauma patients. Russian UI, PostgreSQL backend. Generated `.docx` files are produced by a separate prebuilt `DocParser` binary whose source is **not** in this repo.

## Layout / entrypoints

- `main.cpp` → `DataBase` main window (`GUI/database.*`)
- `Common/` — widget auto-collection, MKB dictionaries, doctors/medicines settings, dosage logic
- `SQLTable/` — the only persistence layer (PostgreSQL via `QPSQL`)
- `JSON/` — hand-rolled JSON builder/printer that prepares `patient_data.json` for DocParser
- `server/` — PostgreSQL provisioning and SQLite→PostgreSQL migration (see `server/README.md`)
- `GUI/*.ui` — main form is ~10k lines; edit in Qt Designer, not by hand

## Build and run

- Qt 6 is required. The system `qmake` here is Qt 5.15 and will not compile the code; use `qmake6`.
- Verified build:

  ```bash
  mkdir -p build-linux && cd build-linux
  qmake6 ../PatientBase.pro
  make -j"$(nproc)"
  ```

- The root `Makefile`, `.qmake.stash`, and `build/` are stale generated artifacts from another machine (`/home/ilya/...`, Qt 6.6.3/6.7.3 MinGW). Don't edit or trust them; regenerate.
- No automated tests, lint config, or CI. Verification is compile + manual smoke test (open/save a case, search, print).
- Runtime layout matters: `settings.ini` and `Sources_files/` are read from the executable's directory, but `patient_data.json` and generated documents are written relative to the **current working directory**. Run from a deployment folder containing: `PatientBase`, `settings.ini`, `Sources_files/`, `DocParser.bin` (Windows: `DocParser.exe`), `Output/`, `шаблоны_основных_документов/`. `build/.../release/` is an example of that layout.
- Without `settings.ini` or a reachable PostgreSQL, the app only warns at startup; every save fails.

## Data-model contract (most common source of bugs)

- One PostgreSQL table named `database` (written unquoted as `DataBase` in C++/SQL). The legacy `DataBase.db` and `cases.csv` are only migration inputs; the app never opens SQLite.
- `SQLTable::table_columns_and_types` in `SQLTable/sqltable.h` is the write schema: every column in it is sent on every INSERT/UPDATE (except `id`). It must stay in sync with `server/schema.sql` (currently 346 columns in both). A mismatch makes all saves fail.
- There is no migration framework: `schema.sql` only uses `CREATE TABLE IF NOT EXISTS`, so adding columns to it does not update an existing server — apply `ALTER TABLE database ADD COLUMN ...` manually.
- Widget `objectName` **is** the DB column name. PostgreSQL lowercases identifiers, and `SQLTable` maps them back case-insensitively; `SELECT *` fills widgets by `objectName`.
- Adding a field means changing all of: the `.ui` objectName, `table_columns_and_types`, `server/schema.sql`, and any derived output in `JSONPrinter`.
- Inputs are auto-discovered per tab with `WidgetHandler::CollectWidgets`; widgets whose names match a column are saved automatically. Caveat: collection walks layouts only — a `QGroupBox` nested inside a tab page is not traversed, so its fields are silently ignored. Extra windows must be registered with `AddWidgetsFromWindow` (see `AddAnalisesWindow`).
- Counts in `Common/common.h` (`Constants::NUMBER_OF_*`) must match the `.ui` and the loops in `JSON/JSONPrinter` (9 medicines, 5 fisio procedures, 8 additional observations, etc.). Change `.ui`, constants, schema, and parser together.
- Tab labels and many strings are functional keys, not just display text: e.g. `"Дополнительные анализы"` and exact medicine names in `MedicinesHandler::TryToParseDescription`. Renaming breaks behavior.
- `settings.ini` sits next to the executable (not repo root). The copy in the repo root is a placeholder template; never hardcode credentials.

## Runtime data files

- `Sources_files/DepartmentInfo.json` — source for combo-box choices by Russian key (`доктора`, `лекарственные препараты`, `импланты`, …); keys are sorted before display.
- `Sources_files/DiagnosesMKB.txt` — one `CODE description` per line; drives diagnosis autofill.
- `Sources_files/OperationsMKB.txt` — 4-line records (`code`, separator, operation name, separator); the parser skips lines 2 and 4, so keep that shape.
- `Sources_files/DiagnosesMKB_travma.txt` exists but is currently not loaded anywhere.
- `Common/diariesparser.*` is dormant/legacy; diary generation happens in DocParser via JSON keys such as `Diaries_every_day`.

## Printing / documents

- Print flow: `JSONPrinter` writes `patient_data.json` into the process CWD, then sends `read_json\n` to DocParser over stdin. DocParser writes `.docx` into `Output/` (or `Output/ALL_HYSTORY/`) and prints `file complite` on stdout.
- Do not look for `.docx` generation in C++: template/layout logic lives only in the `DocParser` binary. C++ changes affect the JSON data it consumes.
- Date formats intentionally differ: SQL uses ISO `yyyy-MM-dd HH:mm:ss`; JSON for templates uses `dd.MM.yyyy` / `dd.MM.yyyy hh:mm`.
- DocParser is launched as a child process and inherits the CWD; `patient_data.json` must therefore be findable there.

## Server

- `server/setup_server.sh` is Debian-only and reads `server/setup.ini` (`LAN`, `DB_PASS`, `DB_NAME`, `DB_USER`). It enables SSL-only access per subnet and applies `server/schema.sql`.
- Migration: `python3 server/sqlite_to_csv.py <path>/DataBase.db` (stdlib only) generates `cases.csv` + `load_cases.sql`; run the latter with `psql` on a machine with server access.
- Keep column names lowercase in `schema.sql`; the Qt app adapts case, but SQL comparisons (`ILIKE` search) rely on the DB's `en_US.UTF-8` collation set up by the script.

## Misc

- UI text is hard-coded Russian. Translations are not wired into the build: `PatientBase.pro` has no `TRANSLATIONS`/`RESOURCES`, `PatientBase_ru_RU.ts` is empty, and `qmake_qmake_qm_files.qrc` points to an absolute path from another machine.
- `DataBase.db`, `cases.csv`, and `Output/` contain real patient records. Treat them as sensitive; don't copy them into commits, fixtures, or logs.
