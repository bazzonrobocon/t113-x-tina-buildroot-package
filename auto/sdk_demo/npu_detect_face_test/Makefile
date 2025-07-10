TARGET := npu_detect_face

CUR_MODULE_VERSION = "V1.0.20231209"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -L$(SDK_LIBS) -L$(CEDAR_LIBS) -L$(SDK_PATH)/sdk_awnn/viplite -L$(SDK_PATH)/sdk_awnn/awf
CFLAGS   += -g -O0 -pipe -Wall -W -Wl,--no-undefined -Wno-unused-parameter -Wno-unused-function -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wl,--no-undefined -Wno-unused-parameter -Wno-unused-function -fPIC

# 声明宏定义
DEFINES += -DUSE_LOGCAT -DLOG_LEVEL=6

# 声明头文件
DEFINC = -I./ \
		 -I$(LICHEE_KERN_DIR)/arch/arm64/include \
 		 -I$(SDK_PATH)/include/npu/common \
		 -I$(SDK_PATH)/include/npu/awnn \
		 -I$(SDK_PATH)/include/npu/awf \
		 -I$(SDK_PATH)/include/npu/postprocess


INCS += $(DEFINC) $(SDKLIB_INC)

LIBS += -lsdk_log -lVIPlite -lVIPuser -lawf_det -law_net -law_utilities

####################################################################
SUBDIRS = $(shell find . -maxdepth 3 -type d)
CSRCS   = $(foreach dir,$(SUBDIRS),$(wildcard $(dir)/*.c))
CPPSRCS = $(foreach dir,$(SUBDIRS),$(wildcard $(dir)/*.cpp))
# CPPSRCS = csi_test_usrptr.cpp

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
