#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/esbcoin.png
ICON_DST=../../src/qt/res/icons/esbcoin.ico
convert ${ICON_SRC} -resize 16x16 esbcoin-16.png
convert ${ICON_SRC} -resize 32x32 esbcoin-32.png
convert ${ICON_SRC} -resize 48x48 esbcoin-48.png
convert esbcoin-16.png esbcoin-32.png esbcoin-48.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/esbcoin_testnet.png
ICON_DST=../../src/qt/res/icons/esbcoin_testnet.ico
convert ${ICON_SRC} -resize 16x16 esbcoin-16.png
convert ${ICON_SRC} -resize 32x32 esbcoin-32.png
convert ${ICON_SRC} -resize 48x48 esbcoin-48.png
convert esbcoin-16.png esbcoin-32.png esbcoin-48.png ${ICON_DST}
rm esbcoin-16.png esbcoin-32.png esbcoin-48.png
