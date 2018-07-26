import sys

from PySide2.QtWidgets import QApplication, QLabel

from . pearlUI import MainWindow

def main():
    app = QApplication([])
    ui = MainWindow()
    sys.exit(app.exec_())

