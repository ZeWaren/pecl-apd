top_srcdir   = /home/george/src/apd
top_builddir = /home/george/src/apd
srcdir       = /home/george/src/apd/
builddir     = /home/george/src/apd/
VPATH        = /home/george/src/apd/
#
# To compile the utility programs:
#
# This must point to the directory containing libphp4.la (typically
# the top level of your PHP source tree). PHP must also be statically
# compiled (specified "--enable-static" when configuring PHP).
#
PHP_SRC_DIR =

LTLIBRARY_NAME			= libapd.la
LTLIBRARY_SOURCES		= php_apd.c apd_lib.c opcode.c apd_stack.c
LTLIBRARY_SHARED_NAME	= php_apd.la
LTLIBRARY_SHARED_LIBADD	= $(TRACE_SHARED_LIBADD)

include $(top_srcdir)/build/dynlib.mk
