TARGET = libsdk_sound.so

CUR_MODULE_VERSION = "V2.0.20220506"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -rdynamic -lpthread -shared
CFLAGS   += -g -O0 -pipe -Wall -W -Wno-unused-parameter -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wno-unused-parameter -fPIC

# 声明宏定义
DEFINES += -DUSE_LOGCAT -DLOG_LEVEL=6

# 声明头文件
DEFINC = -I./ \
		 -I$(SDK_PATH)/cedarx/include/external/include/alsa/
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
ifneq (,$(filter $(LICHEE_IC),t113_s3p t113_s4 t113_s4p))
	@cp -f etc/asound.conf $(ROOTFS_PATH)/etc/asound.conf
	@echo -e '\e[1;33m cp -f etc/asound.conf $(ROOTFS_PATH)/etc/asound.conf \e[0m'
	@cp -f etc/init.d/S60audio_conf.sh $(ROOTFS_PATH)/etc/init.d/S60audio_conf.sh
	@echo -e '\e[1;33m cp -f etc/init.d/S60audio_conf.sh $(ROOTFS_PATH)/etc/init.d/S60audio_conf.sh \e[0m'
endif


clean :
	-rm -f $(TARGET) $(COBJS) $(CPPOBJS)

cleanall : clean
	-rm -f $(SDK_LIBS)/$(TARGET)
	-rm -f $(ROOTFS_PATH)/etc/asound.conf
	-rm -f $(ROOTFS_PATH)/etc/init.d/S60audio_conf.sh

distclean : cleanall
