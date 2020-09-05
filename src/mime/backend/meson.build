if conf_data.get('CONFIG_MAILCAP')
	srcs += files('mailcap.c')
endif
if conf_data.get('CONFIG_MIMETYPES')
	srcs += files('mimetypes.c')
endif

srcs += files('common.c', 'default.c')

##TEST_PROGS = \
## mailcap-cache$(EXEEXT)

# The dependencies are a bit funny here! I don't know why. Just remember to
# make clean before making the test. --jonas
##mailcap-cache.o: $(top_srcdir)/src/mime/backend/mailcap.c
##	$(call cmd,compile,-DTEST_MAILCAP)

##TESTDEPS = \
## common.o \
## $(top_builddir)/src/osdep/osdep.o

##TESTDEPS-$(CONFIG_NLS)	 += $(top_builddir)/src/intl/gettext/lib.o
