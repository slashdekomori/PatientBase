#ifndef PRINTWINDOW_H
#define PRINTWINDOW_H

#include "Common/common.h"
#include <QByteArray>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QProcess>
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

    // case_id <= 0 означает, что случай ещё не сохранён: кнопка записи в БД будет недоступна
    void SetCaseContext(int case_id);

    // Вызывается при закрытии приложения, чтобы не блокировать выход вопросом о DOCX.
    void DisableCloseSavePrompt();

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private slots:
    void AllHystoryCheked();
    void cancel_button_clicked();
    void print_button_clicked();
    void HandleDocParserOutput();
    bool SaveToDatabase();

private:
    void ConnectAll();
    void LaunchDocParser();

    struct DbDocumentData {
        QString file_name;
        QByteArray content;
    };

    void ResetDocumentContext();
    void SetClientBlocked(bool blocked);
    QString AllHystoryFilePath() const;
    void BindDocumentFile(const QString& file_path);
    void FinalizeDocumentFiles();
    bool TryUseExistingAllHystory(const QString& local_path,
                                  const DbDocumentData* db_document);
    bool ConfirmCloseAndSave();
    Ui::PrintWindow *ui;
    WidgetHandler& widget_handler_;
    JSONPrinter json_printer_;
    std::unique_ptr<QProcess> DocParser = nullptr;

    CaseDocumentRepository* documents_ = nullptr;
    int current_case_id_ = -1;
    int pending_case_id_ = -1;
    QString pending_file_path_;
    QString alternative_file_path_;
    QDateTime alternative_viewed_at_;
    QString pending_kind_;
    bool document_saved_ = false;
    bool skip_close_save_prompt_ = false;

    std::vector<std::string> CollectDocumentsToParse();
    QString doc_parser_FilePath = "DocParser.exe";
    void updateProgressBar(const QString &value);
    QRegularExpression regex = QRegularExpression("Progress: (\\d+)");
};

#endif // PRINTWINDOW_H
