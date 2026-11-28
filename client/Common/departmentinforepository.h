#ifndef DEPARTMENTINFOREPOSITORY_H
#define DEPARTMENTINFOREPOSITORY_H

#include <QDateTime>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

// Хранилище DepartmentInfo.json в базе отделения.
// Использует отдельное именованное соединение, чтобы не мешать SQLTable.
class DepartmentInfoRepository
{
public:
    enum class LoadResult { Ok, Empty, Error };

    DepartmentInfoRepository();
    ~DepartmentInfoRepository();

    // Вызывается до создания главного окна: подтягивает настройки с сервера
    // и обновляет локальный файл-кэш для DocParser и виджетов.
    static void Synchronize(QString* warning = nullptr);

    LoadResult Load(QJsonObject* data, QDateTime* updated_at, QString* error = nullptr);
    bool Save(const QJsonObject& data, QString* error = nullptr);

    bool TryLock(const QString& owner, QString* locked_by, QString* error = nullptr);
    void Unlock();

private:
    bool EnsureConnection(QString* error);
    bool EnsureTable(QString* error);

    QString connection_name_;
    QSqlDatabase db_;
    bool table_checked_ = false;
};

// Блокировка окна настроек: в БД, а если сервер недоступен — локальным файлом.
class DepartmentInfoLock
{
public:
    enum class Result { Acquired, Busy, Error };

    Result Acquire(QString* details, QString* error = nullptr);
    void Release();

private:
    static QString LockFilePath();
    static QString CurrentOwner();

    DepartmentInfoRepository repository_;
    bool db_locked_ = false;
    bool file_locked_ = false;
};

#endif // DEPARTMENTINFOREPOSITORY_H
