#include "printwindow.h"
#include "ui_printwindow.h"
#include "Common/casedocumentrepository.h"

#include <QPushButton>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QtGlobal>
#include <QMessageBox>
#include <exception>
#include <memory>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTimer>

PrintWindow::PrintWindow(QWidget* parent,
                         WidgetHandler& widget_handler,
                         CaseDocumentRepository* documents)
    : QDialog(parent)
    , ui(new Ui::PrintWindow)
    , widget_handler_(widget_handler)
    , json_printer_(widget_handler)
    , documents_(documents)
{
    ui->setupUi(this);

    file_watcher_ = new QFileSystemWatcher(this);
    ready_timer_ = new QTimer(this);
    ready_timer_->setInterval(400);
    stabilize_timer_ = new QTimer(this);
    stabilize_timer_->setSingleShot(true);
    stabilize_timer_->setInterval(1500);

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
    connect(ui->SaveToDbButton, &QPushButton::clicked, this, &PrintWindow::SaveToDatabase);
    connect(ui->Hystory, &QCheckBox::clicked, this, &PrintWindow::AllHystoryCheked);
    connect(ready_timer_, &QTimer::timeout, this, &PrintWindow::CheckDocumentReady);
    connect(stabilize_timer_, &QTimer::timeout, this, [this]() { SaveCurrentDocument(true); });
    connect(file_watcher_, &QFileSystemWatcher::fileChanged,
            this, &PrintWindow::OnDocumentChanged);
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
        // Не оставляем unique_ptr на объект, который будет удалён event loop.
        DocParser->deleteLater();
        DocParser.release();
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

QString PrintWindow::AllHystoryFilePath() const
{
    QString name = QString::fromStdString(widget_handler_.GetWidgetValue("PatientFIO") + " " +
                                          widget_handler_.GetWidgetValue("IncomeDate"));
    // отсекаем время от даты поступления
    name = name.chopped(6);
    return QString("Output/ALL_HYSTORY/%1.docx").arg(name);
}

void PrintWindow::SetCaseContext(int case_id, const QString& kind)
{
    pending_case_id_ = case_id;
    pending_kind_ = kind;
}

void PrintWindow::print_button_clicked(){
    std::vector<std::string> doc_to_parse = CollectDocumentsToParse();
    if (doc_to_parse.empty()) { return; }

    ready_timer_->stop();
    stabilize_timer_->stop();
    if (!file_watcher_->files().isEmpty()) {
        file_watcher_->removePaths(file_watcher_->files());
    }
    pending_file_path_.clear();
    document_ready_ = false;
    stable_ticks_ = 0;
    last_size_ = -1;
    ui->SaveToDbButton->setEnabled(false);

    try {
        widget_handler_.FillWidgetsMap();
        if (doc_to_parse[0] == "ALL_HYSTORY") {
            const QString file_path = AllHystoryFilePath();
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
            pending_file_path_ = file_path;
            pending_kind_ = QStringLiteral("ALL_HYSTORY");
        }

        json_printer_.MakeJSONFromData(std::move(doc_to_parse));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Ошибка подготовки печати",
                             QString("Не удалось сформировать patient_data.json:\n\n%1").arg(error.what()));
        ui->PrintButton->setEnabled(true);
        ui->LoadLable->setText("");
        return;
    }

    if (pending_case_id_ <= 0 && !pending_file_path_.isEmpty()) {
        QMessageBox::warning(this, "Внимание",
                             "Случай не сохранён, документ не будет привязан к базе данных.");
    }

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
    if (!DocParser) {
        return;
    }

    // Читаем все доступные данные из стандартного вывода
    while (DocParser->canReadLine()) {
        QByteArray outputLine = DocParser->readLine();
        QString outputString = QString::fromUtf8(outputLine);
        if (outputString.trimmed() == "file complite") {
            hide();
            ui->LoadLable->setText("");
            ui->PrintButton->setEnabled(true);
            if (!pending_file_path_.isEmpty() && pending_case_id_ > 0) {
                StartDocumentWatching();
            }
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

void PrintWindow::StartDocumentWatching()
{
    document_ready_ = false;
    stable_ticks_ = 0;
    last_size_ = -1;
    ui->SaveToDbButton->setEnabled(false);

    if (!file_watcher_->files().isEmpty()) {
        file_watcher_->removePaths(file_watcher_->files());
    }
    if (QFileInfo::exists(pending_file_path_)) {
        file_watcher_->addPath(pending_file_path_);
    }
    ready_timer_->start();
    ui->LoadLable->setText("Ожидание готовности DOCX...");
}

void PrintWindow::CheckDocumentReady()
{
    if (pending_file_path_.isEmpty() || document_ready_) {
        return;
    }

    const QFileInfo info(pending_file_path_);
    if (!info.exists() || !info.isFile() || info.size() <= 0) {
        stable_ticks_ = 0;
        last_size_ = -1;
        return;
    }

    if (info.size() == last_size_ && info.lastModified() == last_modified_) {
        ++stable_ticks_;
    } else {
        stable_ticks_ = 1;
        last_size_ = info.size();
        last_modified_ = info.lastModified();
    }

    if (stable_ticks_ < 2) {
        return;
    }

    ready_timer_->stop();
    document_ready_ = true;
    if (!file_watcher_->files().contains(pending_file_path_)) {
        file_watcher_->addPath(pending_file_path_);
    }
    ui->SaveToDbButton->setEnabled(true);
    ui->LoadLable->setText("Документ готов");
    QDesktopServices::openUrl(QUrl::fromLocalFile(pending_file_path_));
    SaveCurrentDocument(true);
}

void PrintWindow::OnDocumentChanged(const QString& path)
{
    if (!document_ready_ || path != pending_file_path_) {
        return;
    }
    stabilize_timer_->stop();
    stabilize_timer_->start();
    ui->LoadLable->setText("Документ изменён, ожидание сохранения...");
}

void PrintWindow::SaveCurrentDocument(bool automatic)
{
    if (!documents_ || pending_case_id_ <= 0 || pending_file_path_.isEmpty()) {
        return;
    }

    QString error;
    int version = 0;
    if (!documents_->SaveFileVersion(pending_case_id_, pending_kind_, pending_file_path_,
                                     &version, &error)) {
        ui->LoadLable->setText("Не удалось сохранить документ в базу");
        if (!automatic) {
            QMessageBox::warning(this, "Ошибка сохранения в базу", error);
        }
        return;
    }

    ui->LoadLable->setText(QString("Версия %1 сохранена в базу").arg(version));
    if (!automatic) {
        QMessageBox::information(this, "Сохранено в базу",
                                 QString("Версия %1 документа сохранена в базу данных.").arg(version));
    }
}

void PrintWindow::SaveToDatabase()
{
    SaveCurrentDocument(false);
}
