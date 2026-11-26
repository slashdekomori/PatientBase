-- Актуальная версия DOCX для случая. Одна строка на (case_id, kind);
-- клиент обновляет её через INSERT ... ON CONFLICT DO UPDATE.
CREATE TABLE IF NOT EXISTS case_documents (
    case_id bigint NOT NULL,
    kind text NOT NULL,
    file_name text NOT NULL,
    content bytea NOT NULL,
    sha256 text NOT NULL,
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (case_id, kind)
);

CREATE UNIQUE INDEX IF NOT EXISTS case_documents_current_uq
    ON case_documents(case_id, kind);

-- Совместимость с таблицей от прежней реализации, если она уже была создана.
ALTER TABLE case_documents ADD COLUMN IF NOT EXISTS
    updated_at timestamptz NOT NULL DEFAULT now();

DO $$ BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.columns
               WHERE table_schema = current_schema()
                 AND table_name = 'case_documents'
                 AND column_name = 'version_no') THEN
        ALTER TABLE case_documents ALTER COLUMN version_no DROP NOT NULL;
    END IF;
END $$;
