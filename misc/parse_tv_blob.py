import os
import cv2
import numpy as np
# include operation systems, OpenCV (decodes JPEG and creates/writes video), numpy so we can convert raw bites into array data for CV
# OpenCV is a computer vision library used for image processing, machine learning, and other cv applications


START_TOKEN = b"<TV>"
END_TOKEN = b"</TV>"
# Binary tokens representing the start and end of JPEG image frames
# <TV> [JPEG DATA] </TV>

INPUT_FILE = "example_tv_data.bin"
OUTPUT_VIDEO = "output_video.mp4" #output mp4 name
FPS = 11.1  # frame rate



def extract_frames_from_blob(file_path):
    """
    This function extracts the JPEG bytes using the TV tokens, attempts 
    to decode them using OpenCV, and produces valid frames one at a time.
    """

    # reads entire file and stores raw bytes as blob
    with open(file_path, "rb") as f:
        blob = f.read()

    # cursor used to move around in file
    # initialize frames to 0
    cursor = 0
    total_frames = 0
    recovered_frames = 0

    while True:
        start = blob.find(START_TOKEN, cursor)
        if start == -1:
            break
        # finds start token, if none break

        end = blob.find(END_TOKEN, start)
        if end == -1:
            break
        # finds end token, if none break

        #if start and end token found, begin looping through data

        jpeg_data = blob[start + len(START_TOKEN):end]
        # takes all the raw data between start and end token, allocates total frames

        total_frames += 1

        # Basic JPEG sanity check, makes sure the file begins with FF D8
        if not jpeg_data.startswith(b"\xFF\xD8"):
            cursor = end + len(END_TOKEN)
            continue

        # Attempt to decode
        # np.frombuffer() converts raw bytes into a NumPy array called image_array
        # vc2.imdecode() translates image_array into a JPEG image
        try:
            image_array = np.frombuffer(jpeg_data, dtype=np.uint8)
            frame = cv2.imdecode(image_array, cv2.IMREAD_COLOR)

        # if frame is corrupted, skip it
            if frame is None:
                raise ValueError("Decode failed")

        # adds to total recovered/valid frames
            recovered_frames += 1
            yield frame

        except Exception:
            # corrupted frame? skip
            pass

        cursor = end + len(END_TOKEN)

    print(f"Total frames found: {total_frames}")
    print(f"Valid frames recovered: {recovered_frames}")


def write_video(frames, output_path, fps):
    """
    This function stitches frames into a single MP4 video.
    """

    #search for valid frames
    first_frame = next(frames, None)
    if first_frame is None:
        print("No valid frames found.")
        return

    # Define the codec, heigh and width and create VideoWriter object
    height, width, _ = first_frame.shape
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(output_path, fourcc, fps, (width, height))

    writer.write(first_frame)

    frame_count = 1

    for frame in frames:
        # print(frame.shape), prints size of each frame before reframing
        if frame.shape[0] != height or frame.shape[1] != width:
            frame = cv2.resize(frame, (width, height))
            # rewrites all frames to be the same size
            # To Note: it resizes based on the height and width of the first frame

        writer.write(frame)
        frame_count += 1

    writer.release()
    print(f"Video written: {output_path}")
    print(f"Frames written: {frame_count}")

    # output mp4
if __name__ == "__main__":
    frame_generator = extract_frames_from_blob(INPUT_FILE)
    write_video(frame_generator, OUTPUT_VIDEO, FPS)