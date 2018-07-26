# User interface for Pearl program.

from PySide2 import QtCore, QtWidgets
import matplotlib
matplotlib.use('Qt5Agg')
matplotlib.rcParams['backend.qt5']='PySide2'
from matplotlib.animation import FuncAnimation
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.cm import get_cmap
from matplotlib.figure import Figure
from matplotlib.gridspec import GridSpec
from matplotlib.ticker import MultipleLocator, FuncFormatter
import numpy as np

# Add global version number/name
VERSION = 'Pearl v0.1'

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        """Set up the main window."""
        super(MainWindow, self).__init__()
        self.devices = None  # Should be 'devices'
        self.openDevice = None
        main = QtWidgets.QWidget()
        main.setLayout(self._setupMainLayout())
        self.setCentralWidget(main)
        self.status = self.statusBar()
        self.setGeometry(10, 10, 1024, 768)
        self.setWindowTitle(VERSION)
        self.show()

    def _setupMainLayout(self):
        controls = QtWidgets.QVBoxLayout()
        controls.addWidget(QtWidgets.QLabel('<h3>%s</h3>'%VERSION))
        author = QtWidgets.QLabel('by <a href="https://github.com/TeamUTC-InternProject/">Team Underwater Treasure Chest</a>')
        author.setOpenExternalLinks(True)
        controls.addWidget(author)
        controls.addSpacing(10)
        layout = QtWidgets.QHBoxLayout()
        layout.addLayout(controls)
        return layout

