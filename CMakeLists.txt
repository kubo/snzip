cmake_minimum_required(VERSION 3.11)

project(snzip VERSION 1.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SNZIP_SNAPPY_DIR "" CACHE STRING "snappy intallation directory")
set(DEFAULT_FORMAT framing2_format CACHE STRING "default format")
set(PACKAGE_STRING "${CMAKE_PROJECT_NAME} ${PROJECT_VERSION}")

include(CheckIncludeFile)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(byteswap.h HAVE_BYTESWAP_H)

include(CheckSymbolExists)
check_symbol_exists(posix_fadvise "fcntl.h" HAVE_POSIX_FADVISE)
check_symbol_exists(futimens "sys/stat.h" HAVE_FUTIMENS)
check_symbol_exists(futimes "sys/time.h" HAVE_FUTIMES)
check_symbol_exists(getc_unlocked "stdio.h" HAVE_GETC_UNLOCKED)
check_symbol_exists(putc_unlocked "stdio.h" HAVE_PUTC_UNLOCKED)
check_symbol_exists(fread_unlocked "stdio.h" HAVE_FREAD_UNLOCKED)
check_symbol_exists(fwrite_unlocked "stdio.h" HAVE_FWRITE_UNLOCKED)
check_symbol_exists(ferror_unlocked "stdio.h" HAVE_FERROR_UNLOCKED)
check_symbol_exists(feof_unlocked "stdio.h" HAVE_FEOF_UNLOCKED)
check_symbol_exists(_getc_nolock "stdio.h" HAVE__GETC_NOLOCK)
check_symbol_exists(_putc_nolock "stdio.h" HAVE__PUTC_NOLOCK)
check_symbol_exists(_fread_nolock "stdio.h" HAVE__FREAD_NOLOCK)
check_symbol_exists(_fwrite_nolock "stdio.h" HAVE__FWRITE_NOLOCK)
check_symbol_exists(getopt "unistd.h" HAVE_GETOPT)

include(CheckStructHasMember)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_mtimensec sys/stat.h HAVE_STRUCT_STAT_ST_MTIMENSEC LANGUAGE C)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_mtim.tv_nsec sys/stat.h HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC LANGUAGE C)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_mtimespec.tv_nsec sys/stat.h HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC LANGUAGE C)

include(CheckCCompilerFlag)
check_c_compiler_flag(-msse4.2 HAVE_MSSE4_2_C_FLAG)
check_c_compiler_flag(/arch:AVX HAVE_ARCH_AVX_C_FLAG)

set(SSE4_2_code "
#include <nmmintrin.h>
int main() { _mm_crc32_u8(0, 0); _mm_crc32_u32(0, 0); return 0;}
")

if(HAVE_MSSE4_2_C_FLAG)
  string(APPEND CMAKE_C_FLAGS " -msse4.2")
  check_c_source_compiles("${SSE4_2_code}" HAVE_SSE4_2)
endif()

if(HAVE_ARCH_AVX_C_FLAG)
  string(APPEND CMAKE_C_FLAGS " /arch:AVX")
  check_c_source_compiles("${SSE4_2_code}" HAVE_SSE4_2)
endif()

configure_file(cmake_config.h.in config.h)

set(SNZIP_SOURCES
  comment-43-format.c
  crc32.c
  crc32.h
  framing-format.c
  framing2-format.c
  hadoop-snappy-format.c
  iwa-format.c
  raw_format.cpp
  snappy-in-java-format.c
  snappy-java-format.c
  snzip-format.c
  snzip.c
  snzip.h
)
if(HAVE_SSE4_2)
  list(APPEND SNZIP_SOURCES crc32_sse4_2.c)
endif()

if(NOT HAVE_GETOPT)
  list(APPEND SNZIP_SOURCES win32/ya_getopt.c)
endif()

include(FetchContent)
FetchContent_Populate(
  snappy
  URL        https://github.com/google/snappy/archive/refs/tags/1.1.9.zip
  URL_HASH   MD5=1ecaa4c5c662c2d9cb669669d22c28aa
)
option(SNAPPY_BUILD_TESTS OFF)
option(SNAPPY_BUILD_BENCHMARKS OFF)
add_subdirectory("${snappy_SOURCE_DIR}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

add_executable(snzip ${SNZIP_SOURCES})
target_include_directories(snzip PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DHAVE_CONFIG_H)
target_link_libraries(snzip PRIVATE snappy)
