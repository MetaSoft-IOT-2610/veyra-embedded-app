#ifndef EDGE_HTTP_CLIENT_H
#define EDGE_HTTP_CLIENT_H

/**
 * @file EdgeHttpClient.h
 * @brief HTTP client that publishes vital-signs telemetry to the Veyra edge service.
 *
 * @author Metasoft
 * @date June 2026
 * @version 0.1
 */

#include <Arduino.h>

/**
 * @brief Snapshot of every sensor reading available for edge telemetry.
 */
struct TelemetrySnapshot {
    int heartRate;
    bool heartRateValid;
    int oxygenSaturation;
    bool oxygenSaturationValid;
    float bodyTemperatureCelsius;
    bool bodyTemperatureValid;
    float ambientTemperatureCelsius;
    bool ambientTemperatureValid;
    float latitude;
    float longitude;
    bool locationValid;
    int satelliteCount;
    bool satelliteCountValid;
    int satellitesInView;
    bool satellitesInViewValid;
};

/**
 * @brief Per-sensor health and diagnostic signals for remote troubleshooting.
 */
struct SensorDiagnostics {
    bool max30102Initialized;
    bool max30102HeartRateValid;
    bool max30102SpO2Valid;
    bool max30102FingerDetected;
    bool max30102SignalSaturated;
    uint32_t max30102IrAverage;
    uint32_t max30102IrVariation;
    const char* max30102Status;

    float lm35ReadingCelsius;
    bool lm35BodyContact;
    const char* lm35Status;

    bool gpsReceivingNmea;
    bool gpsFixValid;
    int gpsSatelliteCount;
    int gpsSatellitesInView;
    unsigned long gpsBytesReceived;
    long gpsBaudRate;
    const char* gpsStatus;

    bool lcdInitialized;

    bool wifiConnected;
    int wifiRssiDbm;
};

/**
 * @brief Connects to Wi-Fi and POSTs JSON payloads to the edge monitoring API.
 *
 * Identifies the node via sign-in (X-Device-Id, X-Device-Mac) then Bearer token
 * on telemetry POSTs. The JSON body carries sensor readings only.
 */
class EdgeHttpClient {
private:
    bool wifiReady;
    bool wifiResumePending;
    unsigned long lastPublishMs;
    String accessToken;

    bool connectWifi();
    bool signIn();
    bool ensureSignedIn();
    static bool extractAccessToken(const String& responseBody, String& tokenOut);
    bool postJson(const String& body, int& responseCode, bool allowRetry);
    static void appendDiagnostics(String& body, bool& first, const SensorDiagnostics& diagnostics);

public:
    EdgeHttpClient();

    /**
     * @brief Initializes Wi-Fi connectivity.
     * @return true when connected to Wi-Fi.
     */
    bool begin();

    /** @return true when Wi-Fi is connected. */
    bool isConnected() const;

    /** @return true when the edge sign-in returned an access token. */
    bool isAuthenticated() const;

    /**
     * @brief Turns Wi-Fi off so ADC2 GPIOs (32/33) can be used for I2C.
     * @return true when Wi-Fi was connected before suspend.
     */
    bool suspendWifi();

    /** @brief Restores Wi-Fi after @ref suspendWifi(). */
    bool resumeWifi();

    /** @return configured node identifier (DEVICE_ID in secrets.h). */
    const char* getDeviceId() const;

    /**
     * @brief POST sensor readings and diagnostics to the edge.
     *
     * @return true when the edge responded with HTTP 2xx.
     */
    bool publishSnapshot(const TelemetrySnapshot& snapshot, const SensorDiagnostics& diagnostics);
};

#endif // EDGE_HTTP_CLIENT_H
