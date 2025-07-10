#
# Reference http://www.gnu.org/software/make/manual/make.html
#

MAKE = make -j32

# exclude_dirs = cedarx include libs
# dirs := $(shell find . -maxdepth 1 -type d)
# dirs := $(basename $(patsubst ./%,%,$(dirs)))
# dirs := $(filter-out $(exclude_dirs),$(dirs))

ifneq (,$(filter $(LICHEE_IC),a40i a40i_h a40i_c t3 t3_c t3_pro))
SUBDIRS := sdk_log sdk_disp
else
SUBDIRS := sdk_log sdk_memory sdk_g2d sdk_disp sdk_misc sdk_shm sdk_sound sdk_audioenc \
	sdk_rpaf sdk_rpaf_plugin sdk_storage sdk_config sdk_camera sdk_dvr \
	sdk_player sdk_gifdecoder

ifeq ($(LICHEE_IC), $(filter $(LICHEE_IC), t527))
	SUBDIRS += sdk_awnn sdk_render
endif

BR2_PACKAGE_LIBCEDARX := $(shell grep BR2_PACKAGE_LIBCEDARX $(LICHEE_BR_OUT)/.config | sed s/BR2_PACKAGE_LIBCEDARX=//g)
ifeq ($(BR2_PACKAGE_LIBCEDARX), y)
SUBDIRS += sdk_encoder sdk_decoder sdk_stream_player
endif
endif

all:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir ;\
		if [ "$$?" != "0" ]; then\
			echo "compile $$dir fail"; \
			exit 1 ; \
		fi;\
	done

.PHONY: clean distclean cleanall

$(SUBDIRS):ECHO
	+$(MAKE) -C $@
ECHO:
	@echo begin compile $(SUBDIRS)

clean: $(clean_dirs)
	for dir in $(SUBDIRS);\
	do $(MAKE) -C $$dir clean;\
	done

install:
	for dir in $(SUBDIRS);\
	do $(MAKE) -C $$dir install;\
	done

distclean:
	for dir in $(SUBDIRS);\
	do $(MAKE) -C $$dir distclean;\
	done

cleanall:
	for dir in $(SUBDIRS);\
	do $(MAKE) -C $$dir cleanall;\
	done
