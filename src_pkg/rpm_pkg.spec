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
Obsoletes:      tuxedo-cc-wmi
Requires:       dkms >= 1.95
BuildRoot:      %{_tmppath}
Packager:       TUXEDO Computers GmbH <tux@tuxedocomputers.com>

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
%attr(0755,root,root) /usr/src/%{module}-%{version}/src/tuxedo_io/
%attr(0644,root,root) /usr/src/%{module}-%{version}/src/tuxedo_io/*
%attr(0755,root,root) /usr/share/%{module}/
%attr(0755,root,root) /usr/share/%{module}/postinst
%attr(0644,root,root) /usr/share/%{module}/tuxedo_keyboard.conf
%license LICENSE

%post
for POSTINST in /usr/lib/dkms/common.postinst /usr/share/%{module}/postinst; do
    if [ -f $POSTINST ]; then
        $POSTINST %{module} %{version} /usr/share/%{module}
        RET=$?

        # Attempt to (re-)load module immediately, fail silently if not possible at this stage

        # Also stop tccd service if running before
        echo "Check tccd running status"
        if systemctl is-active --quiet tccd.service; then
            TCCD_RUNNING=true
        else
            TCCD_RUNNING=false
        fi

        if $TCCD_RUNNING; then
            echo "Stop tccd temporarily"
            systemctl stop tccd 2>&1 || true
        fi

        % Explicitly unload old tuxedo_cc_wmi if loaded at this point
        rmmod tuxedo_cc_wmi > /dev/null 2>&1 || true

        echo "(Re)load modules if possible"

        rmmod tuxedo_io > /dev/null 2>&1 || true
        rmmod uniwill_wmi > /dev/null 2>&1 || true
        rmmod clevo_wmi > /dev/null 2>&1 || true
        rmmod clevo_acpi > /dev/null 2>&1 || true
        rmmod tuxedo_keyboard > /dev/null 2>&1 || true
        
        modprobe tuxedo_keyboard > /dev/null 2>&1 || true
        modprobe uniwill_wmi > /dev/null 2>&1 || true
        modprobe clevo_wmi > /dev/null 2>&1 || true
        modprobe clevo_acpi > /dev/null 2>&1 || true
        modprobe tuxedo_io > /dev/null 2>&1 || true

        # Install default config if none exist already
        if [ ! -f "/etc/modprobe.d/tuxedo_keyboard.conf" ]; then
            cp -f /usr/share/tuxedo-keyboard/tuxedo_keyboard.conf /etc/modprobe.d/tuxedo_keyboard.conf
        fi

        # Restart tccd after reload if it was running
        if $TCCD_RUNNING; then
            echo "Start tccd again"
            systemctl start tccd 2>&1 || true
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
* Wed Jan 11 2023 C Sandberg <tux@tuxedocomputers.com> 3.1.3-1
- Fix IBP14Gen6 second fan not spinning (alternative fan ctl approach)
- Fix some error-lookalike messages in kernel log (aka prevent uw feature
  id when interface not available)
* Mon Dec 19 2022 C Sandberg <tux@tuxedocomputers.com> 3.1.2-1
- Enables dynamic boost (max offset) for certain devices needing sw ctl
- Adds charging profile interface for devices supporting charging profiles
- Adds charging priority interface for devices supporting USB-C PD charging
  priority setting
* Mon Oct 17 2022 C Sandberg <tux@tuxedocomputers.com> 3.1.1-1
- Reenable fans-off for some devices that got it turned of as a temporary workaround
- Fix default fan curve not being reenabled when tccd is stopped
* Mon Oct 10 2022 C Sandberg <tux@tuxedocomputers.com> 3.1.0-1
- Add power profiles and tdp functionality (uw)
* Thu Oct 06 2022 C Sandberg <tux@tuxedocomputers.com> 3.0.11-1
- Introduce alternative fan control (uw)
- Fan control parameters from driver "has fan off" and "min fan speed"
- Fixes missing/broken fan control on newer devices
* Thu Apr 28 2022 C Sandberg <tux@tuxedocomputers.com> 3.0.10-1
- Add Stellaris Intel gen 4 lightbar support
- Default lightbar to off
* Mon Oct 10 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.9-1
- Add IBS15v6 & IBS17v6 new module name to perf. prof workaround
- Interface modularization (uw)
- Fix Pulse14/15 gen 1 keyboard backlight ctrl dissapearing
* Fri Jul 9 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.8-1
- Add IBS14v6 to perf. prof workaround
* Thu Jun 24 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.7-1
- Add new Polaris devices gen 2 & gen 3 keyb bl support
- Add Stellaris (gen3) lightbar support
- Fix kernel 5.13 build issue (from github BlackIkeEagle)
- Add another Fusion lightbar ID (from github ArlindoFNeto)
* Mon Jun 07 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.6-1
- Add tuxedo-io performance profile set (cl)
* Fri Apr 23 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.5-1
- Add NS50MU to perf. profile workaround
- Add EDUBOOK1502 to perf. profile workaround
- Add XP gen 11 & 12 to perf. profile workaround
- Clean-up cl driver state init (should fix some init color issues)
* Fri Mar 19 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.4-1
- Fixed various possible race conditions on driver init
- Added IBS14v5 to perf. profile workaround
- Added new Aura board name to perf. profile workaround
- Fixed non-initialized firmware fan curve for silent mode (UW)
- Changed default perf. profile to balanced (UW)
* Fri Mar 5 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.3-1
- Added XP14 to perf. profile workaround
* Fri Jan 29 2021 C Sandberg <tux@tuxedocomputers.com> 3.0.2-1
- Fixed clevo keyboard init order
- Added Aura perf. profile workaround
* Mon Dec 21 2020 C Sandberg <tux@tuxedocomputers.com> 3.0.1-1
- Added device support (Trinity)
- Fixed uw fan ramp up issues to some extent (workaround)
* Wed Dec 9 2020 C Sandberg <tux@tuxedocomputers.com> 3.0.0-1
- Changed structure of clevo interfaces
- Added separate clevo-wmi module with existing functionality
- Added clevo-acpi module with implementation of the "new" clevo ACPI interface
- Added tuxedo-io module (former tuxedo-cc-wmi) into package
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
