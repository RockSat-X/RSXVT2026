import time, struct, sensor, micropython



# Although the `pyb` module seems to be deprecated
# and is replaced by `machine`, it is the only one
# with support for CRC. It seems to be sufficient
# in getting things to work, however.
#
# @/url:`github.com/openmv/openmv-doc/issues/24#issuecomment-2676434920`.

import pyb



################################################################################



SPI_BLOCK_SIZE = micropython.const(64) # @/`OpenMV SPI Block Size`: Coupled.

spi = pyb.SPI(
    2,
    mode      = pyb.SPI.MASTER,         # Master because it's the bottleneck.
    prescaler = 153_600_000 // 600_000, # @/`OpenMV SPI Baud`: Coupled; note that prescaler must be 2, 4, 8, ..., 256.
    polarity  = 1,                      # If 1, SCK is high when idle.
    phase     = 0,                      # If 0, data is sampled on first clock edge.
    crc       = 0x107                   # @/`OpenMV CRC Polynomial`:.
)

nss = pyb.Pin(
    'P3',
    pyb.Pin.OUT,
    value = True,
)

led_red   = pyb.LED(1)
led_green = pyb.LED(2)
led_blue  = pyb.LED(3)



sensor.reset()
sensor.set_pixformat(sensor.YUV422)
sensor.set_framesize(sensor.QSIF)



################################################################################



while True:



    # Capture the image that'll be transferred to VFC.

    image_byte_array = sensor.snapshot().compress(quality=50).bytearray()



    # Send the GNC packet to VFC.

    attitude_x                       = 1.0
    attitude_y                       = 2.0
    attitude_z                       = 3.0
    attitude_w                       = 4.0
    computer_vision_processing_time  = 123
    computer_vision_confidence       = 456

    block = struct.pack(
        '<HffffHB',  # @/`OpenMV Packet Format`.
        0,           # @/`OpenMV Sequence Number`.
        attitude_x,
        attitude_y,
        attitude_z,
        attitude_w,
        computer_vision_processing_time,
        computer_vision_confidence,
    )

    block += bytes(SPI_BLOCK_SIZE - len(block))

    nss(False)
    spi.write(block)
    nss(True)



    # Send the image packets to VFC.

    sequence_number  = 1 # @/`OpenMV Sequence Number`.
    image_bytes_sent = 0

    while image_bytes_sent < len(image_byte_array):

        block = struct.pack(
            f'<H{SPI_BLOCK_SIZE - 2}s', # @/`OpenMV Packet Format`.
            sequence_number,
            image_byte_array[image_bytes_sent : image_bytes_sent + (SPI_BLOCK_SIZE - 2)]
        )

        nss(False)
        spi.write(block)
        nss(True)

        sequence_number  += 1
        image_bytes_sent += SPI_BLOCK_SIZE - 2



    # TODO Do other stuff.

    led_green.toggle()

    time.sleep(0.1)
