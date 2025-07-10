#
# Reference http://www.gnu.org/software/make/manual/make.html
#

MAKE = make -j32

# exclude_dirs = bin
# dirs := $(shell find . -maxdepth 1 -type d)
# dirs := $(basename $(patsubst ./%,%,$(dirs)))
# dirs := $(filter-out $(exclude_dirs),$(dirs))
# SUBDIRS := $(dirs)
ifneq (,$(filter $(LICHEE_IC),a40i a40i_h a40i_c t3 t3_c t3_pro))
SUBDIRS := fbinit_test lcd_bright_test multiscreen_test usbcam_test
else
SUBDIRS := fbinit_test lcd_bright_test gif2rgb_test awrpaf_test dsp_debug_test usbcam_test \
	tpadc_test gpio_test

ifeq ($(LICHEE_IC), $(filter $(LICHEE_IC), t527))
	SUBDIRS += npu_detect_test npu_detect_face_test render_test
endif


BR2_PACKAGE_LIBCEDARX := $(shell grep BR2_PACKAGE_LIBCEDARX $(LICHEE_BR_OUT)/.config | sed s/BR2_PACKAGE_LIBCEDARX=//g)
ifeq ($(BR2_PACKAGE_LIBCEDARX), y)
SUBDIRS += alpha_test yuv_test audioenc_test autplayer_test decoder_test stream_player_test \
	   encoder_test record_test csi_test tvd_test dvr_test g2d_test mem_test qt_daemon input_test
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
