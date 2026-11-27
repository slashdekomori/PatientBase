#include "searchwindow.h"
#include "database.h"
#include "qsqlerror.h"
#include "ui_searchwindow.h"

#include "Common/casedocumentrepository.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUrl>

namespace {

constexpr char kAllHistoryKind[] = "ALL_HYSTORY";

QString SafeFileName(QString name)
{
    for (const QChar forbidden : QStringLiteral("/\\:*?\"<>|")) {
        name.remove(forbidden);
    }
    return name.trimmed();
}

QString FormatIncomeDate(const QVariant& value)
{
    if (value.metaType().id() == QMetaType::QString) {
        const QString text = value.toString().trimmed();
        const int space = text.indexOf(' ');
        return space > 0 ? text.left(space) : text;
    }
    const QDateTime date_time = value.toDateTime();
    return date_time.isValid() ? date_time.toString(QStringLiteral("dd.MM.yyyy")) : QString();
}

QString AllHistoryBaseName(const std::unordered_map<QString, QVariant>& data)
{
    const auto fio_it = data.find(QStringLiteral("PatientFIO"));
    const auto date_it = data.find(QStringLiteral("IncomeDate"));

    QString name = fio_it != data.end() ? fio_it->second.toString().trimmed() : QString();
    if (date_it != data.end()) {
        const QString date = FormatIncomeDate(date_it->second);
        if (!date.isEmpty()) {
            if (!name.isEmpty()) {
                name += " ";
            }
            name += date;
        }
    }
    return SafeFileName(name);
}

QString ExportDocumentPath(const CaseDocumentRepository::Document& document,
                           const std::unordered_map<QString, QVariant>& data)
{
    QString base = AllHistoryBaseName(data);
    if (base.isEmpty()) {
        base = SafeFileName(QFileInfo(document.file_name).completeBaseName());
    }
    if (base.isEmpty()) {
        base = QStringLiteral("История болезни");
    }

    const QDir dir(QStringLiteral("Output/ALL_HYSTORY"));
    const QString suffix = QStringLiteral(" (из БД)");
    QString candidate = dir.filePath(base + suffix + QStringLiteral(".docx"));
    for (int number = 2; QFileInfo::exists(candidate); ++number) {
        candidate = dir.filePath(QString("%1%2 %3.docx").arg(base, suffix, QString::number(number)));
    }
    return candidate;
}

bool WriteDocumentToFile(const QByteArray& content,
                         const QString& file_path,
                         QString* error)
{
    const QFileInfo info(file_path);
    QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (error) {
            *error = QString("Не удалось создать каталог: %1").arg(dir.absolutePath());
        }
        return false;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (file.write(content) != content.size()) {
        if (error) {
            *error = QStringLiteral("Не удалось записать документ из базы");
        }
        return false;
    }
    file.close();
    return true;
}

void OpenDocument(const QString& file_path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file_path).absoluteFilePath()));
}

} // namespace



SearchWindow::SearchWindow(QWidget *parent,
                           SQLTable &sql_table,
                           const MKBMap &mkb_map,
                           CaseDocumentRepository* documents)
    : QDialog(parent)
    , ui(new Ui::SearchWindow)
    , sql_table_(sql_table)
    , mkb_map_(mkb_map)
    , documents_(documents)
{
    ui->setupUi(this);

    SetTableViewSettings();
    connect(ui->SearchLine, &QLineEdit::returnPressed, this, &SearchWindow::FilterData);
    connect(ui->SearchLine, &QLineEdit::textChanged, this, &SearchWindow::IfEmptySelectAll);
    connect(ui->ShowAll, &QCheckBox::stateChanged, this, &SearchWindow::IfEmptySelectAll);
    ConnectButtons();
}

SearchWindow::~SearchWindow() {
    delete ui;
}

void SearchWindow::SetTableViewSettings() {

    model_ = new QSqlQueryModel(this);
    SelectWithLimit("");
    ui->tableView->setModel(model_);

    // Запрет редактирования ячеек
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // установка имен таблицы
    model_->setHeaderData(1, Qt::Horizontal, "№ ИБ");
    model_->setHeaderData(2, Qt::Horizontal, "Основной диагноз");
    model_->setHeaderData(3, Qt::Horizontal, "МКБ");
    model_->setHeaderData(4, Qt::Horizontal, "ФИО пациента");
    model_->setHeaderData(5, Qt::Horizontal, "Дата рождения");
    model_->setHeaderData(6, Qt::Horizontal, "Возраст");
    model_->setHeaderData(7, Qt::Horizontal, "ФИО доктора");
    model_->setHeaderData(8, Qt::Horizontal, "Внешняя причина");
    model_->setHeaderData(9, Qt::Horizontal, "Форма оказания помощи");
    model_->setHeaderData(10, Qt::Horizontal, "ЛН");
    model_->setHeaderData(11, Qt::Horizontal, "Осложнения");
    model_->setHeaderData(12, Qt::Horizontal, "Сопутствующие");
    model_->setHeaderData(13, Qt::Horizontal, "Дополнительные сведения");
    model_->setHeaderData(14, Qt::Horizontal, "Жалобы");
    model_->setHeaderData(15, Qt::Horizontal, "Анамнез заболевания");
    model_->setHeaderData(16, Qt::Horizontal, "Анамнез жизни");
    model_->setHeaderData(17, Qt::Horizontal, "Аллергии");
    model_->setHeaderData(18, Qt::Horizontal, "Импланты");
    model_->setHeaderData(19, Qt::Horizontal, "Адресс");
    model_->setHeaderData(20, Qt::Horizontal, "Дата поступления");
    model_->setHeaderData(21, Qt::Horizontal, "Дата выписки");
    model_->setHeaderData(22, Qt::Horizontal, "Манипуляция");
    model_->setHeaderData(23, Qt::Horizontal, "Описание манипуляции");
    model_->setHeaderData(24, Qt::Horizontal, "Операция название");
    model_->setHeaderData(25, Qt::Horizontal, "Операция протокол");
    model_->setHeaderData(26, Qt::Horizontal, "Операция 2 название");
    model_->setHeaderData(27, Qt::Horizontal, "Операция 2 протокол");



    // Установка ширины столбцов по умолчанию
    ui->tableView->setColumnWidth(0, 0);
    ui->tableView->setColumnWidth(1, 65);
    ui->tableView->setColumnWidth(2, 700);
    ui->tableView->setColumnWidth(3, 70);
    ui->tableView->setColumnWidth(4, 240);
    ui->tableView->setColumnWidth(5, 140);
    ui->tableView->setColumnWidth(6, 80);
    ui->tableView->setColumnWidth(7, 150);
    ui->tableView->setColumnWidth(8, 150);
    ui->tableView->setColumnWidth(9, 150);
    ui->tableView->setColumnWidth(10, 150);
    ui->tableView->setColumnWidth(14, 200);
    ui->tableView->setColumnWidth(15, 200);
    ui->tableView->setColumnWidth(19, 400);
    ui->tableView->setColumnWidth(20, 150);
    ui->tableView->setColumnWidth(21, 150);
    ui->tableView->setColumnWidth(22, 400);
    ui->tableView->setColumnWidth(23, 400);
    ui->tableView->setColumnWidth(24, 400);
    ui->tableView->setColumnWidth(25, 400);
    ui->tableView->setColumnWidth(26, 400);
    ui->tableView->setColumnWidth(27, 400);
    ui->tableView->setColumnWidth(28, 400);
    ui->tableView->setColumnWidth(29, 400);
    ui->tableView->setColumnWidth(30, 400);
    ui->tableView->setColumnWidth(31, 400);
    ui->tableView->setColumnWidth(32, 400);


    // Изменение размера шрифта содержимого tableView
    QFont font = ui->tableView->font(); // Получаем текущий шрифт
    font.setPointSize(14); // Устанавливаем размер шрифта (например, 14 пунктов)
    ui->tableView->setFont(font); // Применяем шрифт к tableView

}

void SearchWindow::SelectWithLimit(const QString& filter = "") {

    QVector<QString> columns = {{"id"},
                                { "HystoryNumber"},
                                { "BaseDisease" },
                                { "MKBText" },
                                { "PatientFIO" },
                                { "BirthdayDate" },
                                { "Age" },
                                { "DoctorName" },
                                { "VneshnyaPrichina" },
                                { "AsisstanForm" },
                                { "ListNetrudosposobnosti"},
                                { "Oslojneniya" },
                                { "SoputstvuyushieZabolevaniya" },
                                { "DopolnitelnieSvedeniya" },
                                { "Jalobi" },
                                { "AnamnesDisease" },
                                { "AnamnesVitae" },
                                { "Allergies" },
                                { "ImplantsBeforeOperation" },
                                { "Adress" },
                                { "IncomeDate" },
                                { "OutComeDate" },
                                {"ManipulationVariant1"},
                                {"ManipulationText1"},
                                {"OperationName"},
                                {"OperationProtocol"},
                                {"OperationName2"},
                                {"OperationProtocol2"},
                                {"MRTBeforeIncome"},
                                {"RKTBeforeIncome"},
                                {"RgrBeforeIncome"},
                                {"TextBeforeIncome"}};

    QString query("SELECT ");
    for (const auto& word : columns) {
        query.append(word + ",");
    }
    query.chop(1);


    if (filter.isEmpty()) {
        query.append(" FROM DataBase ORDER BY id DESC ");
    } else {
        query.append(QString(" FROM DataBase WHERE %1 ORDER BY id DESC ").arg(filter));
    }
    if (!ui->ShowAll->isChecked()) {
        query.append("LIMIT 100");
    }

    QSqlQuery sql_query(sql_table_.GetDataBase());
    sql_query.prepare(std::move(query));
    if (!sql_query.exec()) {
        QMessageBox::warning(this, "Ошибка", sql_query.lastError().text());
        return;
    }
    sql_query.finish();
    model_->setQuery(query);

}

void SearchWindow::IfEmptySelectAll() {
    if (ui->SearchLine->text().isEmpty()) {
        SelectWithLimit("");
    }

}

void SearchWindow::FilterData() {

    auto searchTerm = ui->SearchLine->text().trimmed();
    // Проверка на пустой поисковый запрос
    if (searchTerm.isEmpty()) {
        // Если поисковой запрос пустой, сбрасываем фильтр
        SelectWithLimit("");
        return;
    }

    auto search_term_splited = searchTerm.split(" ");
    searchTerm = search_term_splited.join("%");

    // Список столбцов для поиска
    const QStringList columns = {
        "PatientFIO", "BaseDisease", "MKBTEXT", "OperationProtocol",
        "OperationName", "OperationProtocol2", "OperationName2"
    };

    // Сбор условий фильтрации
    QStringList conditions;
    for (const QString& column : columns) {
        conditions << QString("%1 ILIKE '%%2%'").arg(column, searchTerm);
    }

    // Объединяем условия с помощью OR
    QString filter = conditions.join(" OR ");

    SelectWithLimit(filter);

}

void SearchWindow::OpenCaseButtonClicked() {
    auto index = GetCurrentIndex();
    if(!index) {
        return;
    }
    auto data_map = GetDataMapFromIndex(index.value());
    auto main_window = qobject_cast<DataBase*>(parent());
    main_window->FillForms(std::move(data_map));
    // hide();
    close();
}

void SearchWindow::MakeCopyButtonClicked() {
    auto index = GetCurrentIndex();
    if(!index) {
        return;
    }
    auto data_map = GetDataMapFromIndex(index.value());
    //удалим айди из мапы, чтобы создался новый случай
    data_map.erase("id");

    // удалим некоторые поля, которые не должны повторяться
    data_map["ListNetrudosposobnosti"] = "не нуждается";
    data_map["Height"] = "";
    data_map["Weight"] = "";
    data_map["Adress"] = "";
    data_map["Allergies"] = "отрицает";
    data_map["Age"] = "";
    data_map["AnamnesVitae"] = "ребенок от нормально протекавшей беременности и родов. "
                               "Растёт и развивается соответственно возрасту. Болезнью Боткина не болел. "
                               "Активного туберкулеза в семье нет. Прививки по календарю. Внутривенные гемотрансфузии не производились. На диспансерном учете не состоит.";
    data_map["HystoryNumber"] = "";
    data_map["PatientFIO"] = "";
    data_map["BloudGroup"] = "не определялась";
    data_map["KelFactor"] = "не определялся";
    data_map["OperationNumber"] = "";
    data_map["OperationNumber2"] = "";

    auto main_window = qobject_cast<DataBase*>(parent());
    main_window->FillForms(std::move(data_map));
    // hide();
    close();
}

void SearchWindow::DeleteCaseButtonClicked() {
    auto index = GetCurrentIndex();
    if(!index) {
        return;
    }

    const int case_id = model_->data(model_->index(index.value().row(), 0)).toInt();
    if (case_id <= 0) {
        return;
    }

    auto case_data = GetDataMapFromIndex(index.value());

    CaseDocumentRepository::Document history;
    bool has_history = false;
    if (documents_) {
        QString db_error;
        const auto document = documents_->Load(case_id, QString::fromLatin1(kAllHistoryKind), &db_error);
        if (!db_error.isEmpty()) {
            QMessageBox::warning(this, "Ошибка базы данных", db_error);
            return;
        }
        if (document.has_value() && !document->content.isEmpty()) {
            history = *document;
            has_history = true;
        }
    }

    if (has_history) {
        QMessageBox ask(this);
        ask.setIcon(QMessageBox::Question);
        ask.setWindowTitle("Удаление случая");
        ask.setText(QStringLiteral("У случая есть сохранённая история в базе. "
                                   "Просмотреть и сохранить её перед удалением?"));
        QAbstractButton* save_and_open = ask.addButton("Просмотреть и сохранить", QMessageBox::AcceptRole);
        QAbstractButton* delete_with_doc = ask.addButton("Удалить вместе с DOCX", QMessageBox::DestructiveRole);
        ask.addButton("Отмена", QMessageBox::RejectRole);
        ask.exec();

        QAbstractButton* clicked = ask.clickedButton();
        if (clicked != save_and_open && clicked != delete_with_doc) {
            return;
        }

        if (clicked == save_and_open) {
            const QString file_path = ExportDocumentPath(history, case_data);
            QString file_error;
            if (!WriteDocumentToFile(history.content, file_path, &file_error)) {
                QMessageBox::warning(this, "Ошибка",
                                     QString("Не удалось сохранить историю перед удалением:\n%1").arg(file_error));
                return;
            }
            OpenDocument(file_path);
        }
    } else {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::warning(this, "Внимание", "Вы точно хотите удалить случай без возможности востановления?",
                                     QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
    }

    if (!sql_table_.DeleteRowById(case_id)) {
        return;
    }
    SelectWithLimit("");
}


std::unordered_map<QString, QVariant> SearchWindow::GetDataMapFromIndex(const QModelIndex& index) {
    // Получаем идентификатор строки (или другой уникальный идентификатор)
    int id = model_->data(model_->index(index.row(), 0)).toInt();
    return sql_table_.GetDataMapById(id);

}

std::optional<QModelIndex> SearchWindow::GetCurrentIndex() {
    std::optional<QModelIndex> index;
    if(!ui->tableView->currentIndex().isValid()){
        return index;
    }
    return ui->tableView->currentIndex();
}


void SearchWindow::ConnectButtons() {
    connect(ui->OpenCase, &QPushButton::clicked, this, &SearchWindow::OpenCaseButtonClicked);
    connect(ui->MakeCopyCase, &QPushButton::clicked, this, &SearchWindow::MakeCopyButtonClicked);
    connect(ui->DeleteCaseButton, &QPushButton::clicked, this, &SearchWindow::DeleteCaseButtonClicked);
}
