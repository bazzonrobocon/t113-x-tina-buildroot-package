TARGET = yuvtest

CUR_MODULE_VERSION = "V2.1.20220907"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -L$(SDK_LIBS) -L$(CEDAR_LIBS)
CFLAGS   += -g -O0 -pipe -Wall -W -Wl,--no-undefined -Wno-write-strings -Wno-unused-parameter -Wno-unused-function -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wl,--no-undefined -Wno-write-strings -Wno-unused-parameter -Wno-unused-function -fPIC

# 声明宏定义
DEFINES += $(CEDAR_MACORS)

# 声明头文件
DEFINC = -I./ \
		 -I$(SDK_PATH)/cedarx/include/libcedarc/include
INCS += $(DEFINC) $(SDKLIB_INC)

LIBS += -lMemAdapter -lVE -lcdc_base -lvideoengine -lsdk_memory -lsdk_disp -lsdk_log

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
