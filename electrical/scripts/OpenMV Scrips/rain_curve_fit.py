import sensor, image, time, os

# Configuration
FRAME_PATH = "/frames"
FRAME_DELAY_MS = 0
CANNY_LOW = 30
CANNY_HIGH = 100
DROP_SPACING = 5
POINT_WEIGHT = 1.0
COLOR_WEIGHT = 0.5  # Weight for color score (how "horizon-like" the colors are)
CONSISTENCY_WEIGHT = 1.0  # Weight for line consistency (straightness)
MIN_SCORE_THRESHOLD = 50.0  # Minimum score required to detect a horizon

# Initialize sensor
sensor.reset()
sensor.set_framesize(sensor.QVGA)
sensor.set_pixformat(sensor.RGB565)
sensor.skip_frames(time=2000)

# Get list of frames
print("Loading frames...")
frame_files = os.listdir(FRAME_PATH)
frames = sorted([f for f in frame_files if f.endswith(".jpg")])

if not frames:
    raise Exception("No .jpg frames found in {}!".format(FRAME_PATH))

print("Found {} frames".format(len(frames)))

# Pre-allocate buffers
original_img = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.RGB565)
edge_img = sensor.alloc_extra_fb(sensor.width(), sensor.height(), sensor.GRAYSCALE)

clock = time.clock()
frames_with_horizon = 0
frames_without_horizon = 0

# Helper function to calculate color score
def calculate_color_score(points, original_img):
    if len(points) == 0:
        return 0

    total_score = 0

    for point in points:
        x, y = point[0], point[1]

        # Get RGB values from original image (returns tuple)
        pixel = original_img.get_pixel(x, y)
        r, g, b = pixel[0], pixel[1], pixel[2]

        color_score = 0

        # Reward blue-ish colors (common in horizons with sky)
        if b > r and b > g:
            color_score += b - max(r, g)

        # Reward any strong color (high saturation)
        max_channel = max(r, g, b)
        min_channel = min(r, g, b)
        saturation = max_channel - min_channel
        color_score += saturation * 0.5

        total_score += color_score

    return total_score / len(points)


# Helper function to calculate consistency score
def calculate_consistency(points, direction):
    if len(points) < 2:
        return 0

    total_change = 0

    if direction in ['top', 'bottom']:
        # Horizontal line - check Y changes between adjacent points
        for i in range(len(points) - 1):
            y_change = abs(points[i+1][1] - points[i][1])
            total_change += y_change * y_change  # Square to penalize large changes more
    else:
        # Vertical line - check X changes between adjacent points
        for i in range(len(points) - 1):
            x_change = abs(points[i+1][0] - points[i][0])
            total_change += x_change * x_change  # Square to penalize large changes more

    # Average change per point
    avg_change = total_change / (len(points) - 1)
    if avg_change == 0:
        return 1000  # Perfect line
    else:
        return 1000.0 / (1.0 + avg_change)

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

    # Convert to grayscale and run Canny
    if edge_img.format() != sensor.GRAYSCALE:
        edge_img = edge_img.to_grayscale()

    edge_img.mean(1)
    edge_img.find_edges(image.EDGE_CANNY, threshold=(CANNY_LOW, CANNY_HIGH))
    edge_img.dilate(1)

    # Collect 4 curves by dropping pixels from each side
    curves = {
        'top': [],     # Drop down from top
        'bottom': [],  # Drop up from bottom
        'left': [],    # Drop right from left
        'right': []    # Drop left from right
    }

    # 1. Drop from TOP
    for x in range(0, edge_img.width(), DROP_SPACING):
        for y in range(0, edge_img.height()):
            intensity = edge_img.get_pixel(x, y)
            if intensity > 128:  # Hit an edge
                curves['top'].append((x, y))
                break

    # 2. Drop from BOTTOM
    for x in range(0, edge_img.width(), DROP_SPACING):
        for y in range(edge_img.height() - 1, -1, -1):
            intensity = edge_img.get_pixel(x, y)
            if intensity > 128:
                curves['bottom'].append((x, y))
                break

    # 3. Drop from LEFT (pixels fall RIGHT)
    for y in range(0, edge_img.height(), DROP_SPACING):
        for x in range(0, edge_img.width()):
            intensity = edge_img.get_pixel(x, y)
            if intensity > 128:
                curves['left'].append((x, y))
                break

    # 4. Drop from RIGHT (pixels fall LEFT)
    for y in range(0, edge_img.height(), DROP_SPACING):
        for x in range(edge_img.width() - 1, -1, -1):
            intensity = edge_img.get_pixel(x, y)
            if intensity > 128:
                curves['right'].append((x, y))
                break

    # Score each curve - weighted combination of length, color, and consistency
    best_curve = None
    best_score = 0
    best_name = None

    print("  Curves detected:")

    for name, points in curves.items():
        if len(points) < 20:
            print("    {}: {} points (too few)".format(name, len(points)))
            continue

        # Calculate color score from original image RGB values
        color_score = calculate_color_score(points, original_img)

        # Calculate consistency score
        consistency = calculate_consistency(points, name)

        # Weighted scoring: length + color + consistency
        score = (len(points) * POINT_WEIGHT) + (color_score * COLOR_WEIGHT) + (consistency * CONSISTENCY_WEIGHT)

        print("    {}: {} points, color_score={:.1f}, consistency={:.2f}, score={:.1f}".format(
            name, len(points), color_score, consistency, score))

        if score > best_score:
            best_score = score
            best_curve = points
            best_name = name

    # Draw the best curve if it meets minimum threshold
    if best_curve and best_score >= MIN_SCORE_THRESHOLD:
        frames_with_horizon += 1

        # Calculate stats for display
        color_score = calculate_color_score(best_curve, original_img)

        # Draw all points in the curve
        for point in best_curve:
            original_img.draw_circle(point[0], point[1], 2, color=(255, 0, 0), thickness=1, fill=True)

        # Draw arrow showing direction
        arrow_size = 30
        center_x = original_img.width() // 2
        center_y = original_img.height() // 2

        if best_name == 'top':
            original_img.draw_arrow(center_x, center_y - 20, center_x, center_y + 20,
                                   color=(0, 255, 0), thickness=3)
        elif best_name == 'bottom':
            original_img.draw_arrow(center_x, center_y + 20, center_x, center_y - 20,
                                   color=(0, 255, 0), thickness=3)
        elif best_name == 'left':
            original_img.draw_arrow(center_x - 20, center_y, center_x + 20, center_y,
                                   color=(0, 255, 0), thickness=3)
        elif best_name == 'right':
            original_img.draw_arrow(center_x + 20, center_y, center_x - 20, center_y,
                                   color=(0, 255, 0), thickness=3)

        print("[{}/{}] Horizon: {}, Points: {}, Color Score: {:.1f}, Score: {:.1f}, FPS: {:.2f}".format(
            idx+1, len(frames), best_name, len(best_curve), color_score, best_score, clock.fps()))
    else:
        frames_without_horizon += 1
        if best_curve:
            print("[{}/{}] No horizon (score {:.1f} < threshold {:.1f}), FPS: {:.2f}".format(
                idx+1, len(frames), best_score, MIN_SCORE_THRESHOLD, clock.fps()))
        else:
            print("[{}/{}] No horizon, FPS: {:.2f}".format(idx+1, len(frames), clock.fps()))

    sensor.snapshot().draw_image(original_img, 0, 0)
    time.sleep_ms(FRAME_DELAY_MS)

sensor.dealloc_extra_fb()

print("\n" + "="*40)
print("Processing complete!")
print("="*40)
print("Total frames: {}".format(len(frames)))
print("Horizons detected: {} ({:.1f}%)".format(
    frames_with_horizon, 100.0 * frames_with_horizon / len(frames)))
print("No horizon: {} ({:.1f}%)".format(
    frames_without_horizon, 100.0 * frames_without_horizon / len(frames)))
print("="*40)
