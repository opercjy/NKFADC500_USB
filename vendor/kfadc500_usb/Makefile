INSTALL_LIB = $(NKHOME)/lib
INSTALL_INC = $(NKHOME)/include

MODULE = NoticeKFADC500USB

OutPutOpt = -o #
CFLAGS = -I. -I$(LIBUSB_INC) -Wall -Wundef -Wunused -Wstrict-prototypes -Werror-implicit-function-declaration -Wno-pointer-sign -Wshadow -O2 -fPIC -m64

SOFLAGS = -shared -Wall
LDFLAGS = $(SOFLAGS)
LDLIBS = -L$(LIBUSB_LIB) -lusb-1.0
CC = gcc
LD = gcc

INCLUDE = $(MODULE).h 
OBJECT = $(MODULE).o
LIBRARY = lib$(MODULE).so

PROGRAMS = $(LIBRARY)

all: $(PROGRAMS)
clean:
	@rm -rf $(OBJECT) $(PROGRAMS)

$(LIBRARY): $(OBJECT)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) $(OutPutOpt) $@ $(EXPLLINKLIBS)

install: $(LIBRARY) $(INCLUDE)
	install $< $(INSTALL_LIB)
	install $(INCLUDE) $(INSTALL_INC)

#dependicies
$(OBJECT):$(INCLUDE)
