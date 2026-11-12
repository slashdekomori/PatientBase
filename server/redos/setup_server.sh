#!/usr/bin/env bash
set -euo pipefail

# Настройка сервера PostgreSQL для PatientBase на РЕД ОС (7.3 / 8).
# Запуск: sudo bash setup_server.sh
# Параметры (DB_PASS, DB_NAME, DB_USER) читаются из setup.ini рядом со скриптом.
# Версию PostgreSQL можно выбрать явно:
#   sudo env PG_VERSION=16 bash setup_server.sh

# ---- defaults (overridden by setup.ini next to this script) ----
DB_PASS=""
DB_NAME="patientbase"
DB_USER="pb"
PG_VERSION="${PG_VERSION:-}"
# -----------------------------------------------------------------

DIR="$(dirname "$(readlink -f "$0")")"
INI="$DIR/setup.ini"
SCHEMA="$DIR/schema.sql"
[ -f "$SCHEMA" ] || SCHEMA="$DIR/../schema.sql"

[ "$(id -u)" -eq 0 ] || {
    echo "Запускать через sudo: sudo bash setup_server.sh" >&2
    exit 1
}
cd /tmp # чтобы у пользователя postgres не было проблем с текущим каталогом

if [ -f "$INI" ]; then
    while IFS='=' read -r k v; do
        v="${v%$'\r'}"
        case "$k" in
        DB_PASS | DB_NAME | DB_USER) printf -v "$k" '%s' "$v" ;;
        esac
    done < <(grep -v '^\s*[#;]' "$INI")
fi
[ -n "$DB_PASS" ] || {
    echo "DB_PASS не задан в $INI" >&2
    exit 1
}
case "$DB_PASS" in
ПОМЕНЯТЬ* | change-me)
    echo "DB_PASS в $INI все еще содержит заглушку. Сгенерируйте пароль: openssl rand -base64 24" >&2
    exit 1
    ;;
esac
[[ "$DB_NAME" =~ ^[a-z_][a-z0-9_]{0,62}$ ]] || {
    echo "DB_NAME: строчная латиница, цифры и _ (например: travma, hirurgia_2)." >&2
    exit 1
}
case "$DB_NAME" in
postgres | template0 | template1)
    echo "Имя '$DB_NAME' занято системой." >&2
    exit 1
    ;;
esac
[[ "$DB_USER" =~ ^[a-z_][a-z0-9_]{0,62}$ ]] || {
    echo "DB_USER: строчная латиница, цифры и _ (например: pb)." >&2
    exit 1
}
[ "$DB_USER" != "postgres" ] || {
    echo "DB_USER=postgres использовать нельзя: это суперпользователь сервера." >&2
    exit 1
}
[ -f "$SCHEMA" ] || {
    echo "Не найден schema.sql. Ожидается $DIR/schema.sql или $DIR/../schema.sql" >&2
    exit 1
}

# 1. Пакеты. В РЕД ОС PostgreSQL бывает версионным (postgresql15-server и т.п.),
# поэтому сначала берем уже установленный сервер, а если его нет — обычный пакет.
if [ -n "$PG_VERSION" ]; then
    dnf install -y "postgresql${PG_VERSION}-server" openssl
elif ! command -v postgresql-setup >/dev/null 2>&1 &&
    ! compgen -G "/usr/pgsql-*/bin/postgresql-*-setup" >/dev/null 2>&1; then
    dnf install -y postgresql-server openssl
fi

# Локаль нужна для ILIKE по кириллице. Пакет локали есть не во всех версиях,
# поэтому если его нет — генерируем локаль через localedef.
if ! locale -a | grep -qi '^en_US\.utf8$'; then
    dnf install -y glibc-langpack-en >/dev/null 2>&1 || true
fi
if ! locale -a | grep -qi '^en_US\.utf8$'; then
    localedef -i en_US -f UTF-8 en_US.UTF-8 >/dev/null 2>&1 || true
fi
locale -a | grep -qi '^en_US\.utf8$' || {
    echo "Не удалось включить локаль en_US.UTF-8 — поиск по кириллице не будет работать." >&2
    exit 1
}

# 2. Находим setup-скрипт, службу и каталог данных PostgreSQL.
if [ -n "$PG_VERSION" ]; then
    SETUP="$(command -v "postgresql-$PG_VERSION-setup" || true)"
    [ -n "$SETUP" ] || SETUP="/usr/pgsql-$PG_VERSION/bin/postgresql-$PG_VERSION-setup"
    UNIT="postgresql-$PG_VERSION"
    PGDATA="/var/lib/pgsql/$PG_VERSION/data"
elif command -v postgresql-setup >/dev/null 2>&1; then
    SETUP="$(command -v postgresql-setup)"
    UNIT="postgresql"
    PGDATA="/var/lib/pgsql/data"
else
    SETUP="$(ls -1 /usr/pgsql-*/bin/postgresql-*-setup 2>/dev/null | sort -V | tail -1 || true)"
    [ -n "$SETUP" ] || {
        echo "Не найден postgresql-setup. Установите postgresql-server или укажите PG_VERSION." >&2
        exit 1
    }
    PGVER="$(basename "$SETUP" | sed -E 's/^postgresql-([0-9]+)-setup$/\1/')"
    UNIT="postgresql-$PGVER"
    PGDATA="/var/lib/pgsql/$PGVER/data"
fi
[ -x "$SETUP" ] || {
    echo "Не найден $SETUP. Проверьте установку PostgreSQL." >&2
    exit 1
}

# В версионных пакетах РЕД ОС psql может лежать только в /usr/pgsql-N/bin,
# поэтому не полагаемся на PATH.
if [ -n "$PG_VERSION" ] && [ -x "/usr/pgsql-$PG_VERSION/bin/psql" ]; then
    PSQL="/usr/pgsql-$PG_VERSION/bin/psql"
else
    PG_BIN="$(dirname "$SETUP")"
    PSQL="$PG_BIN/psql"
    [ -x "$PSQL" ] || PSQL="$(command -v psql || true)"
fi
[ -n "$PSQL" ] || {
    echo "Не найден psql. Установите клиент PostgreSQL." >&2
    exit 1
}

if [ ! -s "$PGDATA/PG_VERSION" ]; then
    echo "Инициализирую кластер PostgreSQL ($UNIT)..."
    case "$(basename "$SETUP")" in
    postgresql-setup)
        LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 "$SETUP" --initdb --unit "$UNIT" ||
            LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 "$SETUP" initdb
        ;;
    *)
        LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 "$SETUP" initdb
        ;;
    esac
fi

systemctl enable --now "$UNIT"
for _ in $(seq 1 30); do
    if sudo -u postgres "$PSQL" -tAc "SELECT 1" >/dev/null 2>&1; then break; fi
    sleep 1
done
sudo -u postgres "$PSQL" -tAc "SELECT 1" >/dev/null

CONF="$(sudo -u postgres "$PSQL" -tAc "SHOW config_file")"
HBA="$(sudo -u postgres "$PSQL" -tAc "SHOW hba_file")"
PGDATA="$(sudo -u postgres "$PSQL" -tAc "SHOW data_directory")"
PGVERNUM=$(sudo -u postgres "$PSQL" -tAc "SHOW server_version_num")
if [ "$PGVERNUM" -lt 120000 ]; then
    echo "Нужен PostgreSQL 12 или новее (установлен $PGVERNUM)." >&2
    exit 1
fi

# 3. TLS-сертификат. В отличие от Debian, в РЕД ОС нет пакета ssl-cert,
# поэтому делаем самоподписанный сертификат в каталоге данных.
CRT="$PGDATA/server.crt"
KEY="$PGDATA/server.key"
if [ ! -s "$CRT" ] || [ ! -s "$KEY" ]; then
    openssl req -new -x509 -days 3650 -nodes -out "$CRT" -keyout "$KEY" \
        -subj "/CN=patientbase" >/dev/null 2>&1
fi
chown postgres:postgres "$CRT" "$KEY"
chmod 644 "$CRT"
chmod 600 "$KEY"

# 4. Сеть: слушаем все интерфейсы, доступ только по SSL с любого адреса.
# Настройки кладем отдельным файлом и подключаем его из postgresql.conf.
CONF_PB="$PGDATA/patientbase.conf"
cat >"$CONF_PB" <<EOF
# PatientBase: сеть, TLS и пароли
listen_addresses = '0.0.0.0'
ssl = on
ssl_cert_file = 'server.crt'
ssl_key_file = 'server.key'
password_encryption = 'scram-sha-256'
log_connections = on
log_disconnections = on
EOF
if [ "$PGVERNUM" -ge 120000 ]; then
    echo "ssl_min_protocol_version = 'TLSv1.2'" >>"$CONF_PB"
fi
chown postgres:postgres "$CONF_PB"
chmod 600 "$CONF_PB"
if ! grep -qF "include = 'patientbase.conf'" "$CONF"; then
    printf '\n# PatientBase\ninclude = %s\n' "'patientbase.conf'" >>"$CONF"
fi

# Без списка сетей: правило на любой адрес, защита — пароль + SSL (scram-sha-256).
# Строки ставим в начало pg_hba.conf: в РЕД ОС ниже есть host-правила для 127.0.0.1
# с ident, и они бы перекрыли наши. all, а не $DB_NAME: новые базы отделений
# добавляются без правки pg_hba.conf.
L1="hostssl  all  $DB_USER  0.0.0.0/0  scram-sha-256"
L2="hostssl  all  $DB_USER  ::/0  scram-sha-256"
MARK="# PatientBase: SSL + scram-sha-256 for $DB_USER from any address"
TMP="$(mktemp "$HBA.pb.XXXXXX")"
{
    echo "$MARK"
    echo "$L1"
    echo "$L2"
    grep -vxF -e "$MARK" -e "$L1" -e "$L2" "$HBA" || true
} >"$TMP"
chown postgres:postgres "$TMP"
chmod 600 "$TMP"
mv "$TMP" "$HBA"

# 5. SELinux: возвращаем стандартные метки файлам в каталоге данных.
if command -v restorecon >/dev/null 2>&1; then
    restorecon -R "$PGDATA" >/dev/null 2>&1 || true
fi

systemctl restart "$UNIT"
for _ in $(seq 1 30); do
    if sudo -u postgres "$PSQL" -tAc "SELECT 1" >/dev/null 2>&1; then break; fi
    sleep 1
done

# 6. Пользователь и база.
sudo -u postgres "$PSQL" -q -v ON_ERROR_STOP=1 -v pw="$DB_PASS" -v usr="$DB_USER" <<'SQL'
SET password_encryption = 'scram-sha-256';
SELECT CASE WHEN EXISTS (SELECT 1 FROM pg_roles WHERE rolname = :'usr')
  THEN format('ALTER USER %I PASSWORD %L', :'usr', :'pw')
  ELSE format('CREATE USER %I PASSWORD %L', :'usr', :'pw') END
\gexec
SQL
if ! sudo -u postgres "$PSQL" -tAc "SELECT 1 FROM pg_database WHERE datname = '$DB_NAME'" | grep -q 1; then
    sudo -u postgres "$PSQL" -v ON_ERROR_STOP=1 -q -c \
        "CREATE DATABASE \"$DB_NAME\" OWNER \"$DB_USER\" ENCODING 'UTF8' TEMPLATE template0 LC_COLLATE 'en_US.UTF-8' LC_CTYPE 'en_US.UTF-8'"
fi
sudo -u postgres "$PSQL" -q -c "ALTER DATABASE \"$DB_NAME\" OWNER TO \"$DB_USER\""
DB_LOCALE=$(sudo -u postgres "$PSQL" -tAc "SELECT datcollate FROM pg_database WHERE datname = '$DB_NAME'")
if [ -n "$DB_LOCALE" ] && [ "$DB_LOCALE" != "en_US.UTF-8" ]; then
    echo "ВНИМАНИЕ: база '$DB_NAME' создана с локалью '$DB_LOCALE', а не en_US.UTF-8." >&2
    echo "Поиск по кириллице без учета регистра может не работать." >&2
fi

# 7. Схема.
PGPASSWORD="$DB_PASS" PGSSLMODE=require "$PSQL" -q -v ON_ERROR_STOP=1 \
    -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -f "$SCHEMA"

# 8. Файрвол: firewalld в РЕД ОС 8, iptables в 7.3.
if systemctl is-active --quiet firewalld; then
    firewall-cmd --permanent --add-service=postgresql >/dev/null 2>&1 ||
        firewall-cmd --permanent --add-port=5432/tcp >/dev/null
    firewall-cmd --reload >/dev/null
elif systemctl is-active --quiet iptables; then
    iptables -C INPUT -p tcp --dport 5432 -j ACCEPT 2>/dev/null ||
        iptables -I INPUT -p tcp --dport 5432 -j ACCEPT
    if command -v service >/dev/null 2>&1; then
        service iptables save >/dev/null 2>&1 || true
    fi
fi

# Checks
echo -n "ssl: "
sudo -u postgres "$PSQL" -tAc "SHOW ssl;"
echo -n "cyrillic ILIKE (expect t): "
sudo -u postgres "$PSQL" -d "$DB_NAME" -tAc "SELECT 'ЖЕНЯ' ILIKE 'женя';"
echo -n "TCP+SSL+password (expect 1): "
PGPASSWORD="$DB_PASS" PGSSLMODE=require "$PSQL" -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -tAc "SELECT 1;"
echo -n "columns in database: "
PGPASSWORD="$DB_PASS" PGSSLMODE=require "$PSQL" -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -tAc \
    "SELECT count(*) FROM information_schema.columns WHERE table_schema = 'public' AND table_name = 'database';"
echo "Done."
