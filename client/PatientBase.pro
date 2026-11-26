QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Windows: embed the icon into PatientBase.exe
win32: RC_ICONS = icon.ico

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Common/common.cpp \
    Common/departmentinfostorage.cpp \
    Common/diariesparser.cpp \
    GUI/addanaliseswindow.cpp \
    GUI/database.cpp \
    GUI/printwindow.cpp \
    GUI/searchwindow.cpp \
    GUI/settingswindow.cpp \
    JSON/json.cpp \
    JSON/json_builder.cpp \
    JSON/json_printer.cpp \
    SQLTable/sqltable.cpp \
    main.cpp

HEADERS += \
    Common/common.h \
    Common/departmentinfostorage.h \
    Common/diariesparser.h \
    GUI/addanaliseswindow.h \
    GUI/database.h \
    GUI/printwindow.h \
    GUI/searchwindow.h \
    GUI/settingswindow.h \
    JSON/json.h \
    JSON/json_builder.h \
    JSON/json_printer.h \
    SQLTable/sqltable.h

FORMS += \
    GUI/addanaliseswindow.ui \
    GUI/database.ui \
    GUI/printwindow.ui \
    GUI/searchwindow.ui \
    GUI/settingswindow.ui

RESOURCES += \
    resources.qrc


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    Sources_files/DepartmentInfo.json \
    Sources_files/DiagnosesMKB.txt \
    Sources_files/DiagnosesMKB_travma.txt \
    Sources_files/OperationsMKB.txt
