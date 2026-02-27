import sensor, image, time, os

# Configuration
FRAME_PATH = "/frames"
FRAME_DELAY_MS = 0
CANNY_LOW = 70
CANNY_HIGH = 100
DROP_SPACING = 4
MIN_SCORE_THRESHOLD = 15
DARK_THRESHOLD = 20
EDGE_SAMPLE_DEPTH = 5
EDGE_EXCLUSION_MARGIN = 8
MAX_LINE_DISTANCE = 20
MIN_BRIGHTNESS = 10
MAX_BRIGHTNESS = 200

# Initialize sensor
sensor.reset()
sensor.set_framesize(sensor.QVGA)
sensor.set_pixformat(sensor.RGB565)
sensor.skip_frames(time=2000)

frame_files = os.listdir(FRAME_PATH)
frames = sorted([f for f in frame_files if f.endswith(".jpg")])

if not frames:
    raise Exception("No .jpg frames found in {}!".format(FRAME_PATH))

original_img = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.RGB565)
edge_img = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)

clock = time.clock()

def is_on_edge(x, y, width, height, margin=EDGE_EXCLUSION_MARGIN):
    return (x < margin or
            x >= width - margin or
            y < margin or
            y >= height - margin)

def fit_line_and_count_inliers(points, direction, max_distance=MAX_LINE_DISTANCE):
    if len(points) < 3:
        return (0, 0, 0, points)

    slopes = []
    slope_skip = 5

    if direction in ['top', 'bottom']:
        for i in range(len(points) - slope_skip):
            dx = points[i + slope_skip][0] - points[i][0]
            dy = points[i + slope_skip][1] - points[i][1]
            if dx != 0:
                slopes.append(dy / dx)
    else:
        for i in range(len(points) - slope_skip):
            dx = points[i + slope_skip][0] - points[i][0]
            dy = points[i + slope_skip][1] - points[i][1]
            if dy != 0:
                slopes.append(dx / dy)

    if len(slopes) == 0:
        if direction in ['top', 'bottom']:
            coords = [p[1] for p in points]
            coords_sorted = sorted(coords)
            median_coord = coords_sorted[len(coords_sorted) // 2]
            inliers = [p for p in points if abs(p[1] - median_coord) <= max_distance]
            return (0, median_coord, len(inliers), inliers)
        else:
            coords = [p[0] for p in points]
            coords_sorted = sorted(coords)
            median_coord = coords_sorted[len(coords_sorted) // 2]
            inliers = [p for p in points if abs(p[0] - median_coord) <= max_distance]
            return (0, median_coord, len(inliers), inliers)

    slopes_sorted = sorted(slopes)
    median_slope = slopes_sorted[len(slopes_sorted) // 2]

    intercepts = []
    if direction in ['top', 'bottom']:
        for point in points:
            x, y = point[0], point[1]
            intercepts.append(y - median_slope * x)
    else:
        for point in points:
            x, y = point[0], point[1]
            intercepts.append(x - median_slope * y)

    intercepts_sorted = sorted(intercepts)
    median_intercept = intercepts_sorted[len(intercepts_sorted) // 2]

    inlier_points = []
    m = median_slope
    b = median_intercept
    denominator = (m * m + 1) ** 0.5

    if direction in ['top', 'bottom']:
        for point in points:
            x, y = point[0], point[1]
            if abs(m * x - y + b) / denominator <= max_distance:
                inlier_points.append(point)
    else:
        for point in points:
            x, y = point[0], point[1]
            if abs(m * y - x + b) / denominator <= max_distance:
                inlier_points.append(point)

    return (median_slope, median_intercept, len(inlier_points), inlier_points)

# Process each frame
for idx, frame_name in enumerate(frames):

    clock.tick()
    frame_path = "{}/{}".format(FRAME_PATH, frame_name)

    try:
        loaded = image.Image(frame_path)
        original_img.draw_image(loaded, 0, 0)
        edge_img.draw_image(loaded, 0, 0)
        del loaded
    except Exception as e:
        print("ERROR loading frame {}: {}".format(idx+1, e))
        continue

    # Check if frame is within usable brightness range
    if not (MIN_BRIGHTNESS < edge_img.get_statistics().mean() < MAX_BRIGHTNESS):
        sensor.snapshot().draw_image(original_img, 0, 0)
        time.sleep_ms(FRAME_DELAY_MS)
        continue

    # Convert to grayscale and run Canny
    edge_img.mean(1)
    edge_img.find_edges(image.EDGE_CANNY, threshold=(CANNY_LOW, CANNY_HIGH))
    edge_img.dilate(1)

    img_width = edge_img.width()
    img_height = edge_img.height()

    # Check edges for darkness - sample a 1px line at EDGE_SAMPLE_DEPTH inward from each border
    top = bottom = left = right = True

    if original_img.get_statistics(roi=(0, EDGE_SAMPLE_DEPTH, 320, 1)).mean() > DARK_THRESHOLD:
        top = False
    if original_img.get_statistics(roi=(0, 240 - EDGE_SAMPLE_DEPTH, 320, 1)).mean() > DARK_THRESHOLD:
        bottom = False
    if original_img.get_statistics(roi=(EDGE_SAMPLE_DEPTH, 0, 1, 240)).mean() > DARK_THRESHOLD:
        left = False
    if original_img.get_statistics(roi=(320 - EDGE_SAMPLE_DEPTH, 0, 1, 240)).mean() > DARK_THRESHOLD:
        right = False

    edges_with_darkness = [e for e, dark in [('top', top), ('bottom', bottom), ('left', left), ('right', right)] if dark]

    if len(edges_with_darkness) == 0:
        sensor.snapshot().draw_image(original_img, 0, 0)
        time.sleep_ms(FRAME_DELAY_MS)
        continue

    # Collect curves
    curves = {'top': [], 'bottom': [], 'left': [], 'right': []}

    if top:
        for x in range(0, img_width, DROP_SPACING):
            for y in range(0, img_height):
                if edge_img.get_pixel(x, y) > 128:
                    curves['top'].append((x, y))
                    break

    if bottom:
        for x in range(0, img_width, DROP_SPACING):
            for y in range(img_height - 1, -1, -1):
                if edge_img.get_pixel(x, y) > 128:
                    curves['bottom'].append((x, y))
                    break

    if left:
        for y in range(0, img_height, DROP_SPACING):
            for x in range(0, img_width):
                if edge_img.get_pixel(x, y) > 128:
                    curves['left'].append((x, y))
                    break

    if right:
        for y in range(0, img_height, DROP_SPACING):
            for x in range(img_width - 1, -1, -1):
                if edge_img.get_pixel(x, y) > 128:
                    curves['right'].append((x, y))
                    break

    # Remove points that fall within the edge exclusion margin
    for name in curves:
        curves[name] = [p for p in curves[name] if not is_on_edge(p[0], p[1], img_width, img_height)]

    # Fit and score curves
    best_curve_data = None
    best_score = 0
    best_name = None

    for name, points in curves.items():
        if len(points) < 20:
            continue

        slope, intercept, inlier_count, inlier_points = fit_line_and_count_inliers(points, name)
        inlier_ratio = inlier_count / len(points) if len(points) > 0 else 0
        score = inlier_count * (inlier_ratio ** 1.5)

        if score > best_score:
            best_score = score
            best_curve_data = (slope, intercept, inlier_points)
            best_name = name

    # Draw results
    if best_curve_data and best_score >= MIN_SCORE_THRESHOLD:
        slope, intercept, display_points = best_curve_data

        for point in display_points:
            original_img.draw_circle(point[0], point[1], 2, color=(255, 0, 0), thickness=1, fill=True)

        center_x = original_img.width() // 2
        center_y = original_img.height() // 2

        if best_name == 'top':
            original_img.draw_arrow(center_x, center_y - 20, center_x, center_y + 20, color=(0, 255, 0), thickness=3)
        elif best_name == 'bottom':
            original_img.draw_arrow(center_x, center_y + 20, center_x, center_y - 20, color=(0, 255, 0), thickness=3)
        elif best_name == 'left':
            original_img.draw_arrow(center_x - 20, center_y, center_x + 20, center_y, color=(0, 255, 0), thickness=3)
        elif best_name == 'right':
            original_img.draw_arrow(center_x + 20, center_y, center_x - 20, center_y, color=(0, 255, 0), thickness=3)

    if (idx + 1) % 10 == 0:
        print("[{}/{}] FPS: {:.2f}".format(idx+1, len(frames), clock.fps()))
    print("Best score: {:.1f}".format(best_score))

    sensor.snapshot().draw_image(original_img, 0, 0)
    time.sleep_ms(FRAME_DELAY_MS)

sensor.dealloc_extra_fb()
sensor.dealloc_extra_fb()
