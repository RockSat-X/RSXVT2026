import json
from PySide6.QtWidgets import QGraphicsView, QGraphicsScene
from PySide6.QtGui import QPainter
from PySide6.QtCore import Qt
from .mcu_item import MCUItem


class MCUView(QGraphicsView):
    def __init__(self, json_path="data_py/stm32_example.json"):
        super().__init__()

        # Load JSON data_py and ensure all pins have 'assigned_function' field
        with open(json_path, "r+") as f:
            data = json.load(f)
            changed = False
            for pin in data["pins"]:
                if "assigned_function" not in pin:
                    pin["assigned_function"] = None
                    changed = True
            if changed:
                f.seek(0)
                json.dump(data, f, indent=4)
                f.truncate()

        width = data["package"]["width"]
        height = data["package"]["height"]
        pins = data["pins"]
        mcu_name = data.get("mcu_name", None)

        self.scene = QGraphicsScene(self)
        self.setScene(self.scene)
        self.mcu = MCUItem(width, height, self.scene, pin_data=pins, mcu_name=mcu_name)
        self.scene.addItem(self.mcu)
        self.setRenderHint(QPainter.Antialiasing)
        self.setDragMode(QGraphicsView.ScrollHandDrag)

    # Zoom control (Ctrl + mouse wheel)
    def wheelEvent(self, event):
        if event.modifiers() & Qt.ControlModifier:
            factor = 1.12 if event.angleDelta().y() > 0 else 1 / 1.12
            self.scale(factor, factor)
        else:
            super().wheelEvent(event)
