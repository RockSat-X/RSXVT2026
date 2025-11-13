# Current most effective version: 
import cv2 as cv
video_path = "C:\\Users\\RyanS\\moonYOLO\\assets\\Videos\\wvu24_launch.mov"

# Open the video file
cap = cv.VideoCapture(video_path)  

while True:
    # Open/close frames
    ret, frame = cap.read()
    if not ret:
        break   

    # Change brightness (I added this but didn't really do anything.
    # Its adjustable if anyone wants to play with it
    alpha = 1  # Contrast control (1.0-3.0)
    beta = 50    # Brightness control (0-100)
    adjusted = cv.convertScaleAbs(frame, alpha=alpha, beta=beta)

    # Initial images processing. Makes it easier to find edges
    blur = cv.GaussianBlur(adjusted, (5, 5), 0)

    #Those last two values are also adjustable. Lower gets more edges
    # Higher gets less edges
    edges = cv.Canny(blur, 150, 250)
    edges = cv.dilate(edges, None)

     # Find contours using founds edges
    curves, _ = cv.findContours(edges, cv.RETR_TREE, cv.CHAIN_APPROX_SIMPLE)

    # Calculate longest curve
    curves, _ = cv.findContours(edges, cv.RETR_LIST, cv.CHAIN_APPROX_SIMPLE)
    if curves:
        longest_curve = max(curves, key=lambda c: cv.arcLength(c, False))
    else: 
        longest_curve = None

    # Draw longest curve on original frame
    if longest_curve is not None and cv.arcLength(longest_curve, False) > 300:
        cv.drawContours(frame, [longest_curve], -1, (0, 0, 255), 2)

    cv.imshow("Edges", edges)
    cv.imshow("Adjusted Frame", adjusted)
    cv.imshow("Original Frame", frame)

    if cv.waitKey(20) & 0xFF == ord('q'):  # Exit on 'q' key
        break
cap.release()
cv.destroyAllWindows()