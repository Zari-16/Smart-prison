#!/usr/bin/env python3
"""
Edge Ingest & Inference Service (FastAPI)
- Receives HTTPS POSTs (nginx terminates TLS)
- Buffers per-device windows, extracts features, runs ML (TFLite preferred)
- Writes sensor summaries and ai_alerts to InfluxDB
- Publishes alerts to MQTT over TLS
"""
import os, time, logging, yaml
from fastapi import FastAPI, Request, Header, HTTPException
from fastapi.responses import JSONResponse
from collections import deque, defaultdict
import numpy as np
import threading
import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

from ml_engine import MLEngine

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("edge")

# load config
cfg_path = os.environ.get("CONFIG_PATH", "/app/config.yaml")
with open(cfg_path) as f:
    cfg = yaml.safe_load(f)

API_KEY = cfg['server']['api_key']
INFLUX_URL = cfg['influx']['url']
INFLUX_TOKEN = cfg['influx']['token']
INFLUX_ORG = cfg['influx']['org']
INFLUX_BUCKET = cfg['influx']['bucket']

# Influx client
influx_client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
write_api = influx_client.write_api(write_options=SYNCHRONOUS)

# MQTT TLS client
mqtt_client = None
if cfg.get('mqtt',{}).get('enabled', False):
    mqtt_cfg = cfg['mqtt']
    mqtt_client = mqtt.Client()
    if mqtt_cfg.get('username'):
        mqtt_client.username_pw_set(mqtt_cfg['username'], mqtt_cfg['password'])
    cafile = mqtt_cfg.get('cafile')
    if cafile and os.path.exists(cafile):
        mqtt_client.tls_set(ca_certs=cafile)
    mqtt_client.connect(mqtt_cfg['host'], mqtt_cfg.get('port', 8883))
    mqtt_client.loop_start()
    log.info("MQTT connected (TLS)")

# ml engine
ml_engine = MLEngine(cfg)

# buffers
WINDOW = cfg['ml'].get('window_size', 12)
buffers = defaultdict(lambda: deque(maxlen=WINDOW))
lock = threading.Lock()

def extract_features(window_list):
    """
    Compute a concise feature vector for a window.
    Returns numpy array [vib_mean, vib_std, vib_max, vib_min,
                        gas_mean, gas_std, gas_max,
                        pir_count, temp_mean, people_max]
    """
    vib = np.array([float(x.get('vib',0)) for x in window_list])
    gas = np.array([float(x.get('gas',0)) for x in window_list])
    pir = np.array([int(x.get('pir',0)) for x in window_list])
    temp = np.array([float(x.get('temp',0)) for x in window_list])
    people = np.array([int(x.get('people',0)) for x in window_list])
    feat = [
        float(np.mean(vib)), float(np.std(vib)), float(np.max(vib)), float(np.min(vib)),
        float(np.mean(gas)), float(np.std(gas)), float(np.max(gas)),
        int(np.sum(pir)), float(np.mean(temp)), int(np.max(people))
    ]
    return np.array(feat, dtype=np.float32)

def write_influx_sensor(device, ts, feat, score, model_version):
    p = Point("sensor_windows").tag("device", device)
    p.field("vib_mean", float(feat[0])); p.field("vib_std", float(feat[1]))
    p.field("gas_mean", float(feat[4])); p.field("pir_sum", int(feat[7]))
    p.field("temp_mean", float(feat[8])); p.field("people_max", int(feat[9]))
    p.field("ai_score", float(score)); p.field("model_version", model_version)
    p.time(ts)
    write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=p)

def write_influx_alert(device, ts, score, model_version):
    p = Point("ai_alerts").tag("device", device)
    p.field("score", float(score)); p.field("model_version", model_version); p.field("confirmed", 0)
    p.time(ts)
    write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=p)

app = FastAPI(title="Smart Prison Edge")

@app.post("/ingest")
async def ingest(request: Request, authorization: str = Header(None)):
    # validation
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="Missing Authorization")
    token = authorization.split(" ",1)[1]
    if token != API_KEY:
        raise HTTPException(status_code=403, detail="Invalid API key")
    payload = await request.json()
    device = payload.get("device","unknown")
    ts = payload.get("ts", int(time.time()))
    with lock:
        buffers[device].append(payload)
        if len(buffers[device]) == WINDOW:
            window = list(buffers[device])
            feat = extract_features(window)
            score, model_version = ml_engine.score_window(feat)
            write_influx_sensor(device, ts, feat, score, model_version)
            # anomaly handling
            if score >= cfg['ml'].get('anomaly_threshold', 0.6):
                write_influx_alert(device, ts, score, model_version)
                # mqtt publish
                if mqtt_client:
                    topic = f"{cfg['mqtt'].get('topic_prefix','smartprison')}/alerts/{device}"
                    payload_msg = {"device": device, "ts": ts, "score": float(score), "model_version": model_version}
                    mqtt_client.publish(topic, str(payload_msg))
    return JSONResponse({"status":"ok"})

@app.get("/model")
def model_info():
    return {"model_version": ml_engine.model_version, "window": WINDOW}
