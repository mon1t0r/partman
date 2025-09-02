CC:=gcc
CFLAGS:=-Wall -Iinclude/ -Ilibs/libpartman/include/
STATIC:=-static

# Libraries common for release/debug
LDLIBS:=

# Release/debug specific libraries
RELLIBS:=libs/libpartman/release/libpartman.a
DBGLIBS:=libs/libpartman/debug/libpartman.a

# Target file name
TARGET:=partman

# Source and object files configuration
SRCDIR:=src
OBJDIR:=obj

SRCS:=$(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/*/*.c)
OBJS:=$(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Release configuration
RELDIR:=release
RELTARGET:=$(RELDIR)/$(TARGET)
RELOBJS:=$(addprefix $(RELDIR)/, $(OBJS))
RELCFLAGS:=-O3

# Debug configuration
DBGDIR:=debug
DBGTARGET:=$(DBGDIR)/$(TARGET)
DBGOBJS:=$(addprefix $(DBGDIR)/, $(OBJS))
DBGCFLAGS:=-O0 -Werror -Wno-long-long -ansi -pedantic -g -DDEBUG

# Utility commands
rm:=rm -rf
mkdir:=mkdir -p

.PHONY: all release debug clean .FORCE

all: release

# Release rules
release: $(RELTARGET)

$(RELTARGET): $(RELOBJS) $(RELLIBS)
	$(CC) $(CFLAGS) $(RELCFLAGS) $(STATIC) $^ $(LDLIBS) -o $@

$(RELDIR)/$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(@D)
	$(CC) $(CFLAGS) $(RELCFLAGS) -c $^ $(LDLIBS) -o $@

# Debug rules
debug: $(DBGTARGET)

$(DBGTARGET): $(DBGOBJS) $(DBGLIBS)
	$(CC) $(CFLAGS) $(DBGCFLAGS) $(STATIC) $^ $(LDLIBS) -o $@

$(DBGDIR)/$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(@D)
	$(CC) $(CFLAGS) $(DBGCFLAGS) -c $^ $(LDLIBS) -o $@

# Local libraries rules
$(RELLIBS):
	$(MAKE) -C $(dir $(patsubst %/,%,$(dir $@))) \
		$(addprefix $(notdir $(patsubst %/,%,$(dir $@)))/, $(notdir $@))
$(DBGLIBS): .FORCE
	$(MAKE) -C $(dir $(patsubst %/,%,$(dir $@))) \
		$(addprefix $(notdir $(patsubst %/,%,$(dir $@)))/, $(notdir $@))

# Other rules
clean:
	@$(rm) $(RELDIR) $(DBGDIR) $(dir $(RELLIBS)) $(dir $(DBGLIBS))

.FORCE:

