# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for correct working file translations.pro

SOURCES += \
    $$PWD/garment_scene_model.cpp \
    $$PWD/garment_view_widget.cpp \
    $$PWD/piece_geometry.cpp

HEADERS += \
    $$PWD/garment_scene_model.h \
    $$PWD/garment_view_widget.h \
    $$PWD/piece_geometry.h

RESOURCES += \
    $$PWD/garment3d.qrc

# The deploy tools scan this folder to find the QML modules the scene imports.
GARMENT3D_QML_DIR = $$PWD
