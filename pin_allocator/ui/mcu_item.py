from PySide6.QtWidgets import QGraphicsRectItem, QGraphicsSimpleTextItem
from PySide6.QtGui import QPen, QColor, QBrush
from PySide6.QtCore import Qt
from .pin_item import PinItem


class MCUItem(QGraphicsRectItem):
    def __init__(self, width, height, scene, pin_data=None, mcu_name=None):
        super().__init__(0, 0, width, height)

        self._scene = scene
        self.pin_data = pin_data or []
        self.pin_clicked_callback = None
        self.pin_items = []  # List of all PinItem objects

        self.setPen(QPen(Qt.black, 3))
        self.setBrush(QBrush(QColor(240, 240, 240)))

        # Add MCU name in the rectangle
        if mcu_name:
            name_label = QGraphicsSimpleTextItem(mcu_name)
            font = name_label.font()
            font.setPointSize(14)
            name_label.setFont(font)
            rect = self.rect()
            name_rect = name_label.boundingRect()
            # Center the name in the MCU rectangle
            name_label.setPos(
                rect.width() / 2 - name_rect.width() / 2,
                rect.height() / 2 - name_rect.height() / 2
            )
            name_label.setZValue(self.zValue() + 1)  # Ensure name is above rectangle
            self._scene.addItem(name_label)

        self.create_pins()

    def create_pins(self):
        pin_size = 20
        margin = 10

        # Group pins by side for placement
        top_pins    = [p for p in self.pin_data if p["side"] == "top"]
        bottom_pins = [p for p in self.pin_data if p["side"] == "bottom"]
        left_pins   = [p for p in self.pin_data if p["side"] == "left"]
        right_pins  = [p for p in self.pin_data if p["side"] == "right"]

        # Helper for auto-spacing pins along a side
        def calc_positions(count, total_length):
            if count <= 1:
                return [total_length / 2]
            spacing = total_length / (count + 1)
            return [(i + 1) * spacing for i in range(count)]

        # Calculate pin positions for each side
        top_y = -pin_size - margin
        bottom_y = self.rect().height() + margin
        left_x = -pin_size - margin
        right_x = self.rect().width() + margin

        # Place pins on each side
        # TOP SIDE
        x_positions = calc_positions(len(top_pins), self.rect().width())
        for pin, x in zip(top_pins, x_positions):
            px = x - pin_size/2
            py = top_y
            self._add_pin(pin["name"], px, py, side="top", functions=pin.get("functions", []))

        # BOTTOM SIDE
        x_positions = calc_positions(len(bottom_pins), self.rect().width())
        for pin, x in zip(bottom_pins, x_positions):
            px = x - pin_size/2
            py = bottom_y
            self._add_pin(pin["name"], px, py, side="bottom", functions=pin.get("functions", []))

        # LEFT SIDE
        y_positions = calc_positions(len(left_pins), self.rect().height())
        for pin, y in zip(left_pins, y_positions):
            px = left_x
            py = y - pin_size/2
            self._add_pin(pin["name"], px, py, side="left", functions=pin.get("functions", []))

        # RIGHT SIDE
        y_positions = calc_positions(len(right_pins), self.rect().height())
        for pin, y in zip(right_pins, y_positions):
            px = right_x
            py = y - pin_size/2
            self._add_pin(pin["name"], px, py, side="right", functions=pin.get("functions", []))

    def _add_pin(self, name, x, y, side, functions=None, assigned_function=None):
        functions = functions or []

        # Find assigned_function from pin_data
        assigned_function = None
        for pin in self.pin_data:
            if pin["name"] == name:
                assigned_function = pin.get("assigned_function", None)
                break

        pin_item = PinItem(x, y, name, side=side, functions=functions)
        pin_item.assigned_function = assigned_function
        pin_item.update_tooltip()
        self._scene.addItem(pin_item)
        self.pin_items.append(pin_item)

        # Add label and highlight if assigned
        label_offset = 25
        label = QGraphicsSimpleTextItem(name)
        if assigned_function:
            label.setBrush(QColor(0, 180, 0))  # green
        else:
            label.setBrush(QColor(0, 0, 0))  # black
        self._scene.addItem(label)
        # Store label reference for later updates (for live highlighting)
        if not hasattr(self, 'pin_labels'):
            self.pin_labels = {}
        self.pin_labels[name] = label

        # Connect pin click signal to callback
        pin_item.signals.clicked.connect(self._on_pin_clicked)

        if side == "top":
            label.setPos(x, y - label_offset)
        elif side == "bottom":
            label.setPos(x, y + label_offset)
        elif side == "left":
            label.setPos(x - label_offset, y)
        elif side == "right":
            label.setPos(x + label_offset, y)

    def _on_pin_clicked(self, pin_item):
        # Forward pin click to MainWindow
        if self.pin_clicked_callback:
            self.pin_clicked_callback(pin_item)