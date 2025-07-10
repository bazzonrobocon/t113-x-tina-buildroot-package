# the so name is defined by alsa lib and /etc/asound_rpaf.conf
TARGET	= libasound_module_pcm_awrpaf.so

CUR_MODULE_VERSION = "V0.1.0.20220712"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -shared -L$(SDK_LIBS)
CFLAGS   += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-reorder -Wno-write-strings -Wno-unused-function -Wno-unused-value -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-reorder -Wno-write-strings -Wno-unused-function -Wno-unused-value -fPIC

# 声明宏定义
DEFINES += $(SDKLIB_DEF) -DUSE_LOGCAT -DLOG_LEVEL=6

# 声明头文件
DEFINC = -I./ \
		 -I./include
INCS += $(DEFINC) $(SDKLIB_INC)

LIBS += -lsdk_log -lsdk_rpaf -lasound -ldl -lm -lpthread

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
	@mkdir -p $(ROOTFS_PATH)/usr/lib/alsa-lib/
	@mkdir -p $(ROOTFS_PATH)/etc/
	@cp -f $(TARGET) $(ROOTFS_PATH)/usr/lib/alsa-lib/
	@echo -e '\e[1;33m cp -f $(TARGET) $(ROOTFS_PATH)/usr/lib/alsa-lib/ \e[0m'
	@cp -f conf/30-awrpaf.conf $(ROOTFS_PATH)/etc/asound_rpaf.conf
	@echo -e '\e[1;33m cp -f conf/30-awrpaf.conf $(ROOTFS_PATH)/etc/asound_rpaf.conf \e[0m'

clean :
	-rm -f $(TARGET) $(COBJS) $(CPPOBJS)

cleanall : clean
	-rm -f $(SDK_LIBS)/$(TARGET)
	-rm -rf $(ROOTFS_PATH)/usr/lib/alsa-lib/$(TARGET)
	-rm -rf $(ROOTFS_PATH)/etc/asound_rpaf.conf

distclean : cleanall
