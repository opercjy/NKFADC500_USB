ROOTCINT = rootcling
NOTICE_LIB = $(NKHOME)/lib
NOTICE_INC = $(NKHOME)/include

INSTALL_LIB = $(NKHOME)/lib
INSTALL_INC = $(NKHOME)/include

RC     := root-config
ifeq ($(shell which $(RC) 2>&1 | sed -ne "s@.*/$(RC)@$(RC)@p"),$(RC))
MKARCH := $(wildcard $(shell $(RC) --etcdir)/Makefile.arch)
endif
ifneq ($(MKARCH),)
include $(MKARCH)
else
ROOTSYS = ..
include $(ROOTSYS)/etc/Makefile.arch
endif
-include ../MyConfig.mk

#------------------------------------------------------------------------------

SrcSuf = cc
HdrSuf = h
ObjSuf = o
DllSuf = so
OutPutOpt = -o #keep the space after the option "-o"

CXXFLAGS += -I. -I$(LIBUSB_INC) -I$(NOTICE_INC)
ifneq ($(DEBUG),)
CXXFLAGS += -g
endif

#LDFLAGS +=-shared -Wall,-soname
SOFLAGS += -Wall
LDFLAGS = $(SOFLAGS)
LDLIBS = -L$(NOTICE_LIB) -lnkusb

NKUSBROOTS = NKUSBROOT.$(SrcSuf) NKUSBROOTDict.$(SrcSuf)
NKUSBROOTH = NKUSBROOT.$(HdrSuf) LinkDef.h
NKUSBROOTO = $(NKUSBROOTS:.cc=.o)
NKUSBROOTSO = libNKUSBROOT.$(DllSuf)

all: $(NKUSBROOTSO)
clean:
	@rm -rf $(NKUSBROOTSO) $(NKUSBROOTO) *Dict.$(SrcSuf) *Dict.$(HdrSuf)

install: $(NKUSBROOTSO) NKUSBROOT.h
	install *.pcm $(INSTALL_LIB)
	#/bin/rm -f $(INSTALL_LIB)/$<
	#/bin/rm -f $(INSTALL_INC)/NKUSBROOT.h
	install $< $(INSTALL_LIB)
	install NKUSBROOT.h $(INSTALL_INC)

$(NKUSBROOTSO): $(NKUSBROOTO)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) $(OutPutOpt) $@

NKUSBROOTDict.$(SrcSuf): $(NKUSBROOTH) LinkDef.h
	@echo "Generating dictionary $@..."
	$(ROOTCINT) -f $@ -c -I$(NOTICE_INC) $^

#dependicies
$(NKUSBROOTO):$(NKUSBROOTH)
