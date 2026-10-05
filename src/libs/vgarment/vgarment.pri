# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for correct working file translations.pro

SOURCES += \
    $$PWD/body_data.cpp \
    $$PWD/body_fitter.cpp \
    $$PWD/body_measurer.cpp \
    $$PWD/body_model.cpp \
    $$PWD/garment_mesh.cpp \
    $$PWD/piece_mesher.cpp \
    $$PWD/piece_outline.cpp \
    $$PWD/seam_stretch.cpp

*msvc*:SOURCES += $$PWD/stable.cpp

HEADERS += \
    $$PWD/body_data.h \
    $$PWD/body_fitter.h \
    $$PWD/body_measurer.h \
    $$PWD/body_model.h \
    $$PWD/garment_mesh.h \
    $$PWD/piece_mesher.h \
    $$PWD/piece_outline.h \
    $$PWD/seam_stretch.h \
    $$PWD/stable.h

# The avatar's body, built by scripts/avatar/build_avatar_data.py. BodyData::standard() initializes it.
RESOURCES += \
    $$PWD/share/avatar/avatar.qrc
