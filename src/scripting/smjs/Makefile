top_builddir=../../..
include $(top_builddir)/Makefile.config

INCLUDES += $(SPIDERMONKEY_CFLAGS)

OBJS = smjs.obj core.obj global_object.obj hooks.obj elinks_object.obj cache_object.obj \
       view_state_object.obj action_object.obj bookmarks.obj globhist.obj \
       keybinding.obj load_uri.obj session_object.obj terminal_object.obj

include $(top_srcdir)/Makefile.lib
