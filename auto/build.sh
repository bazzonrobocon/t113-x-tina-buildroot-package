#!/bin/bash
LICHEE_BUILDROOT_DIR=${LICHEE_TOP_DIR}/buildroot

if [ "x" = "x$LICHEE_BR_OUT" ]; then
	echo "auto/build.sh:error LICHEE_BR_OUT string is empty"
	echo -e "\033[31mpls source .buildconfig first!\033[0m"
	exit
fi

SDK_PATH=$LICHEE_BUILDROOT_DIR/package/auto/sdk_lib
LOCAL_PATH=$LICHEE_TOP_DIR/platform/allwinner/multimedia
# sync cedarx header files from package/cedarx

rm -rf $SDK_PATH/cedarx/include/external/include/*
if [ ! -d "$SDK_PATH/cedarx/include/external/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/external/include/
fi
cp -rf $LOCAL_PATH/libcedarx/external/include/*				$SDK_PATH/cedarx/include/external/include/

rm -rf $SDK_PATH/cedarx/include/libcore/base/include/*
if [ ! -d "$SDK_PATH/cedarx/include/libcore/base/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcore/base/include/
fi
cp -rf $LOCAL_PATH/libcedarx/libcore/base/include/*			$SDK_PATH/cedarx/include/libcore/base/include/

rm -rf $SDK_PATH/cedarx/include/libcore/common/iniparser/*
if [ ! -d "$SDK_PATH/cedarx/include/libcore/common/iniparser/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcore/common/iniparser/
fi
cp -rf $LOCAL_PATH/libcedarx/libcore/common/iniparser/*.h		$SDK_PATH/cedarx/include/libcore/common/iniparser/

rm -rf $SDK_PATH/cedarx/include/libcore/common/plugin/*
if [ ! -d "$SDK_PATH/cedarx/include/libcore/common/plugin/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcore/common/plugin/
fi
cp -rf $LOCAL_PATH/libcedarx/libcore/common/plugin/*.h			$SDK_PATH/cedarx/include/libcore/common/plugin/

rm -rf $SDK_PATH/cedarx/include/libcore/muxer/include/*
if [ ! -d "$SDK_PATH/cedarx/include/libcore/muxer/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcore/muxer/include/
fi
cp -rf $LOCAL_PATH/libcedarx/libcore/muxer/include/*			$SDK_PATH/cedarx/include/libcore/muxer/include/

rm -rf $SDK_PATH/cedarx/include/libcore/parser/include/*
if [ ! -d "$SDK_PATH/cedarx/include/libcore/parser/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcore/parser/include/
fi
cp -rf $LOCAL_PATH/libcedarx/libcore/parser/include/*			$SDK_PATH/cedarx/include/libcore/parser/include/

rm -rf $SDK_PATH/cedarx/include/libcore/playback/include/*
if [ ! -d "$SDK_PATH/cedarx/include/libcore/playback/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcore/playback/include/
fi
cp -rf $LOCAL_PATH/libcedarx/libcore/playback/include/*		$SDK_PATH/cedarx/include/libcore/playback/include/

rm -rf $SDK_PATH/cedarx/include/libcore/stream/include//*
if [ ! -d "$SDK_PATH/cedarx/include/libcore/stream/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcore/stream/include/
fi
cp -rf $LOCAL_PATH/libcedarx/libcore/stream/include/*			$SDK_PATH/cedarx/include/libcore/stream/include/

rm -rf $SDK_PATH/cedarx/include/xmetadataretriever/include/m/include//*
if [ ! -d "$SDK_PATH/cedarx/include/xmetadataretriever/include/m/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/xmetadataretriever/include/m/include/
fi
cp -rf $LOCAL_PATH/libcedarx/xmetadataretriever/include/*		$SDK_PATH/cedarx/include/xmetadataretriever/include/

rm -rf $SDK_PATH/cedarx/include/xplayer/include/*
if [ ! -d "$SDK_PATH/cedarx/include/xplayer/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/xplayer/include/
fi
cp -rf $LOCAL_PATH/libcedarx/xplayer/include/*					$SDK_PATH/cedarx/include/xplayer/include/

rm -rf $SDK_PATH/cedarx/include/libcedarc/include/*
if [ ! -d "$SDK_PATH/cedarx/include/libcedarc/include/" ]; then
	mkdir -p $SDK_PATH/cedarx/include/libcedarc/include/
fi
cp -rf $LOCAL_PATH/libcedarc/include/*						$SDK_PATH/cedarx/include/libcedarc/include/

echo "build $1 sdk_demo ..."
make -C $LICHEE_BUILDROOT_DIR/package/auto/sdk_lib $1
if [ $? -ne 0 ]; then
	echo  "build sdk_lib fail ... "
	exit 1
else
	cp -rf $LICHEE_BUILDROOT_DIR/package/auto/sdk_lib/libs/*	$LICHEE_BR_OUT/target/usr/lib/
fi

echo "build $1 sdk_demo ..."
make -C $LICHEE_BUILDROOT_DIR/package/auto/sdk_demo/ $1
if [ $? -ne 0 ]; then
	echo  "build sdk_demo fail ..."
	exit 1
else
	cp -rf $LICHEE_BUILDROOT_DIR/package/auto/sdk_demo/bin/*	$LICHEE_BR_OUT/target/usr/bin/
fi

echo "build auto finish"
