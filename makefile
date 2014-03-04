CC=gcc

NAME = recog~

PDPATH?=~/Software/pd-0-45.4/src

LINUXSPHINX = $(shell pkg-config --cflags --libs pocketsphinx sphinxbase)

LIBS = -L/usr/local/lib -Lusr/local/lib64 -L/lib -L/lib64 -L/usr/lib -L/usr/lib64 

LIBS+= $(shell pkg-config --libs pocketsphinx sphinxbase) -ldl

PDPATH?=../../src

current: pd_linux
LINUXINCLUDE=-I../../src
LINUXINCLUDE+=-I$(PDPATH)
LINUXINCLUDE+= $(shell pkg-config --cflags --libs pocketsphinx sphinxbase)

-DMODELDIR= $(shell pkg-config --variable=modeldir pocketsphinx)
# ----------------------- NT -----------------------

pd_nt: $(NAME).dll

.SUFFIXES: .dll

PDNTCFLAGS = /W3 /WX /DPD /DMSW /nologo
VC="C:\Program Files\Microsoft Visual Studio\Vc98"

PDNTINCLUDE = /I. /I..\..\src /I ../pd/src/ /I$(VC)\include

PDNTLDIR = $(VC)\lib
PDNTLIB = $(PDNTLDIR)\libc.lib \
	$(PDNTLDIR)\oldnames.lib \
	$(PDNTLDIR)\kernel32.lib \
	..\pd\bin\pd.lib 

.c.dll:
	cl $(PDNTCFLAGS) $(PDNTINCLUDE) /c $*.c
	link /dll /export:$(CSYM)_setup $*.obj $(PDNTLIB)

# ----------------------- IRIX 5.x -----------------------

pd_irix5: $(NAME).pd_irix5

.SUFFIXES: .pd_irix5

SGICFLAGS5 = -o32 -DPD -DUNIX -DIRIX -O2

SGIINCLUDE =  -I../../src

.c.pd_irix5:
	$(CC) $(SGICFLAGS5) $(SGIINCLUDE) -o $*.o -c $*.c
	ld -elf -shared -rdata_shared -o $*.pd_irix5 $*.o
	rm $*.o

# ----------------------- IRIX 6.x -----------------------

pd_irix6: $(NAME).pd_irix6

.SUFFIXES: .pd_irix6

SGICFLAGS6 = -n32 -DPD -DUNIX -DIRIX -DN32 -woff 1080,1064,1185 \
	-OPT:roundoff=3 -OPT:IEEE_arithmetic=3 -OPT:cray_ivdep=true \
	-Ofast=ip32

.c.pd_irix6:
	$(CC) $(SGICFLAGS6) $(SGIINCLUDE) -o $*.o -c $*.c
	ld -n32 -IPA -shared -rdata_shared -o $*.pd_irix6 $*.o
	rm $*.o

# ----------------------- LINUX i386 -----------------------

pd_linux: $(NAME).pd_linux

.SUFFIXES: .pd_linux
#-DPD -O2 -funroll-loops -fomit-frame-pointer
LINUXCFLAGS = -fPIC -Bsymbolic-functions  \
    -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch 

.c.pd_linux:
	$(CC) $(LINUXCFLAGS) $(LINUXINCLUDE)\
	-o $*.o -c $*.c $(LIBS);
	ld -export_dynamic -shared -Bsymbolic -o $*.pd_linux $*.o $(LIBS) -lc -lm
	strip --strip-unneeded $*.pd_linux


# ----------------------- Mac OSX -----------------------

pd_darwin: $(NAME).pd_darwin

.SUFFIXES: .pd_darwin

DARWINCFLAGS = -DPD -O2 -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch

.c.pd_darwin:
	$(CC) $(DARWINCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	$(CC) -bundle -undefined suppress -flat_namespace -o $*.pd_darwin $*.o 
	rm -f $*.o

# ----------------------------------------------------------

clean:
	rm -f *.o *.pd_* so_locations









