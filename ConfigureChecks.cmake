###########################################
#                                         #
#  This file is released under GPL >= 2   #
#                                         #
###########################################


# required stuff
find_package( TQt )
find_package( TDE )

tde_setup_compiler_flags( )
tde_setup_largefiles( )


##### check for gcc visibility support

if( WITH_GCC_VISIBILITY )
  tde_setup_gcc_visibility( )
endif( WITH_GCC_VISIBILITY )


##### dbus-1-tqt (TQt bindings for D-Bus) - talks to NetworkManager

pkg_search_module( DBUS_TQT dbus-1-tqt )
if( NOT DBUS_TQT_FOUND )
  tde_message_fatal( "dbus-1-tqt is required but was not found on your system" )
endif( )

pkg_search_module( DBUS dbus-1 )
if( NOT DBUS_FOUND )
  tde_message_fatal( "dbus-1 is required but was not found on your system" )
endif( )


##### polkit-tqt - shows whether the user may change system connections

if( WITH_POLKIT )
  pkg_search_module( POLKIT_TQT polkit-tqt-core )
  if( NOT POLKIT_TQT_FOUND )
    tde_message_fatal( "polkit-tqt is required (libpolkit-tqt-dev), or configure with -DWITH_POLKIT=OFF" )
  endif( )
  set( HAVE_POLKIT_TQT 1 )
endif( )
