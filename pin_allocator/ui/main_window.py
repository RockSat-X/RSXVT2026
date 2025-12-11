from PySide6.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QFrame, QLabel, QPushButton, QComboBox
from PySide6.QtCore import Qt
from .mcu_view import MCUView

class MainWindow(QWidget):

    def __init__(self):
        super().__init__()

        self.setWindowTitle("MCU Pin Allocator - RockSat-X")
        self.setMinimumSize(1100, 700)
        self.mcu_view = MCUView(json_path="data_py/stm32_example.json")

        layout = QHBoxLayout(self)

        # ----- Left Panel -----
        controls = QVBoxLayout()
        controls.setAlignment(Qt.AlignTop)


        # Pin info widgets
        self.pin_info_label = QLabel("Select a pin to see details.")
        self.pin_info_label.setWordWrap(True)
        controls.addWidget(self.pin_info_label)

        # Dropdown for assigning function
        self.assign_combo = QComboBox()
        self.assign_combo.setEnabled(False)
        self.assign_combo.currentIndexChanged.connect(self.assign_function_to_pin)
        controls.addWidget(QLabel("Assign Function:"))
        controls.addWidget(self.assign_combo)

        self.selected_pin = None

        control_frame = QFrame()
        control_frame.setLayout(controls)
        control_frame.setFrameShape(QFrame.StyledPanel)
        control_frame.setMinimumWidth(250)

        # ----- Right Panel -----
        self.mcu_view.mcu.pin_clicked_callback = self.on_pin_clicked

        layout.addWidget(control_frame)
        layout.addWidget(self.mcu_view, stretch=1)



    def on_pin_clicked(self, pin_item):
        # Deselect all pins first
        for pin in self.mcu_view.mcu.pin_items:
            if pin is not pin_item and pin.selected:
                pin.selected = False
                pin.update_color()
        # Select the clicked pin (toggle off if already selected)
        if not pin_item.selected:
            pin_item.selected = True
            pin_item.update_color()
            # Update pin info panel with assigned function highlighted
            info = f"<b>Pin:</b> {pin_item.name}<br>"
            info += f"<b>Side:</b> {pin_item.side}<br>"
            if pin_item.functions:
                info += "<b>Functions:</b><ul>"
                for fn in pin_item.functions:
                    if getattr(pin_item, 'assigned_function', None) == fn:
                        info += f"<li><b><span style='color:green'>{fn}</span></b></li>"
                    else:
                        info += f"<li>{fn}</li>"
                info += "</ul>"
            else:
                info += "<b>Functions:</b> None"
            self.pin_info_label.setText(info)
            # Update combo box
            self.assign_combo.blockSignals(True)
            self.assign_combo.clear()
            if pin_item.functions:
                self.assign_combo.addItems(["(Unassigned)"] + pin_item.functions)
                self.assign_combo.setEnabled(True)
                if getattr(pin_item, 'assigned_function', None):
                    idx = pin_item.functions.index(pin_item.assigned_function) + 1
                    self.assign_combo.setCurrentIndex(idx)
                else:
                    self.assign_combo.setCurrentIndex(0)
            else:
                self.assign_combo.setEnabled(False)
            self.assign_combo.blockSignals(False)
            self.selected_pin = pin_item
        else:
            pin_item.selected = False
            pin_item.update_color()
            self.pin_info_label.setText("Select a pin to see details.")
            self.assign_combo.setEnabled(False)
            self.selected_pin = None

    def assign_function_to_pin(self, idx):
        pin_item = self.selected_pin
        if not pin_item or not pin_item.functions:
            return
        if idx == 0:
            print(f"Pin '{pin_item.name}' unassigned (previously '{pin_item.assigned_function}')")
            pin_item.assigned_function = None
        else:
            print(f"Pin '{pin_item.name}' assigned to '{pin_item.functions[idx-1]}' (previously '{pin_item.assigned_function}')")
            pin_item.assigned_function = pin_item.functions[idx-1]
        pin_item.update_tooltip()
        # Update label color for this pin
        mcu = self.mcu_view.mcu
        if hasattr(mcu, 'pin_labels') and pin_item.name in mcu.pin_labels:
            label = mcu.pin_labels[pin_item.name]
            if pin_item.assigned_function:
                label.setBrush(Qt.green)
            else:
                label.setBrush(Qt.black)
        # Update info label and highlight current selection
        info = f"<b>Pin:</b> {pin_item.name}<br>"
        info += f"<b>Side:</b> {pin_item.side}<br>"
        if pin_item.functions:
            info += "<b>Functions:</b><ul>"
            for fn in pin_item.functions:
                if pin_item.assigned_function == fn:
                    info += f"<li><b><span style='color:green'>{fn}</span></b></li>"
                else:
                    info += f"<li>{fn}</li>"
            info += "</ul>"
        else:
            info += "<b>Functions:</b> None"
        self.pin_info_label.setText(info)
        # Save assignment
        self.save_assignments()

    def save_assignments(self):
        import json
        # Gather assignments
        assignments = {}
        for pin in self.mcu_view.mcu.pin_items:
            if hasattr(pin, 'assigned_function') and pin.assigned_function:
                assignments[pin.name] = pin.assigned_function
        # Load original JSON
        json_path = "data_py/stm32_example.json"
        with open(json_path, "r") as f:
            data = json.load(f)
        # Write assignments into pins
        for pin in data["pins"]:
            if pin["name"] in assignments:
                pin["assigned_function"] = assignments[pin["name"]]
            elif "assigned_function" in pin:
                del pin["assigned_function"]
        # Overwrite the original file
        with open(json_path, "w") as f:
            json.dump(data, f, indent=4)


