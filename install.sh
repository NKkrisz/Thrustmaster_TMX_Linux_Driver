#!/bin/bash

INIT_DRIVER_REPO="https://github.com/scarburato/hid-tminit"
VERSION=0.8a

if [ ${EUID} -ne 0 ]
then 
	echo "You are not running this script as root!"
	exit 1
fi

tmx()
{
	echo "==== REMOVING OLD VERSIONS ===="
	old_vers=$(dkms status | grep tmx | awk -F "\"*, \"*" '{print $2}')
	IFS=$'\n'
	for version in $old_vers
	do
		dkms remove tmx/$version --all
	done

	echo "==== CONFIG DKMS ===="
	cd tmx
	#rm -rf /usr/src/tmx-*
	mkdir "/usr/src/tmx-$VERSION"
	mkdir "/usr/src/tmx-$VERSION/build"

	cp -R ./hid-tmx "/usr/src/tmx-$VERSION/hid-tmx"
	mkdir "/usr/src/tmx-$VERSION/hid-tminit"
	git clone $INIT_DRIVER_REPO "/usr/src/tmx-$VERSION/hid-tminit"
	cp ./dkms_make.mak "/usr/src/tmx-$VERSION/Makefile"
	cp ./dkms.conf "/usr/src/tmx-$VERSION/"

	echo "==== DKMS ===="
	dkms add -m tmx -v $VERSION
	dkms build -m tmx -v $VERSION
	dkms install -m tmx -v $VERSION

	echo "==== INSTALLING UDEV RULES ===="
	cp -vR ./files/* /
	udevadm control --reload
	udevadm trigger

	echo "==== LOADING NEW MODULES ===="
	modprobe hid-tminit
	echo "hid-tminit"
	modprobe hid-tmx
	echo "hid-tmx"

	cd ..
}

t150()
{
	echo "==== REMOVING OLD VERSIONS ===="
	old_vers=$(dkms status | grep t150 | awk -F "\"*, \"*" '{print $2}')
	IFS=$'\n'
	for version in $old_vers
	do
		dkms remove t150/$version --all
	done

	echo "==== CONFIG DKMS ===="
	cd t150
	#rm -rf /usr/src/t150-*
	mkdir "/usr/src/t150-$VERSION"
	mkdir "/usr/src/t150-$VERSION/build"

	cp -R ./hid-t150 "/usr/src/t150-$VERSION/hid-t150"
	mkdir "/usr/src/t150-$VERSION/hid-tminit"
	git clone $INIT_DRIVER_REPO "/usr/src/t150-$VERSION/hid-tminit"
	cp ./dkms_make.mak "/usr/src/t150-$VERSION/Makefile"
	cp ./dkms.conf "/usr/src/t150-$VERSION/"

	echo "==== DKMS ===="
	dkms add -m t150 -v $VERSION
	dkms build -m t150 -v $VERSION
	dkms install -m t150 -v $VERSION

	echo "==== INSTALLING UDEV RULES ===="
	cp -vR ./files/* /
	udevadm control --reload
	udevadm trigger

	echo "==== LOADING NEW MODULES ===="
	modprobe hid-tminit
	echo "hid-tminit"
	modprobe hid-t150
	echo "hid-t150"

	cd ..
}


echo "Please Select Your Wheel"
echo "(1) T150"
echo "(2) TMX"
read sel

case $sel in

	1)
		t150
	;;
	2)
		tmx
	;;
	*)
		echo "Input not valid. Please try again"
	;;
esac