"""
MLEngine: TFLite (preferred) + fallback IsolationForest.
- Designed to be lightweight for Pi5 inference
- Expects feature vectors of shape (n_features,)
"""
import os, numpy as np
from sklearn.ensemble import IsolationForest
try:
    import tflite_runtime.interpreter as tflite
    TFLITE_AVAILABLE = True
except Exception:
    TFLITE_AVAILABLE = False

class MLEngine:
    def __init__(self, cfg):
        self.cfg = cfg
        self.model_path = cfg['ml'].get('tflite_model_path', '')
        self.threshold = float(cfg['ml'].get('anomaly_threshold', 0.6))
        self.iforest = None
        self.tflite = None
        self.model_version = "none"
        if self.model_path and os.path.exists(self.model_path) and TFLITE_AVAILABLE:
            self._load_tflite(self.model_path)
        else:
            # try load pre-trained isolation forest (joblib)
            if os.path.exists("/app/models/iforest.joblib"):
                import joblib
                self.iforest = joblib.load("/app/models/iforest.joblib")
                self.model_version = "iforest_loaded"
            else:
                # initialize default iforest (should be trained offline)
                self.iforest = IsolationForest(contamination=0.01, random_state=42)
                self.model_version = "iforest_default"

    def _load_tflite(self, path):
        try:
            self.tflite = tflite.Interpreter(model_path=path)
            self.tflite.allocate_tensors()
            self.model_version = "tflite:" + os.path.basename(path)
            print("[ML] Loaded tflite:", self.model_version)
        except Exception as e:
            print("[ML] tflite load err:", e)
            self.tflite = None

    def run_tflite(self, input_vec):
        # expects input_vec shape (n_features,)
        input_details = self.tflite.get_input_details()
        output_details = self.tflite.get_output_details()
        x = np.array(input_vec, dtype=np.float32).reshape(1,-1)
        self.tflite.set_tensor(input_details[0]['index'], x)
        self.tflite.invoke()
        out = self.tflite.get_tensor(output_details[0]['index'])
        # interpret out: if autoencoder returns reconstruction error or full reconstruction
        if out.shape == x.shape:
            mse = float(((x - out)**2).mean())
            return mse, self.model_version
        else:
            return float(out.flatten().mean()), self.model_version

    def score_window(self, feature_vec):
        if self.tflite:
            try:
                score, ver = self.run_tflite(feature_vec)
                return score, ver
            except Exception as e:
                print("[ML] tflite error:", e)
        if self.iforest is not None:
            s = self.iforest.decision_function(feature_vec.reshape(1,-1))[0]
            score = float(1.0 - (s + 0.5))
            return score, self.model_version
        return 0.0, self.model_version
