path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_LEDS),yes)
ledsobj = leds.o
endif

OBJS = \
 button.o \
 checkbox.o \
 dialog.o \
 group.o \
 hierbox.o \
 hotkey.o \
 inpfield.o \
 inphist.o \
 $(ledsobj) \
 listbox.o \
 listmenu.o \
 menu.o \
 msgbox.o \
 style.o \
 text.o \
 widget.o

all-local: libbfu.a
libbfu.a: $(OBJS)

include $(path_to_top)/Makefile.lib
