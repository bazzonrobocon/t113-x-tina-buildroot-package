TARGET	= libsdk_rpaf.so

CUR_MODULE_VERSION = "V0.1.0.20220712"

DEFINES += -DMODULE_VERSION=\"$(CUR_MODULE_VERSION)\"

include ../../makefile_cfg

LDFLAGS  := -L$(SDK_LIBS) -shared
CFLAGS   += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-reorder -Wno-write-strings -Wno-unused-function -Wno-unused-value -fPIC
CXXFLAGS += -g -O0 -pipe -Wall -W -Wno-unused-parameter -Wno-reorder -Wno-write-strings -Wno-unused-function -Wno-unused-value -fPIC

# 声明宏定义
DEFINES += $(SDKLIB_DEF) -DUSE_LOGCAT

# 声明头文件
DEFINC = -I./ \
		 -I./include
INCS += $(DEFINC) $(SDKLIB_INC)

LIBS += -lsdk_log

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