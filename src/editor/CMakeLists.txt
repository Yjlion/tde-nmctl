include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )

##### nmeditor (static) - connection editor, device list, settings

tde_add_library( nmeditor STATIC_PIC AUTOMOC
  SOURCES
    settingpage.cpp
    eapwidget.cpp
    pages_general.cpp
    pages_wired.cpp
    pages_wifi.cpp
    pages_virtual.cpp
    pages_vpn.cpp
    ippage.cpp
    connectiondialog.cpp
    connectionlist.cpp
    devicelist.cpp
    mainwidget.cpp
  LINK
    nmcommon-static
    tdeio-shared
    tdeui-shared
    tdecore-shared
)
