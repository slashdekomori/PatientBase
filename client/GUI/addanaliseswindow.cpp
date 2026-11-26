#include "addanaliseswindow.h"
#include "ui_addanaliseswindow.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QSize>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

AddAnalisesWindow::AddAnalisesWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddAnalisesWindow)
{
    ui->setupUi(this);

    json_field_ = findChild<QLineEdit*>("DopObsledovaniyaJson");

    connect(ui->SaveButton, &QPushButton::clicked, this, &AddAnalisesWindow::SaveButtonClicked);
    connect(ui->AddRowButton, &QPushButton::clicked, this, &AddAnalisesWindow::AddRowButtonClicked);

    for (const auto& row : FixedRows()) {
        connect(row.variant, &QComboBox::currentTextChanged, this, [this]() { NotifyRowsChanged(); });
        connect(row.date, &QDateEdit::dateChanged, this, [this]() { NotifyRowsChanged(); });
        connect(row.text, &QTextEdit::textChanged, this, [this]() { NotifyRowsChanged(); });
    }

    // Колесико мыши над полями не должно менять даты/варианты. Перенаправляем
    // его на scroll area, чтобы прокрутка работала только по самому окну.
    InstallWheelFilter(ui->scrollAreaWidgetContents);
}

AddAnalisesWindow::~AddAnalisesWindow()
{
    delete ui;
}

void AddAnalisesWindow::NotifyRowsChanged()
{
    QJsonArray rows_json;
    for (const auto& row : GetRows()) {
        QJsonObject object;
        object.insert("date", row.date.toString("dd.MM.yyyy"));
        object.insert("variant", row.variant);
        object.insert("text", row.text);
        rows_json.append(object);
    }

    if (json_field_) {
        const QByteArray json = QJsonDocument(rows_json).toJson(QJsonDocument::Compact);
        json_field_->setText(QString::fromUtf8(json));
    }

    emit RowsChanged();
}

void AddAnalisesWindow::InstallWheelFilter(QWidget* root)
{
    if (!root) {
        return;
    }
    root->installEventFilter(this);
    for (QWidget* child : root->findChildren<QWidget*>()) {
        child->installEventFilter(this);
    }
}

bool AddAnalisesWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event && event->type() == QEvent::Wheel && ui && ui->dialogScrollArea) {
        // Не даём QComboBox/QDateEdit/QLineEdit менять значение колесиком.
        // Событие уходит сразу на scroll area, поэтому прокрутка окна работает.
        QCoreApplication::sendEvent(ui->dialogScrollArea, event);
        return true;
    }
    return QDialog::eventFilter(obj, event);
}

QVector<DopObservationRow> AddAnalisesWindow::ParseRowsJson(const QString& json)
{
    QVector<DopObservationRow> result;

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isArray()) {
        return result;
    }

    const QJsonArray rows = document.array();
    for (const auto& value : rows) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        DopObservationRow row;
        row.date = QDate::fromString(object.value("date").toString().trimmed(), "dd.MM.yyyy");
        row.variant = object.value("variant").toString().trimmed();
        row.text = object.value("text").toString().trimmed();

        if (!row.date.isValid()) {
            row.date = QDate::currentDate();
        }
        if (!row.isEmpty()) {
            result.push_back(row);
        }
    }

    return result;
}

void AddAnalisesWindow::SetVariantItems(const QStringList& items)
{
    variant_items_ = items;

    for (const auto& row : FixedRows()) {
        ApplyVariantItems(row.variant);
    }
    for (const auto& row : extra_rows_) {
        ApplyVariantItems(row.variant);
    }
}

void AddAnalisesWindow::SetRows(const QVector<DopObservationRow>& rows)
{
    const auto fixed_rows = FixedRows();
    for (int i = 0; i < fixed_rows.size(); ++i) {
        if (i < rows.size()) {
            SetRowValues(fixed_rows[i], rows[i]);
        } else {
            ClearRow(fixed_rows[i]);
        }
    }

    const int extra_count = qMax(0, rows.size() - fixed_rows.size());
    EnsureExtraRows(extra_count);

    for (int i = 0; i < extra_rows_.size(); ++i) {
        if (i < extra_count) {
            SetRowValues(extra_rows_[i], rows[fixed_rows.size() + i]);
            extra_rows_[i].container->setVisible(true);
        } else {
            ClearRow(extra_rows_[i]);
            extra_rows_[i].container->setVisible(false);
        }
    }

    NotifyRowsChanged();
}

QVector<DopObservationRow> AddAnalisesWindow::GetRows() const
{
    QVector<DopObservationRow> result;

    auto append_row = [&result](const RowWidgets& row) {
        // isVisible() учитывает видимость родителя: когда диалог закрыт,
        // все дочерние виджеты возвращают false. Нужно проверять именно
        // явное скрытие строки через setVisible(false) в SetRows/RemoveExtraRowAt.
        if (row.container && row.container->isHidden()) {
            return;
        }
        if (!row.variant || !row.date || !row.text) {
            return;
        }

        DopObservationRow values;
        values.variant = row.variant->currentText().trimmed();
        values.date = row.date->date();
        values.text = row.text->toPlainText();

        if (!values.isEmpty()) {
            result.push_back(values);
        }
    };

    for (const auto& row : FixedRows()) {
        append_row(row);
    }
    for (const auto& row : extra_rows_) {
        append_row(row);
    }

    return result;
}

void AddAnalisesWindow::AddEmptyRow()
{
    for (auto& row : extra_rows_) {
        if (!row.container->isVisible()) {
            ClearRow(row);
            row.container->setVisible(true);
            NotifyRowsChanged();
            return;
        }
    }

    extra_rows_.push_back(CreateExtraRow());
    NotifyRowsChanged();
}

void AddAnalisesWindow::SaveButtonClicked()
{
    emit ClientSaveRequested();
    hide();
}

void AddAnalisesWindow::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::WindowStateChange &&
        (windowState() & Qt::WindowMinimized)) {
        emit ClientSaveRequested();
    }
}

void AddAnalisesWindow::reject()
{
    emit ClientSaveRequested();
    QDialog::reject();
}

void AddAnalisesWindow::AddRowButtonClicked()
{
    AddEmptyRow();
}

void AddAnalisesWindow::RemoveRowButtonClicked()
{
    auto button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    for (int i = 0; i < extra_rows_.size(); ++i) {
        if (extra_rows_[i].remove == button) {
            RemoveExtraRowAt(i);
            return;
        }
    }
}

QVector<AddAnalisesWindow::RowWidgets> AddAnalisesWindow::FixedRows() const
{
    QVector<RowWidgets> result;

    for (int i = 1; i <= kFixedRows; ++i) {
        const QString suffix = QString::number(i);
        RowWidgets row;
        row.variant = findChild<QComboBox*>("DopObsledovanieVariant" + suffix);
        row.date = findChild<QDateEdit*>("DopObsledovanieDate" + suffix);
        row.text = findChild<QTextEdit*>("DopObsledovanieText" + suffix);

        if (row.variant && row.date && row.text) {
            result.push_back(row);
        }
    }

    return result;
}

AddAnalisesWindow::RowWidgets AddAnalisesWindow::CreateExtraRow()
{
    const int index = kFixedRows + extra_rows_.size() + 1;
    const QString suffix = QString::number(index);

    RowWidgets row;
    row.container = new QWidget(ui->scrollAreaWidgetContents);
    row.container->setObjectName("DopObsledovanieRow" + suffix);

    auto row_layout = new QHBoxLayout(row.container);
    row_layout->setContentsMargins(2, 2, 2, 2);
    row_layout->setSpacing(6);

    row.variant = new QComboBox(row.container);
    row.variant->setObjectName("DopObsledovanieVariant" + suffix);
    row.variant->setEditable(true);
    row.variant->setMinimumWidth(200);
    row.variant->setMaximumWidth(240);
    row.variant->setMaximumHeight(50);

    row.text = new QTextEdit(row.container);
    row.text->setObjectName("DopObsledovanieText" + suffix);
    row.text->setMaximumHeight(50);
    row.text->setTabChangesFocus(true);
    row.text->setPlaceholderText("Результат обследования");

    row.date = new QDateEdit(row.container);
    row.date->setObjectName("DopObsledovanieDate" + suffix);
    row.date->setMinimumWidth(160);
    row.date->setMaximumWidth(160);
    row.date->setMaximumHeight(50);
    row.date->setDisplayFormat("dd.MM.yyyy");
    row.date->setCalendarPopup(true);
    row.date->setMinimumDate(QDate(1989, 12, 31));
    row.date->setMaximumDate(QDate(2099, 12, 29));

    row.remove = new QPushButton(row.container);
    row.remove->setObjectName("RemoveDopObsledovanieRow" + suffix);
    row.remove->setToolTip("Удалить обследование");
    row.remove->setAccessibleName("Удалить обследование " + suffix);

    QIcon remove_icon = QApplication::style()->standardIcon(QStyle::SP_TrashIcon);
    if (remove_icon.isNull()) {
        remove_icon = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    }
    row.remove->setIcon(remove_icon);
    row.remove->setIconSize(QSize(20, 20));
    row.remove->setFixedSize(32, 32);
    row.remove->setStyleSheet("QPushButton { padding: 0px; }");

    row_layout->addWidget(row.variant, 3);
    row_layout->addWidget(row.text, 6);
    row_layout->addWidget(row.date, 2);
    row_layout->addWidget(row.remove, 1);

    ApplyVariantItems(row.variant);
    ClearRow(row);

    // Кнопки закреплены снизу вне scroll area, поэтому новые строки просто
    // добавляются в конец прокручиваемого списка.
    ui->verticalLayout->addWidget(row.container);
    InstallWheelFilter(row.container);

    connect(row.variant, &QComboBox::currentTextChanged, this, [this]() { NotifyRowsChanged(); });
    connect(row.date, &QDateEdit::dateChanged, this, [this]() { NotifyRowsChanged(); });
    connect(row.text, &QTextEdit::textChanged, this, [this]() { NotifyRowsChanged(); });
    connect(row.remove, &QPushButton::clicked, this, &AddAnalisesWindow::RemoveRowButtonClicked);

    emit VariantComboCreated(row.variant);

    return row;
}

void AddAnalisesWindow::EnsureExtraRows(int count)
{
    while (extra_rows_.size() < count) {
        extra_rows_.push_back(CreateExtraRow());
    }
}

void AddAnalisesWindow::ApplyVariantItems(QComboBox* combo) const
{
    if (!combo) {
        return;
    }

    const QString current = combo->currentText().trimmed();
    QSignalBlocker blocker(combo);

    combo->clear();
    combo->addItem("");
    for (const auto& item : variant_items_) {
        if (!item.isEmpty() && combo->findText(item) < 0) {
            combo->addItem(item);
        }
    }

    if (current.isEmpty()) {
        combo->setCurrentIndex(0);
    } else {
        const int current_index = combo->findText(current);
        if (current_index >= 0) {
            combo->setCurrentIndex(current_index);
        } else {
            combo->setEditText(current);
        }
    }
}

void AddAnalisesWindow::ClearRow(const RowWidgets& row) const
{
    if (!row.variant || !row.date || !row.text) {
        return;
    }

    QSignalBlocker variant_blocker(row.variant);
    QSignalBlocker date_blocker(row.date);
    QSignalBlocker text_blocker(row.text);

    row.variant->setEditText("");
    row.date->setDate(QDate::currentDate());
    row.text->clear();
}

void AddAnalisesWindow::SetRowValues(const RowWidgets& row, const DopObservationRow& values) const
{
    if (!row.variant || !row.date || !row.text) {
        return;
    }

    QSignalBlocker variant_blocker(row.variant);
    QSignalBlocker date_blocker(row.date);
    QSignalBlocker text_blocker(row.text);

    const int variant_index = row.variant->findText(values.variant);
    if (variant_index >= 0) {
        row.variant->setCurrentIndex(variant_index);
    } else {
        row.variant->setEditText(values.variant);
    }

    row.date->setDate(values.date.isValid() ? values.date : QDate::currentDate());
    row.text->setPlainText(values.text);
}

void AddAnalisesWindow::RemoveExtraRowAt(int index)
{
    if (index < 0 || index >= extra_rows_.size()) {
        return;
    }

    for (int i = index; i < extra_rows_.size() - 1; ++i) {
        DopObservationRow values;
        values.variant = extra_rows_[i + 1].variant->currentText();
        values.date = extra_rows_[i + 1].date->date();
        values.text = extra_rows_[i + 1].text->toPlainText();
        SetRowValues(extra_rows_[i], values);
    }

    ClearRow(extra_rows_.last());
    extra_rows_.last().container->setVisible(false);

    NotifyRowsChanged();
}
