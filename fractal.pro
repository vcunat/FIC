TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .

CONFIG += qt debug_and_release warn_on

# Input
HEADERS += *.h FerrisLoki/*.h modules/*.h
SOURCES += *.cpp modules/*.cpp
PRECOMPILED_HEADER = headers.h
#FORMS = settings.ui colors.ui
#TRANSLATIONS = lang-cs_CZ.ts

QMAKE_CLEAN += doxygen/html/*

CONFIG(debug,debug|release) {
	TARGET = debug/$$TARGET
} else {
	TARGET = release/$$TARGET
}

QMAKE_CXXFLAGS_DEBUG *= -ggdb 
#QMAKE_CXXFLAGS_RELEASE -= -ggdb

QMAKE_CXXFLAGS_RELEASE *= -ffunction-sections -msse2
contains(QMAKE_CC,gcc) {
	QMAKE_CXXFLAGS_RELEASE *= -frepo
}
contains(QMAKE_CC,icc) {
	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE *= -O3 -march=pentium4
}

# profiling support
#QMAKE_CXXFLAGS_RELEASE *= -ggdb -pg
#QMAKE_LFLAGS_RELEASE *= -ggdb -pg
