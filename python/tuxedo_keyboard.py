#!/usr/bin/env python3
# tuxedo_keyboard.py
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

import gi
import os
import sys
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, Gio
from dbus import SystemBus

# constants
BUSNAME = 'org.tuxedo.keyboard'
OBJPATH = '/org/tuxedo/keyboard'
CONFIG_FILE = '/etc/modprobe.d/tuxedo_keyboard.conf'
PATH = '/home/YOUR_HOME/.local/share/tuxedo_keyboard/'


class Application(Gtk.Application):
    # main class
    def __init__(self, dbus):
        self.dbus = dbus
        Gtk.Application.__init__(
            self,
            application_id='org.tuxedo.keyboard.settings',
            flags=Gio.ApplicationFlags.FLAGS_NONE)

    def do_activate(self):
        self._builder = Gtk.Builder()
        self._builder.add_from_file(PATH + 'tuxedo_keyboard.glade')
        self.window = self._builder.get_object('tuxedo_keyboard_window')
        self.window.set_application(self)
        self.window.title = 'Tuxedo Keyboard RGB'
        self.window.set_default_size(400,100)
        self.window.set_border_width(5)
        self._buildUI()
        self.window.show_all()

    def do_startup(self):
        Gtk.Application.do_startup(self)

    # init all widgets
    def _buildUI(self):
        # set logo
        self._builder.get_object('logo').set_from_file(
                PATH + 'tuxedo_keyboard.png')
        # init state button widget
        if (self.dbus.Call('state', 'getState')):
            self._builder.get_object('state').set_label('Off')
            self._builder.get_object('state').set_active(True)
        else:
            self._builder.get_object('state').set_label('On')
            self._builder.get_object('state').set_active(False)
        self._builder.get_object('state').connect('toggled', self.setState)
        # brightness hscale widget connect and default value
        self._builder.get_object('brightness').set_value(
            self.dbus.Call('brightness', 'getBrightness'))
        self._builder.get_object('brightness').connect(
            'change-value',
            self.setBrightness)
        # mode spinButton widget connect
        self._builder.get_object('mode').set_active(
                self.dbus.Call('mode', 'getMode'))
        self._builder.get_object('mode').connect('changed', self.setMode)
        # color zone list widget
        self.color = Gdk.Color(0,0,0)
        self._builder.get_object('list_zone').connect(
                'changed',
                self.zoneChanged)
        self.colorSelection = self._builder.get_object('color')
        self.colorSelection.connect('color_changed', self.setColor)
        self.zoneChanged(None) # for init current color for first zone in list
        # check if run as root to save under /etc the current config
        if os.getuid() != 0:
            self._builder.get_object('save').set_label('need to be run as root')
            self._builder.get_object('save').set_sensitive(False)
        else:
            self._builder.get_object('save').connect('pressed', self.save)

    # called when state widget toggle
    def setState(self, *args):
        if (self.dbus.Call('state', 'getState')):
            self.dbus.Call('state', 'setState', 0)
            self._builder.get_object('state').set_label('On')
        else:
            self.dbus.Call('state', 'setState', 1)
            self._builder.get_object('state').set_label('Off')

    # called when brightness widget event
    def setBrightness(self, widget, *args):
        currentBrightness = self._builder.get_object('brightness').get_value()
        self.dbus.Call('brightness', 'setBrightness', int(currentBrightness))

    # called when mode changed
    def setMode(self, widget, *args):
        currentMode = self._builder.get_object('mode').get_active()
        self.dbus.Call('mode', 'setMode', currentMode)

    # called when color changed
    def setColor(self, *args):
        selectedColor = self.colorSelection.get_current_color()
        red = self._32to8bits(selectedColor.red)
        green = self._32to8bits(selectedColor.green)
        blue = self._32to8bits(selectedColor.blue)
        index = self._builder.get_object('list_zone').get_active()
        colorZone = self._builder.get_object('list_zone').get_model()[index][0]
        self.dbus.Call(
                '{}_color'.format(colorZone).lower(),
                'set{}Color'.format(colorZone),
                red,green, blue
        )
        self._builder.get_object('mode').set_active(0)# set custom mode

    # called when zone color changed
    def zoneChanged(self, *args):
        index = self._builder.get_object('list_zone').get_active()
        colorZone = self._builder.get_object('list_zone').get_model()[index][0]
        currentColor = self.dbus.Call(
            '{}_color'.format(colorZone).lower(),
            'get{}Color'.format(colorZone)
        )
        self.color.red = self._8to32bits(int(currentColor[0]))
        self.color.green = self._8to32bits(int(currentColor[1]))
        self.color.blue = self._8to32bits(int(currentColor[2]))
        self.colorSelection.set_current_color(self.color)
        self.colorSelection.set_previous_color(self.color)
        self.colorSelection.set_has_palette(self.color)

    # save current config
    def save(self, *args):
        index = self._builder.get_object('list_zone').get_active()
        colorZone = self._builder.get_object('list_zone').get_model()[index][0]
        currentColor = self.dbus.Call(
            '{}_color'.format(colorZone).lower(),
            'get{}Color'.format(colorZone)
        )
        file = open(CONFIG_FILE, 'w')
        file.write('options tuxedo_keyboard state={} mode={} color_left={} color_center={} color_right={}'.format(
                int(self.dbus.Call('state', 'getState')),
                int(self._builder.get_object('mode').get_active()),
                self._rgbToHexa(self.dbus.Call('left_color', 'getLeftColor')),
                self._rgbToHexa(self.dbus.Call(
                    'center_color',
                    'getCenterColor')),
                self._rgbToHexa(self.dbus.Call(
                    'right_color',
                    'getRightColor'))
            ))
        file.close()

    # convert 32bits colors to 8bits
    def _32to8bits(self, color):
        return round((color*255)/65535)

    # convert 8bits colors to 32bits
    def _8to32bits(self, color):
        return round((color*65535)/255)

    # convert r,g,b to hex 0x000000
    def _rgbToHexa(self, color):
        return '0x{:0^2}{:0^2}{:0^2}'.format(
            hex(int(color[0]))[2:].upper(),
            hex(int(color[1]))[2:].upper(),
            hex(int(color[2]))[2:].upper()
        )


class TuxedoKeyboard():
    # class to connect to tuxedo_keyboard_dbus services
    # connect to dbus tuxedo_keyboard
    def __init__(self):
        bus = SystemBus()
        self.proxy = bus.get_object(BUSNAME, OBJPATH)

    # map all methods in one function
    def Call(self, interface_name, method, *values):
        if not values:
            # call dbus methods without values (get methods)
            return self.proxy.get_dbus_method(
                    method,
                    '{}.{}'.format(BUSNAME, interface_name))()
        else: # call dbus methods with values (set methods)
            self.proxy.get_dbus_method(
                    method,
                    '{}.{}'.format(BUSNAME, interface_name))(*values)


if __name__ == '__main__':
    # init TuxedoKeyboard dbus class
    tuxedoObject = TuxedoKeyboard()

    app = Application(tuxedoObject)
    app.run(None)
    sys.exit(0)
