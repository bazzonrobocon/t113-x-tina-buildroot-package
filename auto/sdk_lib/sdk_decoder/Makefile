TARGET = libsdk_decoder.so

CUR_MODULE_VERSION = "V2.0.20230822"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -L$(SDK_LIBS) -L$(CEDAR_LIBS) -shared
CFLAGS   += -g -O0 -pipe -Wall -W -Wno-unused-parameter -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wno-unused-parameter -fPIC

#依赖库
LIBS = -lMemAdapter -lvdecoder -lVE -lcdc_base -lvideoengine -lcdx_common -lcdx_base -lsdk_memory -lcdx_ion

# 声明宏定义
DEFINES += -DUSE_LOGCAT -DLOG_LEVEL=6

# 声明头文件
DEFINC = -I./ \
		 -I$(SDK_PATH)/cedarx/include/libcedarc/include
INCS += $(DEFINC) $(SDKLIB_INC)

########################################################################
SUBDIRS = $(shell find . -maxdepth 3 -type d)
CSRCS   = $(foreach dir,$(SUBDIRS),$(wildcard $(dir)/*.c))
CPPSRCS = $(foreach dir,$(SUBDIRS),$(wildcard $(dir)/*.cpp))

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

