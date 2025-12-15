"""
Jupyter-friendly training script (also runnable as .py)
1) Generate synthetic dataset
2) Train autoencoder
3) Convert and quantize to tflite
"""
import os
os.makedirs("data", exist_ok=True)
os.makedirs("models", exist_ok=True)

import numpy as np

# Synthetic generator (same logic as earlier)
def gen_normal(n_windows=2000, window_len=12):
    samples=[]
    for _ in range(n_windows):
        vib = np.random.normal(100,20,window_len).clip(0,1023)
        gas = np.random.normal(350,30,window_len).clip(0,1023)
        pir = np.random.binomial(1,0.05,window_len)
        temp = np.random.normal(24,0.4,window_len)
        people = np.random.poisson(0.2,window_len)
        feat = [vib.mean(), vib.std(), vib.max(), vib.min(), gas.mean(), gas.std(), gas.max(), pir.sum(), temp.mean(), people.max()]
        samples.append(feat)
    return np.array(samples, dtype=np.float32)

def gen_anomalies(n=200):
    samples=[]
    for _ in range(n):
        vib = np.random.normal(100,20,12)
        vib[6] += np.random.uniform(400,800)
        gas = np.random.normal(350,30,12)
        pir = np.random.binomial(1,0.05,12)
        temp = np.random.normal(24,0.4,12)
        people = np.random.poisson(0.2,12)
        feat = [vib.mean(), vib.std(), vib.max(), vib.min(), gas.mean(), gas.std(), gas.max(), pir.sum(), temp.mean(), people.max()]
        samples.append(feat)
    return np.array(samples, dtype=np.float32)

# generate
normal = gen_normal(2000)
anom = gen_anomalies(200)
data = np.vstack([normal, anom])
np.random.shuffle(data)
np.save("data/windows_features.npy", data)
print("Saved", "data/windows_features.npy", data.shape)

# Training autoencoder
from tensorflow.keras.models import Model
from tensorflow.keras.layers import Input, Dense
from tensorflow.keras.optimizers import Adam
from sklearn.model_selection import train_test_split

X = data
X_train, X_val = train_test_split(X, test_size=0.2, random_state=42)
inp_dim = X_train.shape[1]
inp = Input(shape=(inp_dim,))
h = Dense(64, activation='relu')(inp)
h = Dense(32, activation='relu')(h)
z = Dense(16, activation='relu')(h)
h2 = Dense(32, activation='relu')(z)
h2 = Dense(64, activation='relu')(h2)
out = Dense(inp_dim, activation='linear')(h2)
model = Model(inp, out)
model.compile(optimizer=Adam(1e-3), loss='mse')
model.fit(X_train, X_train, validation_data=(X_val,X_val), epochs=40, batch_size=64)
model.save("models/autoencoder.h5")
print("Saved models/autoencoder.h5")

# Convert to tflite (float or quantized)
import tensorflow as tf
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
def rep_data_gen():
    for i in range(100):
        yield [X_train[i:i+1].astype("float32")]
converter.representative_dataset = rep_data_gen
tflite_model = converter.convert()
open("models/autoencoder.tflite","wb").write(tflite_model)
print("Saved models/autoencoder.tflite")
