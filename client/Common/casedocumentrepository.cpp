#include "casedocumentrepository.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>

#include <atomic>

namespace {

bool SetError(QString* error, const QString& message)
{
    if (error) {
        *error = message;
    }
    return false;
}

QString ReadString(QSettings& settings, const QString& key, const QString& fallback)
{
    return settings.value(key, fallback).toString();
}

QString HashOf(const QByteArray& content)
{
    return QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
}

} // namespace

QString CaseDocumentRepository::StatusSuffixPath(const QString& file_path,
                                                 const QString& marker,
                                                 const QDateTime& when)
{
    const QFileInfo info(file_path);
    QString base = info.completeBaseName();
    if (base.contains(marker)) {
        return file_path;
    }

    base.remove(QStringLiteral(" (из БД)"));

    const QString suffix = QString(" (%1 %2)")
                               .arg(marker, when.toString(QStringLiteral("dd.MM.yyyy hh-mm")));
    const QDir dir = info.absoluteDir();
    const QString extension = info.suffix().isEmpty() ? QString() : "." + info.suffix();
    QString candidate = dir.filePath(base + suffix + extension);
    for (int number = 2; QFileInfo::exists(candidate); ++number) {
        candidate = dir.filePath(QString("%1%2 %3%4")
                                     .arg(base, suffix, QString::number(number), extension));
    }
    return candidate;
}

CaseDocumentRepository::CaseDocumentRepository()
{
    static std::atomic<int> counter{0};
    connection_name_ = QStringLiteral("case_documents_%1").arg(counter.fetch_add(1) + 1);
}

CaseDocumentRepository::~CaseDocumentRepository()
{
    const QString name = connection_name_;
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

bool CaseDocumentRepository::EnsureConnection(QString* error) const
{
    if (db_.isValid() && db_.isOpen()) {
        return true;
    }

    if (QSqlDatabase::contains(connection_name_)) {
        db_ = QSqlDatabase::database(connection_name_, false);
    } else {
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), connection_name_);
    }

    if (!db_.isValid()) {
        return SetError(error, QStringLiteral("Не удалось создать соединение для хранения документов."));
    }

    if (!db_.isOpen()) {
        const QString settings_path = QCoreApplication::applicationDirPath() + "/settings.ini";
        QSettings settings(settings_path, QSettings::IniFormat);

        db_.setHostName(ReadString(settings, "database/host", "localhost"));
        db_.setPort(ReadString(settings, "database/port", "5432").toInt());
        db_.setDatabaseName(ReadString(settings, "database/name", "patientbase"));
        db_.setUserName(ReadString(settings, "database/user", "pb"));
        db_.setPassword(ReadString(settings, "database/password", ""));
        db_.setConnectOptions(QStringLiteral("connect_timeout=5"));

        if (!db_.open()) {
            return SetError(error, db_.lastError().text());
        }
        table_checked_ = false;
    }

    return true;
}

bool CaseDocumentRepository::EnsureTable(QString* error) const
{
    if (table_checked_) {
        return true;
    }
    if (!EnsureConnection(error)) {
        return false;
    }

    QSqlQuery query(db_);

    const QString create_table = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS case_documents ("
        "case_id bigint NOT NULL, "
        "kind text NOT NULL, "
        "file_name text NOT NULL, "
        "content bytea NOT NULL, "
        "sha256 text NOT NULL, "
        "updated_at timestamptz NOT NULL DEFAULT now(), "
        "PRIMARY KEY (case_id, kind))");
    if (!query.exec(create_table)) {
        return SetError(error, query.lastError().text());
    }
    query.finish();

    // Совместимость с базами, где таблица осталась от прежней реализации с версиями.
    const QString add_updated_at = QStringLiteral(
        "ALTER TABLE case_documents ADD COLUMN IF NOT EXISTS "
        "updated_at timestamptz NOT NULL DEFAULT now()");
    if (!query.exec(add_updated_at)) {
        return SetError(error, query.lastError().text());
    }
    query.finish();

    const QString relax_old_version = QStringLiteral(
        "DO $$ BEGIN "
        "IF EXISTS (SELECT 1 FROM information_schema.columns "
        "WHERE table_schema = current_schema() AND table_name = 'case_documents' "
        "AND column_name = 'version_no') THEN "
        "ALTER TABLE case_documents ALTER COLUMN version_no DROP NOT NULL; "
        "END IF; "
        "END $$");
    if (!query.exec(relax_old_version)) {
        return SetError(error, query.lastError().text());
    }
    query.finish();

    // ON CONFLICT (case_id, kind) требует уникальности именно по этой паре.
    const QString create_unique = QStringLiteral(
        "CREATE UNIQUE INDEX IF NOT EXISTS case_documents_current_uq "
        "ON case_documents(case_id, kind)");
    if (!query.exec(create_unique)) {
        return SetError(error, query.lastError().text());
    }
    query.finish();

    table_checked_ = true;
    return true;
}

bool CaseDocumentRepository::SaveFile(int case_id,
                                      const QString& kind,
                                      const QString& file_path,
                                      QString* error)
{
    const QFileInfo info(file_path);
    if (!info.exists() || !info.isFile()) {
        return SetError(error, QString("Файл документа не найден:\n%1").arg(file_path));
    }
    if (info.size() <= 0) {
        return SetError(error, QString("Файл документа пуст:\n%1").arg(file_path));
    }

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return SetError(error, file.errorString());
    }
    const QByteArray content = file.readAll();
    file.close();

    if (content.isEmpty()) {
        return SetError(error, QString("Не удалось прочитать документ:\n%1").arg(file_path));
    }

    if (!EnsureTable(error)) {
        return false;
    }

    // Один UPSERT: не используем явную транзакцию на общем для программы пуле
    // соединений и не блокируем основную таблицу database.
    QSqlQuery query(db_);
    query.prepare(
        "INSERT INTO case_documents (case_id, kind, file_name, content, sha256, updated_at) "
        "VALUES (?, ?, ?, ?, ?, now()) "
        "ON CONFLICT (case_id, kind) DO UPDATE SET "
        "file_name = EXCLUDED.file_name, "
        "content = EXCLUDED.content, "
        "sha256 = EXCLUDED.sha256, "
        "updated_at = now()");
    query.addBindValue(case_id);
    query.addBindValue(kind);
    query.addBindValue(info.fileName());
    query.addBindValue(content);
    query.addBindValue(HashOf(content));

    if (!query.exec()) {
        return SetError(error, query.lastError().text());
    }
    return true;
}

std::optional<CaseDocumentRepository::Document> CaseDocumentRepository::Load(
    int case_id, const QString& kind, QString* error) const
{
    if (!EnsureTable(error)) {
        return std::nullopt;
    }

    QSqlQuery query(db_);
    query.prepare(
        "SELECT file_name, content, sha256, updated_at "
        "FROM case_documents WHERE case_id = ? AND kind = ?");
    query.addBindValue(case_id);
    query.addBindValue(kind);

    if (!query.exec()) {
        SetError(error, query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }

    Document document;
    document.file_name = query.value(0).toString();
    document.content = query.value(1).toByteArray();
    document.sha256 = query.value(2).toString();
    document.updated_at = query.value(3).toDateTime();
    return document;
}

std::vector<CaseDocumentRepository::CaseMatch> CaseDocumentRepository::FindCases(
    const QString& patient_fio, const QDate& income_date, QString* error) const
{
    std::vector<CaseMatch> result;
    if (!EnsureConnection(error)) {
        return result;
    }

    QSqlQuery query(db_);
    query.prepare(
        "SELECT id FROM database "
        "WHERE lower(btrim(patientfio)) = lower(?) "
        "AND incomedate >= ? AND incomedate < ? "
        "ORDER BY id");
    query.addBindValue(patient_fio.trimmed());
    query.addBindValue(income_date.startOfDay());
    query.addBindValue(income_date.addDays(1).startOfDay());

    if (!query.exec()) {
        SetError(error, query.lastError().text());
        return result;
    }

    while (query.next()) {
        CaseMatch match;
        match.id = query.value(0).toInt();
        result.push_back(match);
    }
    return result;
}
