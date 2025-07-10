TARGET = audioenc_test

CUR_MODULE_VERSION = "V2.1.20220913"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS := -L$(SDK_LIBS) -L$(CEDAR_LIBS)
CFLAGS   += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-unused-function -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-unused-function -fPIC

# 声明宏定义
DEFINES += $(ANDROID_UTILS_DEF) $(SDKLIB_DEF) $(CEDAR_MACORS) -DUSE_LOGCAT -DLOG_LEVEL=6

# 声明头文件
DEFINC = -I./ \
		 -I$(SDK_PATH)/cedarx/include/libcedarc/include
INCS += $(DEFINC) $(SDKLIB_INC)

LIBS += -lMemAdapter -lVE -lcdc_base -lvideoengine -lcdx_common -lcdx_base -lcdx_ion -lcdx_muxer -lcdx_parser \
		-lcdx_playback -lcdx_stream -lsubdecoder -lvdecoder \
		-lvencoder -lxmetadataretriever -lxplayer -lvenc_base -lvenc_common \
		-lsbm -lfbm -laftertreatment -lcrypto -lsdk_audenc -ladecoder -laencoder -lasound \
		-lsdk_player -lsdk_misc -lsdk_cfg -lsdk_storage -lsdk_sound -lsdk_memory -lsdk_disp -lsdk_log \
		-lsdk_camera -lsdk_dvr -lsdk_g2d

ifeq ($(LICHEE_IC), $(filter $(LICHEE_IC), t527))
LIBS +=	-lsdk_awnn -lVIPlite -lVIPuser -lsdk_render -lmali -lawf_det -law_net -law_utilities
endif

BR2_PACKAGE_CEDARE := $(shell grep BR2_PACKAGE_LIBCEDARE $(LICHEE_BR_OUT)/.config | sed s/BR2_PACKAGE_LIBCEDARE=//g)
ifeq ($(BR2_PACKAGE_CEDARE), y)
LIBS += -lcedare
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
	# @cp $(TARGET) $(INSTALL_PATH)
	# @echo -e '\e[1;33m cp -f $(TARGET) $(INSTALL_PATH) \e[0m'

clean :
	-rm -f $(TARGET) $(COBJS) $(CPPOBJS)

cleanall : clean
	# -rm -f $(INSTALL_PATH)/$(TARGET)

distclean : cleanall

