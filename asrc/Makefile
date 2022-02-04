#
## use with: GNU make 
#
TARGET1 = A500KBConfig 
#TARGET2 = FindExp

PREFIX  = /opt/amigaos-68k/os-include/
RM	= rm -f
CC	= m68k-amigaos-gcc 
CFLAGS	= -noixemul -Wall -O2 -fomit-frame-pointer -msmall-code $(DEFINES)
#CFLAGS += -m68020 -m68881
LDFLAGS	= -s -noixemul -lamiga
VASM    = vasmm68k_mot
VASMFLAGS = -Faout -quiet -I$(PREFIX) 

OBJS	= startup.o utils.o config.o cx_main.o window.o \
          capsimage.o pledimage.o pledbutton.o ledmanager.o \
	  ciacomm.o savereq.o
HEADERS = config.h version.h

all: $(TARGET1) 
#$(TARGET2)

$(TARGET1): $(OBJS) 
	$(CC) $(LDFLAGS) -nostartfiles -o $(TARGET1) $(OBJS)

#$(TARGET2): findexp.o
#	$(CC) $(LDFLAGS) -nostartfiles -o $(TARGET2) findexp.o

clean:
	-$(RM) $(TARGET1) $(OBJS) 
# $(TARGET2) findexp.o $(OBJS) 


# dependencies
%.o:	%.c $(HEADERS)
	$(CC) -c $(CFLAGS) -o $@ $<

%.o:	%.s
	$(VASM) $(VASMFLAGS) -o $@ $<
