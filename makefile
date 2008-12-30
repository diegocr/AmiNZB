
ifdef DEBUG
DBGTAG	= _debug
DEBUG	= -g -DDEBUG
else
DEBUG	= -fomit-frame-pointer
endif

PROG	= AmiNZB$(SYS)$(DBGTAG)
CC	= m68k-amigaos-gcc $(CPU) -noixemul  -msmall-code
LIB	= -Wl,-Map,$@.map,--cref -lmui #-ldebug -lamiga
DEFINES	= -DSTACK_SIZE=48000 # -DVERSION='"0.1"'
WARNS	= -W -Wall #-Winline
OPTS	= -O3 -funroll-loops
CFLAGS	=  $(OPTS) $(WARNS) $(DEFINES) $(DEBUG)
LDFLAGS	= #-nostdlib

OBJDIR	= .objs$(SYS)$(DBGTAG)
RM	= rm -frv

OBJS =	\
	$(OBJDIR)/main.o	\
	$(OBJDIR)/muiface.o	\
	$(OBJDIR)/ipc.o		\
	$(OBJDIR)/util.o	\
	$(OBJDIR)/thread.o	\
	$(OBJDIR)/analyzer.o	\
	$(OBJDIR)/logo.o	\
	$(OBJDIR)/nntp.o	\
	$(OBJDIR)/nntp_list.o	\
	$(OBJDIR)/http.o	\
	$(OBJDIR)/decoders.o	\
	$(OBJDIR)/downler.o	\
	$(OBJDIR)/wndp.o	\
	$(OBJDIR)/debug.o

build:
	make all SYS=_060 CPU="-m68060 -m68881"
#	make $(PROG)_020 SYS=_020 CPU=-m68020

all: $(PROG)

$(PROG): $(OBJDIR) $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIB)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c
	@echo Compiling $@
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(PROG)_0*0 $(OBJDIR)*


ty:
	$(CC) $(CFLAGS) -g -DDEBUG -DTEST_YENC -Winline decoders.c -o test_yenc

