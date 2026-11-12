#!/usr/bin/env bash
set -euo pipefail

# Ежедневный бэкап всех баз PatientBase (Debian/Ubuntu и РЕД ОС).
#
#   sudo bash backup.sh
#
# Каталог и срок хранения можно переопределить:
#   PB_BACKUP_DIR=/mnt/backup PB_BACKUP_KEEP_DAYS=14 sudo -E bash backup.sh
#
# Скрипт делает логические дампы pg_dump -Fc (восстанавливаются через pg_restore)
# и отдельный дамп ролей. Копию каталога с дампами нужно уносить с сервера.

OUT="${PB_BACKUP_DIR:-/var/backups/patientbase}"
KEEP_DAYS="${PB_BACKUP_KEEP_DAYS:-30}"

[ "$(id -u)" -eq 0 ] || {
    echo "Запускать через sudo: sudo bash backup.sh" >&2
    exit 1
}

umask 077
mkdir -p "$OUT"
chmod 700 "$OUT"
cd /tmp # чтобы у пользователя postgres не было проблем с текущим каталогом

# psql может быть версионным (/usr/pgsql-16/bin/psql), поэтому ищем его явно.
PSQL="$(command -v psql || true)"
if [ -z "$PSQL" ]; then
    PSQL="$(ls -1 /usr/pgsql-*/bin/psql 2>/dev/null | sort -V | tail -1 || true)"
fi
[ -n "$PSQL" ] || {
    echo "Не найден psql." >&2
    exit 1
}

SERVER_NUM="$(sudo -u postgres "$PSQL" -tAc "SHOW server_version_num" | tr -d '[:space:]')"
MAJOR=$((SERVER_NUM / 10000))

# Для pg_dump важна та же мажорная версия, что у сервера: старый клиент
# не умеет дампить новый сервер. Версионные пакеты РЕД ОС кладут его в /usr/pgsql-N/bin.
PG_DUMP="/usr/pgsql-$MAJOR/bin/pg_dump"
[ -x "$PG_DUMP" ] || PG_DUMP="$(command -v pg_dump || true)"
[ -n "$PG_DUMP" ] && [ -x "$PG_DUMP" ] || {
    echo "Не найден pg_dump." >&2
    exit 1
}

STAMP="$(date +%F_%H%M%S)"
echo "Бэкап в $OUT ($STAMP)"

while IFS= read -r db; do
    [ -n "$db" ] || continue
    echo "  $db"
    sudo -u postgres "$PG_DUMP" -Fc "$db" >"$OUT/$db-$STAMP.dump"
done < <(sudo -u postgres "$PSQL" -tAc "SELECT datname FROM pg_database
    WHERE datallowconn AND NOT datistemplate AND datname <> 'postgres'
    ORDER BY datname")

GLOBALS="/usr/pgsql-$MAJOR/bin/pg_dumpall"
[ -x "$GLOBALS" ] || GLOBALS="$(command -v pg_dumpall || true)"
if [ -n "$GLOBALS" ] && [ -x "$GLOBALS" ]; then
    sudo -u postgres "$GLOBALS" --globals-only >"$OUT/globals-$STAMP.sql"
fi

find "$OUT" -maxdepth 1 -type f \( -name '*.dump' -o -name 'globals-*.sql' \) \
    -mtime +"$KEEP_DAYS" -delete

echo "Готово. Хранение: $KEEP_DAYS дней."
