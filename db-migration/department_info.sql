-- Настройки отделения (DepartmentInfo.json) и блокировка окна настроек.
-- Для уже существующих баз, где эти таблицы ещё не создавались.
-- Клиент создаёт их сам при первом запуске, этот скрипт — для администратора.
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
