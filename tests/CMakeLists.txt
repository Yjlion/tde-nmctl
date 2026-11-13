include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/nm
  ${TDE_INCLUDE_DIR}
  ${TQT_INCLUDE_DIRS}
  ${DBUS_TQT_INCLUDE_DIRS}
)

link_directories(
  ${TQT_LIBRARY_DIRS}
  ${TDE_LIB_DIR}
  ${DBUS_TQT_LIBRARY_DIRS}
  ${POLKIT_TQT_LIBRARY_DIRS}
)

##### clienttest - talks to tests/mock-nm.py (not installed)

tde_add_executable( clienttest AUTOMOC
  SOURCES
    clienttest.cpp
  LINK
    nmcommon-static
    tdeio-shared
    tdeui-shared
    tdecore-shared
)
