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
 * DEVICE_ID is flashed once per band. MAC address is read at runtime via Wi-Fi
 * and sent to POST /api/v1/auth/sign-in. Telemetry uses the returned Bearer token.
 */

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define GATEWAY_SIGN_IN_URL "http://192.168.1.100:5000/api/v1/auth/sign-in"
#define GATEWAY_TELEMETRY_URL "http://192.168.1.100:5000/api/v1/monitoring/data-records"
#define GATEWAY_THRESHOLDS_URL "http://<edge-host>:<port>/api/v1/monitoring/thresholds"
#define DEVICE_ID "band-001"
#define TELEMETRY_INTERVAL_MS 5000
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define HTTP_TIMEOUT_MS 5000

#endif // SECRETS_H
