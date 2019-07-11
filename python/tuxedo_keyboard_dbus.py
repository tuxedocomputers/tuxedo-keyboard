#!/usr/bin/env python3
# tuxedo_keyboard_dbus.py
#
# By Allard Chris 2019 <lyon.allard.chris@gmail.com>
# Thanks TUXEDO Computers GmbH for your works
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

import dbus
import dbus.service
from gi.repository import GLib
from dbus.mainloop.glib import DBusGMainLoop
import os
import signal
import subprocess
import sys


# constants
MODULE_NAME = 'tuxedo_keyboard'
BUSNAME = 'org.tuxedo.keyboard'
OBJPATH = '/org/tuxedo/keyboard'
FS_BRIGHTNESS = '/sys/devices/platform/tuxedo_keyboard/brightness'
FS_MODE = '/sys/devices/platform/tuxedo_keyboard/mode'
FS_STATE = '/sys/devices/platform/tuxedo_keyboard/state'
FS_LEFT = '/sys/devices/platform/tuxedo_keyboard/color_left'
FS_CENTER = '/sys/devices/platform/tuxedo_keyboard/color_center'
FS_RIGHT = '/sys/devices/platform/tuxedo_keyboard/color_right'
FS_EXTRA = '/sys/devices/platform/tuxedo_keyboard/color_extra'


class Tuxedo_dbus(dbus.service.Object):
    def __init__(self):
        # create dbus service
        connection = dbus.service.BusName(BUSNAME, bus=dbus.SystemBus())
        dbus.service.Object.__init__(self, connection, OBJPATH)

    # state
    ## switch off/on keyboard lights, input(boolean).
    @dbus.service.method(BUSNAME + '.state', in_signature='b')
    def setState(self, state):
        if (state):
            self._write(FS_STATE, '1') # switch on
        else :
            self._write(FS_STATE, '0') # switch off
    ## get current state of keyboard lights, output(boolean).
    @dbus.service.method(BUSNAME + '.state', out_signature='b')
    def getState(self):
        if (self._read(FS_STATE) == '0'): return False
        else: return True

    # brightness
    ## set brightness of keyboard backlights, input(int: 0 to 255)
    @dbus.service.method(BUSNAME + '.brightness', in_signature='y')
    def setBrightness(self, brightness):
        self._write(FS_BRIGHTNESS, self._toStr(brightness))
    ## get current keyboard backlights brightness, output(int: 0 to 255)
    @dbus.service.method(BUSNAME + '.brightness', out_signature='y')
    def getBrightness(self):
        return int(self._read(FS_BRIGHTNESS))

    # mode
    ## set mode (0 to 7), input(int: 0 to 7)
    @dbus.service.method(BUSNAME + '.mode', in_signature='y')
    def setMode(self, mode):
        if (mode < 8): self._write(FS_MODE, self._toStr(mode))
    ## get current mode, output(int: 0 to 7)
    @dbus.service.method(BUSNAME + '.mode', out_signature='y')
    def getMode(self):
        return int(self._read(FS_MODE))

    # Left color
    ## set left_color(R, G, B), input(int, int, int: 0 to 255)
    @dbus.service.method(BUSNAME + '.left_color', in_signature='yyy') # R,G,B
    def setLeftColor(self, red, green, blue):
        self._write(FS_LEFT, str(self._toCol(red, green, blue)))
    ## get curent left_color(R, G, B), output(int, int, int: 0 to 255)
    @dbus.service.method(BUSNAME + '.left_color', out_signature='yyy')
    def getLeftColor(self):
        return self._to3Int(self._read(FS_LEFT))

    # Center color
    ## set center_color (R, G, B), input(int, int, int: 0 to 255)
    @dbus.service.method(BUSNAME + '.center_color', in_signature='yyy') # R,G,B
    def setCenterColor(self, red, green, blue):
        self._write(FS_CENTER, str(self._toCol(red, green, blue)))
    ## get curent center_color(R, G, B), output(int, int, int: 0 to 255)
    @dbus.service.method(BUSNAME + '.center_color', out_signature='yyy')
    def getCenterColor(self):
        return self._to3Int(self._read(FS_CENTER))

    # Right color
    ## set right_color (red, green, blue), input(int, int, int: 0 to 255)
    @dbus.service.method(BUSNAME + '.right_color', in_signature='yyy') # R,G,B
    def setRightColor(self, red, green, blue):
        self._write(FS_RIGHT, str(self._toCol(red, green, blue)))
    ## get curent right_color, output(int, int, int: 0 to 255)
    @dbus.service.method(BUSNAME + '.right_color', out_signature='yyy')
    def getRightColor(self):
        return self._to3Int(self._read(FS_RIGHT))

#    Didn't understand extra region. Currently set at 1 on my computer
#    but i don't have one
#    So remove comments if you want extra_color with dbus
#    # Extra color
#    ## set extra_color (red, green, blue), input(int, int, int: 0 to 255)
#    @dbus.service.method(BUSNAME + '.extra_color', in_signature='yyy') # R,G,B
#    def setExtraColor(self, red, green, blue):
#        self._write(FS_EXTRA, str(self._toCol(red, green, blue)))
#    ## get curent extra_color, output(int, int, int: 0 to 255)
#    @dbus.service.method(BUSNAME + '.extra_color', out_signature='yyy')
#    def getExtraColor(self):
#        return self._to3Int(self._read(FS_EXTRA))

    # convert dbus.bytes to str
    def _toStr(self, val):
        return str(int(val))

    # write into FS files under /sys
    def _write(self, file, val):
        if os.path.isfile(file):
            file = open(file, 'w')
            file.write(val)
            file.close()

    # read values from FS files under /sys
    def _read(self, file):
        if os.path.isfile(file):
            file = open(file, 'r')
            val = file.read().split('\n')[0]
            file.close()
            return val

    # convert 3 int into color value
    def _toCol(self, r, g, b):
        print(type(r))
        return (r << 16) + (g << 8) + b

    # convert str to 3 int color value
    def _to3Int(self, str):
        r = int(str[0:2], 16)
        g = int(str[2:4], 16)
        b = int(str[4:6], 16)
        return (r, g, b)

# exit properly
def signal_handler(_signo, _stack_frame):
    sys.exit(0)

if __name__ == '__main__':
    # exit properly
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # check if module tuxedo_keyboard is loaded
    devnull = open(os.devnull, 'w')
    lsmod = subprocess.Popen(['lsmod'], stdout=subprocess.PIPE)
    grep_lsmod = subprocess.Popen(
            ['grep', MODULE_NAME],
            stdin=lsmod.stdout,
            stdout=devnull,
            stderr=devnull)
    grep_lsmod.communicate()
    if (grep_lsmod.returncode == 1):
        sys.exit("Error: no tuxedo_keyboard module loaded.")
    devnull.close()

    # load dbus services
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    object = Tuxedo_dbus()
    loop = GLib.MainLoop()
    loop.run()
    exit(0)
