cmake_minimum_required(VERSION 3.0)

cmake_minimum_required(VERSION 3.10)

if(NOT VERSION)
  set(VERSION "0.2.1")
endif()

if(NOT BUILD_VERSION)
  set(BUILD_VERSION "1")
endif()

project(
  JackFreqD 
  VERSION ${VERSION} 
  DESCRIPTION "Cpu frequency daemon based on the jack DSP load"
  LANGUAGES C
)

# executable

include(FindPkgConfig)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(jackfreqd src/jackfreqd.c src/jack_cpu_load.c src/procps.c)
target_link_libraries(jackfreqd PkgConfig::JACK)
target_link_libraries(jackfreqd Threads::Threads)

install(TARGETS jackfreqd DESTINATION "bin")

# man page
install(FILES man/jackfreqd.1 DESTINATION "man/man1")

# service
install(CODE "configure_file(\"${PROJECT_SOURCE_DIR}/packaging/jackfreq.service.in\" \"\$ENV\{DESTDIR\}\$\{CMAKE_INSTALL_PREFIX\}/lib/systemd/system/jackfreq.service\")")

# packaging
set(CPACK_PACKAGE_NAME "jackfreqd")
set(CPACK_PACKAGE_CONTACT "osamarin68@google.com")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_RELEASE "${BUILD_VERSION}")
set(CPACK_STRIP_FILES ON)
set(CPACK_SOURCE_IGNORE_FILES "/.git/" "/build/" "/nbproject/private/")

set(CPACK_GENERATOR RPM DEB)

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE "${CPACK_PACKAGE_RELEASE}")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE "${CPACK_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)

