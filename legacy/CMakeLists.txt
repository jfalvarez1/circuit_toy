cmake_minimum_required(VERSION 3.16)
project(CircuitPlayground VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find SDL2
find_package(SDL2 REQUIRED)

# Platform-specific settings
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Source files
set(SOURCES
    src/main.c
    src/app.c
    src/circuit.c
    src/circuits.c
    src/component.c
    src/simulation.c
    src/matrix.c
    src/render.c
    src/ui.c
    src/input.c
    src/file_io.c
    src/analysis.c
)

# Header files
set(HEADERS
    include/app.h
    include/circuit.h
    include/circuits.h
    include/component.h
    include/simulation.h
    include/matrix.h
    include/render.h
    include/ui.h
    include/input.h
    include/file_io.h
    include/types.h
    include/analysis.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL2_LIBRARIES}
    m  # Math library
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE mingw32 SDL2main)
endif()

# Copy resources to build directory
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR}/bin)

# Install rules
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY resources/ DESTINATION bin/resources)

# CPack for creating installers
set(CPACK_PACKAGE_NAME "CircuitPlayground")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Circuit Playground Simulator")
set(CPACK_PACKAGE_VENDOR "Circuit Playground")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Circuit Playground")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)
