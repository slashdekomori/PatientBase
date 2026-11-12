# Сервер PatientBase

Один сервер PostgreSQL обслуживает несколько отделений. Учетная запись и пароль
общие, база у каждого отделения своя. Клиент про чужие базы не знает: в его
`settings.ini` в `database/name` прописано только имя своей базы.

Схема у всех баз одинаковая — `server/schema.sql` (таблица `database`,
346 столбцов). Она общая для Debian/Ubuntu и РЕД ОС.

## С чего начать

- **Debian / Ubuntu** — [`debian/README.md`](debian/README.md).
- **РЕД ОС 7.3 / 8** — [`redos/README.md`](redos/README.md).
- **Перенос старых случаев из SQLite** — [`migration/README.md`](migration/README.md).
- **Бэкапы** — [`backup.sh`](backup.sh), раздел «Бэкапы» ниже.

Скрипты рассчитаны на Linux-сервер. IP сервера должен быть доступен всем
клиентским машинам отделения (локальная сеть или VPN).

## Что где лежит

```
server/
  README.md              этот файл: общая часть
  schema.sql             схема таблицы database (общая для всех ОС)
  backup.sh              ежедневный бэкап всех баз
  debian/                установка сервера на Debian/Ubuntu
    setup_server.sh
    add_database.sh
    setup.ini
  redos/                 установка сервера на РЕД ОС
    setup_server.sh
    add_database.sh
    setup.ini
  migration/             перенос данных из старого DataBase.db
    README.md
    sqlite_to_csv.py
    load_cases.sql.example
```

Скрипты ищут `schema.sql` рядом с собой или на уровень выше, поэтому на сервер
удобнее копировать каталог `server/` целиком, а не отдельный файл.

## Как устроен доступ

`setup_server.sh`:

- ставит PostgreSQL и локаль `en_US.UTF-8` (нужна для поиска `ILIKE` по кириллице);
- создает пользователя `DB_USER` и базу `DB_NAME` со схемой;
- включает SSL и `listen_addresses = '0.0.0.0'`;
- добавляет в `pg_hba.conf` правило
  `hostssl all <DB_USER> 0.0.0.0/0 scram-sha-256`;
- открывает порт 5432 в файрволе, если он включен.

Списка разрешенных сетей нет: пускает с любого адреса, но только по SSL и с
паролем. **Пароль из `setup.ini` — единственная защита.** Порт 5432 нельзя
выставлять в интернет, только локальная сеть или VPN.

## Порядок ввода первого отделения

1. Подготовить сервер по README своей ОС. `DB_NAME` — сразу имя отделения
   (например, `travma`), а не `patientbase`.
2. Перенести старые случаи по [`migration/README.md`](migration/README.md),
   если они есть.
3. Собрать и настроить одну клиентскую машину
   (`BUILD_WINDOWS_RU.md` или `build_linux.sh`), прописать `settings.ini`.
4. Смоук-тест на этой машине: открыть старый случай → «Сохранить» → создать
   новый случай → поиск по фамилии → печать документа (документ должен
   появиться в `Output/`).
5. Только после этого раздать ту же папку остальным машинам отделения.
6. Второе отделение — добавить базу (раздел ниже), на его клиентах поменять
   только `database/name`.

## Клиентские машины

В папке рядом с `PatientBase.exe` (или `PatientBase` в Linux) должны лежать:

- `PatientBase.exe` / `PatientBase`;
- `settings.ini`;
- `Sources_files/`;
- `DocParser.exe` / `DocParser.bin`;
- `Output/`;
- `шаблоны_основных_документов/`.

Запускать строго из этой папки: `patient_data.json` и документы пишутся в
текущий каталог.

```ini
[database]
host=<IP-сервера>
port=5432
name=travma
user=pb
password=<пароль из setup.ini>
```

`name` — база своего отделения. Список баз на сервере:

```bash
# Debian/Ubuntu
sudo bash server/debian/add_database.sh --list

# РЕД ОС
sudo bash server/redos/add_database.sh --list
```

`settings.ini` содержит пароль: не класть его в git и не раздавать как
«шаблон с паролем».

## Несколько баз (по одной на отделение)

Сервер и учетка общие, база у каждого отделения своя. Добавить базу можно на
уже настроенном сервере:

```bash
# Debian/Ubuntu
sudo bash server/debian/add_database.sh travma
sudo bash server/debian/add_database.sh hirurgia --from travma

# РЕД ОС
sudo bash server/redos/add_database.sh travma
sudo bash server/redos/add_database.sh hirurgia --from travma
```

- без `--from` создается пустая база со схемой;
- с `--from` копируется существующая база вместе с данными;
- `--list` показывает базы и их размер.

Скрипт проверяет число столбцов в таблице `database` и печатает готовый
блок для `settings.ini` клиента. Правка `pg_hba.conf` и перезапуск PostgreSQL
не нужны.

## Бэкапы (обязательно)

Без бэкапов потеря базы необратима. Логический дамп всех баз и ролей:

```bash
sudo bash server/backup.sh
```

Скрипт кладет `pg_dump -Fc` в `/var/backups/patientbase`, хранит 30 дней и
удаляет старое. Каталог и срок можно переопределить:

```bash
PB_BACKUP_DIR=/mnt/backup PB_BACKUP_KEEP_DAYS=14 sudo -E bash server/backup.sh
```

Ежедневный запуск через cron (`/etc/cron.d/patientbase-backup`):

```
30 2 * * * root /usr/local/bin/pb-backup.sh
```

Для этого скопируйте скрипт в `/usr/local/bin/pb-backup.sh` и сделайте его
исполняемым. Копию каталога с дампами нужно уносить с сервера — на другую
машину или диск.

Восстановление базы (дамп читается через stdin, потому что каталог бэкапов
доступен только root):

```bash
sudo -u postgres createdb -O pb travma_restore
sudo -u postgres pg_restore -d travma_restore < /var/backups/patientbase/travma-2026-11-11_023000.dump
```

Для версионного PostgreSQL в РЕД ОС `pg_restore` может лежать в
`/usr/pgsql-<версия>/bin/` — используйте его полный путь.

Перед восстановлением в рабочую базу обязательно сделайте ее бэкап.

## Обновление схемы

`server/schema.sql` использует только `CREATE TABLE IF NOT EXISTS`, поэтому
повторный запуск не добавляет новые столбцы в существующую базу. При
обновлении программы с новыми полями их надо добавить руками в каждую базу
отделения:

```bash
sudo -u postgres psql -d travma -c "ALTER TABLE database ADD COLUMN ..."
```

После добавления проверьте, что число столбцов совпадает со
`SQLTable::table_columns_and_types` в `SQLTable/sqltable.h`:

```bash
sudo -u postgres psql -d travma -tAc \
  "select count(*) from information_schema.columns
   where table_schema='public' and table_name='database'"
```

## Типовые проблемы

- **Клиент не соединяется** — проверить `host`/`password` в `settings.ini`,
  затем доступность 5432 с клиентской машины, затем `log_connections = on` в
  логе PostgreSQL на сервере.
- **Все сохранения падают, в логе `column ... does not exist`** — схема базы
  отстала от `table_columns_and_types` (см. раздел «Обновление схемы»).
- **Поиск по фамилии не находит случай в другом регистре** — база создана без
  локали `en_US.UTF-8`; создавать базы только через `setup_server.sh` /
  `add_database.sh`.
- **Печать не работает** — `DocParser` должен лежать рядом с программой и
  запускаться из рабочей папки; подробнее — в `BUILD_WINDOWS_RU.md`.
