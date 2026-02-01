"""
Local helper script to copy a built model into edge/models for local dev.
In production, use a secure CI/CD pipeline and signed artifacts.
"""
import os, shutil

SRC = "models/autoencoder.tflite"
DST = "/path/to/edge/models/autoencoder.tflite"  # adjust path if running locally outside container

if not os.path.exists(SRC):
    print("Source model not found:", SRC)
else:
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    shutil.copy2(SRC, DST)
    print("Copied", SRC, "->", DST)
