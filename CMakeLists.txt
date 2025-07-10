cmake_minimum_required(VERSION 3.14)

project(DisconnectionManagement VERSION 1.0 LANGUAGES CXX)

include(GNUInstallDirs)

# Dipendenze AWS
find_package(aws-crt-cpp REQUIRED)

# Crea la libreria
add_library(${PROJECT_NAME} SHARED
    src/DisconnectionManagement.cpp
)

# Includi le intestazioni pubbliche
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link alle librerie AWS
target_link_libraries(${PROJECT_NAME}
    PUBLIC AWS::aws-crt-cpp
)

# Imposta lo standard
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# Installazione della libreria
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Installa la cartella include (opzionale ma utile se ci sono subfolder)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Configura l'esportazione per altri progetti (facoltativo ma consigliato)
include(CMakePackageConfigHelpers)

# Version file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/DisconnectionManagementConfigVersion.cmake"
  VERSION 1.0.0
  COMPATIBILITY AnyNewerVersion
)

# Config file
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/DisconnectionManagementConfig.cmake"
  INSTALL_DESTINATION lib/cmake/DisconnectionManagement
)

# Install everything
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/DisconnectionManagementConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/DisconnectionManagementConfigVersion.cmake"
  DESTINATION lib/cmake/DisconnectionManagement
)

# Esporta i target installati
install(EXPORT DisconnectionManagementTargets
  FILE DisconnectionManagementTargets.cmake
  NAMESPACE DisconnectionManagement::
  DESTINATION lib/cmake/DisconnectionManagement
)

