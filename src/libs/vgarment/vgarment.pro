#-------------------------------------------------
#
# VGarment static library: everything 3D about the sewn garment that doesn't draw anything itself.
# Pattern pieces turned into triangle meshes now; the avatar and the drape simulation later.
# Uses the Delaunay triangulation from VObj, so whatever links VGarment has to link VObj after it.
#
#-------------------------------------------------

# File with common stuff for whole project
message("Entering vgarment.pro")
include(../../../common.pri)

# gui-private for Qt's rendering hardware interface, which runs the cloth solver on the graphics card.
QT += widgets printsupport concurrent gui-private

# Name of library
TARGET = vgarment

# We want create a library
TEMPLATE = lib

CONFIG += staticlib # Making static library

include(vgarment.pri)

# This is static library so no need in "make install"

# directory for executable file
DESTDIR = bin

# files created moc
MOC_DIR = moc

# objecs files
OBJECTS_DIR = obj

include(warnings.pri)

include (../libs.pri)
