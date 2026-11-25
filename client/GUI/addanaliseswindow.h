#ifndef ADDANALISESWINDOW_H
#define ADDANALISESWINDOW_H

#include <QDate>
#include <QDialog>
#include <QStringList>
#include <QVector>

namespace Ui {
class AddAnalisesWindow;
}

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QWidget;

struct DopObservationRow {
    QString variant;
    QDate date = QDate::currentDate();
    QString text;

    bool isEmpty() const {
        return variant.trimmed().isEmpty() && text.trimmed().isEmpty();
    }
};

class AddAnalisesWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AddAnalisesWindow(QWidget *parent = nullptr);
    friend class DataBase;
    ~AddAnalisesWindow();

    void SetVariantItems(const QStringList& items);
    void SetRows(const QVector<DopObservationRow>& rows);
    QVector<DopObservationRow> GetRows() const;
    static QVector<DopObservationRow> ParseRowsJson(const QString& json);
    void AddEmptyRow();

signals:
    void RowsChanged();

private slots:
    void SaveButtonClicked();
    void AddRowButtonClicked();
    void RemoveRowButtonClicked();

private:
    struct RowWidgets {
        QWidget* container = nullptr;
        QComboBox* variant = nullptr;
        QDateEdit* date = nullptr;
        QTextEdit* text = nullptr;
        QPushButton* remove = nullptr;
    };

    Ui::AddAnalisesWindow *ui;
    QVector<RowWidgets> extra_rows_;
    QStringList variant_items_;
    QLineEdit* json_field_ = nullptr;

    void NotifyRowsChanged();
    QVector<RowWidgets> FixedRows() const;
    RowWidgets CreateExtraRow();
    void EnsureExtraRows(int count);
    void ApplyVariantItems(QComboBox* combo) const;
    void ClearRow(const RowWidgets& row) const;
    void SetRowValues(const RowWidgets& row, const DopObservationRow& values) const;
    void RemoveExtraRowAt(int index);

    static constexpr int kFixedRows = 8;
};

#endif // ADDANALISESWINDOW_H
