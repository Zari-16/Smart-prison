```
# Smart Prison — AI Integration (Edge, TLS, MQTT, InfluxDB, ML training)

Repository purpose
- Securely collect sensor telemetry (Arduino MKR1010 devices) over HTTPS.
- Buffer and run edge AI inference (Raspberry Pi 5 recommended).
- Publish AI alerts to InfluxDB and MQTT (TLS).
- Provide training notebooks and tooling to train, quantize (TFLite) and deploy models.

High-level components
- edge/: FastAPI-based Edge ingestion & ML inference service
- docker/ : Docker Compose for Raspberry Pi 5 / arm64 (InfluxDB, Mosquitto, nginx reverse proxy)
- notebooks/: Jupyter-friendly training notebook (autoencoder) + data generation
- sketches/: Arduino sketches (TLS-enabled) for CONTROLROOM and PATROL units
- scripts/: provisioning and helper scripts (certs, deploy)

Top-level quickstart (Pi 5 target)
1. Provision Pi 5 (8GB) with recommended OS (Raspberry Pi OS 64-bit or Ubuntu 22.04 arm64).
2. Run scripts/provision_pi.sh on the Pi to install Docker, Docker Compose, Python, tflite-runtime guidance and other packages.
3. Edit `docker/compose-pi.yml` and `edge/config.yaml` with your real InfluxDB token, domain name, and API keys.
4. For production TLS, configure DNS for your Pi domain (e.g., edge.example.local or public domain), then use certbot/nginx (instructions in scripts/README_TLS.md).
5. Start stack: `docker compose -f docker/compose-pi.yml up -d --build`
6. Upload compiled quantized TFLite model to `edge/models/autoencoder.tflite` (or run training in notebooks/ and convert).
7. Point Arduino devices to EDGE_HOST (DNS or Pi IP), set EDGE_API_KEY same as in edge/config.yaml, and run the MKR1010 sketches.

Security checklist (production)
- Replace all placeholder tokens & secrets; store them in Docker secrets / environment variables / vault.
- Use Let's Encrypt for TLS (nginx reverse proxy), do not use long-lived self-signed certs publicly.
- Use strong MQTT ACL or username/password; limit Mosquitto to internal network and TLS.
- Store Influx token in a secret or environment variable, not in the repo.
- Harden the Pi (firewall, SSH key auth, automatic security updates).
- Implement model/version audit logs and allow operator confirm/deny actions from Grafana UI (not included here, next step).

If you want me to push these files to your GitHub repo (Zari-16/AI-integration-prison) as commits, tell me and I’ll prepare the PR content.
```
