import time



# Although the `pyb` module seems to be deprecated
# and is replaced by `machine`, it is the only one
# with support for CRC. It seems to be sufficient
# in getting things to work, however.
#
# @/url:`github.com/openmv/openmv-doc/issues/24#issuecomment-2676434920`.

import pyb



################################################################################



spi = pyb.SPI(
    2,
    mode     = pyb.SPI.MASTER, # Master because it's the bottleneck.
    baudrate = 600_000,        # @/`OpenMV SPI Baud`: Coupled.
    polarity = 1,              # If 1, SCK is high when idle.
    phase    = 0,              # If 0, data is sampled on first clock edge.
    crc      = 0x107           # @/`OpenMV CRC Polynomial`:.
)

nss = pyb.Pin(
    'P3',
    pyb.Pin.OUT,
    value = True,
)

led_red   = pyb.LED(1)
led_green = pyb.LED(2)
led_blue  = pyb.LED(3)



################################################################################



iteration = 0

while True:



    # Send data to vehicle flight computer.
    #
    # @/`OpenMV SPI Block Size`: Coupled.
    #
    # Note that the amount of data (excluding the CRC)
    # should be what the vehicle flight computer be expecting.

    iteration += 1
    iteration %= 256

    try:

        nss(False)

        block = bytes([iteration]) + b"BCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?"

        spi.write(block)

    finally:

        nss(True)



    # TODO Do other stuff.

    led_red.toggle()

    time.sleep(0.1)
