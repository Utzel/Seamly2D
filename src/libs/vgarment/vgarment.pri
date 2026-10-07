# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for correct working file translations.pro

SOURCES += \
    $$PWD/body_collider.cpp \
    $$PWD/body_data.cpp \
    $$PWD/body_fitter.cpp \
    $$PWD/body_measurer.cpp \
    $$PWD/body_model.cpp \
    $$PWD/body_wrap.cpp \
    $$PWD/cloth_compute.cpp \
    $$PWD/cloth_solver.cpp \
    $$PWD/compute_device.cpp \
    $$PWD/fabric.cpp \
    $$PWD/garment_export.cpp \
    $$PWD/garment_fit.cpp \
    $$PWD/garment_mesh.cpp \
    $$PWD/garment_symmetry.cpp \
    $$PWD/limb_line.cpp \
    $$PWD/piece_mesher.cpp \
    $$PWD/piece_outline.cpp \
    $$PWD/seam_stretch.cpp \
    $$PWD/topstitch.cpp

*msvc*:SOURCES += $$PWD/stable.cpp

HEADERS += \
    $$PWD/body_collider.h \
    $$PWD/body_data.h \
    $$PWD/body_fitter.h \
    $$PWD/body_measurer.h \
    $$PWD/body_model.h \
    $$PWD/body_wrap.h \
    $$PWD/cloth_compute.h \
    $$PWD/cloth_solver.h \
    $$PWD/compute_device.h \
    $$PWD/fabric.h \
    $$PWD/garment_export.h \
    $$PWD/garment_fit.h \
    $$PWD/garment_mesh.h \
    $$PWD/garment_symmetry.h \
    $$PWD/limb_line.h \
    $$PWD/piece_mesher.h \
    $$PWD/piece_outline.h \
    $$PWD/seam_stretch.h \
    $$PWD/topstitch.h \
    $$PWD/stable.h

# The avatar's body, built by scripts/avatar/build_avatar_data.py, which BodyData::standard() initializes; and the
# cloth solver's compute shaders, baked from their GLSL below by scripts/shaders/bake_shaders.py, which ClothCompute
# initializes.
RESOURCES += \
    $$PWD/share/avatar/avatar.qrc \
    $$PWD/shaders/cloth_shaders.qrc

OTHER_FILES += \
    $$PWD/shaders/cloth_accelerate.comp \
    $$PWD/shaders/cloth_contacts.comp \
    $$PWD/shaders/cloth_sweep.comp
