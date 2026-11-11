# Сервер PatientBase

!!! Скрипты расчитаны на Linux Debian !!!
!!! IP сервера должен быть таким, чтобы все клиенты имели к нему доступ !!!

Один сервер, одна учетка, по одной базе на отделение. Клиент про другие базы не знает:
в его `settings.ini` в `database/name` прописано имя базы своего отделения.

## 1. Установка сервера

1. Скопировать на сервер из этой папки: `setup_server.sh`, `schema.sql`, `setup.ini`.
2. Сгенерировать пароль: `openssl rand -base64 24`.
3. Заполнить `setup.ini` (имя базы первого отделения — сразу по названию отделения):
    ```
    DB_PASS=<длинный случайный пароль>
    DB_NAME=travma
    DB_USER=pb
    ```
4. Запустить: `sudo bash setup_server.sh`

Скрипт ставит PostgreSQL и локаль `en_US.UTF-8` (нужна для поиска `ILIKE` по кириллице),
создает пользователя `DB_USER` и базу `DB_NAME` со схемой, включает SSL и `listen_addresses = '0.0.0.0'`,
добавляет в `pg_hba.conf` правило `hostssl all <DB_USER> 0.0.0.0/0 scram-sha-256`.

Списка сетей нет: пускает с любого адреса, но только по SSL и с паролем — **пароль из
`setup.ini` это единственная защита**. Порт 5432 не выставлять в интернет, только локальная
сеть или VPN. `ufw`, если установлен, скрипт открывает на 5432.

Проверить, что все сошлось (вывод в конце установки):

- `ssl: on`
- `cyrillic ILIKE (expect t): t` — если `f`, поиск по русским буквам не будет работать, дальше не идти
- `Done.`

Проверка снаружи, с обычного рабочего места (нужен `psql`):

```
PGPASSWORD='<пароль>' PGSSLMODE=require psql -h <IP-сервера> -U pb -d travma \
  -c "select count(*) from database"
```

Должно вернуться число (0 для пустой базы). Ошибка соединения — это файрвол/сеть, а не программа.

## 2. Несколько баз (по одной на отделение)

Сервер и учетка общие, база у каждого отделения своя. Добавить базу (на сервере, рядом
с `setup.ini` и `schema.sql`):

```
sudo bash add_database.sh travma                   # пустая база со схемой
sudo bash add_database.sh hirurgia --from travma   # копия базы (схема и данные)
sudo bash add_database.sh --list                   # что уже есть на сервере
```

Скрипт создает базу с нужной локалью, накатывает `schema.sql`, проверяет, что в таблице
`database` все столбцы, и печатает готовый блок для `settings.ini` клиента.
Правка `pg_hba.conf` и перезапуск PostgreSQL не нужны.

На клиенте нового отделения меняется только `database/name` — сам клиент одинаковый.

## 3. Порядок ввода первого отделения

1. Сервер по разделу 1, `DB_NAME` = имя отделения (не `patientbase`).
2. Перенос старых случаев по разделу 4, если они есть.
3. Одна клиентская машина по `BUILD_WINDOWS_RU.md` (или `build_linux.sh`), `settings.ini` — по разделу 5.
4. Смоук-тест на этой машине: открыть старый случай → «Сохранить» → создать новый случай →
   поиск по фамилии → печать документа (документ должен появиться в `Output/`).
5. Только после этого раздать ту же папку остальным машинам отделения.
6. Второе отделение — раздел 2, на его клиентах другой `database/name`.

## 4. Миграция данных из SQLite

1. Извлечь данные (на машине, где есть старый `DataBase.db`):
    `python3 sqlite_to_csv.py путь/к/DataBase.db` → появятся `cases.csv` и `load_cases.sql`
2. Залить в базу отделения. Нужна машина с `psql` и файлами `cases.csv` + `load_cases.sql`
   в текущем каталоге (`\copy` читает файл локально; проще всего делать это на самом сервере):
    ```
    PGSSLMODE=require psql -h <IP-сервера> -U pb -d travma -f load_cases.sql
    ```
3. Сверить `select count(*) from database` с числом случаев в старой базе.

## 5. Клиентские машины

В папке рядом с `PatientBase.exe`: `PatientBase.exe`, `settings.ini`, `Sources_files/`,
`DocParser.exe`, `Output/`, `шаблоны_основных_документов/` (подробно — `BUILD_WINDOWS_RU.md`).
Запускать строго из этой папки: `patient_data.json` и документы пишутся в текущий каталог.

```ini
[database]
host=<IP-сервера>
port=5432
name=travma
user=pb
password=<пароль из setup.ini>
```

`name` — база своего отделения (список: `sudo bash add_database.sh --list` на сервере).
`settings.ini` содержит пароль: не класть в git, не раздавать как «шаблон с паролем».

## 6. Бэкапы (обязательно)

Без бэкапов потеря базы необратима. Логический дамп по всем базам, ежедневно:

```bash
#!/usr/bin/env bash
# /usr/local/bin/pb-backup.sh
set -euo pipefail
out=/var/backups/patientbase
mkdir -p "$out"
for db in $(sudo -u postgres psql -tAc "SELECT datname FROM pg_database
             WHERE datallowconn AND NOT datistemplate AND datname <> 'postgres'"); do
    sudo -u postgres pg_dump -Fc "$db" > "$out/$db-$(date +%F).dump"
done
find "$out" -name '*.dump' -mtime +30 -delete
```

Cron (`/etc/cron.d/patientbase-backup`):

```
30 2 * * * root /usr/local/bin/pb-backup.sh
```

Копию каталога `/var/backups/patientbase` уносить с сервера (другая машина/диск).
Восстановление:

```
sudo -u postgres createdb -O pb travma_restore
sudo -u postgres pg_restore -d travma_restore /var/backups/patientbase/travma-2026-11-11.dump
```

## 7. Обновление схемы

`schema.sql` использует только `CREATE TABLE IF NOT EXISTS`, поэтому повторный запуск
не добавляет новые столбцы в существующую базу. При обновлении программы с новыми полями
их надо добавить руками в каждую базу отделения:

```
sudo -u postgres psql -d travma -c "ALTER TABLE database ADD COLUMN ..."
```

## Типовые проблемы

- **Клиент не соединяется** — проверить `host`/`password` в `settings.ini`, затем доступность
  5432 с клиентской машины, затем `log_connections = on` в логе PostgreSQL на сервере.
- **Все сохранения падают, в логе `column ... does not exist`** — схема базы отстала от
  `table_columns_and_types` в `SQLTable/sqltable.h` (см. раздел 7).
- **Поиск по фамилии не находит случай в другом регистре** — база создана без локали
  `en_US.UTF-8`; создавать базы только через `setup_server.sh` / `add_database.sh`.
