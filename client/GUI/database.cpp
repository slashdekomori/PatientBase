#include "database.h"
#include "qevent.h"
#include "ui_addanaliseswindow.h"
#include "ui_database.h"

#include <QCompleter>


DataBase::DataBase(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::DataBase)
    // , search_window_(this, data_base_, mkb_map_)
    , data_base_(this)
    , print_window(new PrintWindow(this, widget_handler_))
    , diaries_parser_(widget_handler_)

{
    ui->setupUi(this);
    ui->TabWidget->setCurrentIndex(0);
    widget_handler_.CollectWidgetFromTab(ui->TabWidget);
    widget_handler_.AddWidgetsFromWindow(dop_analises_window.ui->groupBox, "Дополнительные анализы");
    ConnectAll();
    SetSettings();
    // если вносились изменения в графические интерфейс - получаем все значения полей и их тип для формирования таблицы
    // widget_handler_.Foo();
}




void DataBase::closeEvent(QCloseEvent *event){
    if(!is_saved) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Внимание", "Вы не сохранили случай, сохранить перед завершение программы?",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            SaveButtonClicked();
            if (patient_id_ == std::nullopt) {
                event->ignore();
                return;
            }
        }
    }
    if (print_window->DocParser && print_window->DocParser->state() == QProcess::Running) {
        print_window->DocParser->write("exit\n");
        print_window->DocParser->terminate(); // Попробовать корректно завершить процесс
        print_window->DocParser->waitForFinished(); // Дождаться завершения
    }
    event->accept();
}

void DataBase::ConnectAll() {
    SetCompletters();
    ConnectQObjectToCheckbox();
    connect(ui->BirthdayDate, &QDateEdit::editingFinished, this, &DataBase::FillVitalIfKnownDateofBirth);
    ConnectMedicines();
    ConnectButtons();
    ConnectWidgetsToCheckIfDataChanged();
    connect(ui->PatientFIO, &QLineEdit::editingFinished, this, &DataBase::ChangeNameToCorrect);
    ConnectDopAnalisesWidgetsToDopAnalisesButton();
    ConnectDopAnalisesComboBoxesToDescriptionParser();
    connect(ui->make_copy_action, &QAction::triggered, this, &DataBase::on_copy_triggered);
}

void DataBase::ConnectMedicines(){
    for (int i = 1; i <= 9; i++) {
        QString index = "MedicineVariant" + QString::number(i);
        auto medicine = ui->Medicines_and_fisio_tab->findChild<QComboBox*>(index);
        connect(medicine, &QComboBox::currentTextChanged, this, &DataBase::TryToParseMedicineAdministrationMethod);

    }
    connect(ui->OutComeDate, &QDateTimeEdit::editingFinished, this, &DataBase::OutComeDateEditingFinished);
}



void DataBase::ConnectButtons() {
    connect(ui->PrintButton, &QPushButton::clicked, this, &DataBase::PrintButtonClicked);
    connect(ui->SaveButton, &QPushButton::clicked, this, &DataBase::SaveButtonClicked);
    connect(ui->SearchButton, &QPushButton::clicked, this, &DataBase::SearchButtonClicked);
    connect(ui->AddAnalisesButton, &QPushButton::clicked, this, &DataBase::AddAnalisesButtonClicked);
}

void DataBase::SetSettings() {
    settings_setter_.SetSettings({
                                  {ui->DoctorName, "доктора"},
                                  {ui->Operator, "доктора"},
                                  {ui->Operator2, "доктора"},
                                  {ui->Asistent, "доктора"},
                                  {ui->Asistent2, "доктора"},
                                  {ui->Anesteziolog, "анестезиологи"},
                                  {ui->Anesteziolog2, "анестезиологи"},
                                  {ui->AnestesisticNurse, "медицинские анестезиологические сестры"},
                                  {ui->AnestesisticNurse2, "медицинские анестезиологические сестры"},
                                  {ui->OperationNurse, "медицинские операционные сестры"},
                                  {ui->Sanitar, "медицинские операционные сестры"},
                                  {ui->Sanitar2, "медицинские операционные сестры"},
                                  {ui->OperationNurse2, "медицинские операционные сестры"},
                                  {ui->ImplantsAfterOperation1, "импланты"},
                                  {ui->ImplantsAfterOperation2, "импланты"},
                                  {ui->OborudovanieInOperation1, "медицинское оборудование"},
                                  {ui->OborudovanieInOperation2, "медицинское оборудование"}
    });
    for (int i = 1; i <= Constants::NUMBER_OF_MEDICINES; i++) {
        QString index = "MedicineVariant" + QString::number(i);
        settings_setter_.SetSettings({{ui->Medicines_and_fisio_tab->findChild<QComboBox*>(index), "лекарственные препараты"}});
        settings_setter_.SetSettings({{ui->Medicines_and_fisio_tab->findChild<QComboBox*>(index), "лекарственные препараты дополнительно"}});

    }
    for (int i = 1; i <= Constants::NUMBER_OF_FISIOPROCEDURES; i++) {
        QString index = "FisioVariant" + QString::number(i);
        settings_setter_.SetSettings({{ui->Medicines_and_fisio_tab->findChild<QComboBox*>(index), "физиопроцедуры"}});
    }
    for (int i = 1; i <= Constants::NUMBER_OF_DOP_OBSLEDOVANIYA; i++) {
        QString index = "DopObsledovanieVariant" + QString::number(i);
        settings_setter_.SetSettings({{dop_analises_window.ui->groupBox->findChild<QComboBox*>(index), "обследования"}});
    }
}

void DataBase::FillForms(std::unordered_map<QString, QVariant>&& widgets_data) {

    widget_handler_.FillForms(std::move(widgets_data));
    auto pos_id = widgets_data.find("id");
    if (pos_id != widgets_data.end()) {
        patient_id_ = widgets_data.at("id").toInt();
        dates_is_filled = true;
        ui->statusbar->showMessage("открыт случай для редактирования");
    } else {
        patient_id_.reset();
        dates_is_filled = false;
        ui->statusbar->showMessage("открыта копия случая");
    }

    // обозначим что данные поменялись
    is_changed_from_last_print = false;
    is_saved = false;

    CheckDopAnalises();

    ui->TabWidget->setCurrentIndex(0);
    // for (const auto& child : ui->diaries_scroll_area->children()) {
    //     if (QLabel* label = qobject_cast<QLabel*>(child)) {
    //         delete child;
    //     }
    // }

}


DataBase::~DataBase()
{
    delete ui;
}


void DataBase::SearchButtonClicked(){

    if (is_changed_from_last_print) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Внимание", "Вы хотите сохранить текущий случай?",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            SaveButtonClicked();
        }
    }

    SearchWindow search_window(this, data_base_, mkb_map_);
    search_window.exec();

}


void DataBase::SaveButtonClicked() {

    if (patient_id_ == std::nullopt) {

        patient_id_ = data_base_.AddNewPatient(widget_handler_.GetAllWidgets());
        if (patient_id_ == std::nullopt) {
            QMessageBox::warning(this, "Ошибка сохранения", "Попробуйте еще раз");
        } else {
            ui->statusbar->showMessage("Случай успешно сохранен");
            is_saved = true;
            is_changed_from_last_print = false;
        }

    } else {
        if(data_base_.UpdateRowById(patient_id_.value(), widget_handler_.GetAllWidgets())) {
            ui->statusbar->showMessage("Случай успешно обновлен");
            is_saved = true;
            is_changed_from_last_print = false;
        } else {
            QMessageBox::warning(this, "Ошибка сохранения", "Попробуйте еще раз");
        }
    }
}



void DataBase::on_IncomeDate_editingFinished() {
    auto dates_widgets = widget_handler_.GetAllDates();
    ui->IncomeDate->blockSignals(true);

    if (dates_is_filled) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Вы уже вводили дату поступления", "Вы хотите перезаписать все остальные даты?",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
    }
    for (auto& widget : dates_widgets) {
        widget->blockSignals(true);
        if (const auto& date_edit = qobject_cast<QDateEdit*>(widget)) {
            if (date_edit != ui->BirthdayDate &&
                date_edit != ui->AIGDate &&
                date_edit != ui->KokavDate) {
                date_edit->setDate(ui->IncomeDate->date());

            }
        } else if (const auto& date_time_edit = qobject_cast<QDateTimeEdit*>(widget)) {
            if (date_time_edit != ui->IncomeDate) {
                date_time_edit->setDateTime(ui->IncomeDate->dateTime());
            }
        }
        widget->blockSignals(false);
    }
    dates_is_filled = true;
    ui->IncomeDate->blockSignals(false);
}

void DataBase::OutComeDateEditingFinished() {
    auto tab = ui->Medicines_and_fisio_tab;
    for (int i = 1; i <= 9; i++) {
        QString medicine_index = "MedicineCancelDate" + QString::number(i);
        auto date = tab->findChild<QDateEdit*>(medicine_index);
        if (date->date() == ui->IncomeDate->date()) {
            date->setDate(ui->OutComeDate->date());
        }
    }
    for (int i = 1; i <= 5; i++) {
        QString fisio_index = "FisioCancelDate" + QString::number(i);
        auto date = tab->findChild<QDateEdit*>(fisio_index);
        if (date->date() == ui->IncomeDate->date()) {
            date->setDate(ui->OutComeDate->date());
        }
    }

}

void DataBase::TryToParseBaseDiagnosisAndMKB() {
    QString current_text = ui->BaseDisease->toPlainText();
    if (!current_text.isEmpty() && current_text.size() <= 5) {
        current_text[0] = current_text[0].toUpper();
        if (mkb_map_.GetMKBMap().find(current_text) != mkb_map_.GetMKBMap().end()) {
            for (int i = 0; i < 10; i++) {
                if (mkb_map_.GetMKBMap().find(current_text + '.' + QString::number(i)) != mkb_map_.GetMKBMap().end()) {
                    return;
                }
            }
            // Если код диагноза найден, заполняем поля
            ChangeText(ui->BaseDisease, mkb_map_.GetMKBMap().at(current_text) + ' ');
            ChangeLine(ui->MKBText, std::move(current_text));
        }
    }
    if (!ui->MKBText->text().isEmpty()) {
        SetGystology();
    }
}

// если завершили редактирование мкб заполняем гистологию, если диагноз d16 или m67
void DataBase::SetGystology() {
    QString mkb_diagnosis = ui->MKBText->text().trimmed();
    if (mkb_diagnosis == "D16.2" || mkb_diagnosis == "M67.4") {
        ui->Histology->setText(ui->BaseDisease->toPlainText() + " направлен(а) на гистологическое исследование");
        ui->HistologyCheck->setChecked(true);
    }
}

void DataBase::ChangeDiagnosisText(QTextEdit* diagnosis_text) {
    QString current_text = diagnosis_text->toPlainText();
    if (!current_text.isEmpty()){
        current_text[0] = current_text[0].toUpper();
        if (mkb_map_.GetMKBMap().find(current_text) != mkb_map_.GetMKBMap().end()) {
            for (int i = 0; i < 10; i++) {
                if (mkb_map_.GetMKBMap().find(current_text + '.' + QString::number(i)) != mkb_map_.GetMKBMap().end()) {
                    return;
                }
            }
            // Если код диагноза найден, заполняем поля
            ChangeText(diagnosis_text, mkb_map_.GetMKBMap().at(current_text) + ' ' +  current_text);
        }
    }
}

void DataBase::CheckDopAnalises()
{
    for(const auto& widget : widget_handler_.GetWidgetsFromTabName("Дополнительные анализы")){
        if (auto check_box = qobject_cast<QCheckBox*>(widget)) {
            if (check_box->isChecked()) {
                ui->AddAnalisesButton->setStyleSheet("border-bottom: 3px solid orange;");
                ui->AddAnalisesButton->setText("Есть доп. обследования");
                return;
            }
        } else if (auto combo_box = qobject_cast<QComboBox*>(widget)) {
            if (!combo_box->currentText().trimmed().isEmpty()) {
                ui->AddAnalisesButton->setStyleSheet("border-bottom: 3px solid orange;");
                ui->AddAnalisesButton->setText("Есть доп. обследования");
                return;
            }
        }
    }
    ui->AddAnalisesButton->setStyleSheet("border-bottom: 1px solid #2baaff;");
    ui->AddAnalisesButton->setText("Добавить обследования");

}

void DataBase::TryToParseVnesnyaPrichinaTravmi() {
    ChangeDiagnosisText(ui->VneshnyaPrichina);
}

void DataBase::TryToParseSoputstvyushieZabolevanie() {
    ChangeDiagnosisText(ui->SoputstvuyushieZabolevaniya);
}

void DataBase::TryToParseOsloznenie() {
    ChangeDiagnosisText(ui->Oslojneniya);
}

void DataBase::SetCompletters() {
    operation_list_completer_ = new QCompleter(mkb_map_.GetOperationsList(), this);
    operation_list_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    ui->OperationName->setCompleter(operation_list_completer_);
    ui->OperationName2->setCompleter(operation_list_completer_);

    connect(ui->BaseDisease, &QTextEdit::textChanged, this, &DataBase::TryToParseBaseDiagnosisAndMKB);
    connect(ui->VneshnyaPrichina, &QTextEdit::textChanged, this, &DataBase::TryToParseVnesnyaPrichinaTravmi);
    connect(ui->SoputstvuyushieZabolevaniya, &QTextEdit::textChanged, this, &DataBase::TryToParseSoputstvyushieZabolevanie);
    connect(ui->Oslojneniya, &QTextEdit::textChanged, this, &DataBase::TryToParseOsloznenie);
}

//Заполнение витальных показателей в зависимости от возраста
void DataBase::FillVitalIfKnownDateofBirth() {
    int age = GetAge();
    ui->Age->setText(QString::number(age));

    if (age < 18) {
        ui->Puls->setText(QString::number(static_cast<int>((135 - 4.0 * age - rand() % 7 - 3))));
        ui->SistolicAD->setText(QString::number(static_cast<int>(92 + 2.2 * age / 1.5 + rand() % 7 - 3)));
        ui->DiastolicAD->setText(QString::number(static_cast<int>((58 + 0.9 * age + rand() % 7 - 3))));
        ui->BreathRate->setText(QString::number(static_cast<int>((30 - age * 0.8 - rand() % 3 - 1))));
    }
}


void DataBase::UpdateCheckboxIfEditingText() {
    QCheckBox* check_box = widget_handler_.GetWidgetsAndCheckboxes()[sender()];
    if (auto line_edit = qobject_cast<QLineEdit*>(sender())) {
        if (IsAllLinesEmpty(check_box)) {
            check_box->setChecked(false);
        } else {
            check_box->setChecked(true);
        }
    } else if (auto text_edit = qobject_cast<QTextEdit*>(sender())) {
        if (text_edit->toPlainText().trimmed().isEmpty()) {
            check_box->setChecked(false);
        } else {
            check_box->setChecked(true);
        }
    }
}

bool DataBase::IsAllLinesEmpty(QCheckBox* check_box) {
    auto check_box_and_widgets = widget_handler_.GetCheckboxesAndWidgets();
    auto widgets = check_box_and_widgets[check_box];
    for (const auto& widget : widgets) {
        if (auto line_edit = qobject_cast<QLineEdit*>(widget)) {
            if(!line_edit->text().isEmpty()) {
                return false;
            }
        }
    }
    return true;
}

void DataBase::ConnectQObjectToCheckbox() {
    auto widgets_and_checkboxes = widget_handler_.GetWidgetsAndCheckboxes();
    for (auto& [widget, check_box] : widgets_and_checkboxes) {
        if (auto line_edit = qobject_cast<QLineEdit*>(widget)) {
            connect(line_edit, &QLineEdit::textChanged, this, &DataBase::UpdateCheckboxIfEditingText);
        } else if (auto text_edit = qobject_cast<QTextEdit*>(widget)) {
            connect(text_edit, &QTextEdit::textChanged, this, &DataBase::UpdateCheckboxIfEditingText);
        }
    }

}

void DataBase::ConnectDopAnalisesWidgetsToDopAnalisesButton()
{
    for(const auto& widget : widget_handler_.GetWidgetsFromTabName("Дополнительные анализы")){
        if (auto check_box = qobject_cast<QCheckBox*>(widget)) {
            // connect(check_box, &QCheckBox::checkStateChanged, this, &DataBase::ChangeDopAnalisesBotton);
            connect(check_box, &QCheckBox::stateChanged, this, &DataBase::ChangeDopAnalisesBotton);
        } else if (auto combo_box = qobject_cast<QComboBox*>(widget)) {
            connect(combo_box, &QComboBox::editTextChanged, this, &DataBase::ChangeDopAnalisesBotton);
        }
    }
}

const QString DataBase::GetDiagnosisContext(const QString& context) const {
    QString base_disease = ui->BaseDisease->toPlainText().toLower();
    auto index = base_disease.indexOf(context);
    if (index != -1) {
        index += context.size();
        //удаляем пробелы после слова перелом
        while (index < base_disease.size() && base_disease[index].isSpace()) {
            index++;
        }
        return base_disease.mid(index);
    }
    return "";
}


// double DataBase::CalculateDosage(double weight, std::pair<int, int> range){
//     // Пример расчета дозировки: берем среднее значение из диапазона
//     double result = (range.first + range.second) / 2.0 * weight / 1000.0;
//     if (result > 0.25) {
//         result = std::round(result / 0.25) * 0.25;
//     }
//     return result; // в граммах
// }

int DataBase::GetAge() {
    QDate currentDate = QDate::currentDate();
    int age = currentDate.year() - ui->BirthdayDate->date().year();
    // Если день рождения еще не наступил в текущем году, вычитаем 1 год
    if (currentDate < ui->BirthdayDate->date().addYears(age)) {
        age--;
    }
    return age;
}

void DataBase::TryToParseMedicineAdministrationMethod() {
    auto combobox = qobject_cast<QComboBox*>(sender());
    QString index = combobox->objectName()[combobox->objectName().length() - 1];

    if (ui->Age->text().isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Вы не ввели возраст, введите чтобы продолжить");
        ChangeCombobox(combobox, "");
        // combobox->clear();
        return;
    }
    if (ui->Weight->text().isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Вы не ввели вес, введите чтобы продолжить");
        ChangeCombobox(combobox, "");
        return;
    }
    double weight = ui->Weight->text().toDouble();
    if (weight == 0.) {
        QMessageBox::warning(this, "Внимание", "Вы ввели некорректный вес, введите чтобы продолжить");
        ChangeCombobox(combobox, "");
        return;
    }
    int age = GetAge();
    QString medicine_name = combobox->currentText().toLower();
    QString result = medicines_handler_.TryToParseDescription(medicine_name, weight, age);

    auto change_date = [&](){
        auto begin_date = ui->Medicines_and_fisio_tab->findChild<QDateEdit*>("MedicinePurposeDate" + index);
        auto date_end = ui->Medicines_and_fisio_tab->findChild<QDateEdit*>("MedicineCancelDate" + index);
        begin_date->setDate(ui->IncomeDate->date());
        date_end->setDate(ui->IncomeDate->date().addDays(3));
    };

    if (!result.isEmpty()) {
        auto line_edit = ui->Medicines_and_fisio_tab->findChild<QLineEdit*>("MethodOfAdministrationMedicine" + index);
        line_edit->setText(result);
        if (medicine_name == "аспаркам" || medicine_name == "ацетазоламид" || medicine_name == "этамзилат натрия") {
            change_date();
        }
    }

}



void DataBase::IsDataChangedFromLastPrint() {
    if (is_changed_from_last_print == false) {
        is_changed_from_last_print = true;
        ui->statusbar->showMessage("начальные данные изменены");
    }
}

void DataBase::ChangeNameToCorrect() {
    auto patient_name = ui->PatientFIO->text().trimmed();
    if (patient_name.isEmpty()) {
        return;
    }
    QString new_name;
    for (auto& name : patient_name.split(' ')) {
        if (!name.isEmpty()) {
            name = name.toLower();
            name[0] = name[0].toUpper();
            new_name.append(name + " ");
        }
    }
    ChangeLine(ui->PatientFIO, std::move(new_name));

}

void DataBase::ChangeDopAnalisesBotton() {
    if (QComboBox* combobox = qobject_cast<QComboBox*>(sender())) {
        if (combobox->currentText().trimmed().isEmpty()) {
            ui->AddAnalisesButton->setStyleSheet("border-bottom: 1px solid #2baaff;");
            ui->AddAnalisesButton->setText("Добавить обследования");
            return;
        }
    } else if (QCheckBox* checkbox = qobject_cast<QCheckBox*>(sender())){
        if (!checkbox->isChecked()) {
            ui->AddAnalisesButton->setStyleSheet("border-bottom: 1px solid #2baaff;");
            ui->AddAnalisesButton->setText("Добавить обследования");
            return;
        }
    }
    ui->AddAnalisesButton->setStyleSheet("border-bottom: 3px solid orange;");
    ui->AddAnalisesButton->setText("Есть доп. обследования");

}

void DataBase::ParseDescriptionToDopAnalises()
{
    QComboBox* combo_box = qobject_cast<QComboBox*>(sender());
    QString index = combo_box->objectName()[combo_box->objectName().length() - 1];
    if (combo_box->currentText().trimmed() == "коагулограмма") {
        auto dop_analises_text = dop_analises_window.findChild<QTextEdit*>("DopObsledovanieText" + index);
        dop_analises_text->setText("МНО 1.0; тромбиновое время 19 сек; ПТИ 81 %; АПТВ 33 сек; фибриноген 3.0 г/л");
    }
}

void DataBase::ConnectWidgetsToCheckIfDataChanged(){
    auto widgets = widget_handler_.GetAllWidgets();
    for (auto widget : widgets) {
        if (QDateEdit* dateEdit = qobject_cast<QDateEdit*>(widget)) {
            connect(dateEdit, &QDateEdit::dateChanged, this, &DataBase::IsDataChangedFromLastPrint);
        } else if (QDateTimeEdit* dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget)) {
            connect(dateTimeEdit, &QDateTimeEdit::dateTimeChanged, this, &DataBase::IsDataChangedFromLastPrint);
        } else if (QTextEdit* textEdit = qobject_cast<QTextEdit*>(widget)) {
            connect(textEdit, &QTextEdit::textChanged, this, &DataBase::IsDataChangedFromLastPrint);
        } else if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            connect(lineEdit, &QLineEdit::textChanged, this, &DataBase::IsDataChangedFromLastPrint);
        } else if (QComboBox* comboBox = qobject_cast<QComboBox*>(widget)) {
            connect(comboBox, &QComboBox::currentIndexChanged, this, &DataBase::IsDataChangedFromLastPrint);
        } else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget)) {
            connect(checkBox, &QCheckBox::stateChanged, this, &DataBase::IsDataChangedFromLastPrint);
        }
    }
}

void DataBase::ConnectDopAnalisesComboBoxesToDescriptionParser()
{
    for (int i = 1; i < Constants::NUMBER_OF_DOP_OBSLEDOVANIYA; i++) {
        auto combo_box = dop_analises_window.findChild<QComboBox*>("DopObsledovanieVariant" + QString::number(i));
        connect(combo_box, &QComboBox::currentTextChanged, this, &DataBase::ParseDescriptionToDopAnalises);
    }
}


void DataBase::on_OperationBegin_editingFinished() {
    ui->OperationEnd->setDateTime(ui->OperationBegin->dateTime());
}


void DataBase::on_OperationBegin2_editingFinished() {
    ui->OperationEnd2->setDateTime(ui->OperationBegin2->dateTime());
}


void DataBase::on_ManipulationVariant1_currentTextChanged(const QString &arg1)
{
    if (arg1 == "закрытая репозиция") {
        ui->ManipulationText1->setText("Произведена попытка закрытой репозиции " + GetDiagnosisContext("перелом") + ", пальпаторно и визуально ось конечности ровная, наложена гипсовая иммобилизация");
    } else if (arg1 == "пункция") {
        ui->ManipulationAnestesiya1->setCurrentIndex(1);
        ui->ManipulationText1->setText("Под местной анестезией 0,5 % раствором новокаина произведена пункция коленного сустава, аспирировано 100 мл геморрагического отделяемого");
    } else if (arg1 == "вправление вывиха") {
        ui->ManipulationText1->setText(GetDiagnosisContext("вывих"));
    } else if (arg1 == "закрытая репозиция, наложение скелетного вытяжения") {
        ui->ManipulationText1->setText("После трехкратной обработки операционного поля спиртовыми растворами антисептиков заведена спица Киршнера через");
    }
}


void DataBase::AddAnalisesButtonClicked() {
    dop_analises_window.show();
    dop_analises_window.raise();
    dop_analises_window.activateWindow();
}


void DataBase::on_GepatitiText1_textChanged(const QString &arg1) {
    if (arg1.startsWith(' ')) {
        ChangeLine(ui->GepatitiText1, "Вектоген B-HBs-антиген(-); Милалаб-ИФА-АНТИ-HCV(-);RW(-)");
    }
}

void DataBase::on_KalText1_textChanged(const QString &arg1) {
    if (arg1.startsWith(' ')) {
        ChangeLine(ui->KalText1, "энтеробиоз(-), яйца гельминтов(-)");
    }
}


void DataBase::on_EKGText_textChanged(const QString &arg1) {
    if (arg1.startsWith(' ')){
        ChangeLine(ui->EKGText, "Синусовый ритм, нормограмма");
    }
}

void DataBase::ChangeLine(QLineEdit* line_to_change, QString&& text) {
    line_to_change->blockSignals(true);
    line_to_change->setText(std::move(text));
    line_to_change->blockSignals(false);
}

void DataBase::ChangeText(QTextEdit* text_to_change, QString&& text) {
    text_to_change->blockSignals(true);
    text_to_change->setPlainText(std::move(text));
    text_to_change->blockSignals(false);
}

void DataBase::ChangeCombobox(QComboBox* combobox_to_change, QString&& text) {
    combobox_to_change->blockSignals(true);
    combobox_to_change->setCurrentText(std::move(text));
    combobox_to_change->blockSignals(false);
}



void DataBase::on_Weight_editingFinished() {
    ui->Weight->blockSignals(true);
    ui->Weight->setText(ui->Weight->text().replace(',','.'));
    ui->Weight->blockSignals(false);
}


void DataBase::PrintButtonClicked(){
    if (ui->DoctorName->currentText() == "не выбрано") {
        QMessageBox::warning(this, "Внимание", "Вы не выбрали лечащего доктора");
        return;
    }
    if (ui->Adress->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Вы не ввели адрес");
        return;
    }
    if (ui->Weight->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Вы не ввели вес");
        return;
    }
    if (ui->Height->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Вы не ввели рост");
        return;
    }
    if (is_changed_from_last_print) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Внимание", "Вы хотите сохранить текущий случай?",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            SaveButtonClicked();
        }
    }
    is_changed_from_last_print = false;
    print_window->show();
}


void DataBase::on_action_triggered()
{
    QMessageBox::information(this, "Информация об изготовителе", "Данная программа является собственностью Иванова Ильи Сергеевича, контактный email: firefrog48@yandex.ru\nконтактный телефон Whatsapp 8-980-547-19-58");
}

void DataBase::on_copy_triggered() {
    patient_id_.reset();
    ui->ListNetrudosposobnosti->setText("не нуждается");
    ui->Height->setText("");
    ui->Weight->setText("");
    ui->Adress->setText("");
    ui->Allergies->setText("отрицает");
    ui->Age->setText("");
    ui->AnamnesVitae->setText("ребенок от нормально протекавшей беременности и родов. "
                               "Растёт и развивается соответственно возрасту. Болезнью Боткина не болел. "
                               "Активного туберкулеза в семье нет. Прививки по календарю. Внутривенные гемотрансфузии не производились. На диспансерном учете не состоит.");
    ui->HystoryNumber->setText("");
    ui->PatientFIO->setText("");
    ui->BloudGroup->setCurrentText("не определялась");
    ui->KelFactor->setCurrentText("не определялся");
    ui->OperationNumber->setText("");
    ui->OperationNumber2->setText("");
}


// void DataBase::on_ParseDiaries_button_clicked()
// {
//     auto diaries = diaries_parser_.ParseDiearies();
//     int diary_counter = 1;
//     for (const auto& diary : diaries) {
//         QComboBox *combobox = new QComboBox(this);
//         combobox->setObjectName("Diary" + QString::number(diary_counter++));
//         ui->diaries_scroll_area.set
//         QDateTimeEdit* diary_date = new QDateTimeEdit(diary.date_);
//         QLabel* diary_date_lable = new QLabel("Дата");
//         layout->addWidget(diary_date_lable);
//         layout->addWidget(diary_date);
//     }

// }

