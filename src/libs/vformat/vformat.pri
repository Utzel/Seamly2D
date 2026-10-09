# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for corect working file translations.pro

SOURCES += \
    $$PWD/fabric_file.cpp \
    $$PWD/measurements.cpp \
    $$PWD/svg_generator.cpp \
    $$PWD/vlabeltemplate.cpp

*msvc*:SOURCES += $$PWD/stable.cpp

HEADERS += \
    $$PWD/fabric_file.h \
    $$PWD/measurements.h \
    $$PWD/stable.h \
    $$PWD/svg_generator.h \
    $$PWD/vlabeltemplate.h
