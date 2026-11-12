# Сервер PatientBase на РЕД ОС

Скрипты рассчитаны на РЕД ОС 7.3 и 8 (x86_64/aarch64). Используются штатные
пакеты `dnf`, `systemd`, PostgreSQL из репозитория РЕД ОС. В репозитории, как
правило, доступны PostgreSQL 12 и новее, в том числе версионные пакеты
(`postgresql15-server`, `postgresql16-server` и т.п.).

Общая архитектура, настройка клиентов и бэкапы описаны в
[`../README.md`](../README.md). Здесь — установка и обслуживание сервера
именно на РЕД ОС.

## 1. Файлы

Скопируйте на сервер каталог `server/` целиком (например, `scp -r` или
`git clone`). Для установки нужны:

```
server/schema.sql          общая схема
server/redos/setup_server.sh
server/redos/add_database.sh
server/redos/setup.ini
```

`schema.sql` лежит на уровень выше `redos/` — скрипты сами его найдут.

## 2. Настройка setup.ini

Сгенерируйте длинный пароль:

```bash
openssl rand -base64 24
```

Откройте `server/redos/setup.ini` и заполните:

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
cd server/redos
sudo bash setup_server.sh
```

Скрипт:

1. ставит PostgreSQL (`postgresql-server` или уже установленную версию) и
   `openssl`;
2. включает локаль `en_US.UTF-8` (через `glibc-langpack-en` или `localedef`) —
   без нее не работает поиск `ILIKE` по кириллице;
3. инициализирует кластер (`postgresql-setup initdb` /
   `postgresql-N-setup initdb`) и включает службу
   `postgresql` или `postgresql-N`;
4. создает самоподписанный TLS-сертификат в каталоге данных (в РЕД ОС нет
   Debian-пакета `ssl-cert`);
5. включает SSL и `listen_addresses = '0.0.0.0'`;
6. добавляет в начало `pg_hba.conf` правило
   `hostssl all <DB_USER> 0.0.0.0/0 scram-sha-256` (и для IPv6);
7. возвращает стандартные SELinux-метки каталогу данных;
8. создает пользователя `DB_USER` и базу `DB_NAME` с локалью `en_US.UTF-8`;
9. накатывает `schema.sql`, открывает 5432 в `firewalld`/`iptables` и
   печатает проверки.

Если в системе несколько версий PostgreSQL и нужно выбрать конкретную:

```bash
sudo env PG_VERSION=16 bash setup_server.sh
```

Без `PG_VERSION` скрипт использует уже установленный сервер, а если его нет —
обычный пакет `postgresql-server` из репозитория.

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

Должно вернуться число (0 для пустой базы). Ошибка соединения — это файрвол,
SELinux или сеть, а не программа.

## 5. Несколько баз

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

## Особенности РЕД ОС

### Версии PostgreSQL и пути

В РЕД ОС могут стоять несколько версий сразу. У версионных пакетов свои
службы и каталоги:

| Что | Обычный пакет | Версионный пакет |
|---|---|---|
| Служба | `postgresql` | `postgresql-15` |
| Каталог данных | `/var/lib/pgsql/data` | `/var/lib/pgsql/15/data` |
| Команда initdb | `postgresql-setup` | `/usr/pgsql-15/bin/postgresql-15-setup` |
| Клиент | `/usr/bin/psql` | `/usr/pgsql-15/bin/psql` |

Скрипт определяет версию сам. Для ручных команд смотрите
`systemctl list-units 'postgresql*'` и `psql --version`.

### SELinux

SELinux в РЕД ОС включен в режиме `enforcing`. **Не отключайте его.** Скрипт
возвращает стандартные метки каталогу данных через `restorecon`. Если сервер
не стартует после ручной правки файлов, проверьте:

```bash
getenforce
sudo ausearch -m avc -ts recent
sudo restorecon -Rv /var/lib/pgsql
```

Для стандартного порта 5432 отдельное правило не нужно. Если меняли порт —
добавьте его через `semanage port`.

### Файрвол

В РЕД ОС 8 используется `firewalld`, в 7.3 — `iptables`. Скрипт открывает
5432 в активном файрволе. Вручную:

```bash
# firewalld
sudo firewall-cmd --permanent --add-service=postgresql
sudo firewall-cmd --reload

# iptables (7.3)
sudo iptables -I INPUT -p tcp --dport 5432 -j ACCEPT
sudo service iptables save
```

Проверьте, что порт слушается:

```bash
ss -ltn | grep 5432
```

### Локаль

Если в репозитории нет `glibc-langpack-en`, локаль генерируется через
`localedef`. Проверка:

```bash
locale -a | grep -i en_US
```

## Типовые проблемы

- **`postgresql-setup: command not found`** — не установлен серверный пакет
  (`postgresql-server` или `postgresqlN-server`). Установите его или задайте
  `PG_VERSION`.
- **`Unit postgresql-15.service not found`** — служба называется иначе.
  Посмотрите `systemctl list-unit-files 'postgresql*'` и используйте
  фактическое имя.
- **`could not access private key file "server.key"`** — у ключа не те права.
  Нужно `chown postgres:postgres server.key` и `chmod 600 server.key` в
  каталоге данных.
- **`no pg_hba.conf entry for host ...`** — правило не добавилось или стоит
  ниже перекрывающих строк. Оно должно быть в начале `pg_hba.conf`, затем
  `sudo -u postgres psql -c "select pg_reload_conf()"`.
- **`Peer authentication failed`** — это локальное подключение без пароля.
  Для ручной проверки берите команду с `-h 127.0.0.1` и `PGSSLMODE=require`.
- **Сервер не стартует, в `journalctl -u postgresql` ошибка прав/меток** —
  проверьте `ls -l` в каталоге данных и выполните `restorecon -Rv` по нему.
- **`pg_dump: server version mismatch`** — для версионного PostgreSQL нужен
  `pg_dump` из `/usr/pgsql-N/bin`. `server/backup.sh` подбирает его сам.
- **Клиент не подключается, а с сервера все работает** — проверьте
  `firewall-cmd --list-all` (или `iptables -L -n`) и доступность 5432 с
  клиентской машины.
