# Сервер PatientBase на Debian / Ubuntu

Скрипты рассчитаны на Debian 11+ и Ubuntu 22.04+ с штатным PostgreSQL из
репозитория (12 и новее). Нужны `apt`, `systemd` и права `root` через `sudo`.

Общая архитектура, настройка клиентов и бэкапы описаны в
[`../README.md`](../README.md). Здесь — только установка и обслуживание
сервера на Debian/Ubuntu.

## 1. Файлы

Скопируйте на сервер каталог `server/` целиком (например, `scp -r` или
`git clone`). Для установки нужны:

```
server/schema.sql          общая схема
server/debian/setup_server.sh
server/debian/add_database.sh
server/debian/setup.ini
```

`schema.sql` лежит на уровень выше `debian/` — скрипты сами его найдут.

## 2. Настройка setup.ini

Сгенерируйте длинный пароль:

```bash
openssl rand -base64 24
```

Откройте `server/debian/setup.ini` и заполните:

```ini
DB_PASS=<длинный случайный пароль>
DB_NAME=travma
DB_USER=pb
```

- `DB_NAME` — имя базы первого отделения, сразу по названию отделения
  (не `patientbase`);
- `DB_USER` можно оставить `pb`;
- `DB_PASS` — единственная защита сервера, пароль должен быть длинным и
  случайным.

После заполнения `setup.ini` содержит реальный пароль: не коммитьте его в git
и не пересылайте в мессенджерах.

## 3. Установка

```bash
cd server/debian
sudo bash setup_server.sh
```

Скрипт:

1. ставит `postgresql`, `ssl-cert` и `locales`;
2. включает локаль `en_US.UTF-8` — без нее не работает поиск `ILIKE` по
   кириллице;
3. создает пользователя `DB_USER` и базу `DB_NAME` с локалью `en_US.UTF-8`;
4. включает SSL и прослушивание всех интерфейсов
   (`listen_addresses = '0.0.0.0'`);
5. добавляет в начало `pg_hba.conf` правило
   `hostssl all <DB_USER> 0.0.0.0/0 scram-sha-256` (и для IPv6);
6. открывает 5432 в `ufw`, если он установлен;
7. накатывает `schema.sql` и печатает проверки.

## 4. Проверка

В конце установки должно быть:

```
ssl: on
cyrillic ILIKE (expect t): t
TCP+SSL+password (expect 1): 1
columns in database: 346
Done.
```

Если `cyrillic ILIKE` вернул `f` — поиск по русским буквам работать не будет,
дальше идти нельзя (проверьте локаль и пересоздайте базу).

Проверка снаружи, с рабочего места (нужен `psql`):

```bash
PGPASSWORD='<пароль>' PGSSLMODE=require psql -h <IP-сервера> -U pb -d travma \
  -c "select count(*) from database"
```

Должно вернуться число (0 для пустой базы). Ошибка соединения — это файрвол
или сеть, а не программа.

## 5. Несколько баз

Сервер и учетка общие, база у каждого отделения своя:

```bash
sudo bash add_database.sh travma                   # пустая база со схемой
sudo bash add_database.sh hirurgia --from travma   # копия базы (схема и данные)
sudo bash add_database.sh --list                   # что уже есть на сервере
```

Скрипт создает базу с локалью `en_US.UTF-8`, накатывает схему, проверяет
столбцы и печатает блок для `settings.ini` клиента. Правка `pg_hba.conf` и
перезапуск PostgreSQL не нужны.

## 6. Клиентские машины

В папке рядом с `PatientBase.exe`: `PatientBase.exe`, `settings.ini`,
`Sources_files/`, `DocParser.exe`, `Output/`,
`шаблоны_основных_документов/` (подробно — `BUILD_WINDOWS_RU.md`).

```ini
[database]
host=<IP-сервера>
port=5432
name=travma
user=pb
password=<пароль из setup.ini>
```

`name` — база своего отделения (список: `add_database.sh --list`).

## 7. Бэкапы и обновление схемы

- Бэкапы: [`../README.md`](../README.md), раздел «Бэкапы», и
  [`../backup.sh`](../backup.sh).
- Обновление схемы: [`../README.md`](../README.md), раздел «Обновление схемы».

## Типовые проблемы

- **`locale-gen` не включил `en_US.UTF-8`** — проверьте, что в
  `/etc/locale.gen` есть строка `en_US.UTF-8 UTF-8`, и выполните
  `sudo locale-gen`. Затем перезапустите установку.
- **`ssl: off`** — не установился `ssl-cert` или PostgreSQL не увидел
  сертификат. Проверьте `/etc/ssl/certs/ssl-cert-snakeoil.pem` и
  `ssl = on` в `/etc/postgresql/<версия>/main/postgresql.conf`.
- **`no pg_hba.conf entry for host ...`** — правило не добавилось. Проверьте
  `/etc/postgresql/<версия>/main/pg_hba.conf`, строка должна быть в начале
  файла, и выполните `sudo systemctl reload postgresql`.
- **`Peer authentication failed`** — это локальное подключение без пароля.
  Скрипты используют `sudo -u postgres` для локальных команд и TCP+SSL для
  клиентских; для ручной проверки берите команду с `-h 127.0.0.1` и
  `PGSSLMODE=require`.
- **После обновления Debian поставил новую версию PostgreSQL** — старые базы
  остаются в старом кластере. Проверьте `ls /etc/postgresql` и переносите
  базы через `pg_dump`/`pg_restore`, а не через переустановку скриптом.
- **Клиент не подключается, а с сервера все работает** — проверьте `ufw`
  (`sudo ufw status`) и что 5432 доступен с клиентской машины.
