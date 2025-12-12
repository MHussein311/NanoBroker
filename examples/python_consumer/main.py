import sys
import os
import cv2
import time
import numpy as np
from ultralytics import YOLO


import nanobroker

def main():
    topic = "video_stream"
    model = YOLO("yolov8n.pt") 

    consumer_id = 0
    if len(sys.argv) > 1:
        consumer_id = int(sys.argv[1])
    print(f"Connecting as Consumer ID: {consumer_id}...")

    broker = nanobroker.VideoBroker(topic, False, consumer_id)

    print("Starting Smooth Hybrid Pipeline...")
    
    # Store the last known coordinates: [x1, y1, x2, y2]
    last_box = None
    
    while True:
        data = broker.get_next_frame()
        if data is None: 
            continue
        
        producer_id, frame_id, img = data

        if frame_id % 30 == 0:
            print(f"Consumer processing frame: {frame_id}")

        if img.size == 0 or img.shape[0] == 0 or img.shape[1] == 0:
            # print("Warning: Empty frame received")
            broker.release_frame()
            continue

        # ------------------------------------------------------
        # STRATEGY: Run AI every 3rd frame (10 times/sec)
        # ------------------------------------------------------
        if frame_id % 3 == 0:
            results = model(img, verbose=False)
            
            # Check if we found anything
            if len(results[0].boxes) > 0:
                # Get the first object found
                # boxes.xyxy returns [x1, y1, x2, y2] tensors
                box = results[0].boxes[0].xyxy[0].cpu().numpy().astype(int)
                last_box = box # Save it for later
                
                # Draw "Fresh" Box (Green)
                cv2.rectangle(img, (box[0], box[1]), (box[2], box[3]), (0, 255, 0), 2)
                cv2.putText(img, "AI UPDATE", (box[0], box[1]-10), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,255,0), 2)
            else:
                last_box = None # Object lost
        else:
            # --------------------------------------------------
            # SKIPPING INFERENCE
            # --------------------------------------------------
            # If we have an old box, draw it on the NEW raw image
            if last_box is not None:
                # Draw "Cached" Box (Yellow) - Visual indicator that it's cached
                cv2.rectangle(img, (last_box[0], last_box[1]), (last_box[2], last_box[3]), (0, 255, 255), 2)
        
        # Display the image (img is now modified with the drawn rectangle)
        cv2.imshow("Hybrid Pipeline", img)
        
        broker.release_frame()

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
