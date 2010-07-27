# Choose the right MinGW prefix
if (CMAKE_SYSTEM_NAME STREQUAL Windows)
set(MINGW_PREFIX "")
else()
set(MINGW_PREFIX "mingw32-")
endif()

# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)
SET(CMAKE_SYSTEM_PROCESSIR i686)

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER ${MINGW_PREFIX}gcc)
SET(CMAKE_CXX_COMPILER ${MINGW_PREFIX}g++)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
