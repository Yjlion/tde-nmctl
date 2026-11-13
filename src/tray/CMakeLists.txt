include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )

##### tde-nmctl-tray (network status in kicker's system tray, secret agent)

tde_add_executable( tde-nmctl-tray AUTOMOC
  SOURCES
    traymain.cpp
    nmtray.cpp
    traypopup.cpp
    connector.cpp
    infodialog.cpp
  LINK
    nmeditor-static
    nmcommon-static
    tdeio-shared
    tdeui-shared
    tdecore-shared
  DESTINATION ${BIN_INSTALL_DIR}
)

install( FILES eventsrc DESTINATION ${DATA_INSTALL_DIR}/tde-nmctl-tray )

tde_create_translated_desktop(
  SOURCE tde-nmctl-tray.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR}
)

# started at login unless the user turns it off (see the
# X-TDE-autostart-condition in the file)
tde_create_translated_desktop(
  SOURCE tde-nmctl-tray-autostart.desktop
  DESTINATION ${AUTOSTART_INSTALL_DIR}
)
