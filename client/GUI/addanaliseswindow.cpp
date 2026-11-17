#include "addanaliseswindow.h"
#include "ui_addanaliseswindow.h"

AddAnalisesWindow::AddAnalisesWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddAnalisesWindow)
{
    ui->setupUi(this);
    connect(ui->SaveButton, &QPushButton::clicked, this, &AddAnalisesWindow::SaveButtonClicked);
}

AddAnalisesWindow::~AddAnalisesWindow()
{
    delete ui;
}


void AddAnalisesWindow::SaveButtonClicked()
{
    hide();
}
