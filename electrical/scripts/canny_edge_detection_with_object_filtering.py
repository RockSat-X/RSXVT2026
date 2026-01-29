#! /usr/bin/env python3

import pathlib, sys

# Current most effective version:
import cv2 as cv

"""  
if len(sys.argv) != 2:
    print(f'Please provide a single argument that is the file path to the video; got {sys.argv[1:]}.')
    sys.exit(1)

video_path = pathlib.Path(sys.argv[1])

if not video_path.is_file():
    print(f'{video_path} is not a file.')
    sys.exit(1)
"""
video_path = pathlib.Path("C:/Users/RyanS/moonYOLO/assets/Videos/wvu24_launch.mov")
# Open the video file
cap = cv.VideoCapture(video_path)

while True:

    # Open/close frames
    ret, frame = cap.read()
    if not ret:
        break

    # Change brightness (I added this but didn't really do anything).
    # Its adjustable if anyone wants to play with it
    alpha = 1  # Contrast control (1.0-3.0)
    beta = 50    # Brightness control (0-100)
    adjusted = cv.convertScaleAbs(frame, alpha=alpha, beta=beta)

    # Initial images processing. Makes it easier to find edges
    blur = cv.GaussianBlur(frame, (5, 5), 0)
    gray = cv.cvtColor(blur, cv.COLOR_BGR2GRAY)

    # Those last two values are also adjustable. Lower gets more edges
    # Higher gets less edges
    edges = cv.Canny(gray, 50, 150)
    edges = cv.dilate(edges, None)

     # Find contours using founds edges
    
    curves, _ = cv.findContours(edges, cv.RETR_TREE, cv.CHAIN_APPROX_SIMPLE)

    # Calculate longest curve
    def is_valid_contour(curves):
        valid = []
        for i in curves: 
            length = cv.arcLength(i, False) > 300 # Minimum length
            if (i[0] - i[-1]).astype(int).sum() > 20 and length:
                valid.append(i)
        return valid
    
    curves, _ = cv.findContours(edges, cv.RETR_LIST, cv.CHAIN_APPROX_SIMPLE)
    valid_curves = is_valid_contour(curves)

    if valid_curves:
        longest_curve = max(valid_curves, key=lambda c: cv.arcLength(c, False))
    else:
        longest_curve = None

    # Draw longest curve on original frame
    print(len(valid_curves))
    if longest_curve is not None and cv.arcLength(longest_curve, False) > 300:
        cv.drawContours(frame, [longest_curve], -1, (0, 0, 255), 2)

    cv.imshow("Edges", edges)
    cv.imshow("Gray", gray)
    cv.imshow("Original Frame", frame)

    if cv.waitKey(10) & 0xFF == ord('q'):  # Exit on 'q' key
        break

cap.release()
cv.destroyAllWindows()



        

