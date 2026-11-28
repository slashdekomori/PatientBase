#ifndef DEPARTMENTINFOSTORAGE_H
#define DEPARTMENTINFOSTORAGE_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

// Работа с Sources_files/DepartmentInfo.json.
// Хранит весь JSON-object целиком, чтобы не потерять неизвестные ключи
// при редактировании настроек.
class DepartmentInfoStorage
{
public:
    static QString DefaultFilePath();
    static const QStringList& KnownStringKeys();
    static const QStringList& KnownArrayKeys();
    static const QStringList& KnownDictionaryKeys();
    static QString AdditionalMedicinesKey();
    static QString ExaminationsKey();
    static QString ConsultationsKey();
    static QString ManipulationsKey();

    bool Load(QString* error = nullptr);
    bool Save(const QJsonObject& data, QString* error = nullptr);
    // Запись кэша без резервной копии и без перезаписи, если содержимое то же.
    bool SaveCached(const QJsonObject& data, QString* error = nullptr);
    static bool Validate(const QJsonObject& data, QString* error = nullptr);

    // Сырое чтение нужно для сравнения с меткой последней синхронизации.
    bool ReadFile(QByteArray* bytes, QJsonObject* data, QString* error = nullptr) const;
    static QString Hash(const QByteArray& bytes);
    std::optional<QString> ReadHash(QString* error = nullptr) const;
    bool WriteHash(const QString& hash, QString* error = nullptr) const;

    const QJsonObject& Data() const;
    QJsonObject& Data();

private:
    QJsonObject data_;
};

#endif // DEPARTMENTINFOSTORAGE_H
