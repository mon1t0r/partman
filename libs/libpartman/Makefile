CC:=gcc
CFLAGS:=-Wall -Iinclude/

AR:=ar
ARFLAGS:=rcs

# Target file name
TARGET:=libpartman.a

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

.PHONY: all clean check debug release

all: release

# Release rules
release: $(RELTARGET)

$(RELTARGET): $(RELOBJS)
	$(AR) $(ARFLAGS) -o $@ $^

$(RELDIR)/$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(@D)
	$(CC) $(CFLAGS) $(RELCFLAGS) -c $^ $(LDLIBS) -o $@

# Debug rules
debug: $(DBGTARGET)

$(DBGTARGET): $(DBGOBJS)
	$(AR) $(ARFLAGS) -o $@ $^

$(DBGDIR)/$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(@D)
	$(CC) $(CFLAGS) $(DBGCFLAGS) -c $^ $(LDLIBS) -o $@

# Other rules
clean:
	@$(rm) $(DBGDIR) $(RELDIR)

