# Spectrogram.py
# Copyright 2013 Tony DiCola (tony@tonydicola.com)

# Program to display a real-time spectrogram of data read from a device on the serial port.
# Depends on the following python libraries:
# - PySides (and Qt4)
# - Matplotlib
# - Numpy
# - PySerial
# For PySides, Matplotlib, and Numpy it is _highly_ recommended to install a prebuilt
# python distribution for scientific computing such as Anaconda or Enthought Canopy!

import sys

from PyQt5 import QtWidgets

from . SpectrogramUI import *
from . SerialPortDevice import *

def pearl_main():
    app = QtWidgets.QApplication(sys.argv)
    devices = enumerate_devices()
    ui = MainWindow(devices)
    sys.exit(app.exec_())
