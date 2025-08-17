# Compiler configuration
CC:=gcc
CFLAGS:=-Iinclude/ -Ilibs/libpartman/include/ -Werror -Wall -ansi -pedantic -Wno-long-long
LDLIBS:=

# Local libraries configuration
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
DBGCFLAGS:=-g -O0 -DDEBUG

# Utility commands
rm:=rm -rf
mkdir:=mkdir -p

.PHONY: all check clean debug release

all: release

# Release rules
release: $(RELTARGET)

$(RELTARGET): $(RELOBJS) $(RELLIBS)
	$(CC) $(CFLAGS) $(RELCFLAGS) $^ $(LDLIBS) -o $@

$(RELDIR)/$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(@D)
	$(CC) $(CFLAGS) $(RELCFLAGS) -c $^ $(LDLIBS) -o $@

# Debug rules
debug: $(DBGTARGET)

$(DBGTARGET): $(DBGOBJS) $(DBGLIBS)
	$(CC) $(CFLAGS) $(DBGCFLAGS) $^ $(LDLIBS) -o $@

$(DBGDIR)/$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(@D)
	$(CC) $(CFLAGS) $(DBGCFLAGS) -c $^ $(LDLIBS) -o $@

# Local libraries rules
$(RELLIBS): .FORCE
	$(MAKE) -C $(dir $(patsubst %/,%,$(dir $@))) \
		$(addprefix $(notdir $(patsubst %/,%,$(dir $@)))/, $(notdir $@))
$(DBGLIBS): .FORCE
	$(MAKE) -C $(dir $(patsubst %/,%,$(dir $@))) \
		$(addprefix $(notdir $(patsubst %/,%,$(dir $@)))/, $(notdir $@))

# Other rules
clean:
	@$(rm) $(DBGDIR) $(RELDIR)

.FORCE:

