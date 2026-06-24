/**
 * @file EdgeHttpClient.cpp
 * @brief Implements the EdgeHttpClient class.
 *
 * @author Metasoft
 * @date June 2026
 * @version 0.2
 */

#include "EdgeHttpClient.h"
#include "secrets.h"

#include <HTTPClient.h>
#include <WiFi.h>

EdgeHttpClient::BlockingYieldHook EdgeHttpClient::blockingYieldHook = nullptr;

EdgeHttpClient::EdgeHttpClient()
    : wifiReady(false), wifiResumePending(false), lastPublishMs(0), accessToken("") {}

void EdgeHttpClient::setBlockingYieldHook(BlockingYieldHook hook) {
    blockingYieldHook = hook;
}

bool EdgeHttpClient::connectWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (blockingYieldHook != nullptr) {
            blockingYieldHook();
        }
        if (millis() - startMs >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println(F("Servidor edge: tiempo de espera Wi-Fi agotado"));
            return false;
        }
        delay(50);
    }

    return true;
}

bool EdgeHttpClient::extractAccessToken(const String& responseBody, String& tokenOut) {
    const int keyStart = responseBody.indexOf("\"access_token\"");
    if (keyStart < 0) {
        return false;
    }

    int cursor = keyStart + static_cast<int>(strlen("\"access_token\""));
    while (cursor < responseBody.length()) {
        const char ch = responseBody.charAt(cursor);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            cursor++;
            continue;
        }
        if (ch != ':') {
            return false;
        }
        cursor++;
        break;
    }

    while (cursor < responseBody.length()) {
        const char ch = responseBody.charAt(cursor);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            cursor++;
            continue;
        }
        if (ch != '"') {
            return false;
        }
        cursor++;
        break;
    }

    const int valueStart = cursor;
    const int valueEnd = responseBody.indexOf('"', valueStart);
    if (valueEnd < 0) {
        return false;
    }

    tokenOut = responseBody.substring(valueStart, valueEnd);
    return tokenOut.length() > 0;
}

bool EdgeHttpClient::signIn() {
    if (!connectWifi()) {
        wifiReady = false;
        return false;
    }
    wifiReady = true;

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(GATEWAY_SIGN_IN_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Id", DEVICE_ID);
    http.addHeader("X-Device-Mac", WiFi.macAddress());

    const int responseCode = http.POST("{}");
    const String responseBody = http.getString();
    http.end();

    if (responseCode < 200 || responseCode >= 300) {
        accessToken = "";
        Serial.printf(
            "Servidor edge: sign-in fallido (%d) %s\n",
            responseCode,
            responseBody.c_str()
        );
        return false;
    }

    String token;
    if (!extractAccessToken(responseBody, token)) {
        accessToken = "";
        Serial.println(F("Servidor edge: sign-in sin access_token"));
        return false;
    }

    accessToken = token;
    Serial.printf("Servidor edge: sign-in exitoso (%s)\n", DEVICE_ID);
    return true;
}

bool EdgeHttpClient::ensureSignedIn() {
    if (accessToken.length() > 0) {
        return true;
    }
    return signIn();
}

bool EdgeHttpClient::begin() {
    wifiReady = connectWifi();
    if (!wifiReady) {
        return false;
    }

    Serial.printf("Servidor edge: Wi-Fi conectado (%s)\n", DEVICE_ID);
    Serial.printf("MAC Wi-Fi: %s\n", WiFi.macAddress().c_str());
    return signIn();
}

bool EdgeHttpClient::isConnected() const {
    return wifiReady && WiFi.status() == WL_CONNECTED;
}

bool EdgeHttpClient::isAuthenticated() const {
    return accessToken.length() > 0;
}

bool EdgeHttpClient::suspendWifi() {
    wifiResumePending = WiFi.status() == WL_CONNECTED;
    if (wifiResumePending) {
        WiFi.disconnect(true);
    }
    WiFi.mode(WIFI_OFF);
    delay(1);
    accessToken = "";
    return wifiResumePending;
}

bool EdgeHttpClient::resumeWifi() {
    if (!wifiResumePending) {
        return false;
    }

    wifiResumePending = false;
    wifiReady = connectWifi();
    if (wifiReady) {
        return signIn();
    }
    return false;
}

const char* EdgeHttpClient::getDeviceId() const {
    return DEVICE_ID;
}

bool EdgeHttpClient::postJson(const String& body, int& responseCode, bool allowRetry) {
    if (!connectWifi()) {
        wifiReady = false;
        return false;
    }
    wifiReady = true;

    if (!ensureSignedIn()) {
        return false;
    }

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(GATEWAY_TELEMETRY_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + accessToken);

    responseCode = http.POST(body);
    const String responseBody = http.getString();
    const bool ok = responseCode >= 200 && responseCode < 300;
    http.end();

    if (ok) {
        Serial.println(F("Servidor edge: datos enviados"));
        return true;
    }

    if (responseCode == 401 && allowRetry) {
        accessToken = "";
        if (signIn()) {
            return postJson(body, responseCode, false);
        }
    }

    Serial.printf(
        "Servidor edge: error al enviar (%d) %s\n",
        responseCode,
        responseBody.c_str()
    );
    return false;
}

void EdgeHttpClient::appendDiagnostics(String& body, bool& first, const SensorDiagnostics& diagnostics) {
    if (!first) {
        body += ",";
    }
    first = false;

    body += F("\"diagnostics\":{");

    body += F("\"max30102\":{");
    body += F("\"initialized\":");
    body += diagnostics.max30102Initialized ? "true" : "false";
    body += F(",\"heart_rate_valid\":");
    body += diagnostics.max30102HeartRateValid ? "true" : "false";
    body += F(",\"spo2_valid\":");
    body += diagnostics.max30102SpO2Valid ? "true" : "false";
    body += F(",\"finger_detected\":");
    body += diagnostics.max30102FingerDetected ? "true" : "false";
    body += F(",\"signal_saturated\":");
    body += diagnostics.max30102SignalSaturated ? "true" : "false";
    body += F(",\"ir_average\":");
    body += String(diagnostics.max30102IrAverage);
    body += F(",\"ir_variation\":");
    body += String(diagnostics.max30102IrVariation);
    body += F(",\"status\":\"");
    body += diagnostics.max30102Status;
    body += F("\"},");

    body += F("\"lm35\":{");
    body += F("\"reading_celsius\":");
    body += String(diagnostics.lm35ReadingCelsius, 1);
    body += F(",\"body_contact\":");
    body += diagnostics.lm35BodyContact ? "true" : "false";
    body += F(",\"status\":\"");
    body += diagnostics.lm35Status;
    body += F("\"},");

    body += F("\"gps\":{");
    body += F("\"receiving_nmea\":");
    body += diagnostics.gpsReceivingNmea ? "true" : "false";
    body += F(",\"fix_valid\":");
    body += diagnostics.gpsFixValid ? "true" : "false";
    body += F(",\"satellite_count\":");
    body += String(diagnostics.gpsSatelliteCount);
    body += F(",\"satellites_in_view\":");
    body += String(diagnostics.gpsSatellitesInView);
    body += F(",\"bytes_received\":");
    body += String(diagnostics.gpsBytesReceived);
    body += F(",\"baud_rate\":");
    body += String(diagnostics.gpsBaudRate);
    body += F(",\"status\":\"");
    body += diagnostics.gpsStatus;
    body += F("\"},");

    body += F("\"lcd\":{");
    body += F("\"initialized\":");
    body += diagnostics.lcdInitialized ? "true" : "false";
    body += F("},");

    body += F("\"network\":{");
    body += F("\"wifi_connected\":");
    body += diagnostics.wifiConnected ? "true" : "false";
    body += F(",\"rssi_dbm\":");
    body += String(diagnostics.wifiRssiDbm);
    if (diagnostics.wifiConnected) {
        body += F(",\"mac_address\":\"");
        body += WiFi.macAddress();
        body += F("\"");
    }
    body += F("}}");
}

bool EdgeHttpClient::publishSnapshot(const TelemetrySnapshot& snapshot, const SensorDiagnostics& diagnostics) {
    const unsigned long now = millis();
    if (lastPublishMs != 0 && (now - lastPublishMs) < TELEMETRY_INTERVAL_MS) {
        return false;
    }

    if (!wifiReady && !begin()) {
        return false;
    }

    if (!ensureSignedIn()) {
        return false;
    }

    String body = "{";
    bool first = true;

    auto appendInt = [&](const char* key, int value) {
        if (!first) {
            body += ",";
        }
        first = false;
        body += "\"";
        body += key;
        body += "\":";
        body += String(value);
    };

    auto appendFloat = [&](const char* key, float value, int decimals) {
        if (!first) {
            body += ",";
        }
        first = false;
        body += "\"";
        body += key;
        body += "\":";
        body += String(value, decimals);
    };

    if (snapshot.heartRateValid) {
        appendInt("heart_rate", snapshot.heartRate);
    }
    if (snapshot.oxygenSaturationValid) {
        appendInt("oxygen_saturation", snapshot.oxygenSaturation);
    }
    if (snapshot.bodyTemperatureValid) {
        appendFloat("temperature", snapshot.bodyTemperatureCelsius, 1);
    }
    if (snapshot.ambientTemperatureValid) {
        appendFloat("ambient_temperature", snapshot.ambientTemperatureCelsius, 1);
    }
    if (snapshot.locationValid) {
        appendFloat("latitude", snapshot.latitude, 6);
        appendFloat("longitude", snapshot.longitude, 6);
    }
    if (snapshot.satelliteCountValid) {
        appendInt("satellite_count", snapshot.satelliteCount);
    }
    if (snapshot.satellitesInViewValid) {
        appendInt("satellites_in_view", snapshot.satellitesInView);
    }

    appendDiagnostics(body, first, diagnostics);

    body += "}";

    int responseCode = 0;
    const bool ok = postJson(body, responseCode, true);
    lastPublishMs = now;
    return ok;
}
