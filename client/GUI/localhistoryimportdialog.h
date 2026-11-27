#ifndef LOCALHISTORYIMPORTDIALOG_H
#define LOCALHISTORYIMPORTDIALOG_H

#include "Common/casedocumentrepository.h"

#include <QDate>
#include <QDialog>
#include <QVector>

namespace Ui {
class LocalHistoryImportDialog;
}

class LocalHistoryImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LocalHistoryImportDialog(QWidget* parent, CaseDocumentRepository* documents);
    ~LocalHistoryImportDialog();

private slots:
    void SelectAll();
    void ClearSelection();
    void ImportSelected();

private:
    enum class RowStatus {
        Ready,
        Overwrite,
        Already,
        NotFound,
        Ambiguous,
        Imported,
        Failed
    };

    struct Row {
        QString file_path;
        QString fio;
        QDate date;
        int case_id = -1;
        QVector<int> case_ids;
        RowStatus status = RowStatus::Failed;
        QString detail;
    };

    enum Column {
        ColumnCheck = 0,
        ColumnFile,
        ColumnFio,
        ColumnDate,
        ColumnCase,
        ColumnStatus,
        ColumnCount
    };

    void ScanDirectory();
    void RefreshRow(int row);
    void UpdateSummary();

    static QString StatusText(RowStatus status);
    static bool IsCheckable(RowStatus status);
    static bool IsImportable(RowStatus status);
    static QByteArray FileHash(const QString& file_path, QString* error);

    Ui::LocalHistoryImportDialog* ui;
    CaseDocumentRepository* documents_;
    QVector<Row> rows_;
};

#endif // LOCALHISTORYIMPORTDIALOG_H
