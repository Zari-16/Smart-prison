#!/usr/bin/env bash
# Provision Raspberry Pi 5 for this project (install Docker, Docker Compose, python deps)
set -e
# Update & upgrade
sudo apt update && sudo apt upgrade -y

# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
sudo usermod -aG docker $USER

# Install Docker Compose plugin (if not present)
sudo apt install -y docker-compose-plugin

# Install Python and pip
sudo apt install -y python3 python3-pip python3-venv git build-essential

# tflite-runtime installation guidance:
echo "If you plan to run TFLite runtime on Pi, install the platform wheel for tflite-runtime."
echo "See: https://www.tensorflow.org/lite/guide/python"

# Create project directory and give instructions
echo "Provisioning complete. Reboot is recommended. After reboot, run docker compose -f docker/compose-pi.yml up -d --build in repo root."
