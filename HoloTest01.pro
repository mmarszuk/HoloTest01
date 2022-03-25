QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui


############################
# Intel One API Dependency #
############################

# Intel One Api dependency - required for MCFFT
# Intel One Api dependency - required for MCFFT
win32:CONFIG(release, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lippcore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lippcore

win32:CONFIG(release, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lippi
else:win32:CONFIG(debug, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lippi

win32:CONFIG(release, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lipps
else:win32:CONFIG(debug, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lipps

win32:CONFIG(release, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lippvm
else:win32:CONFIG(debug, debug|release): LIBS += -L$$(ONEAPI_ROOT)/ipp/latest/lib/intel64/ -lippvm

INCLUDEPATH += $$(ONEAPI_ROOT)
DEPENDPATH += $$(ONEAPI_ROOT)


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
