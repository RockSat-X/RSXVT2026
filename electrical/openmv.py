import time
import pyb
from pyb import SPI, Pin

spi = SPI(2, SPI.MASTER, baudrate=600000, polarity=1, phase=0, crc=0x7)
cs = Pin("P3", mode=Pin.OUT, value=1)
red_led = pyb.LED(1)
green_led = pyb.LED(2)
blue_led = pyb.LED(3)

while True:

    try:
        cs(0)                               # Select peripheral.
        spi.write(b"12345678")              # Write 8 bytes, and don't care about received data.
    finally:
        cs(1)                               # Deselect peripheral.

    red_led.toggle()
    time.sleep(0.1)
