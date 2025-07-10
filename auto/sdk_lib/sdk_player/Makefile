
TARGET = libsdk_player.so

CUR_MODULE_VERSION = "V2.1.20221031"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -rdynamic -fPIC -shared
CFLAGS   += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-narrowing -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-narrowing -fPIC

# 声明宏定义
DEFINES += $(ANDROID_UTILS_DEF) $(CEDAR_MACORS) $(SDKLIB_DEF)

# 声明头文件
DEFINC = -I./
INCS += $(DEFINC) $(SDKLIB_INC)

LIBS = -lpthread

########################################################################
SUBDIRS = $(shell find . -maxdepth 3 -type d)
CSRCS   = \
	deinterlace_null.c \
	subtitleControl.c \
	soundControl_tinyalsa.c
CPPSRCS = \
	layerControl_de.cpp \
	AutPlayer.cpp

COBJS := $(CSRCS:.c=.o)
CPPOBJS := $(CPPSRCS:.cpp=.o)

all : check install
	@echo "make finish!!!"

check :
	@echo -e $(TOOLTRAIN_ERR_STRING)

$(COBJS) : %.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) $(DEFINES) $(INCS)
$(CPPOBJS) : %.o: %.cpp
	$(CPP) -c $< -o $@ $(CXXFLAGS) $(DEFINES) $(INCS)
$(TARGET) : $(COBJS) $(CPPOBJS)
	$(CPP) -o $(TARGET) $(COBJS) $(CPPOBJS) $(LIBS) $(LDFLAGS)
	@echo "generate $(TARGET) success!!!"

.PHONY : clean cleanall install distclean

install : $(TARGET)
	@cp $(TARGET) $(SDK_LIBS)
	@echo -e '\e[1;33m cp -f $(TARGET) $(SDK_LIBS) \e[0m'

clean :
	-rm -f $(TARGET) $(COBJS) $(CPPOBJS)

cleanall : clean
	-rm -f $(SDK_LIBS)/$(TARGET)

distclean : cleanall