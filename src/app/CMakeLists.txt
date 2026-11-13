include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )

##### kcm_nmctl (Control Center and System Settings module)

tde_add_kpart( kcm_nmctl AUTOMOC
  SOURCES
    kcm_nmctl.cpp
  LINK
    nmeditor-static
    nmcommon-static
    tdeio-shared
    tdeui-shared
    tdecore-shared
  DESTINATION ${PLUGIN_INSTALL_DIR}
)


##### tde-nmctl (connection editor)

tde_add_executable( tde-nmctl AUTOMOC
  SOURCES
    main.cpp
    mainwindow.cpp
  LINK
    nmeditor-static
    nmcommon-static
    tdeio-shared
    tdeui-shared
    tdecore-shared
  DESTINATION ${BIN_INSTALL_DIR}
)

install( FILES tde-nmctlui.rc DESTINATION ${DATA_INSTALL_DIR}/tde-nmctl )

# Control Center: Internet & Network; System Settings: Network Settings
tde_create_translated_desktop(
  SOURCE nmctl.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR}
)

tde_create_translated_desktop(
  SOURCE tde-nmctl.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR}
)
