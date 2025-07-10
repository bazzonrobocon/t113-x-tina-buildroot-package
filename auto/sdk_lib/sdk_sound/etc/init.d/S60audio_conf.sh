#!/bin/sh
#
# Start audio config
#

tinymix set "ADC2 Input MIC2 Boost Switch" 1 > /dev/null
tinymix set "ADC3 Input MIC3 Boost Switch" 1 > /dev/null
tinymix set "HpSpeaker Switch" 1 > /dev/null
tinymix set "Headphone Switch" 1 > /dev/null

if [ ! -f "/usr/lib/libcrypto.so.1.0.0" ]; then
    ln -s /usr/lib/libcrypto.so.1.1 /usr/lib/libcrypto.so.1.0.0
fi

if [ ! -f "/usr/lib/libssl.so.1.0.0" ]; then
    ln -s /usr/lib/libssl.so.1.1 /usr/lib/libssl.so.1.0.0
fi
