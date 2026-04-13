cmake_minimum_required(VERSION 3.16)
project(server_ift585 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources du serveur
set(SRCS
    main.cpp
    ServerCore.cpp
    AuthUDPHandler.cpp
    RESTServer.cpp
    PersistenceManager.cpp
    ../common/sha256.cpp
)

add_executable(server_ift585 ${SRCS})

target_include_directories(server_ift585 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# Flags de compilation
if(MSVC)
    target_compile_options(server_ift585 PRIVATE /W3 /O2)
else()
    target_compile_options(server_ift585 PRIVATE -Wall -Wextra -O2)
endif()

# Librairies
if(WIN32)
    # Winsock2 est géré via #pragma comment(lib, ...) dans platform.h
    # mais on peut aussi le lier explicitement ici
    target_link_libraries(server_ift585 PRIVATE ws2_32)
else()
    target_link_libraries(server_ift585 PRIVATE pthread)
endif()

# Répertoire de sortie
set_target_properties(server_ift585 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
