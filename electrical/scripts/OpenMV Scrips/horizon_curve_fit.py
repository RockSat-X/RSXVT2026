import sensor, image, time, os, gc

#Configuration
FRAME_PATH = "/frames"
FRAME_DELAY_MS = 0
MIN_BRIGHTNESS = 10
MAX_BRIGHTNESS = 200

# Broad Pass
STRIP_HEIGHT = 8          # Height for ROI scan
BROAD_THRESHOLD = 5     # Minimum Brightness jump to be considered horizon

# Narrow Pass
SCAN_SPACING = 4
SCAN_BAND = 150            # Area adjacent to broad pass to check
GRADIENT_STEP = 4
MIN_GRADIENT = 25
BLUR_SIZE = 1

# Curve fitting
MIN_POINTS = 20
OUTLIER_PASSES = 3        # Number of outlier rejection iterations
OUTLIER_MULT = 2.0        # Reject points with residual > OUTLIER_MULT * median_residual
DRAW_STEP = 3             # Pixel step when drawing the fitted curve

# Reject bad Curves
MAX_CURVATURE = 0.003
MIN_INLIER_RATIO = 0.35
MIN_SPREAD_RATIO = 0.45
MAX_RESIDUAL = 12.0

# Sensors
sensor.reset()
sensor.set_framesize(sensor.QVGA)
sensor.set_pixformat(sensor.RGB565)
sensor.skip_frames(time=2000)

frame_files = os.listdir(FRAME_PATH)
frames = sorted([f for f in frame_files if f.endswith(".jpg")])
if not frames:
    raise Exception("No .jpg frames in {}".format(FRAME_PATH))
print("Found {} frames".format(len(frames)))

color_fb = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.RGB565)
gray_fb = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)

clock = time.clock()
W = sensor.width()   # 320
H = sensor.height()  # 240


# Broad scan
def broad_find_horizon(img):

    best_gradient = 0
    best_position = 0
    best_orientation = 'h'

    # Horizontal strips
    previous_mean = -1
    for y in range(0, H - STRIP_HEIGHT, STRIP_HEIGHT):
        currrent_mean = img.get_statistics(roi=(0, y, W, STRIP_HEIGHT)).mean()
        if previous_mean >= 0:
            grad = currrent_mean - previous_mean
            if abs(grad) > abs(best_gradient):
                best_gradient = grad
                best_position = y + STRIP_HEIGHT // 2
                best_orientation = 'h'
        previous_mean = currrent_mean

    # Vertical strips
    previous_mean = -1
    for x in range(0, W - STRIP_HEIGHT, STRIP_HEIGHT):
        current_mean = img.get_statistics(roi=(x, 0, STRIP_HEIGHT, H)).mean()
        if previous_mean >= 0:
            grad = current_mean - previous_mean
            if abs(grad) > abs(best_gradient):
                best_gradient = grad
                best_position = x + STRIP_HEIGHT // 2
                best_orientation = 'v'
        previous_mean = current_mean

    if abs(best_gradient) < BROAD_THRESHOLD:
        return None

    # Determine which side is space
    if best_orientation == 'h':
        if best_gradient > 0:
            dark_side = 'top'
        else:
            dark_side = 'bottom'
    else:
        if best_gradient > 0:
            dark_side = 'left'
        else:
            dark_side = 'right'

    return (best_orientation, best_position, dark_side)


#Scan within identified region

def find_gradient_points(img, orientation, broad_position, dark_side):

    points = []
    step = GRADIENT_STEP

    if orientation == 'h':
        # Horizontal, scan columns
        y_lo = max(step, coarse_position - SCAN_BAND)
        y_hi = min(H - 1, coarse_position + SCAN_BAND)
        v_sign = 1 if dark_side == 'top' else -1

        for x in range(SCAN_SPACING, W - SCAN_SPACING, SCAN_SPACING):
            best_g = 0
            best_y = -1
            for y in range(y_lo + step, y_hi):
                above = img.get_pixel(x, y - step)
                below = img.get_pixel(x, y)
                g = v_sign * (below - above)
                if g > best_g:
                    best_g = g
                    best_y = y
            if best_y >= 0 and best_g >= MIN_GRADIENT:
                # Check neighboring pixels
                if best_y > y_lo + step and best_y < y_hi - 1:
                    above_g = img.get_pixel(x, best_y - 1) - img.get_pixel(x, best_y - step - 1)
                    below_g = img.get_pixel(x, best_y + 1) - img.get_pixel(x, best_y - step + 1)
                    if dark_side != 'top':
                        above_g = -above_g
                        below_g = -below_g
                    # Ignore negative gradients
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
        # Vertical horizon
        x_lo = max(step, coarse_position - SCAN_BAND)
        x_hi = min(W - 1, coarse_position + SCAN_BAND)
        h_sign = 1 if dark_side == 'left' else -1

        for y in range(SCAN_SPACING, H - SCAN_SPACING, SCAN_SPACING):
            best_g = 0
            best_x = -1
            for x in range(x_lo + step, x_hi):
                left_v = img.get_pixel(x - step, y)
                right_v = img.get_pixel(x, y)
                g = h_sign * (right_v - left_v)
                if g > best_g:
                    best_g = g
                    best_x = x
            if best_x >= 0 and best_g >= MIN_GRADIENT:
                if best_x > x_lo + step and best_x < x_hi - 1:
                    left_g = img.get_pixel(best_x - 1, y) - img.get_pixel(best_x - step - 1, y)
                    right_g = img.get_pixel(best_x + 1, y) - img.get_pixel(best_x - step + 1, y)
                    if dark_side != 'left':
                        left_g = -left_g
                        right_g = -right_g
                    # Ignore negative gradients
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

    return points


#RANSAC

def solve_3x3(m, v):
    # Linear algebra least squares
    a = [[m[0][0], m[0][1], m[0][2], v[0]],
         [m[1][0], m[1][1], m[1][2], v[1]],
         [m[2][0], m[2][1], m[2][2], v[2]]]

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


def fit_quadratic(pts, orient):

    if len(pts) < 3:
        return None

    s0 = float(len(pts))
    s1 = s2 = s3 = s4 = 0.0
    r0 = r1 = r2 = 0.0

    for p in pts:
        if orientation == 'h':
            t = p[0]; val = p[1]
        else:
            t = p[1]; val = p[0]
        t2 = t * t
        s1 += t; s2 += t2; s3 += t * t2; s4 += t2 * t2
        r0 += val; r1 += t * val; r2 += t2 * val

    return solve_3x3(
        [[s4, s3, s2], [s3, s2, s1], [s2, s1, s0]],
        [r2, r1, r0]
    )


def residual(p, coeffs, orientation):
    a, b, c = coeffs
    if orientation == 'h':
        t = p[0]
        return abs(p[1] - (a * t * t + b * t + c))
    else:
        t = p[1]
        return abs(p[0] - (a * t * t + b * t + c))


#Reject outliars
def fit_with_rejection(pts, orientation):
    working = list(pts)

    for _ in range(OUTLIER_PASSES):
        if len(working) < MIN_POINTS:
            return (None, [])

        coeffs = fit_quadratic(working, orientation)
        if coeffs is None:
            return (None, [])

        # Compute residuals
        res = [residual(p, coeffs, orientation) for p in working]

        # Median residual
        sorted_res = sorted(res)
        med = sorted_res[len(sorted_res) // 2]
        thresh = max(OUTLIER_MULT * med + 0.5, 3.0)

        # Keep only inliers
        new_working = [working[i] for i in range(len(working)) if res[i] <= thresh]

        if len(new_working) < MIN_POINTS:
            break
        working = new_working

    if len(working) < MIN_POINTS:
        return (None, [])
    coeffs = fit_quadratic(working, orientation)
    return (coeffs, working) if coeffs else (None, [])


#Reject awful curves

def is_plausible_horizon(coeffs, inliers, n_candidates, orient):

    a, b, c = coeffs

    #Check a value for quadratic fit
    if abs(a) > MAX_CURVATURE:
        return (False, "curvature |a|={:.5f} > {}".format(abs(a), MAX_CURVATURE))

    # 2. Check inliers
    if n_candidates > 0:
        ratio = len(inliers) / n_candidates
        if ratio < MIN_INLIER_RATIO:
            return (False, "inlier ratio {:.2f} < {}".format(ratio, MIN_INLIER_RATIO))

    # check inliar spread
    if orientation == 'h':
        coords = [p[0] for p in inliers]
        frame_dim = W
    else:
        coords = [p[1] for p in inliers]
        frame_dim = H
    span = max(coords) - min(coords)
    if span < MIN_SPREAD_RATIO * frame_dim:
        return (False, "spread {:.0f}px < {:.0f}px".format(span, MIN_SPREAD_RATIO * frame_dim))

    # Check inliar residual
    res = sorted([residual(p, coeffs, orientation) for p in inliers])
    med_res = res[len(res) // 2]
    if med_res > MAX_RESIDUAL:
        return (False, "median residual {:.1f} > {}".format(med_res, MAX_RESIDUAL))

    # 5. Check curve crosses the frame
    if orientation == 'h':
        in_frame = sum(1 for x in range(0, W, 20) if 0 <= int(a * x * x + b * x + c) < H)
    else:
        in_frame = sum(1 for y in range(0, H, 15) if 0 <= int(a * y * y + b * y + c) < W)
    if in_frame < 3:
        return (False, "curve barely crosses frame ({} pts visible)".format(in_frame))

    return (True, '')


#Drawing

def draw_fitted_curve(img, coeffs, orient):
    #Draw the quadratic curve as line segments.
    a, b, c = coeffs
    prev = None

    if orientation == 'h':
        for x in range(0, W, DRAW_STEP):
            y = int(a * x * x + b * x + c + 0.5)
            if 0 <= y < H:
                if prev:
                    img.draw_line(prev[0], prev[1], x, y, color=(0, 255, 0), thickness=2)
                prev = (x, y)
            else:
                prev = None
    else:
        for y in range(0, H, DRAW_STEP):
            x = int(a * y * y + b * y + c + 0.5)
            if 0 <= x < W:
                if prev:
                    img.draw_line(prev[0], prev[1], x, y, color=(0, 255, 0), thickness=2)
                prev = (x, y)
            else:
                prev = None



for idx, fname in enumerate(frames):
    gc.collect()
    clock.tick()

    fpath = "{}/{}".format(FRAME_PATH, fname)
    try:
        loaded = image.Image(fpath)
        color_fb.draw_image(loaded, 0, 0)
        gray_fb.draw_image(loaded, 0, 0)
        del loaded
    except Exception as e:
        print("ERR {}: {}".format(fname, e))
        continue

    # Check minimum brightness
    mean_b = gray_fb.get_statistics().mean()
    if not (MIN_BRIGHTNESS < mean_b < MAX_BRIGHTNESS):
        sensor.snapshot().draw_image(color_fb, 0, 0)
        time.sleep_ms(FRAME_DELAY_MS)
        continue

    # apply blur
    if BLUR_SIZE > 0:
        gray_fb.mean(BLUR_SIZE)

    # Broad scan
    broad = broad_find_horizon(gray_fb)
    if broad is None:
        sensor.snapshot().draw_image(color_fb, 0, 0)
        print("[{}/{}] no horizon".format(idx + 1, len(frames)))
        time.sleep_ms(FRAME_DELAY_MS)
        continue

    orientation, coarse_position, dark_side = broad

    # Narrow Scan
    pts = find_gradient_points(gray_fb, orientation, coarse_position, dark_side)

    if len(pts) < MIN_POINTS:
        sensor.snapshot().draw_image(color_fb, 0, 0)
        print("[{}/{}] too few points ({})".format(idx + 1, len(frames), len(pts)))
        time.sleep_ms(FRAME_DELAY_MS)
        continue

    # RANSAC
    coeffs, inliers = fit_with_rejection(pts, orientation)

    if coeffs is None:
        sensor.snapshot().draw_image(color_fb, 0, 0)
        print("[{}/{}] fit failed".format(idx + 1, len(frames)))
        time.sleep_ms(FRAME_DELAY_MS)
        continue

    # Reject bad a values
    ok, reason = is_plausible_horizon(coeffs, inliers, len(pts), orientation)
    if not ok:
        sensor.snapshot().draw_image(color_fb, 0, 0)
        print("[{}/{}] rejected: {}".format(idx + 1, len(frames), reason))
        time.sleep_ms(FRAME_DELAY_MS)
        continue

    # Draw results

    for p in inliers:
        color_fb.draw_circle(int(p[0] + 0.5), int(p[1] + 0.5), 3,
                             color=(255, 0, 0), thickness=1, fill=True)

    # Green quadratic curve
    draw_fitted_curve(color_fb, coeffs, orientation)

    # Arrow to point to space
    cx, cy = W // 2, H // 2
    arrow_map = {
        'top':    (cx, cy + 20, cx, cy - 20),
        'bottom': (cx, cy - 20, cx, cy + 20),
        'left':   (cx + 20, cy, cx - 20, cy),
        'right':  (cx - 20, cy, cx + 20, cy),
    }
    ax1, ay1, ax2, ay2 = arrow_map[dark_side]
    color_fb.draw_arrow(ax1, ay1, ax2, ay2, color=(0, 0, 255), thickness=3)

    a_c, b_c, c_c = coeffs
    print("[{}/{}] {} side={} pts={} a={:.7f} b={:.4f} c={:.1f}".format(
        idx + 1, len(frames), orientation, dark_side, len(inliers), a_c, b_c, c_c))

    sensor.snapshot().draw_image(color_fb, 0, 0)
    time.sleep_ms(FRAME_DELAY_MS)

sensor.dealloc_extra_fb()
sensor.dealloc_extra_fb()
print("Done. {} frames processed.".format(len(frames)))
