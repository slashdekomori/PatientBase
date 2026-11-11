#!/usr/bin/env bash
set -euo pipefail

# Добавляет на уже настроенный сервер еще одну базу — по одной на отделение.
# Сервер и учетка общие, у каждого отделения своя база: на клиенте в settings.ini
# достаточно поменять database/name. Клиент переделывать не нужно.
#
#   sudo bash add_database.sh travma                 # пустая база со схемой
#   sudo bash add_database.sh hirurgia --from travma # копия базы (схема + данные)
#   sudo bash add_database.sh --list                 # что уже есть на сервере
#
# Параметры setup.ini (DB_PASS, DB_USER) читаются из файла рядом со скриптом.

usage() {
    sed -n '4,10p' "$0" | sed 's/^# \{0,1\}//'
}

[ "$(id -u)" -eq 0 ] || {
    echo "Запускать через sudo: sudo bash add_database.sh <имя_базы>" >&2
    exit 1
}

DIR="$(dirname "$(readlink -f "$0")")"
INI="$DIR/setup.ini"
DB_PASS=""
DB_USER="pb"
NAME=""
FROM=""
DO_LIST=0

while [ $# -gt 0 ]; do
    case "$1" in
    --from) FROM="${2:-}"; shift 2 ;;
    --from=*) FROM="${1#*=}"; shift ;;
    --ini) INI="${2:-}"; shift 2 ;;
    --ini=*) INI="${1#*=}"; shift ;;
    --list) DO_LIST=1; shift ;;
    -h | --help) usage; exit 0 ;;
    -*) echo "Неизвестный параметр: $1" >&2; usage; exit 1 ;;
    *) NAME="$1"; shift ;;
    esac
done

if [ -f "$INI" ]; then
    while IFS='=' read -r k v; do
        v="${v%$'\r'}"
        case "$k" in
        DB_PASS | DB_USER) printf -v "$k" '%s' "$v" ;;
        esac
    done < <(grep -v '^\s*[#;]' "$INI")
fi

psql_root() { sudo -u postgres psql -tAq "$@"; }

if [ "$DO_LIST" = 1 ]; then
    echo "Базы на сервере:"
    psql_root -c "SELECT datname || '  (' || pg_size_pretty(pg_database_size(datname)) || ')'
                  FROM pg_database
                  WHERE datallowconn AND NOT datistemplate
                  ORDER BY datname"
    exit 0
fi

[ -n "$NAME" ] || { echo "Не указано имя базы. Пример: sudo bash add_database.sh travma" >&2; exit 1; }
# имя базы нельзя передать параметром запроса, поэтому проверяем строго
[[ "$NAME" =~ ^[a-z_][a-z0-9_]{0,62}$ ]] || {
    echo "Имя базы: строчная латиница, цифры и _ (например: travma, hirurgia_2)." >&2
    exit 1
}
case "$NAME" in
postgres | template0 | template1)
    echo "Имя '$NAME' занято системой." >&2
    exit 1
    ;;
esac
if [ -n "$FROM" ] && [ "$FROM" = "$NAME" ]; then
    echo "--from не может совпадать с именем новой базы." >&2
    exit 1
fi

if psql_root -c "SELECT 1 FROM pg_database WHERE datname = '$NAME'" | grep -q 1; then
    echo "База '$NAME' уже существует." >&2
    exit 1
fi

# 1. Создание базы. Локаль как в setup_server.sh — нужен ILIKE по кириллице при поиске.
cd /tmp # чтобы у пользователя postgres не было проблем с текущим каталогом
if [ -n "$FROM" ]; then
    if ! psql_root -c "SELECT 1 FROM pg_database WHERE datname = '$FROM'" | grep -q 1; then
        echo "Базы-образца '$FROM' нет на сервере." >&2
        exit 1
    fi
    echo "Копирую '$FROM' -> '$NAME' (подключенных к '$FROM' клиентов отключу на время копирования)."
    psql_root -c "SELECT pg_terminate_backend(pid) FROM pg_stat_activity
                  WHERE datname = '$FROM' AND pid <> pg_backend_pid()" >/dev/null
    psql_root -c "CREATE DATABASE $NAME OWNER $DB_USER ENCODING 'UTF8' TEMPLATE $FROM"
else
    psql_root -c "CREATE DATABASE $NAME OWNER $DB_USER ENCODING 'UTF8' TEMPLATE template0
                  LC_COLLATE 'en_US.UTF-8' LC_CTYPE 'en_US.UTF-8'"
fi

# 2. Схема (для копии она уже есть, CREATE TABLE IF NOT EXISTS просто ничего не сделает)
if [ -f "$DIR/schema.sql" ] && [ -n "$DB_PASS" ]; then
    PGOPTIONS="-c client_min_messages=warning" PGPASSWORD="$DB_PASS" PGSSLMODE=require \
        psql -q -h 127.0.0.1 -U "$DB_USER" -d "$NAME" -f "$DIR/schema.sql" || {
        echo "База '$NAME' создана, но схема не накатилась (проверьте DB_PASS в $INI). Повторить:" >&2
        echo "  PGPASSWORD='...' PGSSLMODE=require psql -h 127.0.0.1 -U $DB_USER -d $NAME -f $DIR/schema.sql" >&2
        exit 1
    }
else
    echo "Пропускаю накат схемы: нет schema.sql или DB_PASS в $INI" >&2
fi

# 3. Доступ по сети. setup_server.sh пускает пользователя с любого адреса по SSL,
# так что новая база уже доступна; для старых установок добавляем это правило здесь.
PGVER=$(ls /etc/postgresql 2>/dev/null | sort -V | tail -1 || true)
HBA="/etc/postgresql/$PGVER/main/pg_hba.conf"
if [ -n "$PGVER" ] && [ -f "$HBA" ]; then
    for CIDR in 0.0.0.0/0 ::/0; do
        LINE="hostssl  all  $DB_USER  $CIDR  scram-sha-256"
        grep -qxF "$LINE" "$HBA" || echo "$LINE" >>"$HBA"
    done
    if command -v ufw >/dev/null; then ufw allow 5432/tcp || true; fi
    psql_root -c "SELECT pg_reload_conf()" >/dev/null
fi

# 4. Проверка и подсказка для клиента
COLUMNS=$(PGPASSWORD="$DB_PASS" PGSSLMODE=require psql -tAq -h 127.0.0.1 -U "$DB_USER" -d "$NAME" \
    -c "SELECT count(*) FROM information_schema.columns WHERE table_schema = 'public' AND table_name = 'database'" \
    2>/dev/null || true)
echo "проверка: ${COLUMNS:-не удалось подключиться} столбцов в таблице database"
cat <<EOF

База '$NAME' готова. В settings.ini клиента:

  [database]
  host=<IP-сервера>
  port=5432
  name=$NAME
  user=$DB_USER
  password=<пароль из $INI>
EOF
