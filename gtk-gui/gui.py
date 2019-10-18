#!/usr/bin/env python

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
import os

class SignalHandler:
    brightness_ctrl = '/sys/devices/platform/tuxedo_keyboard/brightness'
    color_left_ctrl ='/sys/devices/platform/tuxedo_keyboard/color_left'
    color_center_ctrl ='/sys/devices/platform/tuxedo_keyboard/color_center'
    color_right_ctrl ='/sys/devices/platform/tuxedo_keyboard/color_right'
    color_extra_ctrl ='/sys/devices/platform/tuxedo_keyboard/color_extra'
    mode_ctrl ='/sys/devices/platform/tuxedo_keyboard/mode'

    @staticmethod
    def _put(file_name,value):
        f = open(file_name,'w')
        f.write(str(value))
        f.close()

    @staticmethod
    def _get(file_name):
        f = open(file_name,'r')
        value = ''.join(f.readlines())
        f.close()
        return value

    def quit(self, *args, **kwargs):
        Gtk.main_quit()

    def setBrightness(self, event,etype, value):
        value = float(value)
        if value < 0: value = 0
        if value > 100: value = 100
        value = str(int(int(value*255/100)))
        SignalHandler._put(SignalHandler.brightness_ctrl, value)

    def getBrightness(self):
        value = float(SignalHandler._get(SignalHandler.brightness_ctrl))
        value *= 100.
        value /= 255.
        return value

    @staticmethod
    def _rgba_to_hex(rgba):
        r = rgba.red*255
        g = rgba.green*255
        b = rgba.blue*255
        rgb = "0x"+("{:02x}{:02x}{:02x}".format(int(r),int(g),int(b)).upper())
        return rgb

    def setLeft(self, button):
        rgba = button.get_rgba()
        SignalHandler._put(SignalHandler.color_left_ctrl, SignalHandler._rgba_to_hex(rgba))

    def setRight(self, button):
        rgba = button.get_rgba()
        SignalHandler._put(SignalHandler.color_right_ctrl, SignalHandler._rgba_to_hex(rgba))

    def setCenter(self, button):
        rgba = button.get_rgba()
        SignalHandler._put(SignalHandler.color_center_ctrl, SignalHandler._rgba_to_hex(rgba))

    def setExtra(self, button):
        rgba = button.get_rgba()
        SignalHandler._put(SignalHandler.color_extra_ctrl, SignalHandler._rgba_to_hex(rgba))

    def getLeft(self):
        rgb = SignalHandler._get(SignalHandler.color_left_ctrl)
        return (int(rgb[0:2],16),int(rgb[2:4],16),int(rgb[4:6],16))
    def getCenter(self):
        rgb = SignalHandler._get(SignalHandler.color_center_ctrl)
        return (int(rgb[0:2],16),int(rgb[2:4],16),int(rgb[4:6],16))
    def getRight(self):
        rgb = SignalHandler._get(SignalHandler.color_right_ctrl)
        return (int(rgb[0:2],16),int(rgb[2:4],16),int(rgb[4:6],16))
    def getExtra(self):
        rgb = SignalHandler._get(SignalHandler.color_extra_ctrl)
        return (int(rgb[0:2],16),int(rgb[2:4],16),int(rgb[4:6],16))

    def setMode(self,combo):
        SignalHandler._put(SignalHandler.mode_ctrl, combo.get_active())

    def getMode(self):
        return int(SignalHandler._get(SignalHandler.mode_ctrl))



def main():
    sh = SignalHandler()
    builder = Gtk.Builder()
    builder.add_from_file(os.path.dirname(os.path.realpath(__file__)) + "/gui.glade")
    builder.connect_signals(sh)

    window = builder.get_object("window1")

    builder.get_object("scale1").set_value(sh.getBrightness())

    left = builder.get_object("colorbutton1")
    left_rgba = left.get_rgba()
    left_now = sh.getLeft()
    left_rgba.red = left_now[0]/255.
    left_rgba.green = left_now[1]/255.
    left_rgba.blue = left_now[2]/255.
    left.set_rgba(left_rgba)

    center = builder.get_object("colorbutton2")
    center_rgba = center.get_rgba()
    center_now = sh.getCenter()
    center_rgba.red = center_now[0]/255.
    center_rgba.green = center_now[1]/255.
    center_rgba.blue = center_now[2]/255.
    center.set_rgba(center_rgba)

    right = builder.get_object("colorbutton3")
    right_rgba = right.get_rgba()
    right_now = sh.getRight()
    right_rgba.red = right_now[0]/255.
    right_rgba.green = right_now[1]/255.
    right_rgba.blue = right_now[2]/255.
    right.set_rgba(right_rgba)

    extra = builder.get_object("colorbutton4")
    extra_rgba = extra.get_rgba()
    extra_now = sh.getExtra()
    extra_rgba.red = extra_now[0]/255.
    extra_rgba.green = extra_now[1]/255.
    extra_rgba.blue = extra_now[2]/255.
    extra.set_rgba(extra_rgba)

    builder.get_object("combobox1").set_active(sh.getMode())

    window.show_all()
    Gtk.main()

if __name__ == "__main__":
    main()
