# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for correct working file translations.pro

SOURCES += \
    $$PWD/avatar_dialog.cpp \
    $$PWD/avatar_geometry.cpp \
    $$PWD/drape_runner.cpp \
    $$PWD/elastic_editor.cpp \
    $$PWD/fabric_dialog.cpp \
    $$PWD/flow_layout.cpp \
    $$PWD/fold_editor.cpp \
    $$PWD/garment_scene_model.cpp \
    $$PWD/garment_view_widget.cpp \
    $$PWD/piece_geometry.cpp \
    $$PWD/seam_editor.cpp \
    $$PWD/seam_geometry.cpp \
    $$PWD/shown_piece.cpp \
    $$PWD/stitch_editor.cpp \
    $$PWD/stitch_geometry.cpp

HEADERS += \
    $$PWD/avatar_dialog.h \
    $$PWD/avatar_geometry.h \
    $$PWD/drape_runner.h \
    $$PWD/elastic_editor.h \
    $$PWD/fabric_dialog.h \
    $$PWD/flow_layout.h \
    $$PWD/fold_editor.h \
    $$PWD/garment_scene_model.h \
    $$PWD/garment_view_widget.h \
    $$PWD/piece_geometry.h \
    $$PWD/seam_editor.h \
    $$PWD/seam_geometry.h \
    $$PWD/shown_piece.h \
    $$PWD/stitch_editor.h \
    $$PWD/stitch_geometry.h

RESOURCES += \
    $$PWD/garment3d.qrc

# The deploy tools scan this folder to find the QML modules the scene imports.
GARMENT3D_QML_DIR = $$PWD
