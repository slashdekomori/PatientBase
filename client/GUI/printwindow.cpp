#include "printwindow.h"
#include "ui_printwindow.h"

#include <QPushButton>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QtGlobal>
#include <QMessageBox>
#include <memory>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QProcessEnvironment>
#include <QTimer>

PrintWindow::PrintWindow(QWidget* parent, WidgetHandler& widget_handler)
    : QDialog(parent)
    , ui(new Ui::PrintWindow)
    , widget_handler_(widget_handler)
    , json_printer_(widget_handler)
{
    ui->setupUi(this);
    LaunchDocParser();
    ConnectAll();
}

PrintWindow::~PrintWindow()
{
    delete ui;
}

void PrintWindow::ConnectAll(){
    connect(ui->PrintButton, &QPushButton::clicked, this, &PrintWindow::print_button_clicked);
    connect(ui->CancelButton, &QPushButton::clicked, this, &PrintWindow::cancel_button_clicked);
    connect(ui->Hystory, &QCheckBox::clicked, this, &PrintWindow::AllHystoryCheked);
}


void PrintWindow::AllHystoryCheked() {
    auto all_hystory_check_box = qobject_cast<QCheckBox*>(sender());
    auto SetEnabledAndCheckedToFalse = [](QCheckBox* check_box) {
        check_box->setEnabled(false);
        check_box->setChecked(false);
    };
    auto check_boxes = ui->groupBox->findChildren<QCheckBox*>();

    if (all_hystory_check_box->isChecked()) {
        for (auto& check_box : check_boxes) {
            if (check_box != all_hystory_check_box) {
                SetEnabledAndCheckedToFalse(check_box);
            }
        }
    } else if (!all_hystory_check_box->isChecked()) {
        for (auto& check_box : check_boxes) {
            if (check_box != all_hystory_check_box) {
                check_box->setEnabled(true);
            }
        }
    }
}

// void PrintWindow::LaunchDocParser() {
//     DocParser = std::make_unique<QProcess>(this);
//     DocParser->start(doc_parser_FilePath);
//     if (!DocParser->waitForStarted()) {
//         QMessageBox::warning(this, "Не удалось запустить процесс: DocParser", DocParser->errorString());
//         // Удаляем объект в случае ошибки
//         DocParser.get()->deleteLater();
//     }
// }



void PrintWindow::LaunchDocParser() {
    QString doc_parser_FilePath;
    QString basePath = QCoreApplication::applicationDirPath();

#ifdef Q_OS_WIN
    doc_parser_FilePath = basePath + "/DocParser.exe";
#elif defined(Q_OS_LINUX)
    doc_parser_FilePath = basePath + "/DocParser.bin";

    // Для Linux: убеждаемся, что файл имеет права на выполнение
    QFile::Permissions perms = QFile::permissions(doc_parser_FilePath);
    if (!(perms & QFile::ExeUser)) {
        QFile::setPermissions(doc_parser_FilePath,
                              perms | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
    }
#else
    doc_parser_FilePath = basePath + "/DocParser";
#endif

    // Проверяем существование файла
    if (!QFile::exists(doc_parser_FilePath)) {
        QMessageBox::warning(this, "Ошибка",
                           QString("Файл не найден: %1").arg(doc_parser_FilePath));
        return;
    }

    DocParser = std::make_unique<QProcess>(this);
    connect(DocParser.get(), &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        // Обработка завершения процесса
        if (exitStatus == QProcess::CrashExit) {
            QMessageBox::warning(this, "Ошибка", "DocParser завершился с ошибкой.");
            QTimer::singleShot(0, this, [this] { LaunchDocParser(); }); // Перезапустить процесс
        } else if (exitCode != 0) {
            // например, не найден шаблон или не тот язык системы: не оставляем окно висеть
            ui->PrintButton->setEnabled(true);
            ui->LoadLable->setText("");
            QMessageBox::warning(this, "Ошибка", QString("DocParser завершился с ошибкой (код %1).").arg(exitCode));
            QTimer::singleShot(0, this, [this] { LaunchDocParser(); });
        }
    });
    connect(DocParser.get(), &QProcess::readyReadStandardOutput, this, &PrintWindow::HandleDocParserOutput);
    DocParser->start(doc_parser_FilePath);

    if (!DocParser->waitForStarted()) {
        QMessageBox::warning(this, "Не удалось запустить процесс: DocParser",
                           DocParser->errorString());
        DocParser.get()->deleteLater();
    }
}

void PrintWindow::cancel_button_clicked(){
    for (auto& widget : ui->groupBox->children()) {
        if (auto check_box = qobject_cast<QCheckBox*>(widget)) {
            check_box->setChecked(false);
        }
    }
    ui->PrintButton->setEnabled(true);
    ui->LoadLable->setText("");
    hide();
}

void PrintWindow::print_button_clicked(){
    std::vector<std::string> doc_to_parse = CollectDocumentsToParse();
        if (doc_to_parse.empty()) { return; }

        widget_handler_.FillWidgetsMap();
        QString file_path = QString::fromStdString(widget_handler_.GetWidgetValue("PatientFIO") + " " + widget_handler_.GetWidgetValue("IncomeDate"));
        // отсекаем время от даты поступления
        file_path = file_path.chopped(6);
        if (doc_to_parse[0] == "ALL_HYSTORY") {
            file_path = "Output/ALL_HYSTORY/" + file_path + ".docx";
            QFileInfo file_info(file_path);
            if (file_info.exists() && file_info.isFile()) {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this, "Внимание", "Вы уже выводили и редактировали всю историю, загрузить ее?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(file_path));
                    hide();
                    return;
                }
            }
        }

        json_printer_.MakeJSONFromData(std::move(doc_to_parse));
        if (DocParser && DocParser->state() == QProcess::Running) {
            DocParser->write("read_json\n");
        } else {
            QMessageBox::warning(this, "Упс...", "Что-то пошло не так, попробуйте еще раз");
            LaunchDocParser();
        }
        ui->PrintButton->setEnabled(false);
        ui->LoadLable->setText("Вывод истории в docx, ожидайте...");
}

void AddToPrintIfChecked(const QCheckBox* checkbox, std::vector<std::string>& docs_to_parse) {
    if (checkbox->isChecked()) {
        docs_to_parse.push_back(checkbox->objectName().toStdString());
    }
}

std::vector<std::string> PrintWindow::CollectDocumentsToParse() {
    std::vector<std::string> doc_to_parse;
    if (ui->Hystory->isChecked()) {
        doc_to_parse.push_back("ALL_HYSTORY");
    } else {
        AddToPrintIfChecked(ui->FirstExamination, doc_to_parse);
        AddToPrintIfChecked(ui->RoundExamination, doc_to_parse);
        AddToPrintIfChecked(ui->Operation1, doc_to_parse);
        AddToPrintIfChecked(ui->Operation2, doc_to_parse);
        AddToPrintIfChecked(ui->Epicrisis, doc_to_parse);
        AddToPrintIfChecked(ui->Form045, doc_to_parse);
        AddToPrintIfChecked(ui->AppointmentList, doc_to_parse);
    }
    return doc_to_parse;
}



void PrintWindow::updateProgressBar(const QString& value){
    // Обновляем прогресс бар
    ui->LoadLable->setText(value);
    // QCoreApplication::processEvents();
}

void PrintWindow::HandleDocParserOutput()
{
    // Читаем все доступные данные из стандартного вывода
    while (DocParser->canReadLine()) {
        QByteArray outputLine = DocParser->readLine();
        QString outputString = QString::fromUtf8(outputLine);
        if (outputString.trimmed() == "file complite") {
            hide();
            ui->LoadLable->setText("");
            ui->PrintButton->setEnabled(true);
        }
        // Пример: парсинг выводимых данных для получения значения прогресса
        // Предположим, что вывод содержит строку "Progress: 50"

        // QRegularExpressionMatch match = regex.match(outputString);
        // if (match.hasMatch()) {
        //     QString progressValue = match.captured(1);
        //     updateProgressBar(progressValue);
        //     // if (progressValue == 100) {
        //     //     updateProgressBar(0);

        //     // }
        // }
    }
    // hide();
}
