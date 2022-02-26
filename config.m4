PHP_ARG_ENABLE(solib,
  [whether to enable solib rust sapi library],
  [  --enable-solib[=TYPE]   Enable building of solib Rust SAPI library [TYPE=shared-zts]
                           Options: shared, static, shared-zts and static-zts (zts = thread-safe)  
  ], no, no
)

AC_MSG_CHECKING([for solib Rust SAPI library support])

SAPI_SOLIB_PATH=sapi/solib/solib-rust.so

SAPI_SHARED=libs/solib$PHP_MAJOR_VERSION.$SHLIB_DL_SUFFIX_NAME
SAPI_STATIC=libs/solib$PHP_MAJOR_VERSION.a
SAPI_LIBTOOL=solib$PHP_MAJOR_VERSION.la
OVERALL_TARGET=libs/solib$PHP_MAJOR_VERSION.la

if test "$PHP_SOLIB" != "no"; then
  case "$PHP_SOLIB" in
    shared)
      PHP_SOLIB_TYPE=shared
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0755 $SAPI_SHARED \$(INSTALL_ROOT)\$(prefix)/lib"
      PHP_SELECT_SAPI(solib, shared, sapi_solib.c)
      ;;
    static)
      PHP_SOLIB_TYPE=static
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0644 $SAPI_STATIC \$(INSTALL_ROOT)\$(prefix)/lib"
      PHP_SELECT_SAPI(solib, static, sapi_solib.c)
      ;;
    yes|shared-zts)
      PHP_SOLIB_TYPE=shared-zts
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0755 $SAPI_SHARED \$(INSTALL_ROOT)\$(prefix)/lib $SAPI_SHARED"
      PHP_BUILD_THREAD_SAFE
      PHP_SELECT_SAPI(solib, shared, sapi_solib.c)
      SAPI_SHARED=libs/solib$PHP_MAJOR_VERSION.$SHLIB_DL_SUFFIX_NAME
      SAPI_STATIC=libs/solib$PHP_MAJOR_VERSION.a
      SAPI_LIBTOOL=solib$PHP_MAJOR_VERSION.la
      OVERALL_TARGET=solib$PHP_MAJOR_VERSION.la
      PHP_ADD_MAKEFILE_FRAGMENT([$abs_srcdir/sapi/solib/Makefile.frag], [$abs_srcdir/sapi/solib], [$abs_builddir/sapi/solib])	
      ;;
    static-zts)
      PHP_SOLIB_TYPE=static-zts
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0644 $SAPI_STATIC \$(INSTALL_ROOT)\$(prefix)/lib"
      PHP_BUILD_THREAD_SAFE
      PHP_SELECT_SAPI(solib, static, sapi_solib.c)
      ;;
    *)
      PHP_SOLIB_TYPE=no
      ;;
  esac
  AC_MSG_RESULT([$PHP_SOLIB_TYPE])
else
  AC_MSG_RESULT(no)
fi
