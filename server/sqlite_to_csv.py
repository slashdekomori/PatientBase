#!/usr/bin/env python3
"""
Выгрузка старой базы SQLite (DataBase.db) в CSV для загрузки в PostgreSQL.
Только стандартная библиотека Python 3, ничего устанавливать не нужно.

Использование:
    python sqlite_to_csv.py путь/к/DataBase.db
Создаст рядом cases.csv и load_cases.sql. Дальше:
    psql -h <сервер> -U pb -d patientbase -f load_cases.sql
(запускать из той же папки, где лежит cases.csv)

Что делает по дороге:
  * DATE      -> оставляет только дату (2024-10-18T03:00:00.000Z -> 2024-10-18); пустое и мусор вроде "2024--" -> NULL
  * DATETIME  -> 2024-10-18 10:37:00 (без миллисекунд и Z); пустое и мусор -> NULL
  * BOOLEAN   -> true / false
  * остальное -> как есть, числа превращаются в текст так же, как их показывал Qt (63.0 -> 63)
"""
import csv, datetime, re, sqlite3, sys
from pathlib import Path


def norm_date(v):
    if v is None:
        return None
    m = re.match(r"\s*(\d{4})-(\d{2})-(\d{2})", str(v))
    if not m:
        return None
    try:
        datetime.date(int(m[1]), int(m[2]), int(m[3]))
    except ValueError:
        return None
    return f"{m[1]}-{m[2]}-{m[3]}"


def norm_datetime(v):
    d = norm_date(v)
    if d is None:
        return None
    m = re.match(r"\s*\d{4}-\d{2}-\d{2}[T ](\d{2}):(\d{2})(?::(\d{2}))?", str(v))
    if not m:
        return d + " 00:00:00"
    hh, mm, ss = int(m[1]), int(m[2]), int(m[3] or 0)
    if hh > 23 or mm > 59 or ss > 59:
        return d + " 00:00:00"
    return f"{d} {hh:02d}:{mm:02d}:{ss:02d}"


def norm_bool(v):
    if v is None or v == "":
        return None
    return "true" if str(v).strip().lower() in ("1", "true", "t", "yes") else "false"


def norm_text(v):
    if v is None:
        return None
    if isinstance(v, float):
        return "%.15g" % v
    if isinstance(v, bytes):
        return v.decode("utf-8", "replace")
    return str(v)


def q(v):
    """CSV-поле: NULL -> пусто без кавычек, строка (в т.ч. пустая) -> в кавычках."""
    if v is None:
        return ""
    return '"' + v.replace('"', '""') + '"'


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = Path(sys.argv[1])
    db = sqlite3.connect(str(src))
    info = db.execute("PRAGMA table_info(DataBase)").fetchall()
    if not info:
        sys.exit("В файле нет таблицы DataBase")
    names = [r[1] for r in info]
    types = [(r[2] or "").upper() for r in info]
    conv = []
    for t in types:
        conv.append(norm_date if t == "DATE" else norm_datetime if t == "DATETIME" else norm_bool if t == "BOOLEAN" else norm_text)

    out_csv = src.parent / "cases.csv"
    rows = nulled = 0
    with open(out_csv, "w", encoding="utf-8", newline="") as f:
        f.write(",".join(n.lower() for n in names) + "\n")
        cur = db.execute("SELECT * FROM DataBase ORDER BY id")
        for row in cur:
            fields = []
            for i, v in enumerate(row):
                c = conv[i](v)
                if c is None and v not in (None, "") and types[i] in ("DATE", "DATETIME"):
                    nulled += 1
                fields.append(q(c))
            f.write(",".join(fields) + "\n")
            rows += 1

    cols = ", ".join(n.lower() for n in names)
    with open(src.parent / "load_cases.sql", "w", encoding="utf-8", newline="\n") as f:
        f.write(f"\\copy database ({cols}) FROM 'cases.csv' WITH (FORMAT csv, HEADER true, ENCODING 'UTF8')\n")
        f.write("SELECT setval(pg_get_serial_sequence('database', 'id'), (SELECT max(id) FROM database));\n")
        f.write("SELECT count(*) AS cases_in_postgres FROM database;\n")

    print(f"Выгружено случаев: {rows} -> {out_csv}")
    print(f"Значений дат, которые были мусором и стали NULL: {nulled}")
    print("Дальше: psql -h <сервер> -U pb -d patientbase -f load_cases.sql")


if __name__ == "__main__":
    main()
