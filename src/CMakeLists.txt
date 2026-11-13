include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/nm
  ${CMAKE_CURRENT_SOURCE_DIR}/editor
  ${CMAKE_CURRENT_SOURCE_DIR}/tray
  ${TDE_INCLUDE_DIR}
  ${TQT_INCLUDE_DIRS}
  ${DBUS_TQT_INCLUDE_DIRS}
  ${DBUS_INCLUDE_DIRS}
  ${POLKIT_TQT_INCLUDE_DIRS}
)

link_directories(
  ${TQT_LIBRARY_DIRS}
  ${TDE_LIB_DIR}
  ${DBUS_TQT_LIBRARY_DIRS}
  ${POLKIT_TQT_LIBRARY_DIRS}
)

add_subdirectory( nm )
add_subdirectory( editor )
add_subdirectory( app )
add_subdirectory( tray )
