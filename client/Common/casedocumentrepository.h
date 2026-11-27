#ifndef CASEDOCUMENTREPOSITORY_H
#define CASEDOCUMENTREPOSITORY_H

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>

#include <optional>
#include <vector>

// Хранит актуальную версию DOCX в отдельной таблице case_documents.
// Использует собственное именованное соединение с PostgreSQL, чтобы транзакции
// и запросы документов не мешали основной таблице database.
class CaseDocumentRepository
{
public:
    struct Document {
        QString file_name;
        QByteArray content;
        QString sha256;
        QDateTime updated_at;
    };

    struct CaseMatch {
        int id = -1;
    };

    static QString StatusSuffixPath(const QString& file_path,
                                    const QString& marker,
                                    const QDateTime& when);

    CaseDocumentRepository();
    ~CaseDocumentRepository();

    bool SaveFile(int case_id,
                  const QString& kind,
                  const QString& file_path,
                  QString* error = nullptr);

    std::optional<Document> Load(int case_id,
                                 const QString& kind,
                                 QString* error = nullptr) const;

    std::vector<CaseMatch> FindCases(const QString& patient_fio,
                                     const QDate& income_date,
                                     QString* error = nullptr) const;

private:
    bool EnsureConnection(QString* error) const;
    bool EnsureTable(QString* error) const;

    QString connection_name_;
    mutable QSqlDatabase db_;
    mutable bool table_checked_ = false;
};

#endif // CASEDOCUMENTREPOSITORY_H
