#!/bin/bash
XORG_MODULES="$1"

mkdir -p ${XORG_MODULES}/input
rm -f ${XORG_MODULES}/input/loriei_drv.so
ln -s ${XORG_MODULES}/drivers/lorie_drv.so ${XORG_MODULES}/input/loriei_drv.so
