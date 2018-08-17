
Debian
====================
This directory contains files used to package lytixd/lytix-qt
for Debian-based Linux systems. If you compile lytixd/lytix-qt yourself, there are some useful files here.

## lytix: URI support ##


lytix-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install lytix-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your lytixqt binary to `/usr/bin`
and the `../../share/pixmaps/lytix128.png` to `/usr/share/pixmaps`

lytix-qt.protocol (KDE)

