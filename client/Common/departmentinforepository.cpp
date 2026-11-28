#include "departmentinforepository.h"

#include "departmentinfostorage.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>

#include <atomic>
#include <optional>

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
    const QVariant value = settings.value(key, fallback);
    if (value.metaType().id() == QMetaType::QStringList) {
        return value.toStringList().join(",");
    }
    return value.toString();
}

void AppendWarning(QString* warning, const QString& text)
{
    if (!warning) {
        return;
    }
    if (!warning->isEmpty()) {
        warning->append(QStringLiteral("\n\n"));
    }
    warning->append(text);
}

} // namespace


DepartmentInfoRepository::DepartmentInfoRepository()
{
    static std::atomic<int> counter{0};
    connection_name_ = QStringLiteral("department_info_%1").arg(counter.fetch_add(1) + 1);
}

DepartmentInfoRepository::~DepartmentInfoRepository()
{
    const QString name = connection_name_;
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

bool DepartmentInfoRepository::EnsureConnection(QString* error)
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
        return SetError(error, QStringLiteral("Не удалось создать соединение для настроек отделения."));
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

bool DepartmentInfoRepository::EnsureTable(QString* error)
{
    if (table_checked_) {
        return true;
    }
    if (!EnsureConnection(error)) {
        return false;
    }

    QSqlQuery query(db_);

    const QString create_info = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS department_info ("
        "id integer PRIMARY KEY CHECK (id = 1), "
        "content jsonb NOT NULL, "
        "updated_at timestamptz NOT NULL DEFAULT now())");
    if (!query.exec(create_info)) {
        return SetError(error, query.lastError().text());
    }
    query.finish();

    const QString create_lock = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS department_info_lock ("
        "id integer PRIMARY KEY CHECK (id = 1), "
        "locked_by text NOT NULL, "
        "locked_at timestamptz NOT NULL DEFAULT now())");
    if (!query.exec(create_lock)) {
        return SetError(error, query.lastError().text());
    }
    query.finish();

    table_checked_ = true;
    return true;
}

DepartmentInfoRepository::LoadResult DepartmentInfoRepository::Load(
    QJsonObject* data, QDateTime* updated_at, QString* error)
{
    if (!EnsureConnection(error) || !EnsureTable(error)) {
        return LoadResult::Error;
    }

    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral(
            "SELECT CAST(content AS text), updated_at FROM department_info WHERE id = 1"))) {
        SetError(error, query.lastError().text());
        return LoadResult::Error;
    }
    if (!query.next()) {
        query.finish();
        return LoadResult::Empty;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(
        query.value(0).toByteArray(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        SetError(error, parse_error.error != QJsonParseError::NoError
                            ? parse_error.errorString()
                            : QStringLiteral("Содержимое department_info не является JSON-объектом."));
        query.finish();
        return LoadResult::Error;
    }

    if (data) {
        *data = document.object();
    }
    if (updated_at) {
        *updated_at = query.value(1).toDateTime();
    }
    query.finish();
    return LoadResult::Ok;
}

bool DepartmentInfoRepository::Save(const QJsonObject& data, QString* error)
{
    if (!EnsureConnection(error) || !EnsureTable(error)) {
        return false;
    }

    const QByteArray json = QJsonDocument(data).toJson(QJsonDocument::Compact);
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "INSERT INTO department_info (id, content) VALUES (1, CAST(:content AS jsonb)) "
        "ON CONFLICT (id) DO UPDATE SET content = EXCLUDED.content, updated_at = now()"));
    query.bindValue(QStringLiteral(":content"), QString::fromUtf8(json));
    if (!query.exec()) {
        return SetError(error, query.lastError().text());
    }
    query.finish();
    return true;
}

bool DepartmentInfoRepository::TryLock(
    const QString& owner, QString* locked_by, QString* error)
{
    if (!EnsureConnection(error) || !EnsureTable(error)) {
        return false;
    }

    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "UPDATE department_info_lock SET locked_by = :owner, locked_at = now() "
        "WHERE id = 1 AND locked_at < now() - interval '2 hours' "
        "RETURNING locked_by"));
    query.bindValue(QStringLiteral(":owner"), owner);
    if (!query.exec()) {
        return SetError(error, query.lastError().text());
    }
    if (query.next()) {
        query.finish();
        return true;
    }
    query.finish();

    query.prepare(QStringLiteral(
        "INSERT INTO department_info_lock (id, locked_by, locked_at) "
        "VALUES (1, :owner, now()) "
        "ON CONFLICT (id) DO NOTHING RETURNING locked_by"));
    query.bindValue(QStringLiteral(":owner"), owner);
    if (!query.exec()) {
        return SetError(error, query.lastError().text());
    }
    if (query.next()) {
        query.finish();
        return true;
    }
    query.finish();

    if (locked_by) {
        QSqlQuery select(db_);
        if (select.exec(QStringLiteral("SELECT locked_by FROM department_info_lock WHERE id = 1"))
            && select.next()) {
            *locked_by = select.value(0).toString();
        }
        select.finish();
    }
    return false;
}

void DepartmentInfoRepository::Unlock()
{
    if (!db_.isValid() || !db_.isOpen()) {
        return;
    }
    QSqlQuery query(db_);
    query.exec(QStringLiteral("DELETE FROM department_info_lock WHERE id = 1"));
}

void DepartmentInfoRepository::Synchronize(QString* warning)
{
    DepartmentInfoStorage storage;
    const QString path = DepartmentInfoStorage::DefaultFilePath();
    const bool local_file_exists = QFile::exists(path);

    QByteArray local_bytes;
    QJsonObject local_data;
    bool local_valid = false;
    if (local_file_exists) {
        QString local_error;
        if (storage.ReadFile(&local_bytes, &local_data, &local_error)) {
            QString validation_error;
            if (DepartmentInfoStorage::Validate(local_data, &validation_error)) {
                local_valid = true;
            } else {
                AppendWarning(warning, QStringLiteral(
                    "Локальный файл настроек содержит некорректные данные: %1\n"
                    "Серверные настройки не будут записаны в него.")
                                   .arg(validation_error));
            }
        } else {
            AppendWarning(warning, QStringLiteral(
                "Локальный файл настроек повреждён: %1\n"
                "Серверные настройки не будут записаны в него.")
                               .arg(local_error));
        }
    }

    DepartmentInfoRepository repository;
    QJsonObject remote_data;
    QDateTime remote_updated;
    QString error;
    const LoadResult result = repository.Load(&remote_data, &remote_updated, &error);
    if (result == LoadResult::Error) {
        AppendWarning(warning, QStringLiteral(
            "Не удалось получить настройки с сервера: %1\n\n"
            "Будет использован локальный файл: %2")
                                   .arg(error, path));
        return;
    }

    if (result == LoadResult::Empty) {
        if (!local_valid) {
            if (!local_file_exists) {
                AppendWarning(warning, QStringLiteral(
                    "На сервере и локально нет настроек отделения.\n"
                    "Загрузите DepartmentInfo.json через скрипт db-migration/load_department_info.py "
                    "или положите файл в %1.")
                                       .arg(path));
            }
            return;
        }
        if (!repository.Save(local_data, &error)) {
            AppendWarning(warning, QStringLiteral(
                "Не удалось загрузить настройки на сервер: %1\n"
                "Локальный файл остался без изменений.").arg(error));
            return;
        }
        if (!storage.WriteHash(
                DepartmentInfoStorage::Hash(local_bytes), &error)) {
            AppendWarning(warning, error);
        }
        return;
    }

    QString validation_error;
    if (!DepartmentInfoStorage::Validate(remote_data, &validation_error)) {
        AppendWarning(warning, QStringLiteral(
            "Настройки на сервере некорректны: %1\nЛокальный файл оставлен без изменений.")
                                   .arg(validation_error));
        return;
    }

    if (!local_valid) {
        // Файла нет — создаём кэш из серверных данных. Если файл есть, но повреждён,
        // его не перезаписываем, чтобы не потерять возможные ручные правки.
        if (!local_file_exists
            && !storage.SaveCached(remote_data, &error)) {
            AppendWarning(warning, QStringLiteral(
                "Не удалось создать локальный файл настроек: %1").arg(error));
        }
        return;
    }

    const std::optional<QString> synced_hash = storage.ReadHash(&error);
    if (!error.isEmpty()) {
        AppendWarning(warning, error);
        error.clear();
    }

    const QString current_hash = DepartmentInfoStorage::Hash(local_bytes);
    bool upload_local = false;
    if (!synced_hash.has_value()) {
        // Метки нет: если содержимое совпадает — просто запомнить его,
        // иначе считать локальный файл результатом ручного редактирования.
        upload_local = !(local_data == remote_data);
        if (!upload_local) {
            if (!storage.WriteHash(current_hash, &error)) {
                AppendWarning(warning, error);
            }
            return;
        }
    } else if (current_hash != *synced_hash) {
        upload_local = true;
    }

    if (upload_local) {
        if (!repository.Save(local_data, &error)) {
            AppendWarning(warning, QStringLiteral(
                "Не удалось загрузить локальные изменения настроек на сервер: %1").arg(error));
            return;
        }
        if (!storage.WriteHash(current_hash, &error)) {
            AppendWarning(warning, error);
        }
        return;
    }

    if (local_data == remote_data) {
        return;
    }

    if (!storage.SaveCached(remote_data, &error)) {
        AppendWarning(warning, QStringLiteral(
            "Не удалось обновить локальный файл настроек: %1").arg(error));
    }
}

QString DepartmentInfoLock::LockFilePath()
{
    return DepartmentInfoStorage::DefaultFilePath() + QStringLiteral(".lock");
}

QString DepartmentInfoLock::CurrentOwner()
{
    QByteArray user = qgetenv("USERNAME");
    if (user.isEmpty()) {
        user = qgetenv("USER");
    }
    const QString name = QString::fromUtf8(user);
    const QString host = QSysInfo::machineHostName();
    return name.isEmpty() ? host : host + QStringLiteral(" (") + name + QStringLiteral(")");
}

DepartmentInfoLock::Result DepartmentInfoLock::Acquire(QString* details, QString* error)
{
    const QString owner = CurrentOwner();

    QString locked_by;
    QString db_error;
    if (repository_.TryLock(owner, &locked_by, &db_error)) {
        db_locked_ = true;
        return Result::Acquired;
    }
    if (db_error.isEmpty()) {
        if (details) {
            *details = locked_by.isEmpty() ? QStringLiteral("неизвестный пользователь") : locked_by;
        }
        return Result::Busy;
    }

    // Сервер недоступен: блокируем файл локально, чтобы не открыть два окна
    // на одной машине. На других машинах в офлайне согласование невозможно.
    const QString path = LockFilePath();
    const QFileInfo info(path);
    if (info.exists()) {
        QFile file(path);
        QString holder;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            holder = QString::fromUtf8(file.readAll()).trimmed();
            file.close();
        }
        const qint64 age = QDateTime::currentSecsSinceEpoch()
                           - info.lastModified().toSecsSinceEpoch();
        if (age < 2 * 60 * 60) {
            if (details) {
                *details = holder.isEmpty() ? QStringLiteral("неизвестный пользователь") : holder;
            }
            return Result::Busy;
        }
        QFile::remove(path);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        SetError(error, file.errorString());
        return Result::Error;
    }
    file.write(owner.toUtf8());
    file.close();
    file_locked_ = true;
    return Result::Acquired;
}

void DepartmentInfoLock::Release()
{
    if (db_locked_) {
        repository_.Unlock();
        db_locked_ = false;
    }
    if (file_locked_) {
        QFile::remove(LockFilePath());
        file_locked_ = false;
    }
}
