include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )


##### nmcommon (static) - NetworkManager client, secret agent, wallet

tde_add_library( nmcommon STATIC_PIC AUTOMOC
  SOURCES
    nmdbus.cpp
    nmclient.cpp
    nmtext.cpp
    authorization.cpp
    secretstore.cpp
    secretagent.cpp
    passworddialog.cpp
    nmctlsettings.cpp
  LINK
    tdeio-shared
    tdeui-shared
    tdecore-shared
    ${DBUS_TQT_LIBRARIES}
    ${POLKIT_TQT_LIBRARIES}
)
