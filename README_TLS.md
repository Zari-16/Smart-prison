```
TLS & production certificates (recommended)
1. Best practice: use a real DNS name (edge.example.com) and get Let's Encrypt certs on the Pi.
2. Use nginx reverse proxy to terminate TLS (see docker/nginx conf). Configure certbot on Pi to create /etc/letsencrypt/live/edge.example.com.
3. Ensure nginx is configured to proxy to the edge container (http://edge:5000).
4. For MQTT TLS use server cert (mosquitto) and CA. You may reuse Let's Encrypt certs for mosquitto but consider using a separate CA for broker mutual auth.
5. If devices cannot validate public CA (private LAN), distribute your CA certificate to devices (MKR clients) for server validation (or rely on API key + internal VLAN).
6. For mutual TLS (client certs), configure Mosquitto and nginx to require client certificates. MKR1010 support for client certs is limited; consider tokens instead.
```
