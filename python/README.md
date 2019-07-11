# Table of Content
- <a href="#description">Description</a>
- <a href="#install">Install</a>

# Description <a name="description"></a>
DBUS services for Tuxedo Kerboard kernel module.
The goal is to provide all methods needed for all desklets/applets for Gnome.
Why? because Sysfs must not be let written by users except root. So if you want to develop some programs, dialog via DBus is the best way.
![Image of tuxedo_keyboard](tuxedo_keyboard_dbus.jpg)

# Install <a name="building"></a>

## Dependencies
- working with python2/python3
- dbus python module (already installed)

## Launch DBus service at boot
Copy files :
```sh
sudo su
cp tuxedo_keyboard_dbus.service /usr/lib/systemd/system/
cp tuxedo_keyboard_dbus.py /usr/sbin/
```

Now you can start or stop dbus service for tuxedo_keyboard with:
```sh
sudo systemctl start tuxedo_keyboard_dbus.service
sudo systemctl stop tuxedo_keyboard_dbus.service
```

Enable at boot:
```sh
sudo systemctl enable tuxedo_keyboard_dbus.service
```

## Authorized DBus service access for user
copy dbus config file :
```sh
sudo su
cp org.tuxedo.keyboard.conf /etc/dbus-1/system.d/
```
# Test
Launch testing soft
```sh
sudo su
python tuxedo_keyboard.py
```
