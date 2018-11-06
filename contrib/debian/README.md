
Debian
====================
This directory contains files used to package esbcoind/esbcoin-qt
for Debian-based Linux systems. If you compile esbcoind/esbcoin-qt yourself, there are some useful files here.

## esbcoin: URI support ##


esbcoin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install esbcoin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your esbcoinqt binary to `/usr/bin`
and the `../../share/pixmaps/esbcoin128.png` to `/usr/share/pixmaps`

esbcoin-qt.protocol (KDE)
