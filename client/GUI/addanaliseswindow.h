#ifndef ADDANALISESWINDOW_H
#define ADDANALISESWINDOW_H

#include <QDialog>

namespace Ui {
class AddAnalisesWindow;
}

class AddAnalisesWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AddAnalisesWindow(QWidget *parent = nullptr);
    friend class DataBase;
    ~AddAnalisesWindow();

private slots:
    void SaveButtonClicked();

private:
    Ui::AddAnalisesWindow *ui;
};

#endif // ADDANALISESWINDOW_H
