#ifndef PRINTWINDOW_H
#define PRINTWINDOW_H

#include "Common/common.h"
#include <QProcess>
#include <QDateTime>
#include <QDialog>
#include <QFileSystemWatcher>
#include <QTimer>
#include <JSON/json_printer.h>

namespace Ui {
class PrintWindow;
}

class CaseDocumentRepository;

class PrintWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PrintWindow(QWidget *parent,
                         WidgetHandler &widget_handler,
                         CaseDocumentRepository* documents);
    friend class DataBase;
    ~PrintWindow();

    void SetCaseContext(int case_id, const QString& kind = QStringLiteral("ALL_HYSTORY"));

protected:
    // void closeEvent(QCloseEvent *event) override;

private slots:
    void AllHystoryCheked();
    void cancel_button_clicked();
    void print_button_clicked();
    void HandleDocParserOutput();
    void SaveToDatabase();
    void OnDocumentChanged(const QString& path);
    void CheckDocumentReady();

private:
    void ConnectAll();
    void StartDocumentWatching();
    void SaveCurrentDocument(bool automatic);
    QString AllHystoryFilePath() const;
    void LaunchDocParser();
    Ui::PrintWindow *ui;
    WidgetHandler& widget_handler_;
    JSONPrinter json_printer_;
    std::unique_ptr<QProcess> DocParser = nullptr;

    CaseDocumentRepository* documents_ = nullptr;
    QFileSystemWatcher* file_watcher_ = nullptr;
    QTimer* ready_timer_ = nullptr;
    QTimer* stabilize_timer_ = nullptr;

    QString pending_file_path_;
    QString pending_kind_;
    int pending_case_id_ = -1;
    qint64 last_size_ = -1;
    QDateTime last_modified_;
    int stable_ticks_ = 0;
    bool document_ready_ = false;

    std::vector<std::string> CollectDocumentsToParse();
    QString doc_parser_FilePath = "DocParser.exe";
    void updateProgressBar(const QString &value);
    QRegularExpression regex = QRegularExpression("Progress: (\\d+)");
};

#endif // PRINTWINDOW_H
