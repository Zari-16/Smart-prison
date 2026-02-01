"""
Deploy a tflite model to the edge container (local dev helper)
- Copies models/autoencoder.tflite into edge/models/
- In production use a secure CI/CD flow (signed model artifacts + audit)
"""
import os, shutil
src = "models/autoencoder.tflite"
dst = "../edge/models/autoencoder.tflite"
if not os.path.exists(src):
    raise FileNotFoundError(src)
shutil.copy(src, dst)
print("Copied model to edge/models/autoencoder.tflite")
