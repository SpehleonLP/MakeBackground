
CONFIG -= qt

CONFIG += c++14

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
	/mnt/Passport/Libraries/squish-1.11 \
	/mnt/Passport/Libraries/glfw-3.3/include/ \
	/mnt/Passport/Libraries/nativefiledialog/src/include/ \
	/mnt/Passport/Libraries/Boxer/include/ \
	/mnt/Passport/Libraries/fx-gltf/test/thirdparty/ \
	/mnt/Passport/Libraries/basis_universal/ \
	/mnt/Passport/Libraries/fx-gltf/src/ \
	/mnt/Passport/Libraries/

LIBS += -pthread -lpng \
	-L"/mnt/Passport/Libraries/squish-1.11" -lsquish \
	-L\"/mnt/Passport/Libraries/lz4/build/cmake\" -llz4 \
	`pkg-config --libs gtk+-3.0`

QMAKE_CXXFLAGS += `pkg-config --cflags gtk+-3.0`
QMAKE_CFLAGS += `pkg-config --cflags gtk+-3.0`

SOURCES += \
    /mnt/Passport/Libraries/Boxer/src/boxer_linux.cpp \
    /mnt/Passport/Libraries/fx-gltf/src/fx/gltf.cpp \
    /mnt/Passport/Libraries/nativefiledialog/src/nfd_common.c \
    /mnt/Passport/Libraries/nativefiledialog/src/nfd_gtk.c \
    main.cpp \
    src/alphafile.cpp \
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
    src/gltffile.cpp \
    src/height_mask.cpp \
    src/linearimage.cpp \
    src/png_file.cpp

HEADERS += \
    /mnt/Passport/Libraries/fx-gltf/src/fx/gltf.h \
    /mnt/Passport/Libraries/fx-gltf/test/thirdparty/nlohmann/json.hpp \
    /mnt/Passport/Libraries/nativefiledialog/src/common.h \
    /mnt/Passport/Libraries/nativefiledialog/src/include/nfd.h \
    /mnt/Passport/Libraries/nativefiledialog/src/nfd_common.h \
    /mnt/Passport/Libraries/nativefiledialog/src/simple_exec.h \
    src/UniqueCPtr.h \
    src/alphafile.h \
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
    src/glm_iostream.hpp \
    src/gltffile.h \
    src/height_mask.h \
    src/linearimage.h \
    src/png_file.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
	/mnt/Passport/Libraries/nativefiledialog/src/nfd_cocoa.m
