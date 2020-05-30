
CONFIG -= qt

CONFIG += c++14

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
	../../Libraries/squish-1.11 \
	../../Libraries/glfw-3.3/include/ \
	../../Libraries/nativefiledialog/src/include/ \
	../../Libraries/Boxer/include/ \
	../../Libraries/fx-gltf/test/thirdparty/ \
	../../Libraries/basis_universal/

LIBS += -pthread -L"/mnt/Passport/Libraries/squish-1.11" -lsquish -lpng -lz `pkg-config --libs gtk+-3.0`

QMAKE_CXXFLAGS += `pkg-config --cflags gtk+-3.0`
QMAKE_CFLAGS += `pkg-config --cflags gtk+-3.0`

SOURCES += \
    ../../Libraries/Boxer/src/boxer_linux.cpp \
    ../../Libraries/nativefiledialog/src/nfd_common.c \
    ../../Libraries/nativefiledialog/src/nfd_gtk.c \
    main.cpp \
    src/alpha_mask.cpp \
    src/backgroundexception.cpp \
    src/backgroundfile.cpp \
    src/backgroundlayer.cpp \
    src/blur_config.cpp \
    src/blurheightmap.cpp \
    src/ddsfile.cpp \
    src/depthfile.cpp \
    src/filebase.cpp \
    src/generatenormals.cpp \
    src/generateocclusion.cpp \
    src/height_mask.cpp \
    src/huffmantree.cpp \
    src/linearimage.cpp \
    src/png_file.cpp

HEADERS += \
    ../../Libraries/nativefiledialog/src/common.h \
    ../../Libraries/nativefiledialog/src/include/nfd.h \
    ../../Libraries/nativefiledialog/src/nfd_common.h \
    ../../Libraries/nativefiledialog/src/simple_exec.h \
    src/UniqueCPtr.h \
    src/alpha_mask.h \
    src/backgroundexception.h \
    src/backgroundfile.h \
    src/backgroundlayer.h \
    src/bg_type.h \
    src/blur_config.h \
    src/blurheightmap.h \
    src/dds_header.h \
    src/ddsfile.h \
    src/depthfile.h \
    src/filebase.h \
    src/generatenormals.h \
    src/generateocclusion.h \
    src/height_mask.h \
    src/huffmantree.h \
    src/linearimage.h \
    src/png_file.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
	../../Libraries/nativefiledialog/src/nfd_cocoa.m
