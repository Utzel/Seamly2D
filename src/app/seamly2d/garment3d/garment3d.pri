# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for correct working file translations.pro

SOURCES += \
    $$PWD/avatar_geometry.cpp \
    $$PWD/garment_scene_model.cpp \
    $$PWD/garment_view_widget.cpp \
    $$PWD/piece_geometry.cpp \
    $$PWD/seam_editor.cpp \
    $$PWD/seam_geometry.cpp

HEADERS += \
    $$PWD/avatar_geometry.h \
    $$PWD/garment_scene_model.h \
    $$PWD/garment_view_widget.h \
    $$PWD/piece_geometry.h \
    $$PWD/seam_editor.h \
    $$PWD/seam_geometry.h

RESOURCES += \
    $$PWD/garment3d.qrc

# The deploy tools scan this folder to find the QML modules the scene imports.
GARMENT3D_QML_DIR = $$PWD
