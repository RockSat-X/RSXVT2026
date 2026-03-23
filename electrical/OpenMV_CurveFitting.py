import sensor, image, time, os, gc, micropython

# Configuration.
FRAME_DELAY_MS = 1000

# Broad pass.
STRIP_HEIGHT    = 8  # Height for ROI scan

# Narrow Pass.
SCAN_SPACING  = 4
SCAN_BAND     = 150  # Area adjacent to broad pass to check
GRADIENT_STEP = 4
MIN_GRADIENT  = 25

# Curve fitting.
MIN_POINTS     = 20
OUTLIER_PASSES = 3   # Number of outlier rejection iterations
OUTLIER_MULT   = 2.0 # Reject points with residual > OUTLIER_MULT * median_residual
DRAW_STEP      = 3   # Pixel step when drawing the fitted curve

# Sensors.
sensor.reset()
sensor.set_framesize(sensor.QVGA)
sensor.set_pixformat(sensor.RGB565)



################################################################################
#
# TODO Document.
#


micropython.alloc_emergency_exception_buf(200)



################################################################################



clock = time.clock()
W     = sensor.width()
H     = sensor.height()



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



################################################################################
#
# Perform CVT algorithm on pre-existing sample frames.
#
# TODO Enable-switch?
#



def process_sample_frame(sample_frame_file_path):



    # TODO Explain.

    gc.collect()
    clock.tick()



    # Load the sample frame, which may fail if the image we're loading takes up too much memory.

    try:

        working_framebuffer.draw_image(
            image = image.Image(sample_frame_file_path),
            x     = 0,
            y     = 0,
            hint  = image.SCALE_ASPECT_IGNORE
        )

    except Exception as error:
        return f'Exception :: `{error}`.'



    # Check minimum brightness.

    mean_brightness = working_framebuffer.get_statistics().mean()

    if not (10 <= mean_brightness <= 200):
        return 'Rejected :: Not within expected brightness.'



    # Apply blur.

    working_framebuffer.mean(1) # TODO By what amount?



    # Horizontal strips.

    best_gradient   = 0
    coarse_position = 0
    orientation     = 'h'

    previous_mean = -1

    for y in range(0, H - STRIP_HEIGHT, STRIP_HEIGHT):

        current_mean = working_framebuffer.get_statistics(roi=(0, y, W, STRIP_HEIGHT)).mean()

        if previous_mean >= 0:

            grad = current_mean - previous_mean

            if abs(grad) > abs(best_gradient):
                best_gradient   = grad
                coarse_position = y + STRIP_HEIGHT // 2
                orientation     = 'h'

        previous_mean = current_mean



    # Vertical strips.

    previous_mean = -1

    for x in range(0, W - STRIP_HEIGHT, STRIP_HEIGHT):

        current_mean = working_framebuffer.get_statistics(roi=(x, 0, STRIP_HEIGHT, H)).mean()

        if previous_mean >= 0:

            grad = current_mean - previous_mean

            if abs(grad) > abs(best_gradient):
                best_gradient   = grad
                coarse_position = x + STRIP_HEIGHT // 2
                orientation     = 'v'

        previous_mean = current_mean

    if abs(best_gradient) < 5:
        return f'Rejected :: Best gradient too small.'



    # Determine which side is space.

    if orientation == 'h':
        if best_gradient > 0:
            dark_side = 'top'
        else:
            dark_side = 'bottom'
    else:
        if best_gradient > 0:
            dark_side = 'left'
        else:
            dark_side = 'right'



    # Narrow scan.

    points = []
    step = GRADIENT_STEP

    if orientation == 'h':

        y_lo   = max(step, coarse_position - SCAN_BAND)
        y_hi   = min(H - 1, coarse_position + SCAN_BAND)
        v_sign = 1 if dark_side == 'top' else -1

        for x in range(SCAN_SPACING, W - SCAN_SPACING, SCAN_SPACING):
            best_g = 0
            best_y = -1
            for y in range(y_lo + step, y_hi):
                above = working_framebuffer.get_pixel(x, y - step)
                below = working_framebuffer.get_pixel(x, y)
                g = v_sign * (below - above)
                if g > best_g:
                    best_g = g
                    best_y = y
            if best_y >= 0 and best_g >= MIN_GRADIENT:

                # Check neighboring pixels.

                if best_y > y_lo + step and best_y < y_hi - 1:
                    above_g = working_framebuffer.get_pixel(x, best_y - 1) - working_framebuffer.get_pixel(x, best_y - step - 1)
                    below_g = working_framebuffer.get_pixel(x, best_y + 1) - working_framebuffer.get_pixel(x, best_y - step + 1)
                    if dark_side != 'top':
                        above_g = -above_g
                        below_g = -below_g

                    # Ignore negative gradients.

                    above_g = max(0, above_g)
                    below_g = max(0, below_g)
                    total = above_g + best_g + below_g
                    if total > 0:
                        refined_y = (above_g * (best_y - 1) + best_g * best_y + below_g * (best_y + 1)) / total
                        points.append((x, refined_y))
                    else:
                        points.append((x, float(best_y)))
                else:
                    points.append((x, float(best_y)))

    else:

        x_lo   = max(step, coarse_position - SCAN_BAND)
        x_hi   = min(W - 1, coarse_position + SCAN_BAND)
        h_sign = 1 if dark_side == 'left' else -1

        for y in range(SCAN_SPACING, H - SCAN_SPACING, SCAN_SPACING):
            best_g = 0
            best_x = -1
            for x in range(x_lo + step, x_hi):
                left_v = working_framebuffer.get_pixel(x - step, y)
                right_v = working_framebuffer.get_pixel(x, y)
                g = h_sign * (right_v - left_v)
                if g > best_g:
                    best_g = g
                    best_x = x
            if best_x >= 0 and best_g >= MIN_GRADIENT:

                if best_x > x_lo + step and best_x < x_hi - 1:
                    left_g = working_framebuffer.get_pixel(best_x - 1, y) - working_framebuffer.get_pixel(best_x - step - 1, y)
                    right_g = working_framebuffer.get_pixel(best_x + 1, y) - working_framebuffer.get_pixel(best_x - step + 1, y)
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



    # RANSAC.

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



    # Check a value for quadratic fit.

    a, b, c = coefficients

    if abs(a) > 0.003:
        return f'Rejected :: Too large of a curvature (|a| = {abs(a) :.5f}).'



    # Check inliers.

    ratio = len(inliers) / len(points)

    if ratio < 0.35:
        return f'Rejected :: Inlier ratio too small ({ratio :.2f}).'



    # Check inlier spread.

    if orientation == 'h':
        coords    = [point[0] for point in inliers]
        frame_dim = W
    else:
        coords    = [point[1] for point in inliers]
        frame_dim = H

    span = max(coords) - min(coords)

    if span < 0.45 * frame_dim:
        return 'Rejected :: Span too small ({span :.0f}px).'



    # Check inlier residual.

    res     = sorted([residual(point, coefficients, orientation) for point in inliers])
    med_res = res[len(res) // 2]

    if med_res > 12.0:
        return f'Rejected :: Median residual too large ({med_res :.1f}).'



    # Check curve crosses the frame.

    if orientation == 'h':
        in_frame = sum(1 for x in range(0, W, 20) if 0 <= int(a * x * x + b * x + c) < H)
    else:
        in_frame = sum(1 for y in range(0, H, 15) if 0 <= int(a * y * y + b * y + c) < W)

    if in_frame < 3:
        return f'Rejected :: Curve barely crosses frame (only {in_frame} points visible).'



    # Draw results.

    for point in inliers:

        working_framebuffer.draw_circle(
            round(point[0] + 0.5),
            round(point[1] + 0.5),
            3,
            color     = (255, 0, 0),
            thickness = 1,
            fill      = True,
        )



    # Draw the fitted quadratic curve.

    a, b, c = coefficients
    prev    = None

    if orientation == 'h':

        for x in range(0, W, DRAW_STEP):

            y = round(a*x**2 + b*x + c)

            if 0 <= y < H:

                if prev:
                    working_framebuffer.draw_line(
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

        for y in range(0, H, DRAW_STEP):

            x = round(a*y**2 + b*y + c)

            if 0 <= x < W:

                if prev:
                    working_framebuffer.draw_line(
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



    # Arrow to point to space.

    cx = W // 2
    cy = H // 2

    arrow_map = {
        'top':    (cx     , cy + 20, cx     , cy - 20),
        'bottom': (cx     , cy - 20, cx     , cy + 20),
        'left':   (cx + 20, cy     , cx - 20, cy     ),
        'right':  (cx - 20, cy     , cx + 20, cy     ),
    }

    ax1, ay1, ax2, ay2 = arrow_map[dark_side]

    working_framebuffer.draw_arrow(
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

    sensor.snapshot().draw_image(working_framebuffer, 0, 0)

    time.sleep_ms(FRAME_DELAY_MS)



sensor.snapshot()             # TODO Temporary hack here to make the IDE display the last sample frame;
time.sleep_ms(FRAME_DELAY_MS) # TODO it probably has something to do with deallocation of the previous `sensor.snapshot()` call.

sensor.dealloc_extra_fb()
sensor.dealloc_extra_fb()
print(f'Done. {len(sample_frame_file_paths)} frames processed.')
