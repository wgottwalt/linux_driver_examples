CMAKE_MINIMUM_REQUIRED  (VERSION 3.0)
PROJECT                 (ppstool LANGUAGES CXX)

# includes and paths/directories
SET                     (CMAKE_INCLUDE_CURRENT_DIR on)
INCLUDE_DIRECTORIES     (${PROJECT_SOURCE_DIR}/sources)
SET                     (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
SET                     (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugin)
SET                     (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

# compiler settings
IF                      (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    MESSAGE             ("--> building on Linux")
    SET                 (CMAKE_CXX_FLAGS "-std=c++11 -pthread -flto -fanalyzer -pedantic -fPIC -W -Wall -Wextra -Os -s")
    SET                 (CMAKE_C_FLAGS "-std=c11 -pthread -flto -fanalyzer -pedantic -fPIC -W -Wall -Wextra -Os -s")
ELSE                    ()
    MESSAGE             (FATAL_ERROR "--> unsupported build host: ${CMAKE_SYSTEM_NAME}")
ENDIF                   ()

# sources
ADD_SUBDIRECTORY        (sources)
