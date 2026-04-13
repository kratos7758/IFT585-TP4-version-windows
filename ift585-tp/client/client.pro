# =============================================================
#  client.pro – Fichier de projet Qt5 pour le client IFT585-TP
# =============================================================
QT       += core gui widgets
TARGET    = client_ift585
TEMPLATE  = app
CONFIG   += c++17

# Sources
SOURCES += \
    main.cpp \
    ClientApp.cpp \
    NetworkProvider.cpp \
    SyncEngine.cpp \
    SurveillanceLocale.cpp \
    InterfaceUtilisateur.cpp \
    ../common/sha256.cpp

HEADERS += \
    ClientApp.h \
    NetworkProvider.h \
    SyncEngine.h \
    SurveillanceLocale.h \
    InterfaceUtilisateur.h \
    ../common/sha256.h \
    ../common/json.h \
    ../common/FileMetadata.h \
    ../common/UDPProtocol.h

# Flags de compilation
QMAKE_CXXFLAGS += -std=c++17 -Wall -Wextra -O2

# Librairies selon la plateforme
win32: LIBS += -lws2_32
else:  LIBS += -lpthread

# Répertoire de sortie
DESTDIR = ../bin
