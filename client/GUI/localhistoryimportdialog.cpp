#include "localhistoryimportdialog.h"
#include "ui_localhistoryimportdialog.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QTableWidgetItem>

namespace {

bool IsSkippedName(const QString& base_name)
{
    return base_name.contains(QStringLiteral("(из БД)")) ||
           base_name.contains(QStringLiteral("(загружено в БД")) ||
           base_name.contains(QStringLiteral("(not valid"));
}

QString JoinIds(const QVector<int>& ids)
{
    QStringList result;
    for (int id : ids) {
        result.push_back(QString::number(id));
    }
    return result.join(QStringLiteral(", "));
}

} // namespace

LocalHistoryImportDialog::LocalHistoryImportDialog(QWidget* parent,
                                                   CaseDocumentRepository* documents)
    : QDialog(parent)
    , ui(new Ui::LocalHistoryImportDialog)
    , documents_(documents)
{
    ui->setupUi(this);

    QStringList headers;
    headers << QString() << QStringLiteral("Файл") << QStringLiteral("ФИО")
            << QStringLiteral("Дата") << QStringLiteral("id случая") << QStringLiteral("Статус");
    ui->filesTable->setColumnCount(ColumnCount);
    ui->filesTable->setHorizontalHeaderLabels(headers);
    ui->filesTable->verticalHeader()->setVisible(false);
    ui->filesTable->horizontalHeader()->setSectionResizeMode(ColumnFile, QHeaderView::Stretch);
    ui->filesTable->horizontalHeader()->setSectionResizeMode(ColumnFio, QHeaderView::ResizeToContents);
    ui->filesTable->horizontalHeader()->setSectionResizeMode(ColumnDate, QHeaderView::ResizeToContents);
    ui->filesTable->horizontalHeader()->setSectionResizeMode(ColumnCase, QHeaderView::ResizeToContents);
    ui->filesTable->horizontalHeader()->setSectionResizeMode(ColumnStatus, QHeaderView::ResizeToContents);

    connect(ui->SelectAllButton, &QPushButton::clicked, this, &LocalHistoryImportDialog::SelectAll);
    connect(ui->ClearSelectionButton, &QPushButton::clicked, this, &LocalHistoryImportDialog::ClearSelection);
    connect(ui->ImportButton, &QPushButton::clicked, this, &LocalHistoryImportDialog::ImportSelected);
    connect(ui->CloseButton, &QPushButton::clicked, this, &QDialog::reject);

    ScanDirectory();
}

LocalHistoryImportDialog::~LocalHistoryImportDialog()
{
    delete ui;
}

QByteArray LocalHistoryImportDialog::FileHash(const QString& file_path, QString* error)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return QByteArray();
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

void LocalHistoryImportDialog::ScanDirectory()
{
    rows_.clear();
    ui->filesTable->setRowCount(0);

    if (!documents_) {
        ui->summaryLabel->setText(QStringLiteral("Хранилище документов недоступно"));
        return;
    }

    const QDir dir(QStringLiteral("Output/ALL_HYSTORY"));
    if (!dir.exists()) {
        ui->summaryLabel->setText(QStringLiteral("Каталог Output/ALL_HYSTORY не найден"));
        return;
    }

    const QRegularExpression name_pattern(
        QStringLiteral("^(.+) (\\d{2}\\.\\d{2}\\.\\d{4})$"));

    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.docx"),
                                                 QDir::Files, QDir::Name);
    for (const QFileInfo& file_info : files) {
        const QString base_name = file_info.completeBaseName();
        if (IsSkippedName(base_name)) {
            continue;
        }

        const QRegularExpressionMatch match = name_pattern.match(base_name);
        if (!match.hasMatch()) {
            continue;
        }

        Row row;
        row.file_path = file_info.absoluteFilePath();
        row.fio = match.captured(1).simplified();
        row.date = QDate::fromString(match.captured(2), QStringLiteral("dd.MM.yyyy"));
        if (row.fio.isEmpty() || !row.date.isValid()) {
            continue;
        }

        QString error;
        const auto cases = documents_->FindCases(row.fio, row.date, &error);
        if (!error.isEmpty()) {
            row.detail = error;
        } else if (cases.empty()) {
            row.status = RowStatus::NotFound;
        } else if (cases.size() > 1) {
            row.status = RowStatus::Ambiguous;
            for (const auto& found : cases) {
                row.case_ids.push_back(found.id);
            }
            row.detail = QStringLiteral("id: ") + JoinIds(row.case_ids);
        } else {
            row.case_id = cases.front().id;
            row.case_ids.push_back(row.case_id);

            const auto document = documents_->Load(row.case_id, QStringLiteral("ALL_HYSTORY"), &error);
            if (!error.isEmpty()) {
                row.detail = error;
            } else if (!document.has_value()) {
                row.status = RowStatus::Ready;
            } else {
                const QByteArray file_hash = FileHash(row.file_path, &error);
                if (!error.isEmpty()) {
                    row.detail = error;
                } else if (document->sha256.compare(QString::fromLatin1(file_hash),
                                                    Qt::CaseInsensitive) == 0) {
                    row.status = RowStatus::Already;
                } else {
                    row.status = RowStatus::Overwrite;
                }
            }
        }

        rows_.push_back(row);
        const int row_index = rows_.size() - 1;
        ui->filesTable->insertRow(row_index);
        RefreshRow(row_index);
    }

    UpdateSummary();
}

QString LocalHistoryImportDialog::StatusText(RowStatus status)
{
    switch (status) {
    case RowStatus::Ready:
        return QStringLiteral("готов");
    case RowStatus::Overwrite:
        return QStringLiteral("перезаписать");
    case RowStatus::Already:
        return QStringLiteral("совпадает");
    case RowStatus::NotFound:
        return QStringLiteral("случай не найден");
    case RowStatus::Ambiguous:
        return QStringLiteral("несколько случаев");
    case RowStatus::Imported:
        return QStringLiteral("импортировано");
    case RowStatus::Failed:
        return QStringLiteral("ошибка");
    }
    return QString();
}

bool LocalHistoryImportDialog::IsCheckable(RowStatus status)
{
    return status == RowStatus::Ready || status == RowStatus::Overwrite;
}

bool LocalHistoryImportDialog::IsImportable(RowStatus status)
{
    return IsCheckable(status);
}

void LocalHistoryImportDialog::RefreshRow(int row)
{
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    if (ui->filesTable->rowCount() <= row) {
        ui->filesTable->insertRow(row);
    }

    const Row& data = rows_[row];
    Qt::CheckState checked = Qt::Unchecked;
    if (IsCheckable(data.status)) {
        if (QTableWidgetItem* old_check = ui->filesTable->item(row, ColumnCheck)) {
            checked = old_check->checkState();
        }
    }

    QTableWidgetItem* check = new QTableWidgetItem;
    check->setFlags(IsCheckable(data.status)
                        ? Qt::ItemIsUserCheckable | Qt::ItemIsEnabled
                        : Qt::NoItemFlags);
    check->setCheckState(checked);
    ui->filesTable->setItem(row, ColumnCheck, check);

    ui->filesTable->setItem(row, ColumnFile, new QTableWidgetItem(QFileInfo(data.file_path).fileName()));
    ui->filesTable->setItem(row, ColumnFio, new QTableWidgetItem(data.fio));
    ui->filesTable->setItem(row, ColumnDate, new QTableWidgetItem(data.date.toString(QStringLiteral("dd.MM.yyyy"))));
    ui->filesTable->setItem(row, ColumnCase, new QTableWidgetItem(JoinIds(data.case_ids)));

    QString status_text = StatusText(data.status);
    if (!data.detail.isEmpty()) {
        status_text += QStringLiteral(": ") + data.detail;
    }
    QTableWidgetItem* status = new QTableWidgetItem(status_text);
    status->setToolTip(status_text);
    ui->filesTable->setItem(row, ColumnStatus, status);
}

void LocalHistoryImportDialog::UpdateSummary()
{
    int ready = 0;
    int overwrite = 0;
    int imported = 0;
    int failed = 0;
    int other = 0;
    for (const Row& row : rows_) {
        switch (row.status) {
        case RowStatus::Ready:
            ++ready;
            break;
        case RowStatus::Overwrite:
            ++overwrite;
            break;
        case RowStatus::Imported:
            ++imported;
            break;
        case RowStatus::Failed:
            ++failed;
            break;
        default:
            ++other;
            break;
        }
    }

    ui->summaryLabel->setText(
        QStringLiteral("Файлов: %1. Готово: %2. Перезаписать: %3. Импортировано: %4. Ошибок: %5. Прочее: %6.")
            .arg(rows_.size()).arg(ready).arg(overwrite).arg(imported).arg(failed).arg(other));
}

void LocalHistoryImportDialog::SelectAll()
{
    for (int row = 0; row < rows_.size(); ++row) {
        if (rows_[row].status != RowStatus::Ready) {
            continue;
        }
        if (QTableWidgetItem* check = ui->filesTable->item(row, ColumnCheck)) {
            check->setCheckState(Qt::Checked);
        }
    }
}

void LocalHistoryImportDialog::ClearSelection()
{
    for (int row = 0; row < ui->filesTable->rowCount(); ++row) {
        if (QTableWidgetItem* check = ui->filesTable->item(row, ColumnCheck)) {
            check->setCheckState(Qt::Unchecked);
        }
    }
}

void LocalHistoryImportDialog::ImportSelected()
{
    if (!documents_) {
        QMessageBox::warning(this, QStringLiteral("Импорт локальных историй"),
                             QStringLiteral("Хранилище документов недоступно"));
        return;
    }

    int imported = 0;
    int failed = 0;
    int skipped = 0;

    for (int row = 0; row < rows_.size(); ++row) {
        QTableWidgetItem* check = ui->filesTable->item(row, ColumnCheck);
        if (!check || check->checkState() != Qt::Checked) {
            continue;
        }

        Row& data = rows_[row];
        if (!IsImportable(data.status)) {
            check->setCheckState(Qt::Unchecked);
            ++skipped;
            continue;
        }

        QString error;
        if (!documents_->SaveFile(data.case_id, QStringLiteral("ALL_HYSTORY"),
                                  data.file_path, &error)) {
            data.status = RowStatus::Failed;
            data.detail = error;
            ++failed;
            RefreshRow(row);
            continue;
        }

        const QString renamed = CaseDocumentRepository::StatusSuffixPath(
            data.file_path, QStringLiteral("загружено в БД"), QDateTime::currentDateTime());
        if (renamed != data.file_path) {
            if (QFile::rename(data.file_path, renamed)) {
                data.file_path = renamed;
            } else {
                data.detail = QStringLiteral("в базу сохранено, но переименовать файл не удалось");
            }
        }

        data.status = RowStatus::Imported;
        ++imported;
        RefreshRow(row);
    }

    UpdateSummary();
    QMessageBox::information(this, QStringLiteral("Импорт локальных историй"),
                             QStringLiteral("Импортировано: %1. Ошибок: %2. Пропущено: %3.")
                                 .arg(imported).arg(failed).arg(skipped));
}
