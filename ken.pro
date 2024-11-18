QMAKE_CXXFLAGS += -std=c++11
QMAKE_CFLAGS += -std=gnu11

HEADERS += decoderkenfactory.h \
           decoder_ken.h \
           kenhelper.h \
           libken/adlib.h \
           libken/common.h \
           libken/kdmdecoder.h \
           libken/ksmdecoder.h \
           libken/smdecoder.h

SOURCES += decoderkenfactory.cpp \
           decoder_ken.cpp \
           kenhelper.cpp \
           libken/adlib.cpp \
           libken/kdmdecoder.cpp \
           libken/ksmdecoder.cpp \
           libken/smdecoder.cpp

INCLUDEPATH += $$PWD/libken

#CONFIG += BUILD_PLUGIN_INSIDE
contains(CONFIG, BUILD_PLUGIN_INSIDE){
    include($$PWD/../../plugins.pri)
    TARGET = $$PLUGINS_PREFIX/Input/ken

    unix{
        target.path = $$PLUGIN_DIR/Input
        INSTALLS += target
    }
}else{
    QT += widgets
    CONFIG += warn_off plugin lib thread link_pkgconfig c++11
    TEMPLATE = lib

    unix{
        equals(QT_MAJOR_VERSION, 5){
            QMMP_PKG = qmmp-1
        }else:equals(QT_MAJOR_VERSION, 6){
            QMMP_PKG = qmmp
        }else{
            error("Unsupported Qt version: 5 or 6 is required")
        }
        
        PKGCONFIG += $${QMMP_PKG}

        PLUGIN_DIR = $$system(pkg-config $${QMMP_PKG} --variable=plugindir)/Input
        INCLUDEPATH += $$system(pkg-config $${QMMP_PKG} --variable=prefix)/include

        plugin.path = $${PLUGIN_DIR}
        plugin.files = lib$${TARGET}.so
        INSTALLS += plugin
    }
}
