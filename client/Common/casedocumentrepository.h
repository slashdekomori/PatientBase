#ifndef CASEDOCUMENTREPOSITORY_H
#define CASEDOCUMENTREPOSITORY_H

#include <QByteArray>
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>

#include <optional>
#include <vector>

class CaseDocumentRepository
{
public:
    struct DocumentVersion {
        int version_no = 0;
        QString file_name;
        QByteArray content;
        QString sha256;
        QDateTime created_at;
    };

    explicit CaseDocumentRepository(const QSqlDatabase& database);

    bool EnsureSchema(QString* error = nullptr) const;

    bool SaveVersion(int case_id,
                     const QString& kind,
                     const QString& file_name,
                     const QByteArray& content,
                     int* version_no = nullptr,
                     QString* error = nullptr) const;

    bool SaveFileVersion(int case_id,
                         const QString& kind,
                         const QString& file_path,
                         int* version_no = nullptr,
                         QString* error = nullptr) const;

    std::optional<DocumentVersion> Latest(int case_id,
                                          const QString& kind,
                                          QString* error = nullptr) const;

    std::vector<DocumentVersion> ListVersions(int case_id,
                                              const QString& kind,
                                              QString* error = nullptr) const;

private:
    mutable QSqlDatabase db_;
};

#endif // CASEDOCUMENTREPOSITORY_H
