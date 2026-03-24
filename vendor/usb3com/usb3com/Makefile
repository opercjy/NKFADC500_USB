NOTICE_LIB = $(NKHOME)/lib
NOTICE_INC = $(NKHOME)/include

INSTALL_LIB = $(NKHOME)/lib
INSTALL_INC = $(NKHOME)/include

SrcSuf = c
HdrSuf = h
ObjSuf = o
DllSuf = so
OutPutOpt = -o #
CFLAGS = -I. -I$(LIBUSB_INC) -I$(NOTICE_INC) -Wall -Wundef -Wunused -Wstrict-prototypes -Werror-implicit-function-declaration -Wno-pointer-sign -Wshadow -O2 -fPIC -m64
SOFLAGS = -shared -Wall
LDFLAGS = $(SOFLAGS)
LDLIBS = -L$(NOTICE_LIB) -lnkusb
#SOFLAGS = -shared -Wall,-soname -nostartfiles -s -m64
CC = gcc
LD = gcc

SOURCE = usb3com.$(SrcSuf)
INCLUDE = usb3com.$(HdrSuf)
OBJECT = usb3com.$(ObjSuf)
LIBRARY = libusb3com.$(DllSuf)

PROGRAMS = $(LIBRARY)

all: $(PROGRAMS)
clean:
	@rm -rf $(OBJECT) $(PROGRAMS)

$(LIBRARY): $(OBJECT)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) $(OutPutOpt) $@ $(EXPLLINKLIBS)

install: $(LIBRARY) $(INCLUDE)
	#/bin/rm -f $(INSTALL_LIB)/$<
	#/bin/rm -f $(INSTALL_INC)/$(INCLUDE)
	install $< $(INSTALL_LIB)
	install $(INCLUDE) $(INSTALL_INC)

#dependicies
$(OBJECT):$(INCLUDE)
