NOTICE_LIB = $(NKHOME)/lib
NOTICE_INC = $(NKHOME)/include

INSTALL_LIB = $(NKHOME)/lib
INSTALL_INC = $(NKHOME)/include

SrcSuf = c
HdrSuf = h
ObjSuf = o
DllSuf = so
OutPutOpt = -o #
CFLAGS = -I. -I$(LIBUSB_INC) -Wall -Wundef -Wunused -Wstrict-prototypes -Werror-implicit-function-declaration -Wno-pointer-sign -Wshadow -O2 -fPIC -m64

#SOFLAGS = -shared -Wall,-soname -nostartfiles -s -m64
SOFLAGS = -shared -Wall
LDFLAGS = $(SOFLAGS)
LDLIBS = -L$(LIBUSB_LIB) -lusb-1.0
CC = gcc
LD = gcc

NKUSBS = nkusb.$(SrcSuf)
NKUSBH = nkusb.$(HdrSuf) nkusbconst.$(HdrSuf)
NKUSBO = nkusb.$(ObjSuf)
NKUSBSO = libnkusb.$(DllSuf)

PROGRAMS = $(NKUSBSO)

all: $(PROGRAMS)
clean:
	@rm -rf $(NKUSBO) $(PROGRAMS)

$(NKUSBSO): $(NKUSBO)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) $(OutPutOpt) $@ $(EXPLLINKLIBS)

install: $(NKUSBSO) $(NKUSBH)
	#/bin/rm -f $(INSTALL_LIB)/$<
	#/bin/rm -f $(INSTALL_INC)/$(NKUSBH)
	install $< $(INSTALL_LIB)
	install $(NKUSBH) $(INSTALL_INC)

#dependicies
$(NKUSBO):$(NKUSBH)
