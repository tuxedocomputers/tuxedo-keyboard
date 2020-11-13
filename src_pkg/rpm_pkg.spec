%define module module-name

#
# spec file for package tuxedo-keyboard
#
# Copyright (c) 2019 SUSE LINUX GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Summary:        Kernel module for TUXEDO keyboards
Name:           %{module}
Version:        x.x.x
Release:        x
License:        GPLv3+
Group:          Hardware/Other
BuildArch:      noarch
Url:            https://www.tuxedocomputers.com
Source:         %{module}-%{version}.tar.bz2
Provides:       tuxedo_keyboard = %{version}-%{release}
Obsoletes:      tuxedo_keyboard < %{version}-%{release}
Obsoletes:      tuxedo-xp-xc-touchpad-key-fix
Obsoletes:      tuxedo-touchpad-fix <= 1.0.1
Requires:       dkms >= 1.95
BuildRoot:      %{_tmppath}
Packager:       Tomte <tux@tuxedocomputers.com>

%description
Keyboard & keyboard backlight driver for TUXEDO notebooks
meant for DKMS framework.

%prep
%setup -n %{module}-%{version} -q

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/src/%{module}-%{version}/
cp dkms.conf Makefile %{buildroot}/usr/src/%{module}-%{version}
cp -R src/ %{buildroot}/usr/src/%{module}-%{version}
mkdir -p %{buildroot}/usr/share/
mkdir -p %{buildroot}/usr/share/%{module}/
cp postinst %{buildroot}/usr/share/%{module}
cp tuxedo_keyboard.conf %{buildroot}/usr/share/%{module}

%clean
rm -rf %{buildroot}

%files
%defattr(0644,root,root,0755)
%attr(0755,root,root) /usr/src/%{module}-%{version}/
%attr(0644,root,root) /usr/src/%{module}-%{version}/*
%attr(0755,root,root) /usr/src/%{module}-%{version}/src/
%attr(0644,root,root) /usr/src/%{module}-%{version}/src/*
%attr(0755,root,root) /usr/share/%{module}/
%attr(0755,root,root) /usr/share/%{module}/postinst
%attr(0644,root,root) /usr/share/%{module}/tuxedo_keyboard.conf
%license LICENSE

%post
for POSTINST in /usr/lib/dkms/common.postinst /usr/share/%{module}/postinst; do
    if [ -f $POSTINST ]; then
        $POSTINST %{module} %{version} /usr/share/%{module}
        RET=$?
        rmmod %{module} > /dev/null 2>&1 || true
        modprobe %{module} > /dev/null 2>&1 || true

        # Install default config if none exist already
        if [ ! -f "/etc/modprobe.d/tuxedo_keyboard.conf" ]; then
            cp -f /usr/share/tuxedo-keyboard/tuxedo_keyboard.conf /etc/modprobe.d/tuxedo_keyboard.conf
        fi

        exit $RET
    fi
    echo "WARNING: $POSTINST does not exist."
done

echo -e "ERROR: DKMS version is too old and %{module} was not"
echo -e "built with legacy DKMS support."
echo -e "You must either rebuild %{module} with legacy postinst"
echo -e "support or upgrade DKMS to a more current version."
exit 1


%preun
echo -e
echo -e "Uninstall of %{module} module (version %{version}-%{release}) beginning:"
dkms remove -m %{module} -v %{version} --all --rpm_safe_upgrade
if [ $1 != 1 ];then
    /usr/sbin/rmmod %{module} > /dev/null 2>&1 || true
    rm -f /etc/modprobe.d/tuxedo_keyboard.conf || true
fi
exit 0


%changelog
* Fri Nov 13 2020 C Sandberg <tux@tuxedocomputers.com> 2.1.0-1
- Added device support (XMG Fusion)
- Added uniwill lightbar driver (with led_classdev interface)
- Added uniwill keymapping brightness up/down
- Fixed uniwill touchpad toggle (some platforms)
- Fixed module cleanup crash
* Fri Sep 25 2020 C Sandberg <tux@tuxedocomputers.com> 2.0.6-1
- Added uw kbd color backlight support
* Thu Jun 18 2020 C Sandberg <tux@tuxedocomputers.com> 2.0.5-1
- Restructure to allow for more devices
- Added device support
- Added rudimentary device detection
* Tue May 26 2020 C Sandberg <tux@tuxedocomputers.com> 2.0.4-1
- Added rfkill key event
- Fix volume button events, ignore
* Tue May 19 2020 C Sandberg <tux@tuxedocomputers.com> 2.0.3-1
- General key event mapping support
- Events added for backlight and touchpad
- Fix not removing module on rpm update
* Tue Apr 14 2020 C Sandberg <tux@tuxedocomputers.com> 2.0.2-0
- Mark old named packages as conflicting and obsolete
- Fix not restoring state on resume
- Fix autoload issues
- Add standard config tuxedo_keyboard.conf to package
* Tue Mar 17 2020 C Sandberg <tux@tuxedocomputers.com> 2.0.1-0
- New packaging
* Wed Dec 18 2019 Richard Sailer <tux@tuxedocomputers.com> 2.0.0-1
- Initial DKMS package for back-lit keyboard 2nd generation