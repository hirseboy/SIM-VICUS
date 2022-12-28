# Project file for DummyDatabasePlugin

TARGET        = SVDummyDatabasePlugin
TEMPLATE      = lib
CONFIG       += plugin
QT           += widgets

# this pri must be sourced from all our applications
include( ../Qt/plugin.pri )

LIBS += \
	-lVicus \
	-lNandrad \
	-lIBKMK \
	-lIBK \
	-lTiCPP

INCLUDEPATH  += \
	../../SIM-VICUS/src \
	../../SIM-VICUS/src/plugins \
	../../externals/Vicus/src \
	../../externals/IBK/src \
	../../externals/IBKMK/src \
	../../externals/TiCPP/src \
	../../externals/Nandrad/src

HEADERS       = src/DummyDatabasePlugin.h
SOURCES       = src/DummyDatabasePlugin.cpp

DISTFILES += data/metadata.json
