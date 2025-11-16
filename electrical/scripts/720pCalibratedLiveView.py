import sensor, image, time, pyb

# ------------------------------
# CAMERA SETUP
# ------------------------------
sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)   # Grayscale, could do color, but slower
sensor.set_framesize(sensor.HD)          # 720p resolution (Use FHD for 1080p)
sensor.skip_frames(time=3000)            # Let the camera auto-adjust first

clock = time.clock()



# ------------------------------
# MAIN LOOP
# ------------------------------

while True:
    clock.tick()

    start_time = time.ticks_ms()

    # Capture 720p frame with continuous auto exposure/gain
    img = sensor.snapshot()

    # Apply lens correction
    img.lens_corr(1.3)

    # Overlay FPS text directly onto image (top-left corner)
    img.draw_string(10, 10, "FPS: %.2f" % clock.fps(), scale=2)

    # Optional console print for debugging
    print("FPS:", round(clock.fps(), 1))
