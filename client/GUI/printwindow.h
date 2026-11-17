#ifndef PRINTWINDOW_H
#define PRINTWINDOW_H

#include "Common/common.h"
#include <QProcess>
#include <QDialog>
#include <JSON/json_printer.h>

namespace Ui {
class PrintWindow;
}

class PrintWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PrintWindow(QWidget *parent, WidgetHandler &widget_handler);
    friend class DataBase;
    ~PrintWindow();

protected:
    // void closeEvent(QCloseEvent *event) override;

private slots:
    void AllHystoryCheked();
    void cancel_button_clicked();
    void print_button_clicked();
    void HandleDocParserOutput();

private:
    void ConnectAll();
    void LaunchDocParser();
    Ui::PrintWindow *ui;
    WidgetHandler& widget_handler_;
    JSONPrinter json_printer_;
    std::unique_ptr<QProcess> DocParser = nullptr;
    std::vector<std::string> CollectDocumentsToParse();
    QString doc_parser_FilePath = "DocParser.exe";
    void updateProgressBar(const QString &value);
    QRegularExpression regex = QRegularExpression("Progress: (\\d+)");
};

#endif // PRINTWINDOW_H
