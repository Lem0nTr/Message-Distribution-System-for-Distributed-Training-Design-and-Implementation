# train.py (原始可用版本)
import sys
import json
import argparse
from ultralytics import YOLO

def train(output_dir):
    sys.stdout = sys.stderr
    
    model = YOLO('/home/stl/Yolo/ultralytics-main/yolov8n.pt')
    results = model.train(
        data='/home/stl/Yolo/ultralytics-main/ultralytics/cfg/datasets/coco128.yaml',
        epochs=1,
        project=output_dir,
        verbose=False,
        batch=1
    )
    
    os.write(1, json.dumps({
        "status": "success",
        "model_path": str(results.save_dir / "weights" / "best.pt")
    }).encode('utf-8'))

if __name__ == "__main__":
    try:
        import os
        parser = argparse.ArgumentParser()
        parser.add_argument("--output-dir", required=True)
        args = parser.parse_args()
        train(args.output_dir)
    except Exception as e:
        os.write(1, json.dumps({
            "status": "error",
            "message": str(e)
        }).encode('utf-8'))
        sys.exit(1)
