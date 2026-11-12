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
    cat <<'EOF'
Использование:
  sudo bash add_database.sh <имя_базы>                 # пустая база со схемой
  sudo bash add_database.sh <имя_базы> --from <образец> # копия базы (схема + данные)
  sudo bash add_database.sh --list                     # список баз на сервере

Имя базы: строчная латиница, цифры и _ (например: travma, hirurgia_2).
Параметры DB_PASS и DB_USER берутся из setup.ini рядом со скриптом.
EOF
}

[ "$(id -u)" -eq 0 ] || {
    echo "Запускать через sudo: sudo bash add_database.sh <имя_базы>" >&2
    exit 1
}

DIR="$(dirname "$(readlink -f "$0")")"
INI="$DIR/setup.ini"
SCHEMA="$DIR/schema.sql"
[ -f "$SCHEMA" ] || SCHEMA="$DIR/../schema.sql"
cd /tmp # чтобы у пользователя postgres не было проблем с текущим каталогом
DB_PASS=""
DB_USER="pb"
NAME=""
FROM=""
DO_LIST=0

while [ $# -gt 0 ]; do
    case "$1" in
    --from)
        [ $# -ge 2 ] || { echo "После --from нужно имя базы-образца." >&2; exit 1; }
        FROM="$2"
        shift 2
        ;;
    --from=*) FROM="${1#*=}"; shift ;;
    --ini)
        [ $# -ge 2 ] || { echo "После --ini нужен путь к setup.ini." >&2; exit 1; }
        INI="$2"
        shift 2
        ;;
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
[[ "$DB_USER" =~ ^[a-z_][a-z0-9_]{0,62}$ ]] || {
    echo "DB_USER: строчная латиница, цифры и _ (например: pb)." >&2
    exit 1
}
[ "$DB_USER" != "postgres" ] || {
    echo "DB_USER=postgres использовать нельзя: это суперпользователь сервера." >&2
    exit 1
}

psql_root() { sudo -u postgres psql -tAq "$@"; }

if [ "$DO_LIST" = 1 ]; then
    echo "Базы на сервере:"
    psql_root -c "SELECT datname || '  (' || pg_size_pretty(pg_database_size(datname)) || ')'
                  FROM pg_database
                  WHERE datallowconn AND NOT datistemplate
                  ORDER BY datname"
    exit 0
fi

[ -f "$SCHEMA" ] || {
    echo "Не найден schema.sql. Ожидается $DIR/schema.sql или $DIR/../schema.sql" >&2
    exit 1
}
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
if [ -n "$FROM" ] && ! [[ "$FROM" =~ ^[a-z_][a-z0-9_]{0,62}$ ]]; then
    echo "Имя базы-образца: строчная латиница, цифры и _." >&2
    exit 1
fi

if psql_root -c "SELECT 1 FROM pg_database WHERE datname = '$NAME'" | grep -q 1; then
    echo "База '$NAME' уже существует." >&2
    exit 1
fi

# 1. Создание базы. Локаль как в setup_server.sh — нужен ILIKE по кириллице при поиске.
if [ -n "$FROM" ]; then
    if ! psql_root -c "SELECT 1 FROM pg_database WHERE datname = '$FROM'" | grep -q 1; then
        echo "Базы-образца '$FROM' нет на сервере." >&2
        exit 1
    fi
    echo "Копирую '$FROM' -> '$NAME' (подключенных к '$FROM' клиентов отключу на время копирования)."
    psql_root -c "SELECT pg_terminate_backend(pid) FROM pg_stat_activity
                  WHERE datname = '$FROM' AND pid <> pg_backend_pid()" >/dev/null
    sudo -u postgres psql -v ON_ERROR_STOP=1 -q \
        -c "CREATE DATABASE \"$NAME\" OWNER \"$DB_USER\" ENCODING 'UTF8' TEMPLATE \"$FROM\""
else
    sudo -u postgres psql -v ON_ERROR_STOP=1 -q -c \
        "CREATE DATABASE \"$NAME\" OWNER \"$DB_USER\" ENCODING 'UTF8' TEMPLATE template0 LC_COLLATE 'en_US.UTF-8' LC_CTYPE 'en_US.UTF-8'"
fi
sudo -u postgres psql -q -c "ALTER DATABASE \"$NAME\" OWNER TO \"$DB_USER\""

# 2. Доступ по сети. setup_server.sh пускает пользователя с любого адреса по SSL,
# так что новая база уже доступна; для старых установок добавляем это правило здесь.
HBA="$(psql_root -c "SHOW hba_file" | head -n1)"
if [ -n "$HBA" ] && [ -f "$HBA" ]; then
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
    psql_root -c "SELECT pg_reload_conf()" >/dev/null
fi

# 3. Схема (для копии она уже есть, CREATE TABLE IF NOT EXISTS просто ничего не сделает)
PGOPTIONS="-c client_min_messages=warning" PGPASSWORD="$DB_PASS" PGSSLMODE=require \
    psql -q -v ON_ERROR_STOP=1 -h 127.0.0.1 -U "$DB_USER" -d "$NAME" -f "$SCHEMA" || {
    echo "База '$NAME' создана, но схема не накатилась (проверьте DB_PASS в $INI). Повторить:" >&2
    echo "  PGPASSWORD='...' PGSSLMODE=require psql -h 127.0.0.1 -U $DB_USER -d $NAME -f $SCHEMA" >&2
    exit 1
}

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
