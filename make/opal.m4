dnl
dnl OPAL macros
dnl

dnl Assumes my_macros.m4 has been incldued in configure.ac


dnl OPAL_SIMPLE_OPTION
dnl Change a given variable according to arguments and subst and define it
dnl $1 name of configure option
dnl $2 the variable to change, subst and define
dnl $3 the configure argument description
dnl $4..$6 dependency variable(s)
AC_DEFUN([OPAL_SIMPLE_OPTION],[
   MY_ARG_ENABLE(
      [$1],
      [$3],
      [$$2],
      [
         AC_SUBST($2, yes)
         AC_DEFINE([$2], [1], [$3])
      ],
      [$2=no],
      [$4],
      [$5],
      [$6]
   )
])


dnl OPAL_CHECK_PTLIB_OPTION
dnl Check if has a specified option
dnl $1 Name of feature
dnl $2 Variable to set & define
dnl $3 Error message
AC_DEFUN([OPAL_CHECK_PTLIB_OPTION],[
   AC_MSG_CHECKING([PTLib for $1])
   OPAL_$2=`pkg-config --variable=$2 ptlib`
   AC_MSG_RESULT([${OPAL_$2}])
   AC_SUBST(OPAL_$2)

   m4_ifnblank([$3],[
      if test "x$OPAL_$2" != "xyes" ; then
         AC_MSG_ERROR([$3])
      fi
  ])
])


dnl OPAL_MODULE_OPTION
dnl as MY_MODULE_OPTION but defines OPAL_xxx for make and define
AC_DEFUN([OPAL_MODULE_OPTION],[
   MY_MODULE_OPTION([$1],[$2],[$3],[$4],[$5],[$6],[$7],[$8],[$9],[$10],[$11],[$12],[$13],[$14])
   if test "x$usable" = "xyes" ; then
      AC_DEFINE(OPAL_$1, 1)
   fi
   AC_SUBST(OPAL_$1, $usable)
])


dnl OPAL_DETERMINE_PLUGIN_DIR
dnl Determine plugin install directory
dnl $OPAL_VERSION
dnl $PLUGIN_DIR
AC_DEFUN([OPAL_DETERMINE_PLUGIN_DIR],[
   AC_MSG_CHECKING(where plugins are being installed)

   AC_ARG_WITH(
      [plugin-installdir],
      AS_HELP_STRING([--with-plugin-installdir=DIR],[Location where plugins are installed]),
      [OPAL_PLUGIN_DIR="\${libdir}/$withval"],
      [
         if test "x$OPALDIR" != "x" && test "x$prefix" = "xNONE" ; then
            OPAL_PLUGIN_DIR="$OPALDIR/lib_$target/plugins"
         else
            OPAL_PLUGIN_DIR="\${libdir}/opal-${OPAL_VERSION}"
         fi
      ]
   )

   AC_MSG_RESULT("$OPAL_PLUGIN_DIR")
   AC_SUBST(OPAL_PLUGIN_DIR)
])


dnl OPAL_SIMPLE_PLUGIN
dnl $1 module name
dnl $2 command line name
dnl $3 command line help
dnl $4 source directory
AC_DEFUN([OPAL_SIMPLE_PLUGIN],[
   if test -d "$4" ; then
      AC_ARG_ENABLE(
         [$2],
         [AC_HELP_STRING([--disable-$2],[disable $3])],
         [
            if test "x$enableval" = "xno" ; then
               HAVE_$1="no (disabled by user)"
            else
               HAVE_$1="yes"
            fi
         ],
         [
            if test "x${HAVE_$1}" = "x" ; then
               HAVE_$1="yes"
            fi
         ]
      )
      if test "x$HAVE_$1" = "xyes" ; then
         PLUGIN_SUBDIRS="$PLUGIN_SUBDIRS $4"
      fi
   else
      HAVE_$1="no (missing directory $4)"
   fi
])


dnl OPAL_SYSTEM_PLUGIN
dnl as MY_MODULE_OPTION but defines OPAL_xxx for make and define
AC_DEFUN([OPAL_SYSTEM_PLUGIN],[
   if test -d "$4" ; then
      dnl MY_MODULE_OPTION adds to LIBS and normally this right, but for plugins
      dnl we want each plugin to only have its libs so don't add it for everyone
      OPAL_SYSTEM_PLUGIN_LIBS="$LIBS"
      MY_MODULE_OPTION([$1],[$2],[$3],[$5],[$6],[$7],[$8],[$9],[$10],[$11],[$12],[$13],[$14],[$15])
      LIBS="$OPAL_SYSTEM_PLUGIN_LIBS"
      if test "x$$1[_SYSTEM]" = "xno" ; then
         if test "x$usable" = "xyes" ; then
            PLUGIN_SUBDIRS="$PLUGIN_SUBDIRS $4"
            HAVE_$1="yes (internal)"
         else
            HAVE_$1="no (internal library incompatible with build environment)"
         fi
      else
         if test "x$usable" = "xyes" ; then
            AC_SUBST($1[_CFLAGS])
            AC_SUBST($1[_LIBS])
            PLUGIN_SUBDIRS="$PLUGIN_SUBDIRS $4"

            if test "x$$1[_SYSTEM]" = "xyes" ; then
               HAVE_$1="yes (system)"
            else
               HAVE_$1="yes"
            fi
         else
            HAVE_$1="no (package $5 not found)"
         fi
      fi
   else
      HAVE_$1="no (missing directory $4)"
   fi
])


