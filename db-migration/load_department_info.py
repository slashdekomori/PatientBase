#!/usr/bin/env python3
"""Загрузить DepartmentInfo.json в базу отделения.

Пример:
  PGPASSWORD='<пароль>' PGSSLMODE=require \
    python3 load_department_info.py DepartmentInfo.json -h <сервер> -U pb -d travma

Параметры подключения, не заданные аргументами, берутся из стандартных
переменных PostgreSQL (PGHOST, PGPORT, PGUSER, PGDATABASE, PGPASSWORD, PGSSLMODE).
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


DDL = """
CREATE TABLE IF NOT EXISTS department_info (
    id integer PRIMARY KEY CHECK (id = 1),
    content jsonb NOT NULL,
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS department_info_lock (
    id integer PRIMARY KEY CHECK (id = 1),
    locked_by text NOT NULL,
    locked_at timestamptz NOT NULL DEFAULT now()
);
"""


def dollar_tag(content):
    """Тег dollar-quoting, гарантированно отсутствующий в JSON."""
    number = 1
    while f"$pb_info_{number}$" in content:
        number += 1
    return f"$pb_info_{number}$"


def main():
    parser = argparse.ArgumentParser(description=__doc__, add_help=False)
    parser.add_argument("--help", action="help", help="показать справку")
    parser.add_argument("json_file", type=Path, help="путь к DepartmentInfo.json")
    parser.add_argument("-h", "--host", default=os.environ.get("PGHOST"))
    parser.add_argument("-p", "--port", default=os.environ.get("PGPORT"))
    parser.add_argument("-U", "--username", default=os.environ.get("PGUSER"))
    parser.add_argument("-d", "--dbname", default=os.environ.get("PGDATABASE"))
    args = parser.parse_args()

    if not args.json_file.is_file():
        parser.error(f"файл не найден: {args.json_file}")

    raw = args.json_file.read_text(encoding="utf-8")
    try:
        data = json.loads(raw)
    except json.JSONDecodeError as exc:
        parser.error(f"некорректный JSON: {exc}")
    if not isinstance(data, dict):
        parser.error("ожидался JSON-объект")

    tag = dollar_tag(raw)
    sql = DDL + f"""
INSERT INTO department_info (id, content)
VALUES (1, {tag}{raw}{tag}::jsonb)
ON CONFLICT (id) DO UPDATE
SET content = EXCLUDED.content, updated_at = now();
"""

    command = ["psql", "-v", "ON_ERROR_STOP=1", "-q"]
    if args.host:
        command += ["-h", args.host]
    if args.port:
        command += ["-p", args.port]
    if args.username:
        command += ["-U", args.username]
    if args.dbname:
        command += ["-d", args.dbname]

    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", suffix=".sql", delete=False
    ) as temp:
        temp.write(sql)
        sql_path = temp.name

    try:
        command += ["-f", sql_path]
        result = subprocess.run(command, check=False)
    finally:
        Path(sql_path).unlink(missing_ok=True)

    if result.returncode != 0:
        print(f"psql завершился с кодом {result.returncode}", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
