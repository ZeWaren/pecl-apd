#  +----------------------------------------------------------------------+
#  | PHP version 4.0                                                      |
#  +----------------------------------------------------------------------+
#  | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
#  +----------------------------------------------------------------------+
#  | This source file is subject to version 2.02 of the PHP license,      |
#  | that is bundled with this package in the file LICENSE, and is        |
#  | available at through the world-wide-web at                           |
#  | http://www.php.net/license/2_02.txt.                                 |
#  | If you did not receive a copy of the PHP license and are unable to   |
#  | obtain it through the world-wide-web, please send a note to          |
#  | license@php.net so we can mail you a copy immediately.               |
#  +----------------------------------------------------------------------+
#  | Authors: Sascha Schumann <sascha@schumann.cx>                        |
#  +----------------------------------------------------------------------+
#
# $Id$ 
#

include $(top_srcdir)/build/rules_common.mk

install_targets = install-modules

all: all-recursive
install: install-recursive

distclean-recursive depend-recursive clean-recursive all-recursive install-recursive:
	@otarget=`echo $@|sed s/-recursive//`; \
	list='$(SUBDIRS)'; for i in $$list; do \
		target="$$otarget"; \
		echo "Making $$target in $$i"; \
		if test "$$i" = "."; then \
			ok=yes; \
			target="$$target-p"; \
		fi; \
		(cd $$i && $(MAKE) $$target) || exit 1; \
	done; \
	if test "$$otarget" = "all" && test -z '$(targets)'; then ok=yes; fi; \
	if test "$$ok" != "yes"; then $(MAKE) "$$otarget-p" || exit 1; fi

all-p: $(targets)
install-p: $(targets) $(install_targets)
distclean-p depend-p clean-p:

depend: depend-recursive
	@echo $(top_srcdir) $(top_builddir) $(srcdir) $(CPP) $(INCLUDES) $(EXTRA_INCLUDES) $(DEFS) $(CPPFLAGS) $(srcdir)/*.c *.c | $(AWK) -f $(top_srcdir)/build/mkdep.awk > $(builddir)/.deps || true

clean: clean-recursive clean-x

clean-x:
	rm -f $(targets) *.lo *.slo *.la *.o $(CLEANFILES)
	rm -rf .libs

distclean: distclean-recursive clean-x
	rm -f config.cache config.log config.status config_vars.mk libtool \
	config.h stamp-h Makefile build-defs.h php4.spec libphp4.module

cvsclean:
	@for i in `find . -name .cvsignore`; do \
		(cd `dirname $$i` 2>/dev/null && rm -rf `cat .cvsignore` *.o *.a || true); \
	done
	@rm -f $(SUBDIRS) 2>/dev/null || true

install-modules:
	@test -d modules && \
	$(mkinstalldirs) $(moduledir) && \
	echo "installing shared modules into $(moduledir)" && \
	rm -f modules/*.la && \
	cp modules/* $(moduledir) || true

include $(builddir)/.deps

.PHONY: all-recursive clean-recursive install-recursive \
$(install_targets) install all clean depend depend-recursive shared \
distclean-recursive distclean clean-x all-p install-p distclean-p \
depend-p clean-p
