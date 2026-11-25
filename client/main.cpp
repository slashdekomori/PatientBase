#include "GUI/database.h"

#include <QApplication>
#include <QFontDatabase>
#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
// #include <QSharedMemory>
// #include <QSystemSemaphore>
#include <QTimer>


int main(int argc, char *argv[])
{


    QApplication a(argc, argv);
    const QString fontDir = QCoreApplication::applicationDirPath() + "/Sources_files/";
    QFontDatabase::addApplicationFont(fontDir + "NotoSans-Regular.ttf");
    QFontDatabase::addApplicationFont(fontDir + "NotoSans-Bold.ttf");
    // const QString uniqueKey = "Demon48Lipetsk.ODB";
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "PatientBase_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

    // QSystemSemaphore semaphore(uniqueKey, 1);
    // semaphore.acquire();

    // QSharedMemory sharedMemory(uniqueKey);
    // if (sharedMemory.attach()) {
    //     // Если attach успешен, значит, приложение уже запущено
    //     semaphore.release(); // Освобождаем семафор
    //     QMessageBox::warning(nullptr, "Внимание", "Программа уже запущена.");
    //     return 0; // Завершаем выполнение
    // }

    // sharedMemory.create(1);

    DataBase db;
    db.showMaximized();



    // sharedMemory.detach();
    // semaphore.release();
    return a.exec();
}
