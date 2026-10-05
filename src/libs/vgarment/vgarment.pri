# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for correct working file translations.pro

SOURCES += \
    $$PWD/garment_mesh.cpp \
    $$PWD/piece_mesher.cpp

*msvc*:SOURCES += $$PWD/stable.cpp

HEADERS += \
    $$PWD/garment_mesh.h \
    $$PWD/piece_mesher.h \
    $$PWD/stable.h
