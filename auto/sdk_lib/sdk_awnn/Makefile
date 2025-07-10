TARGET = libsdk_awnn.so
NB_FILE = 4.0.0_Beta.nb

CUR_MODULE_VERSION = "V2.5.20231209"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -rdynamic -lpthread -shared
CFLAGS   += -g -O0 -pipe -Wall -W -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -fPIC

# 声明宏定义
#DEFINES += -DUSE_LOGCAT -DLOG_LEVEL=2

# 声明头文件
DEFINC = -I./ \
		-I$(SDK_PATH)/include/npu/common/ \
		-I$(SDK_PATH)/include/npu/awnn/ \
		-I$(SDK_PATH)/include/npu/postprocess/ \

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
	@cp ./viplite/*.so $(SDK_LIBS)
	@cp ./awf/*.so $(SDK_LIBS)
	@cp -f $(NB_FILE) $(ROOTFS_PATH)/etc/
	@echo -e '\e[1;33m cp -f $(TARGET) $(SDK_LIBS) \e[0m'

clean :
	-rm -f $(TARGET) $(COBJS) $(CPPOBJS)

cleanall : clean
	-rm -f $(SDK_LIBS)/$(TARGET)

distclean : cleanall
