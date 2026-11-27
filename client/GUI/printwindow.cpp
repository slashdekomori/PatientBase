#include "printwindow.h"
#include "ui_printwindow.h"

#include "Common/casedocumentrepository.h"

#include <QPushButton>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
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

namespace {

void WriteDocumentToFile(const QByteArray& content,
                         const QString& file_path,
                         QString* error)
{
    const QFileInfo info(file_path);
    QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (error) {
            *error = QString("Не удалось создать каталог: %1").arg(dir.absolutePath());
        }
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return;
    }
    if (file.write(content) != content.size()) {
        file.close();
        if (error) {
            *error = "Не удалось записать документ из базы";
        }
        return;
    }
    file.close();
}

QString DbCopyPath(const QString& local_path)
{
    const QFileInfo info(local_path);
    return info.absoluteDir().filePath(
        QString("%1 (из БД).%2").arg(info.completeBaseName(), info.suffix()));
}

QString SafeFileName(const QString& file_name, const QString& fallback)
{
    const QString name = QFileInfo(file_name).fileName();
    if (name.isEmpty() || name == "." || name == "..") {
        return fallback;
    }
    return name;
}

void OpenDocument(const QString& file_path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file_path).absoluteFilePath()));
}

} // namespace

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

void PrintWindow::SetCaseContext(int case_id)
{
    current_case_id_ = case_id;
    skip_close_save_prompt_ = false;
    ui->SaveToDbButton->setEnabled(false);
}

void PrintWindow::ResetDocumentContext()
{
    pending_case_id_ = current_case_id_;
    pending_file_path_.clear();
    alternative_file_path_.clear();
    alternative_viewed_at_ = QDateTime();
    pending_kind_.clear();
    document_saved_ = false;
    ui->SaveToDbButton->setEnabled(false);
}

void PrintWindow::SetClientBlocked(bool blocked)
{
    const Qt::WindowModality wanted = blocked ? Qt::ApplicationModal : Qt::NonModal;
    if (windowModality() == wanted) {
        return;
    }

    // Смена modality на уже показанном окне вступает в силу после hide/show.
    const bool was_visible = isVisible();
    if (was_visible) {
        hide();
    }
    setWindowModality(wanted);
    if (was_visible) {
        show();
        raise();
        activateWindow();
    }
}

void PrintWindow::BindDocumentFile(const QString& file_path)
{
    pending_case_id_ = current_case_id_;
    pending_file_path_ = file_path;
    pending_kind_ = QStringLiteral("ALL_HYSTORY");
    document_saved_ = false;

    const bool can_save = pending_case_id_ > 0;
    ui->SaveToDbButton->setEnabled(can_save);
    ui->PrintButton->setEnabled(true);
    if (can_save) {
        ui->LoadLable->setText(QString("Файл для сохранения: %1").arg(file_path));
    } else {
        ui->LoadLable->setText(
            QString("Открыт файл: %1. Случай не сохранён, запись в базу недоступна.")
                .arg(file_path));
    }
}

bool PrintWindow::TryUseExistingAllHystory(const QString& local_path,
                                           const DbDocumentData* db_document)
{
    const QFileInfo local_info(local_path);
    const bool local_exists = local_info.exists() && local_info.isFile() &&
                              local_info.size() > 0;

    if (db_document && local_exists) {
        QMessageBox ask(this);
        ask.setIcon(QMessageBox::Question);
        ask.setWindowTitle("Вся история");
        ask.setText(QStringLiteral("В базе и локально есть история. Что открыть?"));
        QAbstractButton* open_db = ask.addButton("Из БД", QMessageBox::AcceptRole);
        QAbstractButton* open_local = ask.addButton("Локальный", QMessageBox::AcceptRole);
        QAbstractButton* open_both = ask.addButton("Оба (сравнить)", QMessageBox::AcceptRole);
        QAbstractButton* regenerate = ask.addButton("Сгенерировать новый", QMessageBox::DestructiveRole);
        ask.addButton("Отмена", QMessageBox::RejectRole);
        ask.exec();

        QAbstractButton* clicked = ask.clickedButton();
        if (clicked == regenerate) {
            return false;
        }
        if (clicked != open_db && clicked != open_both && clicked != open_local) {
            return true;
        }

        QString db_path;
        if (clicked == open_db || clicked == open_both) {
            db_path = DbCopyPath(local_path);
            QString db_error;
            WriteDocumentToFile(db_document->content, db_path, &db_error);
            if (!db_error.isEmpty()) {
                QMessageBox::warning(this, "Ошибка", db_error);
                BindDocumentFile(local_path);
                OpenDocument(local_path);
                return true;
            }
        }

        if (clicked == open_db) {
            alternative_file_path_ = local_path;
            alternative_viewed_at_ = QDateTime::currentDateTime();
            BindDocumentFile(db_path);
            OpenDocument(db_path);
            return true;
        }

        if (clicked == open_local) {
            alternative_file_path_ = db_path;
            alternative_viewed_at_ = QDateTime::currentDateTime();
            BindDocumentFile(local_path);
            OpenDocument(local_path);
            return true;
        }

        alternative_viewed_at_ = QDateTime::currentDateTime();
        OpenDocument(local_path);
        OpenDocument(db_path);

        QMessageBox choose(this);
        choose.setIcon(QMessageBox::Question);
        choose.setWindowTitle("Вся история");
        choose.setText(QStringLiteral("Какой файл связать с кнопкой «Сохранить в базу»? "
                                      "Запись в базу произойдёт только после нажатия кнопки."));
        QAbstractButton* use_local = choose.addButton("Локальный", QMessageBox::AcceptRole);
        QAbstractButton* use_db = choose.addButton("Из БД", QMessageBox::AcceptRole);
        QAbstractButton* choose_regenerate = choose.addButton("Сгенерировать новый", QMessageBox::DestructiveRole);
        choose.addButton("Отмена", QMessageBox::RejectRole);
        choose.exec();

        QAbstractButton* choice = choose.clickedButton();
        if (choice == use_local) {
            alternative_file_path_ = db_path;
            BindDocumentFile(local_path);
        } else if (choice == use_db) {
            alternative_file_path_ = local_path;
            BindDocumentFile(db_path);
        } else if (choice == choose_regenerate) {
            // Локальный файл будет перезаписан новой историей, невалидной остаётся копия из БД.
            alternative_file_path_ = db_path;
            return false;
        }
        return true;
    }

    if (db_document) {
        QMessageBox ask(this);
        ask.setIcon(QMessageBox::Question);
        ask.setWindowTitle("Вся история");
        ask.setText(QStringLiteral("В базе есть история, локального файла нет. "
                                   "Открыть документ из базы или сгенерировать новую историю болезни?"));
        QAbstractButton* open_db = ask.addButton("Открыть из базы", QMessageBox::AcceptRole);
        QAbstractButton* regenerate = ask.addButton("Сгенерировать новый", QMessageBox::DestructiveRole);
        ask.addButton("Отмена", QMessageBox::RejectRole);
        ask.exec();

        if (ask.clickedButton() == regenerate) {
            return false;
        }
        if (ask.clickedButton() != open_db) {
            return true;
        }

        const QString fallback = local_info.fileName();
        QString db_path = QDir(local_info.absolutePath())
                              .filePath(SafeFileName(db_document->file_name, fallback));
        if (QFileInfo::exists(db_path)) {
            db_path = DbCopyPath(local_path);
        }

        QString db_error;
        WriteDocumentToFile(db_document->content, db_path, &db_error);
        if (!db_error.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", db_error);
            return true;
        }

        BindDocumentFile(db_path);
        OpenDocument(db_path);
        return true;
    }

    if (local_exists) {
        QMessageBox ask(this);
        ask.setIcon(QMessageBox::Question);
        ask.setWindowTitle("Вся история");
        ask.setText(QStringLiteral("Локальный файл истории есть, в базе его нет. "
                                   "Открыть локальный файл или сгенерировать новую историю болезни?"));
        QAbstractButton* open_local = ask.addButton("Открыть локальный", QMessageBox::AcceptRole);
        QAbstractButton* regenerate = ask.addButton("Сгенерировать новый", QMessageBox::DestructiveRole);
        ask.addButton("Отмена", QMessageBox::RejectRole);
        ask.exec();

        if (ask.clickedButton() == regenerate) {
            return false;
        }
        if (ask.clickedButton() == open_local) {
            BindDocumentFile(local_path);
            OpenDocument(local_path);
        }
        return true;
    }

    return false;
}

QString PrintWindow::AllHystoryFilePath() const
{
    QString name = QString::fromStdString(widget_handler_.GetWidgetValue("PatientFIO") + " " +
                                          widget_handler_.GetWidgetValue("IncomeDate"));
    // отсекаем время от даты поступления
    name = name.chopped(6);
    return QString("Output/ALL_HYSTORY/%1.docx").arg(name);
}

void PrintWindow::cancel_button_clicked(){
    for (auto& widget : ui->groupBox->children()) {
        if (auto check_box = qobject_cast<QCheckBox*>(widget)) {
            check_box->setChecked(false);
        }
    }
    ui->PrintButton->setEnabled(true);
    ui->LoadLable->setText("");
    reject();
}

void PrintWindow::print_button_clicked(){
    std::vector<std::string> doc_to_parse = CollectDocumentsToParse();
    if (doc_to_parse.empty()) { return; }

    ResetDocumentContext();
    SetClientBlocked(doc_to_parse[0] == "ALL_HYSTORY");

    try {
        widget_handler_.FillWidgetsMap();
        if (doc_to_parse[0] == "ALL_HYSTORY") {
            const QString file_path = AllHystoryFilePath();

            DbDocumentData db_document;
            const DbDocumentData* db_ptr = nullptr;
            if (current_case_id_ > 0 && documents_) {
                QString db_error;
                const auto document = documents_->Load(
                    current_case_id_, QStringLiteral("ALL_HYSTORY"), &db_error);
                if (!db_error.isEmpty()) {
                    QMessageBox::warning(this, "Ошибка базы данных", db_error);
                } else if (document.has_value() && !document->content.isEmpty()) {
                    db_document.file_name = document->file_name;
                    db_document.content = document->content;
                    db_ptr = &db_document;
                }
            }

            if (TryUseExistingAllHystory(file_path, db_ptr)) {
                return;
            }

            if (current_case_id_ <= 0) {
                QMessageBox::warning(this, "Внимание",
                                     "Случай не сохранён, документ не будет привязан к базе.");
            }
            BindDocumentFile(file_path);
        }

        json_printer_.MakeJSONFromData(std::move(doc_to_parse));
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "Ошибка подготовки печати",
                             QString("Не удалось сформировать patient_data.json:\n\n%1").arg(error.what()));
        ui->PrintButton->setEnabled(true);
        ui->LoadLable->setText("");
        return;
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
            const bool can_save = pending_case_id_ > 0 && !pending_file_path_.isEmpty();
            ui->SaveToDbButton->setEnabled(can_save);
            ui->PrintButton->setEnabled(true);
            if (can_save) {
                // DocParser печатает "file complite" до открытия DOCX, поэтому existence
                // проверяем только в момент нажатия кнопки. Окно оставляем открытым.
                ui->LoadLable->setText(
                    QFileInfo::exists(pending_file_path_)
                        ? QString("Документ готов. Сохраните его в базу: %1").arg(pending_file_path_)
                        : QString("Документ выводится. Сохраните его в базу после появления файла: %1")
                              .arg(pending_file_path_));
            } else {
                ui->LoadLable->setText("");
                // Для всей истории окно остаётся открытым до явного закрытия пользователем.
                if (pending_kind_ != QStringLiteral("ALL_HYSTORY")) {
                    hide();
                }
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

bool PrintWindow::SaveToDatabase()
{
    if (!documents_ || pending_case_id_ <= 0 || pending_file_path_.isEmpty()) {
        return false;
    }

    QString error;
    if (!documents_->SaveFile(pending_case_id_, pending_kind_, pending_file_path_, &error)) {
        ui->LoadLable->setText("Не удалось сохранить документ в базу");
        QMessageBox::warning(this, "Ошибка сохранения в базу", error);
        return false;
    }

    document_saved_ = true;
    FinalizeDocumentFiles();
    ui->LoadLable->setText(QString("Документ сохранён в базу данных: %1").arg(pending_file_path_));
    QMessageBox::information(this, "Сохранено в базу",
                             "Актуальная версия документа сохранена в базу данных.");
    return true;
}

bool PrintWindow::ConfirmCloseAndSave()
{
    if (skip_close_save_prompt_) {
        return true;
    }
    if (pending_case_id_ <= 0 || pending_file_path_.isEmpty() || document_saved_) {
        return true;
    }

    const QFileInfo info(pending_file_path_);
    if (!info.exists() || !info.isFile() || info.size() <= 0) {
        // Вариант А: файл ещё не появился — закрываем окно без вопроса.
        return true;
    }

    QMessageBox ask(this);
    ask.setIcon(QMessageBox::Question);
    ask.setWindowTitle("Закрытие окна печати");
    ask.setText(QStringLiteral("Отправить файл в базу данных или оставить локально?\n\n%1")
                    .arg(info.absoluteFilePath()));
    QAbstractButton* save = ask.addButton("Отправить в базу", QMessageBox::AcceptRole);
    QAbstractButton* keep_local = ask.addButton("Оставить локально", QMessageBox::RejectRole);
    QAbstractButton* cancel_close = ask.addButton("Отмена", QMessageBox::ActionRole);
    ask.exec();

    if (ask.clickedButton() == save) {
        return SaveToDatabase();
    }
    if (ask.clickedButton() == keep_local) {
        return true;
    }
    if (ask.clickedButton() == cancel_close) {
        return false;
    }
    return true;
}

void PrintWindow::closeEvent(QCloseEvent* event)
{
    if (!ConfirmCloseAndSave()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void PrintWindow::reject()
{
    if (!ConfirmCloseAndSave()) {
        return;
    }
    QDialog::reject();
}

void PrintWindow::DisableCloseSavePrompt()
{
    skip_close_save_prompt_ = true;
}

void PrintWindow::FinalizeDocumentFiles()
{
    const QString selected_before = pending_file_path_;
    if (selected_before.isEmpty()) {
        alternative_file_path_.clear();
        return;
    }

    const QString selected_target =
        CaseDocumentRepository::StatusSuffixPath(selected_before,
                                                 QStringLiteral("загружено в БД"),
                                                 QDateTime::currentDateTime());
    if (selected_target == selected_before) {
        ui->LoadLable->setText("Файл уже помечен как загруженный в базу");
    } else if (QFile::rename(selected_before, selected_target)) {
        pending_file_path_ = selected_target;
    } else {
        QMessageBox::warning(this, "Ошибка",
                             QString("Документ сохранён в базу, но не удалось переименовать файл:\n%1")
                                 .arg(selected_before));
    }

    if (!alternative_file_path_.isEmpty() && alternative_file_path_ != selected_before) {
        const QDateTime viewed_at = alternative_viewed_at_.isValid()
                                        ? alternative_viewed_at_
                                        : QDateTime::currentDateTime();
        const QString alternative_target =
            CaseDocumentRepository::StatusSuffixPath(alternative_file_path_,
                                                     QStringLiteral("not valid"), viewed_at);
        if (alternative_target != alternative_file_path_ &&
            !QFile::rename(alternative_file_path_, alternative_target)) {
            QMessageBox::warning(this, "Ошибка",
                                 QString("Не удалось пометить неиспользуемую копию:\n%1")
                                     .arg(alternative_file_path_));
        }
    }

    alternative_file_path_.clear();
}
