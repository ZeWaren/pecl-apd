dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(apd, for apd support,
dnl Make sure that the comment is aligned:
dnl [  --with-apd             Include apd support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(apd, whether to enable apd support,
     [  --enable-apd            Enable apd support])

if test "$PHP_APD" != "no"; then
  dnl If you will not be testing anything external, like existence of
  dnl headers, libraries or functions in them, just uncomment the 
  dnl following line and you are ready to go.
  AC_DEFINE(HAVE_APD, 1, [ ])

  dnl Write more examples of tests here...
  PHP_EXTENSION(apd, $ext_shared)
fi

