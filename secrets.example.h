#ifndef SECRETS_H
#define SECRETS_H

/**
 * Local device secrets — EXAMPLE ONLY (safe to commit).
 *
 * Copy to secrets.h (gitignored) and set your real values:
 *
 *   copy secrets.example.h secrets.h    # Windows
 *   cp secrets.example.h secrets.h      # Linux / macOS
 *
 * DEVICE_ID + API_KEY identify the node to the edge gateway (HTTP headers).
 * The gateway resolves device_type when syncing to the cloud; nursing-home and
 * resident correlation is handled by the backend from DEVICE_ID.
 */

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define GATEWAY_TELEMETRY_URL "http://192.168.1.100:5000/api/v1/monitoring/data-records"
#define DEVICE_ID "band-001"
#define API_KEY "your-api-key"
#define TELEMETRY_INTERVAL_MS 5000
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define HTTP_TIMEOUT_MS 5000

#endif // SECRETS_H
