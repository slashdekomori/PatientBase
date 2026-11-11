#ifndef SEARCHWINDOW_H
#define SEARCHWINDOW_H

#include <QDialog>
#include <QSqlTableModel>

#include "SQLTable/sqltable.h"
#include "Common/common.h"

namespace Ui {
class SearchWindow;
}

class SearchWindow : public QDialog
{
    Q_OBJECT

public:
    SearchWindow(QWidget *parent, SQLTable &sql_table, const MKBMap& mkb_map);
    friend class DataBase;
    ~SearchWindow();

private slots:
    void FilterData();
    void IfEmptySelectAll();
    // void on_OpenCase_clicked();
    // void on_MakeCopyCase_clicked();
    // void on_pushButton_clicked();

    void OpenCaseButtonClicked();
    void MakeCopyButtonClicked();
    void DeleteCaseButtonClicked();
private:
    Ui::SearchWindow *ui;

    std::optional<QModelIndex> GetCurrentIndex();
    std::unordered_map<QString, QVariant> GetDataMapFromIndex(const QModelIndex& index);
    void ConnectButtons();
    void SetTableViewSettings();
    void SelectWithLimit(const QString& filter);

    SQLTable& sql_table_;
    const MKBMap& mkb_map_;
    // QSqlTableModel* model_;
    QSqlQueryModel* model_;
};

#endif // SEARCHWINDOW_H
