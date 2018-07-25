import sys

from PySide2.QtWidgets import QApplication, QLabel

def main():
    app = QApplication([])
    label = QLabel("Hello Pearl")
    label.show()
    sys.exit(app.exec_())

