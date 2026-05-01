import sensor, image, time, os, gc, micropython, struct, math



# Although the `pyb` module seems to be deprecated
# and is replaced by `machine`, it is the only one
# with support for CRC. It seems to be sufficient
# in getting things to work, however.
#
# @/url:`github.com/openmv/openmv-doc/issues/24#issuecomment-2676434920`.

import pyb



################################################################################
#
# Top-level initializations.
#


micropython.alloc_emergency_exception_buf(200)

USE_SAMPLE_FRAMES = False
SPI_BLOCK_SIZE    = micropython.const(64) # @/`OpenMV SPI Block Size`: Coupled.

clock = time.clock()

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
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QQVGA)

working_framebuffer = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)



################################################################################
#
# Helpers.
#



def scan_peak_delta(*, xs, f):

    points = tuple(
        (x, f(x))
        for x in xs
    )

    peak_delta    = None
    peak_x        = None
    peak_gradient = None

    for i in range(len(points) - 1):

        current_x, current_value = points[i    ]
        next_x   , next_value    = points[i + 1]

        current_delta = next_value - current_value

        if peak_delta is None or peak_delta < current_delta:
            peak_delta    = current_delta
            peak_x        = (current_x + next_x) / 2
            peak_gradient = current_delta / (next_x - current_x)

    return peak_x, peak_gradient



def solve_3x3(matrix, vector):



    # Linear algebra least squares.

    a = [[matrix[0][0], matrix[0][1], matrix[0][2], vector[0]],
         [matrix[1][0], matrix[1][1], matrix[1][2], vector[1]],
         [matrix[2][0], matrix[2][1], matrix[2][2], vector[2]]]

    for col in range(3):
        mx_row = col
        mx_val = abs(a[col][col])
        for row in range(col + 1, 3):
            if abs(a[row][col]) > mx_val:
                mx_val = abs(a[row][col])
                mx_row = row
        if mx_val < 1e-10:
            return None
        if mx_row != col:
            a[col], a[mx_row] = a[mx_row], a[col]

        pivot = a[col][col]
        for row in range(col + 1, 3):
            f = a[row][col] / pivot
            for j in range(col, 4):
                a[row][j] -= f * a[col][j]

    x = [0.0, 0.0, 0.0]
    for i in range(2, -1, -1):
        if abs(a[i][i]) < 1e-10:
            return None
        s = a[i][3]
        for j in range(i + 1, 3):
            s -= a[i][j] * x[j]
        x[i] = s / a[i][i]
    return x



def fit_quadratic(points, orientation):

    if len(points) < 3:
        return None

    s0 = float(len(points))
    s1 = s2 = s3 = s4 = 0.0
    r0 = r1 = r2 = 0.0

    for point in points:
        if orientation == 'h':
            t = point[0]; val = point[1]
        else:
            t = point[1]; val = point[0]
        t2 = t * t
        s1 += t; s2 += t2; s3 += t * t2; s4 += t2 * t2
        r0 += val; r1 += t * val; r2 += t2 * val

    return solve_3x3(
        [[s4, s3, s2], [s3, s2, s1], [s2, s1, s0]],
        [r2, r1, r0]
    )



def residual(point, coefficients, orientation):
    a, b, c = coefficients
    if orientation == 'h':
        t = point[0]
        return abs(point[1] - (a * t * t + b * t + c))
    else:
        t = point[1]
        return abs(point[0] - (a * t * t + b * t + c))



def cubic_roots_of(a, b, c, d): # TODO Check edge cases.

    d0 = b**2 - 3 * a * c
    d1 = 2 * b**3 - 9 * a * b * c + 27 * a**2 * d
    ks = [
        ((d1 + (d1**2 - 4 * d0**3)**0.5) / 2)**(1/3) * ((-1 + (-3)**0.5) / 2)**n
        for n in (0, 1, 2)
    ]

    roots = [
        -1 / (3 * a) * (b + k + d0 / k)
        for k in ks
    ]

    return roots



################################################################################
#
# The actual CVT algorithm.
#



def process_framebuffer():

    global working_framebuffer



    ########################################
    #
    # Check minimum brightness.
    # TODO Think about edge-cases.
    #

    mean_brightness = working_framebuffer.get_statistics().mean()

    if not (10 <= mean_brightness <= 200):
        return (None, 'Rejected :: Not within expected brightness.')



    ########################################
    #
    # Apply blur.
    # TODO By what amount?
    #

    working_framebuffer.mean(1)



    ########################################
    #
    # Divide the image into strips and find
    # the largest change in brightness;
    # this is where we're assuming the
    # horizon will roughly be.
    #
    # TODO This is making the assumption that the curvature of the horizon is small
    #      enough that it can be approximated as a rectangular strip?
    #

    STRIP_THICKNESS = 8



    spacetop_position, spacetop_gradient = scan_peak_delta(
        xs = range(0, working_framebuffer.height() - STRIP_THICKNESS, STRIP_THICKNESS),
        f  = lambda y: working_framebuffer.get_statistics(
            roi = (
                0,
                y,
                working_framebuffer.width(),
                STRIP_THICKNESS
            )
        ).mean(),
    )

    spacebottom_position, spacebottom_gradient = scan_peak_delta(
        xs = reversed(range(0, working_framebuffer.height() - STRIP_THICKNESS, STRIP_THICKNESS)),
        f  = lambda y: working_framebuffer.get_statistics(
            roi = (
                0,
                y,
                working_framebuffer.width(),
                STRIP_THICKNESS
            )
        ).mean(),
    )

    spaceleft_position, spaceleft_gradient = scan_peak_delta(
        xs = range(0, working_framebuffer.width() - STRIP_THICKNESS, STRIP_THICKNESS),
        f  = lambda x: working_framebuffer.get_statistics(
            roi = (
                x,
                0,
                STRIP_THICKNESS,
                working_framebuffer.height()
            )
        ).mean(),
    )

    spaceright_position, spaceright_gradient = scan_peak_delta(
        xs = reversed(range(0, working_framebuffer.width() - STRIP_THICKNESS, STRIP_THICKNESS)),
        f  = lambda x: working_framebuffer.get_statistics(
            roi = (
                x,
                0,
                STRIP_THICKNESS,
                working_framebuffer.height()
            )
        ).mean(),
    )



    coarse_position, coarse_gradient, orientation, dark_side = max(
        (
            (round(spacetop_position   ), spacetop_gradient   , 'h', 'top'   ),
            (round(spacebottom_position), spacebottom_gradient, 'h', 'bottom'),
            (round(spaceleft_position  ), spaceleft_gradient  , 'v', 'left'  ),
            (round(spaceright_position ), spaceright_gradient , 'v', 'right' ),
        ),
        key = lambda t: abs(t[1])
    )

    if abs(coarse_gradient) < 1:
        return (None, f'Rejected :: Gradient too small.')



    ########################################
    #
    # Draw broad scan.
    #

    if False:

        if orientation == 'h':

            sensor.get_fb().draw_line(
                0,
                coarse_position,
                sensor.get_fb().width(),
                coarse_position,
                color     = (0, 255, 255),
                thickness = STRIP_THICKNESS,
            )

        else:

            sensor.get_fb().draw_line(
                coarse_position,
                0,
                coarse_position,
                sensor.get_fb().height(),
                color     = (0, 255, 255),
                thickness = STRIP_THICKNESS,
            )



    ########################################
    #
    # Narrow scan.
    #

    MIN_POINTS    = 20
    SCAN_SPACING  = 4
    SCAN_BAND     = 150
    MIN_GRADIENT  = 6

    points = []

    if orientation == 'h':

        scan_start = max(coarse_position - SCAN_BAND, 0                           )
        scan_end   = min(coarse_position + SCAN_BAND, working_framebuffer.height())

        for x in range(0, working_framebuffer.width(), SCAN_SPACING):

            best_position, best_gradient = scan_peak_delta(
                xs = range(scan_start, scan_end),
                f  = lambda y: (1 if dark_side == 'top' else -1) * working_framebuffer.get_pixel(x, y),
            )

            if abs(best_gradient) >= MIN_GRADIENT:
                points += [(x, round(best_position))]

    else:

        scan_start = max(coarse_position - SCAN_BAND, 0                          )
        scan_end   = min(coarse_position + SCAN_BAND, working_framebuffer.width())

        for y in range(0, working_framebuffer.height(), SCAN_SPACING):

            best_position, best_gradient = scan_peak_delta(
                xs = range(scan_start, scan_end),
                f  = lambda x: (1 if dark_side == 'left' else -1) * working_framebuffer.get_pixel(x, y),
            )

            if abs(best_gradient) >= MIN_GRADIENT:
                points += [(round(best_position), y)]



    if len(points) < MIN_POINTS:
        return (None, f'Rejected :: Too few points ({len(points)}).')



    ########################################
    #
    # RANSAC.
    #

    OUTLIER_PASSES = 3   # Number of outlier rejection iterations
    OUTLIER_MULT   = 2.0 # Reject points with residual > OUTLIER_MULT * median_residual

    inliers = list(points)

    for _ in range(OUTLIER_PASSES):

        if len(inliers) < MIN_POINTS:
            return (None, f'Rejected :: Too few inliers.')

        coefficients = fit_quadratic(inliers, orientation)
        if coefficients is None:
            return (None, f'Rejected :: `fit_quadratic` failed.')



        # Compute residuals.

        res = [residual(point, coefficients, orientation) for point in inliers]



        # Median residual.

        sorted_res = sorted(res)
        med        = sorted_res[len(sorted_res) // 2]
        thresh     = max(OUTLIER_MULT * med + 0.5, 3.0)

        # Keep only inliers.

        new_working = [inliers[i] for i in range(len(inliers)) if res[i] <= thresh]

        if len(new_working) < MIN_POINTS:
            break
        inliers = new_working



    if len(inliers) < MIN_POINTS:
        return (None, f'Rejected :: Too few inliers.')

    coefficients = fit_quadratic(inliers, orientation)

    if coefficients is None:
        return (None, f'Rejected :: `fit_quadratic` failed.')


    ########################################
    #
    # Check a value for quadratic fit.
    #

    a, b, c = coefficients

    if not (0.000001 <= abs(a) < 0.003):
        return (None, f'Rejected :: Curvature not within desired range (|a| = {abs(a) :.5f}).')



    ########################################
    #
    # Check inliers.
    #

    ratio = len(inliers) / len(points)

    if ratio < 0.35:
        return (None, f'Rejected :: Inlier ratio too small ({ratio :.2f}).')



    ########################################
    #
    # Check inlier spread.
    #

    if orientation == 'h':
        coords    = [point[0] for point in inliers]
        frame_dim = working_framebuffer.width()
    else:
        coords    = [point[1] for point in inliers]
        frame_dim = working_framebuffer.height()

    span = max(coords) - min(coords)

    if span < 0.45 * frame_dim:
        return (None, 'Rejected :: Span too small ({span :.0f}px).')



    ########################################
    #
    # Check inlier residual.
    #

    res = sorted([residual(point, coefficients, orientation) for point in inliers])

    # TODO: med_res = res[len(res) // 2]
    mean_res = sum(res)/len(res)

    if mean_res > 2.0:
        return (None, f'Rejected :: Median residual too large ({mean_res :.1f}).') # TODO Mean or median?



    ########################################
    #
    # Check curve crosses the frame.
    #

    if orientation == 'h':
        in_frame = sum(1 for x in range(0, working_framebuffer.width(), 20) if 0 <= int(a * x * x + b * x + c) < working_framebuffer.height())
    else:
        in_frame = sum(1 for y in range(0, working_framebuffer.height(), 15) if 0 <= int(a * y * y + b * y + c) < working_framebuffer.width())

    if in_frame < 3:
        return (None, f'Rejected :: Curve barely crosses frame (only {in_frame} points visible).')



    ########################################
    #
    # Draw inliers.
    #

    if False:

        for point in inliers:

            sensor.get_fb().draw_circle(
                round(point[0]),
                round(point[1]),
                2,
                color     = (255, 0, 0),
                thickness = 1,
                fill      = True,
            )


    ########################################
    #
    # Draw the fitted quadratic curve.
    #

    if False:

        DRAW_STEP = 3   # Pixel step when drawing the fitted curve

        a, b, c = coefficients
        prev    = None

        if orientation == 'h':

            for x in range(0, sensor.get_fb().width(), DRAW_STEP):

                y = round(a*x**2 + b*x + c)

                if 0 <= y < sensor.get_fb().height():

                    if prev:
                        sensor.get_fb().draw_line(
                            prev[0],
                            prev[1],
                            x,
                            y,
                            color     = (0, 255, 0),
                            thickness = 2,
                        )

                    prev = (x, y)

                else:

                    prev = None

        else:

            for y in range(0, sensor.get_fb().height(), DRAW_STEP):

                x = round(a*y**2 + b*y + c)

                if 0 <= x < sensor.get_fb().width():

                    if prev:
                        sensor.get_fb().draw_line(
                            prev[0],
                            prev[1],
                            x,
                            y,
                            color     = (0, 255, 0),
                            thickness = 2,
                        )

                    prev = (x, y)

                else:

                    prev = None



    ########################################
    #
    # Arrow to point to space.
    #

    if False:

        cx = sensor.get_fb().width()  // 2
        cy = sensor.get_fb().height() // 2

        arrow_map = {
            'top':    (cx     , cy + 20, cx     , cy - 20),
            'bottom': (cx     , cy - 20, cx     , cy + 20),
            'left':   (cx + 20, cy     , cx - 20, cy     ),
            'right':  (cx - 20, cy     , cx + 20, cy     ),
        }

        ax1, ay1, ax2, ay2 = arrow_map[dark_side]

        sensor.get_fb().draw_arrow(
            ax1,
            ay1,
            ax2,
            ay2,
            color     = (0, 0, 255),
            thickness = 3,
        )



    a_c, b_c, c_c = coefficients



    ########################################
    #
    # Find closest point to the parabola.
    #

    if orientation == 'h':

        px = sensor.get_fb().width()  // 2
        py = sensor.get_fb().height() // 2

        roots = cubic_roots_of(
            4 * a**2,
            6 * a * b,
            2 * b**2 + 2 - 4 * a * py + 4 * a * c,
            2 * b * c - 2 * px - 2 * b * py,
        )

        root_points = [
            (root.real, a * root.real**2 + b * root.real + c)
            for root in roots
            if abs(root.imag) <= 0.0001
        ]

        slope = 2 * a * px + b

    else:

        px = sensor.get_fb().height() // 2
        py = sensor.get_fb().width()  // 2

        roots = cubic_roots_of(
            4 * a**2,
            6 * a * b,
            2 * b**2 + 2 - 4 * a * py + 4 * a * c,
            2 * b * c - 2 * px - 2 * b * py,
        )

        root_points = [
            (a * root.real**2 + b * root.real + c, root.real)
            for root in roots
            if abs(root.imag) <= 0.0001
        ]

        slope = 2 * a * py + b



    if root_points:

        closest_distance_squared, closest_point = min(
            ((x**2 + y**2), (x, y))
            for x, y in root_points
        )

        error_x                    = (closest_point[0] - sensor.get_fb().width() // 2) * 0.0072
        error_y                    = -(closest_point[1] - sensor.get_fb().height() // 2) * 0.0075
        error_z                    = abs(math.atan(error_y/error_x)-math.pi/2)
        computer_vision_confidence = 1

    else:

        error_x                    = math.nan
        error_y                    = math.nan
        error_z                    = math.nan
        computer_vision_confidence = 0



    ########################################
    #
    # Draw attitude estimate.
    #

    if False:

        sensor.get_fb().draw_line(
            round(sensor.get_fb().width () // 2),
            round(sensor.get_fb().height() // 2),
            round(closest_point[0]),
            round(closest_point[1]),
            (255, 0, 255),
            3
        )

        sensor.get_fb().draw_circle(
            round(sensor.get_fb().width () // 2),
            round(sensor.get_fb().height() // 2),
            3,
            color     = (0, 255, 0),
            thickness = 1,
            fill      = True,
        )

        sensor.get_fb().draw_circle(
            round(closest_point[0]),
            round(closest_point[1]),
            3,
            color     = (255, 0, 0),
            thickness = 1,
            fill      = True,
        )



    ########################################



    return (
        (
            error_x,
            error_y,
            error_z,
            computer_vision_confidence,
        ),
        f'X: {error_z*180/math.pi :8.3f}, Y: {error_y*180/math.pi :8.3f}, Z: {error_x*180/math.pi :8.3f} (deg) {orientation=} {dark_side=} {len(inliers)=} {a_c=:.7f} {b_c=:.4f} {c_c=:.1f}'
    )



################################################################################
#
# Run-time logic.
#



# Find the sample frames.

if USE_SAMPLE_FRAMES:

    SAMPLE_FRAME_DIRECTORY_PATH = 'frames'

    sample_frame_file_paths = sorted(
        f'{SAMPLE_FRAME_DIRECTORY_PATH}/{file_name}'
        for file_name in os.listdir(SAMPLE_FRAME_DIRECTORY_PATH)
        if file_name.endswith('.jpg')
    )

    print(f'Found {len(sample_frame_file_paths)} sample frames.')

    sample_frame_file_path_i = -1 # To be immediately incremented later on.



################################################################################


while True:



    # TODO Explain.

    gc.collect()
    clock.tick()



    # Capture an image.

    snapshot_time = time.time()
    sensor.snapshot()



    # For testing purposes, we can immediately replace the captured image with the next sample frame.

    if USE_SAMPLE_FRAMES:


        sample_frame_file_path_i += 1
        sample_frame_file_path_i %= len(sample_frame_file_paths)
        sample_frame_file_path    = sample_frame_file_paths[sample_frame_file_path_i]

        try:

            sensor.get_fb().draw_image(
                image = image.Image(sample_frame_file_path),
                x     = 0,
                y     = 0,
                hint  = image.SCALE_ASPECT_IGNORE
            )

        except Exception as error:
            print(f'Exception :: `{error}`.')
            continue



    # The framebuffer that's kept by `sensor` will be RGB and
    # will have CVT annotations drawn on it, but for the purpose
    # of CVT processing, we only need a grayscale image, so that'll
    # be copied over to `working_framebuffer`.

    working_framebuffer.draw_image(
        image = sensor.get_fb(),
        x     = 0,
        y     = 0,
        hint  = image.SCALE_ASPECT_IGNORE
    )



    # Do the CVT magic.

    processing_result, processing_message = process_framebuffer()
    
    if False:
        if USE_SAMPLE_FRAMES:
            print(f'[{sample_frame_file_path_i + 1}/{len(sample_frame_file_paths)}] `{sample_frame_file_path}` : {processing_message}')
        else:
            print(f'{processing_message}')



    # Compress the captured and annotated image so we can send it to VFC.

    image_byte_array = sensor.get_fb().compress(quality = 50).bytearray()



    # Send the GNC packet to VFC.

    attitude_yaw, attitude_pitch, attitude_roll, computer_vision_confidence = (
        (
            math.nan,
            math.nan,
            math.nan,
            0,
        )
        if processing_result is None else
        processing_result
    )

    computer_vision_processing_time = time.time() - snapshot_time

    block = struct.pack(
        '<HfffHB',  # @/`OpenMV Packet Format`.
        0,          # @/`OpenMV Sequence Number`.
        attitude_yaw,
        attitude_pitch,
        attitude_roll,
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



    # Heartbeat.

    if processing_result:

        led_red.off()
        led_green.toggle()
        led_blue.off()

    else:

        led_red.off()
        led_green.off()
        led_blue.toggle()
