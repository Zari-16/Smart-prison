# optional helper for Influx writes (used in examples)
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

class InfluxWriter:
    def __init__(self, url, token, org, bucket):
        self.client = InfluxDBClient(url=url, token=token, org=org)
        self.write_api = self.client.write_api(write_options=SYNCHRONOUS)
        self.bucket = bucket
        self.org = org

    def write_sensor_window(self, device, fields: dict, timestamp=None, measurement="sensor_windows"):
        p = Point(measurement).tag("device", device)
        for k, v in fields.items():
            if isinstance(v, int):
                p.field(k, int(v))
            elif isinstance(v, float):
                p.field(k, float(v))
            else:
                p.field(k, str(v))
        if timestamp:
            p.time(timestamp)
        self.write_api.write(bucket=self.bucket, org=self.org, record=p)

    def write_ai_alert(self, device, fields: dict, timestamp=None, measurement="ai_alerts"):
        self.write_sensor_window(device, fields, timestamp, measurement)
