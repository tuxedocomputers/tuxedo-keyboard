#
# Copyright (c) 2018-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
#
# This file is part of tuxedo-keyboard.
#
# tuxedo-keyboard is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software.  If not, see <https://www.gnu.org/licenses/>.
#
obj-m :=	./src/tuxedo_keyboard.o \
		./src/clevo_wmi.o \
		./src/clevo_acpi.o \
		./src/tuxedo_io/tuxedo_io.o \
		./src/uniwill_wmi.o

PWD := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	make -C $(KDIR) M=$(PWD) modules_install

# Package version and name from dkms.conf
VER := $(shell sed -n 's/^PACKAGE_VERSION=\([^\n]*\)/\1/p' dkms.conf 2>&1 /dev/null)
MODULE_NAME := $(shell sed -n 's/^PACKAGE_NAME=\([^\n]*\)/\1/p' dkms.conf 2>&1 /dev/null)

dkmsinstall:
	cp -R . /usr/src/$(MODULE_NAME)-$(VER)
	dkms install -m $(MODULE_NAME) -v $(VER)

dkmsremove:
	dkms remove -m $(MODULE_NAME) -v $(VER) --all
	rm -rf /usr/src/$(MODULE_NAME)-$(VER)

# --------------
# Packaging only
# ---------------

DEB_PACKAGE_NAME := $(MODULE_NAME)-$(VER)

# Deb package folder variables
DEB_PACKAGE_BASE := deb/$(DEB_PACKAGE_NAME)
DEB_PACKAGE_SRC := $(DEB_PACKAGE_BASE)/usr/src/$(DEB_PACKAGE_NAME)
DEB_PACKAGE_CTRL := $(DEB_PACKAGE_BASE)/DEBIAN

package: package-deb package-rpm
package-clean: package-deb-clean package-rpm-clean

package-deb:
	# Create/complete folder structure according to current version
	rm -rf $(DEB_PACKAGE_BASE) || true
	cp -rf deb/module-name $(DEB_PACKAGE_BASE)
	mv $(DEB_PACKAGE_BASE)/usr/share/doc/module-name $(DEB_PACKAGE_BASE)/usr/share/doc/$(MODULE_NAME)
	mkdir -p $(DEB_PACKAGE_BASE)/usr/src || true
	mkdir -p $(DEB_PACKAGE_SRC) || true
	mkdir -p $(DEB_PACKAGE_BASE)/usr/share/$(MODULE_NAME) || true

	# Replace name/version numbers in control/script files
	sed -i 's/^Version:[^\n]*/Version: $(VER)/g' $(DEB_PACKAGE_CTRL)/control
	sed -i 's/^Package:[^\n]*/Package: $(MODULE_NAME)/g' $(DEB_PACKAGE_CTRL)/control
	sed -i 's/^version=[^\n]*/version=$(VER)/g' $(DEB_PACKAGE_CTRL)/postinst
	sed -i 's/^module=[^\n]*/module=$(MODULE_NAME)/g' $(DEB_PACKAGE_CTRL)/postinst
	sed -i 's/^version=[^\n]*/version=$(VER)/g' $(DEB_PACKAGE_CTRL)/prerm
	sed -i 's/^module=[^\n]*/module=$(MODULE_NAME)/g' $(DEB_PACKAGE_CTRL)/prerm
	# Copy source
	cp -rf dkms.conf $(DEB_PACKAGE_SRC)
	cp -rf Makefile $(DEB_PACKAGE_SRC)
	cp -rf src $(DEB_PACKAGE_SRC)
	cp -rf src_pkg/dkms_postinst $(DEB_PACKAGE_BASE)/usr/share/$(MODULE_NAME)/postinst
	cp -rf tuxedo_keyboard.conf $(DEB_PACKAGE_BASE)/usr/share/$(MODULE_NAME)/tuxedo_keyboard.conf
	# Make sure files and folders have acceptable permissions
	chmod -R 755 $(DEB_PACKAGE_CTRL)
	chmod 644 $(DEB_PACKAGE_CTRL)/control
	find deb/$(DEB_PACKAGE_NAME)/usr -type d -exec chmod 755 {} \;
	find deb/$(DEB_PACKAGE_NAME)/usr -type f -exec chmod 644 {} \;
	chmod 755 $(DEB_PACKAGE_BASE)/usr/share/$(MODULE_NAME)/postinst
	chmod 644 $(DEB_PACKAGE_BASE)/usr/share/$(MODULE_NAME)/tuxedo_keyboard.conf
	
	gunzip $(DEB_PACKAGE_BASE)/usr/share/doc/$(MODULE_NAME)/changelog.gz
	gzip -n9 $(DEB_PACKAGE_BASE)/usr/share/doc/$(MODULE_NAME)/changelog

	# Make deb package
	dpkg-deb --root-owner-group -b $(DEB_PACKAGE_BASE) $(DEB_PACKAGE_NAME).deb

package-deb-clean:
	rm -rf deb/$(MODULE_NAME)-* > /dev/null 2>&1 || true
	rm *.deb > /dev/null 2>&1 || true

RPM_PACKAGE_NAME := $(MODULE_NAME)-$(VER)
RPM_PACKAGE_SRC := rpm/SOURCES/$(RPM_PACKAGE_NAME)
RPM_SPEC := rpm/SPECS/$(MODULE_NAME).spec
RELEASE := 1

package-rpm:
	# Create folder source structure according to current version
	rm -rf rpm || true
	mkdir -p $(RPM_PACKAGE_SRC)
	mkdir -p rpm/SPECS
	# Copy spec template
	cp -rf src_pkg/rpm_pkg.spec $(RPM_SPEC)
	# Modify spec file with version etc.
	sed -i 's/^%define module[^\n]*/%define module $(MODULE_NAME)/g' $(RPM_SPEC)
	sed -i 's/^Version:[^\n]*/Version:        $(VER)/g' $(RPM_SPEC)
	sed -i 's/^Release:[^\n]*/Release:        $(RELEASE)/g' $(RPM_SPEC)
	# Copy source
	cp -rf dkms.conf $(RPM_PACKAGE_SRC)
	cp -rf Makefile $(RPM_PACKAGE_SRC)
	cp -rf src $(RPM_PACKAGE_SRC)
	cp -rf LICENSE $(RPM_PACKAGE_SRC)
	cp -rf src_pkg/dkms_postinst $(RPM_PACKAGE_SRC)/postinst
	cp -rf tuxedo_keyboard.conf $(RPM_PACKAGE_SRC)
	# Compress/package source
	cd rpm/SOURCES && tar cjvf $(RPM_PACKAGE_NAME).tar.bz2 $(RPM_PACKAGE_NAME)
	# Make rpm package
	rpmbuild --debug -bb --define "_topdir `pwd`/rpm" $(RPM_SPEC)
	# Copy built package
	cp rpm/RPMS/noarch/*.rpm .

package-rpm-clean:
	rm -rf rpm > /dev/null 2>&1 || true
	rm *.rpm > /dev/null 2>&1 || true
