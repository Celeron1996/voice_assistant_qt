QT += core gui widgets

TARGET = VoiceAssistantQt
TEMPLATE = app
CONFIG += c++11

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    spectrumwidget.cpp \
    assistantprocess.cpp

HEADERS += \
    mainwindow.h \
    spectrumwidget.h \
    assistantprocess.h

# ========== IMX6ULL Qt 头文件路径（与 photo_album.pro 保持一致） ==========
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5/QtWidgets
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5/QtCore

# ARM 交叉编译工具链
QMAKE_CC  = arm-buildroot-linux-gnueabihf-gcc
QMAKE_CXX = arm-buildroot-linux-gnueabihf-g++
QMAKE_LINK = arm-buildroot-linux-gnueabihf-g++

QMAKE_CXXFLAGS += -Wno-all -Wno-psabi
