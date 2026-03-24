import sensor, image, time, os, gc, micropython

# Configuration.
FRAME_DELAY_MS = 10

# Curve fitting.
MIN_POINTS     = 20
OUTLIER_PASSES = 3   # Number of outlier rejection iterations
OUTLIER_MULT   = 2.0 # Reject points with residual > OUTLIER_MULT * median_residual
DRAW_STEP      = 3   # Pixel step when drawing the fitted curve

# Sensors.
sensor.reset()
sensor.set_framesize(sensor.QQVGA)
sensor.set_pixformat(sensor.RGB565)



################################################################################
#
# TODO Document.
#


micropython.alloc_emergency_exception_buf(200)



################################################################################



clock = time.clock()



################################################################################
#
# RANSAC.
#



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



################################################################################
#
# TODO Explain.
#



working_framebuffer = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)
colored_framebuffer = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.RGB565   )



################################################################################
#
# Perform CVT algorithm on pre-existing sample frames.
#
# TODO Enable-switch?
#



def process_sample_frame(sample_frame_file_path):



    ########################################
    #
    # TODO Explain.
    #

    gc.collect()
    clock.tick()



    ########################################
    #
    # Load the sample frame, which may fail
    # if the image we're loading takes up
    # too much memory.
    #

    try:

        working_framebuffer.draw_image(
            image = image.Image(sample_frame_file_path),
            x     = 0,
            y     = 0,
            hint  = image.SCALE_ASPECT_IGNORE
        )

        colored_framebuffer.draw_image(
            image = image.Image(sample_frame_file_path),
            x     = 0,
            y     = 0,
            hint  = image.SCALE_ASPECT_IGNORE
        )

    except Exception as error:
        return f'Exception :: `{error}`.'



    ########################################
    #
    # Check minimum brightness.
    # TODO Think about edge-cases.
    #

    mean_brightness = working_framebuffer.get_statistics().mean()

    if not (10 <= mean_brightness <= 200):
        return 'Rejected :: Not within expected brightness.'



    ########################################
    #
    # Apply blur.
    # TODO By what amount?
    #

    working_framebuffer.mean(1)



    ########################################
    #
    # Determine orientation of the horizon
    # based on the brightness gradient.
    #
    # TODO This is making the assumption that the curvature of the horizon is small
    #      enough that it can be approximated as a rectangular strip.
    #

    STRIP_THICKNESS = 8



    def find_peak_delta(*, step, xs, f, use_abs):

        points = tuple(
            (x, f(x))
            for x in xs
        )

        peak_x     = None
        peak_delta = None

        for i in range(len(points) - step):

            current_x, current_value = points[i       ]
            next_x   , next_value    = points[i + step]

            current_delta = next_value - current_value

            if peak_delta is None or (
                abs(peak_delta) < abs(current_delta)
                if use_abs else # TODO Get rid of.
                peak_delta < current_delta
            ):
                peak_x     = (current_x + next_x) / 2
                peak_delta = current_delta

        return peak_x, peak_delta



    horizontal_gradient_position, largest_horizontal_gradient = find_peak_delta(
        step = 1,
        xs   = range(0, working_framebuffer.height() - STRIP_THICKNESS, STRIP_THICKNESS),
        f    = lambda y: working_framebuffer.get_statistics(
            roi = (
                0,
                y,
                working_framebuffer.width(),
                STRIP_THICKNESS
            )
        ).mean(),
        use_abs = True,
    )

    vertical_gradient_position, largest_vertical_gradient = find_peak_delta(
        step = 1,
        xs   = range(0, working_framebuffer.width() - STRIP_THICKNESS, STRIP_THICKNESS),
        f    = lambda x: working_framebuffer.get_statistics(
            roi = (
                x,
                0,
                STRIP_THICKNESS,
                working_framebuffer.height()
            )
        ).mean(),
        use_abs = True,
    )

    if abs(largest_horizontal_gradient) > abs(largest_vertical_gradient):

        largest_gradient = largest_horizontal_gradient
        coarse_position  = round(horizontal_gradient_position)
        orientation      = 'h'

        colored_framebuffer.draw_line(
            0,
            coarse_position,
            colored_framebuffer.width(),
            coarse_position,
            color     = (0, 255, 255),
            thickness = STRIP_THICKNESS,
        )

    else:
        largest_gradient = largest_vertical_gradient
        coarse_position  = round(vertical_gradient_position)
        orientation      = 'v'

        colored_framebuffer.draw_line(
            coarse_position,
            0,
            coarse_position,
            colored_framebuffer.height(),
            color     = (0, 255, 255),
            thickness = STRIP_THICKNESS,
        )

    if abs(largest_gradient) < 5:
        return f'Rejected :: Gradient too small.'

    if orientation == 'h':
        if largest_gradient > 0:
            dark_side = 'top'
        else:
            dark_side = 'bottom'
    else:
        if largest_gradient > 0:
            dark_side = 'left'
        else:
            dark_side = 'right'



    ################################################################################



    if False: # TODO.

        ########################################
        #
        # Narrow scan.
        #

        GRADIENT_STEP = 4
        SCAN_SPACING  = 4
        SCAN_BAND     = 150
        MIN_GRADIENT  = 25

        points = []

        if orientation == 'h':

            scan_start = max(coarse_position - SCAN_BAND, GRADIENT_STEP                   )
            scan_end   = min(coarse_position + SCAN_BAND, working_framebuffer.height() - 1)

            for x in range(SCAN_SPACING, working_framebuffer.width() - SCAN_SPACING, SCAN_SPACING):

                best_position, best_gradient = find_peak_delta(
                    step    = GRADIENT_STEP,
                    xs      = range(scan_start + GRADIENT_STEP, scan_end),
                    f       = lambda y: (1 if dark_side == 'top' else -1) * working_framebuffer.get_pixel(x, y),
                    use_abs = False,
                )

                best_position = round(best_position)

                if best_gradient >= MIN_GRADIENT:

                    point_position = best_position

                    if scan_start + GRADIENT_STEP < best_position < scan_end - 1:

                        above_g = working_framebuffer.get_pixel(x, best_position - 1) - working_framebuffer.get_pixel(x, best_position - GRADIENT_STEP - 1)
                        below_g = working_framebuffer.get_pixel(x, best_position + 1) - working_framebuffer.get_pixel(x, best_position - GRADIENT_STEP + 1)

                        if dark_side != 'top':
                            above_g = -above_g
                            below_g = -below_g

                        above_g = max(0, above_g)
                        below_g = max(0, below_g)
                        total = above_g + best_gradient + below_g

                        if total > 0:
                            point_position = (above_g * (best_position - 1) + best_gradient * best_position + below_g * (best_position + 1)) / total

                    points += [(x, point_position)]



        else:

            x_lo   = max(GRADIENT_STEP, coarse_position - SCAN_BAND)
            x_hi   = min(working_framebuffer.width() - 1, coarse_position + SCAN_BAND)
            h_sign = 1 if dark_side == 'left' else -1

            for y in range(SCAN_SPACING, working_framebuffer.height() - SCAN_SPACING, SCAN_SPACING):
                best_g = 0
                best_x = -1
                for x in range(x_lo + GRADIENT_STEP, x_hi):
                    left_v = working_framebuffer.get_pixel(x - GRADIENT_STEP, y)
                    right_v = working_framebuffer.get_pixel(x, y)
                    g = h_sign * (right_v - left_v)
                    if g > best_g:
                        best_g = g
                        best_x = x
                if best_x >= 0 and best_g >= MIN_GRADIENT:

                    if best_x > x_lo + GRADIENT_STEP and best_x < x_hi - 1:
                        left_g = working_framebuffer.get_pixel(best_x - 1, y) - working_framebuffer.get_pixel(best_x - GRADIENT_STEP - 1, y)
                        right_g = working_framebuffer.get_pixel(best_x + 1, y) - working_framebuffer.get_pixel(best_x - GRADIENT_STEP + 1, y)
                        if dark_side != 'left':
                            left_g = -left_g
                            right_g = -right_g

                        # Ignore negative gradients.

                        left_g = max(0, left_g)
                        right_g = max(0, right_g)
                        total = left_g + best_g + right_g
                        if total > 0:
                            refined_x = (left_g * (best_x - 1) + best_g * best_x + right_g * (best_x + 1)) / total
                            points.append((refined_x, y))
                        else:
                            points.append((float(best_x), y))

                    else:
                        points.append((float(best_x), y))



        if len(points) < MIN_POINTS:
            return f'Rejected :: Too few points ({len(points)}).'



        ########################################
        #
        # RANSAC.
        #

        inliers = list(points)

        for _ in range(OUTLIER_PASSES):

            if len(inliers) < MIN_POINTS:
                return f'Rejected :: Too few inliers.'

            coefficients = fit_quadratic(inliers, orientation)
            if coefficients is None:
                return f'Rejected :: `fit_quadratic` failed.'



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
            return f'Rejected :: Too few inliers.'

        coefficients = fit_quadratic(inliers, orientation)

        if coefficients is None:
            return f'Rejected :: `fit_quadratic` failed.'


        ########################################
        #
        # Check a value for quadratic fit.
        #

        a, b, c = coefficients

        if abs(a) > 0.003:
            return f'Rejected :: Too large of a curvature (|a| = {abs(a) :.5f}).'



        ########################################
        #
        # Check inliers.
        #

        ratio = len(inliers) / len(points)

        if ratio < 0.35:
            return f'Rejected :: Inlier ratio too small ({ratio :.2f}).'



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
            return 'Rejected :: Span too small ({span :.0f}px).'



        ########################################
        #
        # Check inlier residual.
        #

        res     = sorted([residual(point, coefficients, orientation) for point in inliers])
        med_res = res[len(res) // 2]

        if med_res > 12.0:
            return f'Rejected :: Median residual too large ({med_res :.1f}).'



        ########################################
        #
        # Check curve crosses the frame.
        #

        if orientation == 'h':
            in_frame = sum(1 for x in range(0, working_framebuffer.width(), 20) if 0 <= int(a * x * x + b * x + c) < working_framebuffer.height())
        else:
            in_frame = sum(1 for y in range(0, working_framebuffer.height(), 15) if 0 <= int(a * y * y + b * y + c) < working_framebuffer.width())

        if in_frame < 3:
            return f'Rejected :: Curve barely crosses frame (only {in_frame} points visible).'



        ########################################
        #
        # Draw results.
        #

        for point in inliers:

            colored_framebuffer.draw_circle(
                round(point[0]),
                round(point[1]),
                3,
                color     = (255, 0, 0),
                thickness = 1,
                fill      = True,
            )



        ########################################
        #
        # Draw the fitted quadratic curve.
        #

        a, b, c = coefficients
        prev    = None

        if orientation == 'h':

            for x in range(0, colored_framebuffer.width(), DRAW_STEP):

                y = round(a*x**2 + b*x + c)

                if 0 <= y < colored_framebuffer.height():

                    if prev:
                        colored_framebuffer.draw_line(
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

            for y in range(0, colored_framebuffer.height(), DRAW_STEP):

                x = round(a*y**2 + b*y + c)

                if 0 <= x < colored_framebuffer.width():

                    if prev:
                        colored_framebuffer.draw_line(
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

        cx = colored_framebuffer.width()  // 2
        cy = colored_framebuffer.height() // 2

        arrow_map = {
            'top':    (cx     , cy + 20, cx     , cy - 20),
            'bottom': (cx     , cy - 20, cx     , cy + 20),
            'left':   (cx + 20, cy     , cx - 20, cy     ),
            'right':  (cx - 20, cy     , cx + 20, cy     ),
        }

        ax1, ay1, ax2, ay2 = arrow_map[dark_side]

        colored_framebuffer.draw_arrow(
            ax1,
            ay1,
            ax2,
            ay2,
            color     = (0, 0, 255),
            thickness = 3,
        )

        a_c, b_c, c_c = coefficients

        return f'{orientation=} {dark_side=} {len(inliers)=} {a_c=:.7f} {b_c=:.4f} {c_c=:.1f}'



################################################################################



SAMPLE_FRAME_DIRECTORY_PATH = 'frames'

sample_frame_file_paths = sorted(
    f'{SAMPLE_FRAME_DIRECTORY_PATH}/{file_name}'
    for file_name in os.listdir(SAMPLE_FRAME_DIRECTORY_PATH)
    if file_name.endswith('.jpg')
)

print(f'Found {len(sample_frame_file_paths)} sample frames.')

for sample_frame_file_path_i, sample_frame_file_path in enumerate(sample_frame_file_paths):

    result_message = process_sample_frame(sample_frame_file_path)

    print(f'[{sample_frame_file_path_i + 1}/{len(sample_frame_file_paths)}] `{sample_frame_file_path}` : {result_message}')

    sensor.snapshot().draw_image(colored_framebuffer, 0, 0)

    time.sleep_ms(FRAME_DELAY_MS)



sensor.snapshot()             # TODO Temporary hack here to make the IDE display the last sample frame;
time.sleep_ms(FRAME_DELAY_MS) # TODO it probably has something to do with deallocation of the previous `sensor.snapshot()` call.

sensor.dealloc_extra_fb()
sensor.dealloc_extra_fb()
print(f'Done. {len(sample_frame_file_paths)} frames processed.')
