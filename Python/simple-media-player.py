import os

import gi

gi.require_version('Gst', '1.0')
gi.require_version('Gtk', '3.0')
from gi.repository import Gst, Gtk, GObject


class MediaPlayer(Gtk.Window):
    def __init__(self):
        window = Gtk.Window(Gtk.WindowType.TOPLEVEL)
        window.set_default_size(300, -1)
        window.connect("delete-event", Gtk.main_quit)

        self.box = Gtk.VBox()
        window.add(self.box)

        self.entry = Gtk.Entry()
        self.entry.set_tooltip_text("Relative path to audio/video file")
        self.box.pack_start(self.entry, False, True, 0)

        self.button = Gtk.Button("Start")
        self.button.connect("clicked", self.start_stop)
        self.box.add(self.button)
        window.show_all()

        self.player = Gst.ElementFactory.make("playbin", "player")
        bus = self.player.get_bus()
        bus.add_signal_watch()
        bus.connect("message", self.on_message)

    def start_stop(self, w):
        if self.button.get_label() == "Start":
            path = self.entry.get_text().strip()

            if os.path.isfile(path):
                path = os.path.realpath(path)
                self.button.set_label("Stop")
                self.player.set_property("uri", "file://" + path)
                self.player.set_state(Gst.State.PLAYING)

        else:
            self.player.set_state(Gst.State.NULL)
            self.button.set_label("Start")

    def on_message(self, bus, message):
        t = message.type

        if t == Gst.MessageType.EOS:
            self.player.set_state(Gst.State.NULL)
            self.button.set_label("Start")
        elif t == Gst.MessageType.ERROR:
            self.player.set_state(Gst.State.NULL)
            err, debug = message.parse_error()
            print("Error: %s" % err, debug)
            self.button.set_label("Start")


Gst.init(None)
MediaPlayer()
GObject.threads_init()
Gtk.main()
