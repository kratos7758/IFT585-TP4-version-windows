cmake_minimum_required(VERSION 3.16)
project(client_ift585 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Chercher Qt5 (ou Qt6 en fallback)
find_package(Qt5 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt5_FOUND)
    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    set(QT_LIB Qt6::Core Qt6::Gui Qt6::Widgets)
else()
    set(QT_LIB Qt5::Core Qt5::Gui Qt5::Widgets)
endif()

# Activer le MOC automatique (nécessaire pour les classes Qt avec Q_OBJECT)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Sources du client
set(SRCS
    main.cpp
    ClientApp.cpp
    NetworkProvider.cpp
    SyncEngine.cpp
    SurveillanceLocale.cpp
    InterfaceUtilisateur.cpp
    ../common/sha256.cpp
)

add_executable(client_ift585 ${SRCS})

target_include_directories(client_ift585 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(client_ift585 PRIVATE ${QT_LIB})

# Flags de compilation
if(MSVC)
    target_compile_options(client_ift585 PRIVATE /W3 /O2)
    # Éviter le conflit min/max avec NOMINMAX (déjà dans platform.h)
    target_compile_definitions(client_ift585 PRIVATE NOMINMAX)
else()
    target_compile_options(client_ift585 PRIVATE -Wall -Wextra -O2)
endif()

# Librairies
if(WIN32)
    target_link_libraries(client_ift585 PRIVATE ws2_32)
else()
    target_link_libraries(client_ift585 PRIVATE pthread)
endif()

# Répertoire de sortie
set_target_properties(client_ift585 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
)
