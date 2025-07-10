TARGET = libsdk_render.so

CUR_MODULE_VERSION = "V2.5.20231209"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -rdynamic -lpthread -shared
CFLAGS   += -g -O0 -pipe -Wall -W -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -fPIC

# 声明宏定义
DEFINES += $(ANDROID_UTILS_DEF) $(CEDAR_MACORS) $(SDKLIB_DEF) -DWATERMARK -DUSE_LOGCAT -DLOG_LEVEL=6 \
-DDEBU -DUSE_OZONE -DEGL_EGLEXT_PROTOTYPES -DGL_GLEXT_PROTOTYPES -DEGL_FBDEV

# 声明头文件
DEFINC = -I./ \
		-I./include \
		-I${LICHEE_TOP_DIR}/platform/allwinner/display/libgpu/mali-g57/include \
		-I$(SDK_PATH)/include/render

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
