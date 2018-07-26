import sys

from PyQt5.QtWidgets import QApplication

from . pearlUI import MainWindow

def main():
    app = QApplication([])
    ui = MainWindow([])
    sys.exit(app.exec_())

