TARGET = gpio_test

include ../../makefile_cfg

####################################################################
SUBDIRS = $(shell find . -maxdepth 3 -type d)
SHSRCS  = $(foreach dir,$(SUBDIRS),$(wildcard $(dir)/*.sh))

all : check install
	@echo "make finish!!!"

check :
	@echo -e $(TOOLTRAIN_ERR_STRING)

.PHONY : clean cleanall install distclean

$(TARGET) :
	@cp $(SHSRCS) $(TARGET)
	@echo "generate $(TARGET) success!!!"

install : $(TARGET)
	@cp $(TARGET) $(INSTALL_PATH)
	@echo -e '\e[1;33m cp -f $(TARGET) $(INSTALL_PATH) \e[0m'

clean :
	-rm -f $(TARGET)

cleanall : clean
	-rm -f $(INSTALL_PATH)/$(TARGET)

distclean : cleanall
