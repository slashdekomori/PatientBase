#!/usr/bin/env bash
set -euo pipefail

# Настройка сервера PostgreSQL для PatientBase на Debian/Ubuntu.
# Запуск: sudo bash setup_server.sh
# Параметры (DB_PASS, DB_NAME, DB_USER) читаются из setup.ini рядом со скриптом.

# ---- defaults (overridden by setup.ini next to this script) ----
DB_PASS=""
DB_NAME="patientbase"
DB_USER="pb"
# -----------------------------------------------------------------

DIR="$(dirname "$(readlink -f "$0")")"
INI="$DIR/setup.ini"
SCHEMA="$DIR/schema.sql"
[ -f "$SCHEMA" ] || SCHEMA="$DIR/../schema.sql"

[ "$(id -u)" -eq 0 ] || {
    echo "Запускать через sudo: sudo bash setup_server.sh" >&2
    exit 1
}

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

# 1. Install + locale (Cyrillic case-insensitive search)
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y postgresql ssl-cert locales
sed -i 's/^# *en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen
locale-gen
locale -a | grep -qi '^en_US\.utf8$' || {
    echo "Не удалось включить локаль en_US.UTF-8 — поиск по кириллице не будет работать." >&2
    exit 1
}

cd /tmp # so the postgres user doesn't complain about the current dir
CONF_FILE=$(sudo -u postgres psql -tAc "SHOW config_file")
HBA=$(sudo -u postgres psql -tAc "SHOW hba_file")
CONF=$(dirname "$CONF_FILE")
[ -d "$CONF/conf.d" ] || {
    echo "Не найдена папка conf.d в $CONF." >&2
    exit 1
}

# 2. User (create or update password) and database
#    SET password_encryption нужен для старых PostgreSQL, где по умолчанию md5.
sudo -u postgres psql -q -v ON_ERROR_STOP=1 -v pw="$DB_PASS" -v usr="$DB_USER" <<'SQL'
SET password_encryption = 'scram-sha-256';
SELECT CASE WHEN EXISTS (SELECT 1 FROM pg_roles WHERE rolname = :'usr')
  THEN format('ALTER USER %I PASSWORD %L', :'usr', :'pw')
  ELSE format('CREATE USER %I PASSWORD %L', :'usr', :'pw') END
\gexec
SQL
if ! sudo -u postgres psql -tAc "SELECT 1 FROM pg_database WHERE datname = '$DB_NAME'" | grep -q 1; then
    sudo -u postgres psql -v ON_ERROR_STOP=1 -c \
        "CREATE DATABASE \"$DB_NAME\" OWNER \"$DB_USER\" ENCODING 'UTF8' TEMPLATE template0 LC_COLLATE 'en_US.UTF-8' LC_CTYPE 'en_US.UTF-8';"
fi
sudo -u postgres psql -q -c "ALTER DATABASE \"$DB_NAME\" OWNER TO \"$DB_USER\""

# 3. Network: слушаем все интерфейсы, доступ только по SSL с любого адреса
PGVERNUM=$(sudo -u postgres psql -tAc "SHOW server_version_num")
if [ "$PGVERNUM" -lt 120000 ]; then
    echo "Нужен PostgreSQL 12 или новее (установлен $PGVERNUM)." >&2
    exit 1
fi
cat >"$CONF/conf.d/patientbase.conf" <<EOF
listen_addresses = '0.0.0.0'
ssl = on
password_encryption = 'scram-sha-256'
log_connections = on
log_disconnections = on
EOF
if [ -f /etc/ssl/certs/ssl-cert-snakeoil.pem ] && [ -f /etc/ssl/private/ssl-cert-snakeoil.key ]; then
    cat >>"$CONF/conf.d/patientbase.conf" <<EOF
ssl_cert_file = '/etc/ssl/certs/ssl-cert-snakeoil.pem'
ssl_key_file = '/etc/ssl/private/ssl-cert-snakeoil.key'
EOF
fi
if [ "$PGVERNUM" -ge 120000 ]; then
    echo "ssl_min_protocol_version = 'TLSv1.2'" >>"$CONF/conf.d/patientbase.conf"
fi

# Без списка сетей: правило на любой адрес, защита — пароль + SSL (scram-sha-256).
# Строки ставим в начало pg_hba.conf: на некоторых сборках ниже есть host-правила
# для 127.0.0.1 с другим методом аутентификации, и они бы перекрыли наши.
# all, а не $DB_NAME: новые базы отделений добавляются без правки pg_hba.conf
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
chown --reference="$HBA" "$TMP" 2>/dev/null || chown postgres:postgres "$TMP"
chmod --reference="$HBA" "$TMP" 2>/dev/null || chmod 600 "$TMP"
mv "$TMP" "$HBA"

if command -v ufw >/dev/null 2>&1; then ufw allow 5432/tcp || true; fi
systemctl restart postgresql

# 4. Schema
PGPASSWORD="$DB_PASS" PGSSLMODE=require psql -q -v ON_ERROR_STOP=1 \
    -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -f "$SCHEMA"

# Checks
echo -n "ssl: "
sudo -u postgres psql -tAc "SHOW ssl;"
echo -n "cyrillic ILIKE (expect t): "
sudo -u postgres psql -d "$DB_NAME" -tAc "SELECT 'ЖЕНЯ' ILIKE 'женя';"
echo -n "TCP+SSL+password (expect 1): "
PGPASSWORD="$DB_PASS" PGSSLMODE=require psql -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -tAc "SELECT 1;"
echo -n "columns in database: "
PGPASSWORD="$DB_PASS" PGSSLMODE=require psql -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -tAc \
    "SELECT count(*) FROM information_schema.columns WHERE table_schema = 'public' AND table_name = 'database';"
echo "Done."
