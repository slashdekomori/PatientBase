#!/usr/bin/env bash
set -euo pipefail

# ---- defaults (overridden by setup.ini next to this script) ----
DB_PASS=""
DB_NAME="patientbase"
DB_USER="pb"
# -----------------------------------------------------------------

DIR="$(dirname "$(readlink -f "$0")")"
INI="$DIR/setup.ini"
if [ -f "$INI" ]; then
    while IFS='=' read -r k v; do
        v="${v%$'\r'}"
        case "$k" in
        DB_PASS | DB_NAME | DB_USER) printf -v "$k" '%s' "$v" ;;
        esac
    done < <(grep -v '^\s*[#;]' "$INI")
fi
[ -n "$DB_PASS" ] || {
    echo "DB_PASS is not set (setup.ini)"
    exit 1
}

# 1. Install + locale (Cyrillic case-insensitive search)
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y postgresql ssl-cert locales
sed -i 's/^# *en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen
locale-gen

PGVER=$(ls /etc/postgresql | sort -V | tail -1)
CONF=/etc/postgresql/$PGVER/main
cd /tmp # so the postgres user doesn't complain about the current dir

# 2. User (create or update password) and database
sudo -u postgres psql -q -v pw="$DB_PASS" -v usr="$DB_USER" <<'SQL'
SELECT CASE WHEN EXISTS (SELECT 1 FROM pg_roles WHERE rolname = :'usr')
  THEN format('ALTER USER %I PASSWORD %L', :'usr', :'pw')
  ELSE format('CREATE USER %I PASSWORD %L', :'usr', :'pw') END
\gexec
SQL
sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='$DB_NAME'" | grep -q 1 ||
    sudo -u postgres psql -c "CREATE DATABASE $DB_NAME OWNER $DB_USER ENCODING 'UTF8' TEMPLATE template0 LC_COLLATE 'en_US.UTF-8' LC_CTYPE 'en_US.UTF-8';"

# 3. Network: слушаем все интерфейсы, доступ только по SSL с любого адреса
cat >"$CONF/conf.d/patientbase.conf" <<EOF
listen_addresses = '0.0.0.0'
ssl = on
ssl_min_protocol_version = 'TLSv1.2'
log_connections = on
log_disconnections = on
EOF

# Без списка сетей: правило на любой адрес, защита — пароль + SSL (scram-sha-256).
# all, а не $DB_NAME: новые базы отделений добавляются без правки pg_hba.conf
for CIDR in 0.0.0.0/0 ::/0; do
    LINE="hostssl  all  $DB_USER  $CIDR  scram-sha-256"
    grep -qxF "$LINE" "$CONF/pg_hba.conf" || echo "$LINE" >>"$CONF/pg_hba.conf"
done
if command -v ufw >/dev/null; then ufw allow 5432/tcp || true; fi
systemctl restart postgresql

# 4. Schema
if [ -f "$DIR/schema.sql" ]; then
    PGPASSWORD="$DB_PASS" PGSSLMODE=require psql -h 127.0.0.1 -U "$DB_USER" -d "$DB_NAME" -f "$DIR/schema.sql"
fi

# Checks
echo -n "ssl: "
sudo -u postgres psql -tAc "SHOW ssl;"
echo -n "cyrillic ILIKE (expect t): "
sudo -u postgres psql -d "$DB_NAME" -tAc "SELECT 'ЖЕНЯ' ILIKE 'женя';"
echo "Done."
