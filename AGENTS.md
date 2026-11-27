# AGENTS.md

## Build / verification

- There is **no test/lint/CI/typecheck config** in this repo. Verify manually: open an old case → save → create a new case → search by surname → print a document; output must appear under `Output/`. For DOCX storage also: print `Вся история`, choose «Сохранить в базу», reopen the history from the print window, then run «Импорт локальных историй».
- Windows env prep (reuses installed tools; does **not** download by default):
  ```powershell
  powershell -ExecutionPolicy Bypass -File .\setup_windows.ps1
  powershell -ExecutionPolicy Bypass -File .\setup_windows.ps1 -Download
  ```
  It looks for Qt 6.7.3 `win64_llvm_mingw`, its toolchain, and x64 `libpq.dll`, then runs `build_windows.ps1`.
- Direct Windows build needs `qmake`, `make`, `windeployqt` on PATH and a x64 PostgreSQL bin dir:
  ```powershell
  powershell -ExecutionPolicy Bypass -File .\build_windows.ps1 -PgBin "C:\Program Files\PostgreSQL\16\bin"
  ```
  Without `-PgBin`, `libpq.dll` is not bundled and the client fails on machines without PostgreSQL.
- Linux/РЕД ОС entrypoints are `./build_redos7.sh` and `./build_redos8.sh`; add `--install-deps` to install packages first. RED OS 7 allows Qt 5 fallback; RED OS 8 is Qt 6 only. `tools/build_common.sh` is a shared script; do not run it directly.
- Build outputs (`build-win/`, `dist-win/`, `build-redos*/`, `dist-redos*/`, `*.zip`, `*.tar.gz`) are generated/ignored; do not treat them as source.
- Current packaging bug: Windows packaging copies `tools/install.bat`, but `tools/build_common.sh` copies `tools/install.sh`. Only `install.bat` exists, so Linux builds fail/stop there until `install.sh` is added or the copy is removed.

## Architecture facts that are easy to miss

- `client/PatientBase.pro` lists sources only. Runtime assets are copied by build scripts: `DocParser.exe`/`.bin`, `settings.ini`, `Sources_files/`, `шаблоны_основных_документов/`, and `Output/ALL_HYSTORY/`. A new asset is not deployed unless a build script copies it.
- PostgreSQL uses **one flat table** `database` with **347 columns** (the header comment in `server/schema.sql` and several READMEs still say 346 — stale); one row = one case. `id` is the only PK. There are no FKs, audit tables, or secondary indexes on `database`. DOCX files are stored separately (see below).
- Widget `objectName` is the DB column name and JSON key. PostgreSQL returns lowercase names, so `SQLTable::column_names_by_lower_` maps `patientfio → PatientFIO`. Keep `server/schema.sql`, `client/SQLTable/sqltable.h`, `.ui` forms, JSON keys, and DOCX placeholders in sync.
- `schema.sql` uses `CREATE TABLE IF NOT EXISTS`: it does **not** migrate existing databases. Adding a field requires `ALTER TABLE` on every department DB; verify with `columns in database: 347`.
- The client writes all non-`id` columns on INSERT/UPDATE. Missing widget → `NULL`; schema drift silently loses data.
- Search uses `ILIKE` on Cyrillic text; databases must be created with `en_US.UTF-8` via server setup scripts. Manual DB creation with another locale breaks search.
- `SearchWindow::FilterData()` builds the `ILIKE` filter by string interpolation, not `bindValue`; adding fields there should also fix escaping.
- DOCX storage (commit `7a12cc7`): `CaseDocumentRepository` uses a **separate named QSqlDatabase connection** and lazily creates `case_documents` via `EnsureConnection()`/`EnsureTable()`; nothing DOCX-related happens at client startup. One current version per `(case_id, kind)` in `case_documents` (`content bytea`, `sha256`, `updated_at`, PK `(case_id, kind)`, unique index `case_documents_current_uq`), written with `INSERT ... ON CONFLICT DO UPDATE`. No version history; only `ALL_HYSTORY` is stored for now.
- `PrintWindow` binds the chosen file to «Сохранить в базу» and, on close, asks send-to-DB / leave-local / cancel (only if the DOCX actually exists). Client-side locking is only for `Вся история`. The old «Открыть историю из БД» menu action was removed; DB opening is done inside the print window. Successful saves are renamed with ` (загружено в БД dd.MM.yyyy hh-mm)`, rejected copies with ` (not valid ...)`; the ` (из БД)` suffix is stripped first.
- «Импорт локальных историй» scans `Output/ALL_HYSTORY`, matches cases by FIO + `IncomeDate`, compares `sha256`, and imports selected files through `CaseDocumentRepository::SaveFile()`. «Выбрать все» checks only histories not yet in the DB; overwrite rows require manual selection.
- deleting a case checks `case_documents` for `ALL_HYSTORY` first: if a DOCX exists, `SearchWindow` offers «Просмотреть и сохранить» / «Удалить вместе с DOCX» / «Отмена». Export goes to `Output/ALL_HYSTORY/<PatientFIO> <IncomeDate> (из БД).docx`. `SQLTable::DeleteRowById()` removes the case and its `case_documents` rows in one transaction.
- DocParser source is **not** in this repo: it is an external Python project (`DocParser.py`, `common.py`, `Constants.py`, parser modules; `docxtpl` + `plyer`). Only packed binaries `client/DocParser.exe`/`.bin` are committed; `TODO` says to migrate. No `requirements.txt`/build manifest exists in that source tree.
- DocParser is started once at application startup via `QProcess`, not when pressing Print. No CLI args.
- Protocol is exact: client sends `read_json\n` to generate and `exit\n` on close. DocParser prints `output hystory`, then `file complite` (typo intentional); it never emits `Progress: N`. `file complite` is printed before DocParser opens the DOCX; errors only raise a `plyer` notification and never send success, so the client can hang with Print disabled.
- `patient_data.json` is written to the **current working directory**, while `settings.ini` is read from `applicationDirPath()`. Launch from the dist folder; DocParser and templates must sit beside the executable.
- JSON dates are `dd.MM.yyyy` / `dd.MM.yyyy hh:mm`; PostgreSQL uses `yyyy-MM-dd` / `yyyy-MM-dd HH:mm:ss`. DocParser parses the Russian format.
- `JSONPrinter` already contains logic moved out of DocParser (analyses, medicines, consultations, manipulations, diary flags). It always writes `TreatmentInStacionare: true`, which disables DocParser's legacy `common.parse_treatment_in_stacionare()` block; fix derived data in C++, not twice.
- DocParser's `Constants.py` uses exclusive upper bounds (`range(1, N)`) while C++ `NUMBER_OF_*` are inclusive counts; they currently align at 3/8/4 but drift easily.
- `ALL_HYSTORY` is assembled in reverse: `parse_all_history()` builds an internal document list, `print_hystory()` renders each doc to a temp DOCX under `Output/`, nests the previous one via `NextDiary`/`new_subdoc()`, saves final under `Output/ALL_HYSTORY/`, then deletes temps. `print_settings[0]` must be `ALL_HYSTORY` or this branch is skipped.
- Diaries are generated only inside `ALL_HYSTORY`; individual print checkboxes do not. Current DocParser uses only `шаблоны_дневников/Консервативно.docx` and `Послеоперационный.docx`; `ParseDiaries(data)` is a stub returning `False`.
- DocParser still derives render-time values: `*_rus` dates, operation duration, recommendations, gender, and randomized `Current*` vitals (± pulse/AD/breath/temperature). Repeated prints can produce different numbers.
- Live DocParser bug: `common.py` looks up `AnalisesResult{date}`/`ConsultationsResult{date}` with `'% d. % m. % Y'` (spaces), so diary/epicrisis lookups miss JSON keys formatted `dd.MM.yyyy`. Dormant behind `TreatmentInStacionare`: `KalDate1` is parsed as `%Y-%m-%d %H:%M`, and `add_consultation_to_date()` checks singular `Consultation{date}` but writes plural.
- `Sources_files/DepartmentInfo.json` drift: DocParser copy has `лекарственные препараты дополнительно` and `анестезиологи`; client copy has `лекарственные препараты1` and no `анестезиологи`. Fix the client copy if those combo boxes are empty.
- Template copies drift: `ВыписнойЭпикриз.docx`, `Манипуляция.docx`, `Форма_045_АИГ_Лошадиный.docx` differ between DocParser source and `client/`; DocParser source also has `прочее/`, while client has extra unused old `.doc`/`Форма_045.docx` templates.
- One shared `pb` account serves all departments; department is selected by `settings.ini [database] name`. Server allows any IP over SSL+password; never expose port 5432 directly to the internet.

## Server / migration commands

- **Access note (this workstation only):** `ssh patient-database` works here (alias in the local `~/.ssh/config`). It is **not** guaranteed on other devices — do not assume the alias or key exists elsewhere; configure your own access or use the local scripts below. Verified from this machine: RED OS 8.0.3, PostgreSQL 16.15, `ssl = on`, databases `travma` and `neurohir` (plus `postgres`) in `en_US.UTF-8`, `database` table = **347** columns.
  ```bash
  ssh patient-database 'sudo -n -u postgres psql -Atc "select version()"'
  ```
  In Windows PowerShell escape the inner quotes: `ssh patient-database 'sudo -n -u postgres psql -Atc \"select version()\"'`.
- Copy the entire `server/` directory to the host; setup scripts locate `../schema.sql` relative to their own directory.
- Fill `server/debian/setup.ini` or `server/redos/setup.ini`, then:
  ```bash
  cd server/debian && sudo bash setup_server.sh
  cd server/redos && sudo bash setup_server.sh   # optional: sudo env PG_VERSION=16 bash setup_server.sh
  ```
  Expected checks: `ssl: on`, `cyrillic ILIKE: t`, `TCP+SSL+password: 1`, `columns in database: 347`. The setup-script output is authoritative; `server/README.md`, `server/debian/README.md`, `server/redos/README.md` and the `schema.sql` header still say 346.
- Add department DBs from the matching OS directory:
  ```bash
  sudo bash add_database.sh travma
  sudo bash add_database.sh hirurgia --from travma
  sudo bash add_database.sh --list
  ```
- РЕД ОС keeps SELinux enforcing; do not disable it. After manual data-dir edits use `restorecon -Rv /var/lib/pgsql`.
- Migration directory is actually `db-migration/`; `server/README.md` references stale `migration/`. Run from `db-migration/`:
  ```bash
  python3 sqlite_to_csv.py DataBase.db
  PGPASSWORD='<password>' PGSSLMODE=require psql -h <ip> -U pb -d <db> -f load_cases.sql
  ```
  `load_cases.sql.example` referenced in docs is absent. For an existing DB, `db-migration/README.md` also documents `case_documents.sql` to add the DOCX storage table.
- Backups: `sudo bash server/backup.sh`; override with `PB_BACKUP_DIR` and `PB_BACKUP_KEEP_DAYS`. `pg_dump` already includes `case_documents`; no separate DOCX backup step.

## Known doc/build drift

- `server/README.md`, `server/debian/README.md`, and `server/redos/README.md` reference removed files `BUILD_WINDOWS_RU.md` and `build_linux.sh`; trust the scripts in the repo.
- `client/settings.ini` is committed with `password=change-me` and copied into dist packages. Never commit a real deployment password.
- `шаблоны_основных_документов/шаблоны_дневников/послеоперационные_плановые.docx` is 0 bytes in both copies but is not referenced by current DocParser `Constants.py`; do not assume it breaks generation unless a caller starts using it.
