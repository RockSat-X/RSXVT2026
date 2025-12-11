from PySide6.QtWidgets import QGraphicsRectItem
from PySide6.QtGui import QBrush, QColor, QPen
from PySide6.QtCore import Qt, Signal, QObject


class PinSignals(QObject):
    clicked = Signal(object)   # emits the PinItem itself


class PinItem(QGraphicsRectItem):
    def __init__(self, x, y, name, side=None, functions=None):
        super().__init__(x, y, 20, 20)
        self.name = name
        self.side = side
        self.functions = functions or []
        self.selected = False
        self.hovered = False
        self.assigned_signal = None
        self.assigned_function = None
        self.signals = PinSignals()
        self.update_tooltip()
    def update_tooltip(self):
        # # Show pin name, functions, and assignment in tooltip
        # tooltip_lines = [f"<b>{self.name}</b>"]
        # if self.functions:
        #     tooltip_lines.append("<u>Functions:</u>")
        #     for f in self.functions:
        #         tooltip_lines.append(f" • {f}")
        # if self.assigned_function:
        #     tooltip_lines.append(f"<br><b>Assigned:</b> {self.assigned_function}")
        # self.setToolTip("<br>".join(tooltip_lines))
        # self.setAcceptHoverEvents(True)
        
        # Disable tooltip and hover events
        self.setToolTip("")
        self.setAcceptHoverEvents(False)
        self.update_color()

    def update_color(self):
        # Fill color reflects selection and assignment
        if self.assigned_signal:
            self.setBrush(QBrush(QColor(120, 255, 120)))
        elif self.selected:
            self.setBrush(QBrush(QColor(120, 220, 120)))
        elif self.hovered:
            self.setBrush(QBrush(QColor(200, 200, 255)))
        else:
            self.setBrush(QBrush(QColor(200, 200, 200)))
        self.setPen(QPen(Qt.black, 1))

    def mousePressEvent(self, event):
        self.signals.clicked.emit(self)
        super().mousePressEvent(event)

    def hoverEnterEvent(self, event):
        self.hovered = True
        self.update_color()

    def hoverLeaveEvent(self, event):
        self.hovered = False
        self.update_color()

    # For assignment mode:
    def supports_function(self, fn):
        return fn in self.functions

    def set_assigned_signal(self, sig):
        self.assigned_signal = sig
        self.update_color()
