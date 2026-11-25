#include "sqltable.h"

#include <QSqlQuery>
#include <QLineEdit>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSqlError>
#include <QSqlRecord>
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>


namespace {

// Читает строку из settings.ini. QSettings отдает значения с запятой как список,
// поэтому склеиваем обратно (иначе пароль с запятой потеряется).
QString ReadString(const QSettings& settings, const QString& key, const QString& default_value) {
    const QVariant value = settings.value(key, default_value);
    if (value.metaType().id() == QMetaType::QStringList) {
        return value.toStringList().join(",");
    }
    return value.toString();
}

// Привязывает значения виджетов к параметрам запроса (имя параметра = имя виджета = имя столбца).
// Даты передаем строкой ISO (PostgreSQL сам приведет к типу столбца), пустая дата -> NULL.
void BindWidgets(QSqlQuery& query,
                 const std::vector<QObject*>& objects,
                 const QHash<QString, QString>& column_names_by_lower) {
    const QVariant null_string = QVariant(QMetaType::fromType<QString>());

    for (QObject* widget : objects) {
        const QString object_name = widget->objectName();
        if (object_name == "id" || !column_names_by_lower.contains(object_name.toLower())) {
            continue;
        }
        const QString param = ":" + object_name;

        if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
            query.bindValue(param, lineEdit->text());
        } else if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
            query.bindValue(param, textEdit->toPlainText());
        } else if (auto dateEdit = qobject_cast<QDateEdit*>(widget)) {
            // QDateEdit наследует QDateTimeEdit, поэтому проверяем его раньше
            const QDate date = dateEdit->date();
            query.bindValue(param, date.isValid() ? QVariant(date.toString("yyyy-MM-dd")) : null_string);
        } else if (auto dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget)) {
            const QDateTime date_time = dateTimeEdit->dateTime();
            query.bindValue(param, date_time.isValid() ? QVariant(date_time.toString("yyyy-MM-dd HH:mm:ss")) : null_string);
        } else if (auto checkBox = qobject_cast<QCheckBox*>(widget)) {
            query.bindValue(param, checkBox->isChecked());
        } else if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
            query.bindValue(param, comboBox->currentText());
        }
    }
}

} // namespace


SQLTable::SQLTable(QMainWindow* data_base_gui) : main_window(data_base_gui) {
    // PostgreSQL отдает имена столбцов в нижнем регистре, а код ищет виджеты по именам вроде PatientFIO.
    for (const auto& column : table_columns_and_types) {
        column_names_by_lower_.insert(column.first.toLower(), column.first);
    }

    OpenConnection();
}

bool SQLTable::OpenConnection() {
    const QString settings_path = QCoreApplication::applicationDirPath() + "/settings.ini";
    QSettings settings(settings_path, QSettings::IniFormat);

    const QString host = ReadString(settings, "database/host", "localhost");
    const int port = ReadString(settings, "database/port", "5432").toInt();
    const QString name = ReadString(settings, "database/name", "patientbase");

    if (!db_.isValid()) {
        db_ = QSqlDatabase::addDatabase("QPSQL");
    }
    db_.setHostName(host);
    db_.setPort(port);
    db_.setDatabaseName(name);
    db_.setUserName(ReadString(settings, "database/user", "pb"));
    db_.setPassword(ReadString(settings, "database/password", ""));
    // чтобы программа не висела минуту, если сервер недоступен
    db_.setConnectOptions("connect_timeout=5");

    if (!db_.open()) {
        QMessageBox::warning(main_window, "Ошибка подключения к базе данных",
                             QString("Не удалось подключиться к PostgreSQL (%1:%2, база \"%3\").\n"
                                     "Проверьте файл настроек:\n%4\n\n%5")
                                 .arg(host).arg(port).arg(name, settings_path, db_.lastError().text()));
        return false;
    }

    if (!EnsureSchema()) {
        db_.close();
        return false;
    }

    return true;
}


bool SQLTable::EnsureSchema() {
    QSqlQuery query(db_);

    for (const auto& column : table_columns_and_types) {
        if (column.first == "id") {
            continue;
        }

        // SQLite-совместимый тип из таблицы колонок приводим к типу PostgreSQL.
        const QString sql_type = column.second == "DATETIME" ? "TIMESTAMP" : column.second;
        // DDL нельзя выполнять через server-side prepared statement QPSQL.
        const QString sql = QString("ALTER TABLE \"%1\" ADD COLUMN IF NOT EXISTS %2 %3")
                                .arg(data_base_name.toLower(), column.first.toLower(), sql_type);
        if (!query.exec(sql)) {
            QMessageBox::warning(main_window, "Ошибка обновления схемы базы данных",
                                 QString("Не удалось добавить столбец %1 в таблицу %2.\n\n"
                                         "SQL: %3\n\n%4")
                                     .arg(column.first, data_base_name, sql, query.lastError().text()));
            return false;
        }
        query.finish();
    }

    return true;
}


const QSqlDatabase& SQLTable::GetDataBase() {
    return db_;
}

std::optional<int> SQLTable::AddNewPatient(const std::vector<QObject*>& objects) {

    if (!MakeTransaction()) {
        return std::nullopt;
    }
    QSqlQuery query;

    // id генерирует сама база, поэтому его в INSERT не включаем
    QStringList columns;
    QStringList values;
    for (const auto& column : table_columns_and_types) {
        if (column.first == "id") {
            continue;
        }
        columns << column.first;
        values << ':' + column.first;
    }

    query.prepare("INSERT INTO DataBase ( " + columns.join(", ") + " ) VALUES ( " + values.join(", ") + " ) RETURNING id");
    BindWidgets(query, objects, column_names_by_lower_);

    // Выполнение запроса
    if (!query.exec()) {
        QMessageBox::warning(main_window, "Ошибка выполнения запроса \"добавления случая\":", query.lastError().text());
        db_.rollback(); // Отмена транзакции в случае ошибки
        return std::nullopt;
    }

    // Получение ID добавленной строки
    int lastInsertedId = 0;
    if (query.next()) {
        lastInsertedId = query.value(0).toInt();
    }
    query.finish();

    if (!MakeCheckPoint()) {
        return std::nullopt;
    }

    return lastInsertedId;
}


bool SQLTable::MakeTransaction() {
    if (!db_.isOpen()) {
        if (!OpenConnection()) {
            return false;
        }
    }
    if (!db_.transaction()) { // Начало транзакции
        QMessageBox::warning(main_window, "Ошибка начала транзакции", db_.lastError().text());
        return false;
    }
    return true;
}

std::unordered_map<QString, QVariant> SQLTable::GetDataMapById(const int id) {

    std::unordered_map<QString, QVariant> data_map;
    // Формируем SQL-запрос для получения данных по ID
    QSqlQuery query;
    query.prepare("SELECT * FROM DataBase WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
        return data_map; // Возвращаем пустую карту в случае ошибки
    }

    // Проверяем, что запрос вернул результат
    if (query.next()) {
        QSqlRecord record = query.record();
        for (int column = 0; column < record.count(); ++column) {
            // имя столбца из PostgreSQL (нижний регистр) -> имя виджета
            const QString lower_name = record.fieldName(column);
            QString column_name = column_names_by_lower_.value(lower_name, lower_name);
            data_map[std::move(column_name)] = record.value(column);
        }
    }
    query.finish();
    return data_map;

}

bool SQLTable::UpdateRowById(const int id, const std::vector<QObject*>& objects) {

    if (!MakeTransaction()) {
        return false;
    }

    QSqlQuery query;

    // id менять нельзя, поэтому в SET его нет
    QStringList assignments;
    for (const auto& column : table_columns_and_types) {
        if (column.first == "id") {
            continue;
        }
        assignments << column.first + " = :" + column.first;
    }

    query.prepare("UPDATE DataBase SET " + assignments.join(", ") + " WHERE id = :id");
    query.bindValue(":id", id);
    BindWidgets(query, objects, column_names_by_lower_);

    // Выполнение запроса
    if (!query.exec()) {
        QMessageBox::warning(main_window, "Ошибка выполнения запроса \"обновление сведений о пациенте\":", query.lastError().text());
        db_.rollback(); // Отмена транзакции в случае ошибки
        return false;
    }
    query.finish();

    return MakeCheckPoint();
}


bool SQLTable::DeleteRowById(const int id) {

    if (!MakeTransaction()) {
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM DataBase WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        QMessageBox::warning(main_window, "Ошибка выполнения запроса: \"удаление случая\"", query.lastError().text());
        db_.rollback(); // Отмена транзакции в случае ошибки
        return false;
    }
    if (!db_.commit()) {
        QMessageBox::warning(main_window, "Ошибка создания коммита", db_.lastError().text());
        db_.rollback(); // Отмена транзакции в случае ошибки
        return false;
    }

    query.finish();

    return true;
}


bool SQLTable::MakeCheckPoint(){
    if (!db_.commit()) {
        QMessageBox::warning(main_window, "Ошибка создания коммита", db_.lastError().text());
        db_.rollback(); // Отмена транзакции в случае ошибки
        return false;
    }
    return true;
}
