"""
Synthetic dataset generation and training pipeline (autoencoder) for edge inference.
- Generates synthetic windows features and saves to data/windows_features.npy
- Trains a small dense autoencoder with Keras and saves models/autoencoder.h5
- Converts Keras model to quantized TFLite and saves models/autoencoder.tflite

Run in a workstation or the Pi (Pi training is slower). For Pi inference, copy the resulting .tflite into edge/models/.
"""
import os
import numpy as np
os.makedirs("data", exist_ok=True)
os.makedirs("models", exist_ok=True)

def gen_normal(n_windows=2000, window_len=12):
    samples = []
    for _ in range(n_windows):
        vib = np.random.normal(loc=100, scale=20, size=window_len).clip(0, 1023)
        gas = np.random.normal(loc=350, scale=30, size=window_len).clip(0, 1023)
        pir = np.random.binomial(1, 0.05, size=window_len)
        temp = np.random.normal(24, 0.5, size=window_len)
        people = np.random.poisson(0.2, size=window_len)
        feat = [
            vib.mean(), vib.std(), vib.max(), vib.min(),
            gas.mean(), gas.std(), gas.max(),
            pir.sum(), temp.mean(), people.max()
        ]
        samples.append(feat)
    return np.array(samples, dtype=np.float32)

def gen_vibration_anomalies(n=200, window_len=12):
    samples=[]
    for _ in range(n):
        vib = np.random.normal(100,20,window_len)
        vib[window_len//2] += np.random.uniform(400,800) # spike
        gas = np.random.normal(350,30,window_len)
        pir = np.random.binomial(1,0.05,window_len)
        temp = np.random.normal(24,0.5,window_len)
        people = np.random.poisson(0.2,window_len)
        feat = [vib.mean(), vib.std(), vib.max(), vib.min(), gas.mean(), gas.std(), gas.max(), pir.sum(), temp.mean(), people.max()]
        samples.append(feat)
    return np.array(samples, dtype=np.float32)

def gen_gas_anomalies(n=200, window_len=12):
    samples=[]
    for _ in range(n):
        vib = np.random.normal(100,20,window_len)
        gas = np.random.normal(800,30,window_len) # high gas values
        pir = np.random.binomial(1,0.1,window_len)
        temp = np.random.normal(24,0.5,window_len)
        people = np.random.poisson(0.3,window_len)
        feat = [vib.mean(), vib.std(), vib.max(), vib.min(), gas.mean(), gas.std(), gas.max(), pir.sum(), temp.mean(), people.max()]
        samples.append(feat)
    return np.array(samples, dtype=np.float32)

if __name__ == "__main__":
    print("Generating synthetic dataset...")
    normal = gen_normal(2000)
    vib = gen_vibration_anomalies(200)
    gas = gen_gas_anomalies(200)
    data = np.vstack([normal, vib, gas])
    np.random.shuffle(data)
    np.save("data/windows_features.npy", data)
    print("Saved data/windows_features.npy shape=", data.shape)

    # Training autoencoder with TensorFlow/Keras (recommended to run on workstation)
    try:
        from tensorflow.keras.models import Model
        from tensorflow.keras.layers import Input, Dense
        from tensorflow.keras.optimizers import Adam
        from sklearn.model_selection import train_test_split

        print("Loading data for training...")
        X = data
        X_train, X_val = train_test_split(X, test_size=0.2, random_state=42)

        input_dim = X_train.shape[1]
        inp = Input(shape=(input_dim,))
        h = Dense(64, activation='relu')(inp)
        h = Dense(32, activation='relu')(h)
        z = Dense(16, activation='relu')(h)
        h2 = Dense(32, activation='relu')(z)
        h2 = Dense(64, activation='relu')(h2)
        out = Dense(input_dim, activation='linear')(h2)
        model = Model(inputs=inp, outputs=out)
        model.compile(optimizer=Adam(1e-3), loss='mse')

        print("Training autoencoder...")
        model.fit(X_train, X_train, validation_data=(X_val, X_val), epochs=40, batch_size=64)
        model.save("models/autoencoder.h5")
        print("Saved models/autoencoder.h5")
    except Exception as e:
        print("Training skipped due to missing TensorFlow or other issue:", e)
        print("If you want to train, run this script on a workstation with TensorFlow installed.")

    # Convert to tflite (if TensorFlow available)
    try:
        import tensorflow as tf
        model = tf.keras.models.load_model("models/autoencoder.h5")
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        converter.optimizations = [tf.lite.Optimize.DEFAULT]

        # representative dataset generator for quantization
        def rep_data_gen():
            for i in range(min(100, X_train.shape[0])):
                yield [X_train[i:i+1].astype('float32')]
        converter.representative_dataset = rep_data_gen

        tflite_model = converter.convert()
        with open("models/autoencoder.tflite", "wb") as f:
            f.write(tflite_model)
        print("Saved models/autoencoder.tflite")
    except Exception as e:
        print("TFLite conversion skipped (TensorFlow not present?):", e)
        print("You can convert model on a workstation with TensorFlow and then copy the .tflite to the edge/models/ folder.")
