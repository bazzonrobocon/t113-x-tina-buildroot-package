TARGET = dvr_test

CUR_MODULE_VERSION = "V2.1.20220518"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg



LDFLAGS  := -L$(SDK_LIBS) -L$(CEDAR_LIBS) -no-pie
CFLAGS   += -g -O0 -pipe -Wall -W -Wl,--no-undefined -Wno-write-strings -Wno-unused-parameter -Wno-unused-function -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wl,--no-undefined -Wno-write-strings -Wno-unused-parameter -Wno-unused-function -fPIC

# 声明宏定义
DEFINES += $(CEDAR_MACORS) $(ANDROID_UTILS_DEF) $(SDKLIB_DEF) -DUSE_LOGCAT -DLOG_LEVEL=6 -DWATERMARK

# 声明头文件
DEFINC = -I./ \
		 -I$(SDK_PATH)/sdk_misc/ \
		 -I$(SDK_PATH)/sdk_camera/ \

INCS += $(DEFINC) $(SDKLIB_INC)

LIBS += -ladecoder -laencoder -lcdx_base -lcdx_common \
		-lcdx_muxer -lcdx_parser -lcdx_playback -lcdx_stream -lMemAdapter -lcdc_base -lsubdecoder -lvdecoder \
		-lvencoder -lVE -lvideoengine -lxmetadataretriever -lxplayer -lvenc_base -lcdx_ion	-lasound \
		-lvenc_common -lvenc_h264 -lvenc_h265 -lvenc_jpeg -lsbm -laftertreatment -lscaledown -lfbm \
		-lsdk_disp -lsdk_g2d -lsdk_dvr -lsdk_player -lsdk_log -lsdk_camera \
		-lsdk_memory  -lsdk_sound -lsdk_storage -lsdk_audenc -lsdk_cfg -lsdk_misc \
		-lrt -lpthread -lz -lcrypto -lssl

ifeq ($(LICHEE_IC), $(filter $(LICHEE_IC), t527))
LIBS +=	-lsdk_awnn -lVIPlite -lVIPuser -lsdk_render -lmali -lawf_det -law_net -law_utilities
endif


BR2_PACKAGE_CEDARE := $(shell grep BR2_PACKAGE_LIBCEDARE $(LICHEE_BR_OUT)/.config | sed s/BR2_PACKAGE_LIBCEDARE=//g)
ifeq ($(BR2_PACKAGE_CEDARE), y)
LIBS += -lcedare
DEFINES += -DDVR_USE_CEDARE
endif

####################################################################
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
	@cp $(TARGET) $(INSTALL_PATH)
	@echo -e '\e[1;33m cp -f $(TARGET) $(INSTALL_PATH) \e[0m'

clean :
	-rm -f $(TARGET) $(COBJS) $(CPPOBJS)

cleanall : clean
	-rm -f $(INSTALL_PATH)/$(TARGET)

distclean : cleanall
