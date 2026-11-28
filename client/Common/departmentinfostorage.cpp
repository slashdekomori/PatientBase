#include "departmentinfostorage.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

bool SetError(QString* error, const QString& message)
{
    if (error) {
        *error = message;
    }
    return false;
}

bool WriteJsonFile(const QString& path, const QByteArray& bytes, bool make_backup, QString* error)
{
    const QFileInfo file_info(path);
    QDir directory = file_info.absoluteDir();
    if (!directory.exists() && !QDir().mkpath(directory.absolutePath())) {
        return SetError(error, QString("Не удалось создать папку для файла настроек:\n%1")
                                   .arg(directory.absolutePath()));
    }

    QString backup_path;
    if (make_backup && QFile::exists(path)) {
        backup_path = path + QStringLiteral(".") +
                      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")) +
                      QStringLiteral(".bak");
        if (!QFile::copy(path, backup_path)) {
            return SetError(error, QString("Не удалось создать резервную копию:\n%1")
                                       .arg(backup_path));
        }
    }

    const QString temp_path = path + QStringLiteral(".tmp");
    QFile temp_file(temp_path);
    if (!temp_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return SetError(error, QString("Не удалось открыть временный файл для записи:\n%1\n\n%2")
                                   .arg(temp_path, temp_file.errorString()));
    }

    const qint64 written = temp_file.write(bytes);
    temp_file.flush();
    temp_file.close();
    if (written != bytes.size()) {
        QFile::remove(temp_path);
        return SetError(error, QString("Не удалось полностью записать временный файл:\n%1")
                                   .arg(temp_path));
    }

    if (QFile::exists(path) && !QFile::remove(path)) {
        QFile::remove(temp_path);
        return SetError(error, QString("Не удалось заменить файл настроек:\n%1").arg(path));
    }

    if (!QFile::rename(temp_path, path)) {
        if (!backup_path.isEmpty() && QFile::exists(backup_path)) {
            QFile::copy(backup_path, path);
        }
        return SetError(error, QString("Не удалось сохранить файл настроек:\n%1").arg(path));
    }

    return true;
}

} // namespace

const QStringList& DepartmentInfoStorage::KnownStringKeys()
{
    static const QStringList keys = {
        "заведующий отделения",
        "лечащий врач",
        "отделение"
    };
    return keys;
}

const QStringList& DepartmentInfoStorage::KnownArrayKeys()
{
    static const QStringList keys = {
        "доктора",
        "анестезиологи",
        "лекарственные препараты",
        "медицинские операционные сестры",
        "медицинские анестезиологические сестры",
        "импланты",
        "медицинское оборудование",
        "физиопроцедуры"
    };
    return keys;
}

const QStringList& DepartmentInfoStorage::KnownDictionaryKeys()
{
    static const QStringList keys = {
        AdditionalMedicinesKey(),
        ExaminationsKey(),
        ConsultationsKey(),
        ManipulationsKey()
    };
    return keys;
}

QString DepartmentInfoStorage::AdditionalMedicinesKey()
{
    return QStringLiteral("лекарственные препараты дополнительно");
}

QString DepartmentInfoStorage::ExaminationsKey()
{
    return QStringLiteral("обследования");
}

QString DepartmentInfoStorage::ConsultationsKey()
{
    return QStringLiteral("консультации");
}

QString DepartmentInfoStorage::ManipulationsKey()
{
    return QStringLiteral("манипуляции после поступления");
}

QString DepartmentInfoStorage::DefaultFilePath()
{
    return QCoreApplication::applicationDirPath() +
           QStringLiteral("/Sources_files/DepartmentInfo.json");
}

const QJsonObject& DepartmentInfoStorage::Data() const
{
    return data_;
}

QJsonObject& DepartmentInfoStorage::Data()
{
    return data_;
}

bool DepartmentInfoStorage::ReadFile(QByteArray* bytes, QJsonObject* data, QString* error) const
{
    const QString path = DefaultFilePath();

    QFile file(path);
    if (!file.exists()) {
        return SetError(error, QString("Файл настроек не найден:\n%1").arg(path));
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return SetError(error, QString("Не удалось открыть файл настроек:\n%1\n\n%2")
                                   .arg(path, file.errorString()));
    }

    const QByteArray content = file.readAll();
    file.close();

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(content, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        return SetError(error, QString("Ошибка разбора JSON в файле:\n%1\n\n%2")
                                   .arg(path, parse_error.errorString()));
    }
    if (!document.isObject()) {
        return SetError(error, QString("Ожидался JSON-объект, но файл содержит другую структуру:\n%1")
                                   .arg(path));
    }

    if (bytes) {
        *bytes = content;
    }
    if (data) {
        *data = document.object();
    }
    return true;
}

bool DepartmentInfoStorage::Load(QString* error)
{
    if (!ReadFile(nullptr, &data_, error)) {
        return false;
    }
    return Validate(data_, error);
}

QString DepartmentInfoStorage::Hash(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

std::optional<QString> DepartmentInfoStorage::ReadHash(QString* error) const
{
    const QString path = DefaultFilePath() + QStringLiteral(".synced");
    QFile file(path);
    if (!file.exists()) {
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SetError(error, QString("Не удалось прочитать метку синхронизации:\n%1\n\n%2")
                            .arg(path, file.errorString()));
        return std::nullopt;
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool DepartmentInfoStorage::WriteHash(const QString& hash, QString* error) const
{
    const QString path = DefaultFilePath() + QStringLiteral(".synced");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return SetError(error, QString("Не удалось записать метку синхронизации:\n%1\n\n%2")
                                   .arg(path, file.errorString()));
    }
    file.write(hash.toUtf8());
    file.close();
    return true;
}

bool DepartmentInfoStorage::Validate(const QJsonObject& data, QString* error)
{
    for (const QString& key : KnownStringKeys()) {
        if (data.contains(key) && !data.value(key).isString()) {
            return SetError(error, QString("Ключ \"%1\" должен быть строкой.").arg(key));
        }
    }

    for (const QString& key : KnownArrayKeys()) {
        if (!data.contains(key)) {
            continue;
        }
        if (!data.value(key).isArray()) {
            return SetError(error, QString("Ключ \"%1\" должен быть списком.").arg(key));
        }
        const QJsonArray values = data.value(key).toArray();
        for (const QJsonValue& value : values) {
            if (!value.isString()) {
                return SetError(error, QString("Список \"%1\" должен содержать только строки.").arg(key));
            }
        }
    }

    for (const QString& key : KnownDictionaryKeys()) {
        if (!data.contains(key)) {
            continue;
        }
        if (!data.value(key).isObject()) {
            return SetError(error, QString("Ключ \"%1\" должен быть словарём.").arg(key));
        }
        const QJsonObject dictionary = data.value(key).toObject();
        for (const QString& name : dictionary.keys()) {
            if (!dictionary.value(name).isString()) {
                return SetError(error, QString("Значение \"%1\" в словаре \"%2\" должно быть строкой.")
                                           .arg(name, key));
            }
        }
    }

    return true;
}

bool DepartmentInfoStorage::Save(const QJsonObject& data, QString* error)
{
    if (!Validate(data, error)) {
        return false;
    }

    const QString path = DefaultFilePath();
    const QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Indented);
    if (!WriteJsonFile(path, bytes, true, error)) {
        return false;
    }

    // Метку синхронизации обновляет вызывающий: при офлайн-правке она должна
    // остаться старой, чтобы следующий запуск загрузил изменения на сервер.
    data_ = data;
    return true;
}

bool DepartmentInfoStorage::SaveCached(const QJsonObject& data, QString* error)
{
    if (!Validate(data, error)) {
        return false;
    }

    const QString path = DefaultFilePath();
    const QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Indented);

    QFile current(path);
    const bool same_content = current.exists()
                              && current.open(QIODevice::ReadOnly)
                              && current.readAll() == bytes;
    if (current.isOpen()) {
        current.close();
    }
    if (!same_content && !WriteJsonFile(path, bytes, false, error)) {
        return false;
    }
    if (!WriteHash(Hash(bytes), error)) {
        return false;
    }

    data_ = data;
    return true;
}
