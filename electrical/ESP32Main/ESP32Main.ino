#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h> 
#include <esp_wifi.h>
// --- Configurable Parameters ---
// Must match the transmitter's currentPayloadSize to calculate CRC correctly!
const uint8_t currentPayloadSize = 64;

// Size of the payload buffer (must match transmitter's MAX_PAYLOAD_SIZE)
//It is also something you can change for testing purposes
//keep at 240 as that is what is best for espnow protocol
#define MAX_PAYLOAD_SIZE 240
// -----------------------------

// 1. Data Structure Definition (must match transmitter)
// __attribute__((packed)) prevents invisible padding bytes
typedef struct __attribute__((packed)) struct_message {
    uint32_t sequenceNum;
     uint32_t crc32;//using this to implement CRC
    uint8_t payload[MAX_PAYLOAD_SIZE];
} struct_message;

// Create a variable to store received data
struct_message receivedData;

// 2. CRC32 Calculation Function (Standard IEEE 802.3)
uint32_t calculateCRC32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        uint8_t c = data[i];
        for (uint32_t j = 0; j < 8; j++) {
            if ((c ^ crc) & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
            c >>= 1;
        }
    }
    return ~crc;
}

// 3. Statistics Variables
volatile uint32_t packetsReceived = 0;
volatile uint32_t packetsDropped = 0;
volatile uint32_t packetsCorrupted = 0;
volatile uint32_t lastSequenceNum = 0; // Use volatile as it's modified in ISR (callback)
volatile bool firstPacket = true;     // Use volatile
volatile uint32_t initialSequenceNum = 0; // New variable to remember where we started
volatile int32_t currentRSSI = 0; // Stores signal strength in dBm
unsigned long startTime = 0;
unsigned long totalBytesReceived = 0; // For throughput calculation
const unsigned long statInterval = 2000; // Print stats every 2000 ms (2 seconds)
unsigned long lastStatTime = 0;


// 4. Callback function when data is received
void OnDataRecv(const esp_now_recv_info * info, const uint8_t *incomingData, int len) { 
currentRSSI = info->rx_ctrl->rssi; // Get RSSI from the packet info
    // Check if received data size matches our expected structure size
// Check against size of Seq + CRC32 + CurrentPayload
    size_t expectedSize = sizeof(receivedData.sequenceNum) + sizeof(receivedData.crc32) + currentPayloadSize;

if (len != expectedSize) {
    packetsCorrupted++; 
    return;
}
  

    memcpy(&receivedData, incomingData, len);

    // --- Critical Section Start (Minimize processing time here) ---

    bool isFirst = firstPacket; // Local copy
    uint32_t lastSeq = lastSequenceNum; // Local copy

    // 1. Check for Dropped Packets using Sequence Number
    uint32_t dropsDetected = 0;
    if (!isFirst) {
        if (receivedData.sequenceNum > lastSeq + 1) {
            dropsDetected = (receivedData.sequenceNum - (lastSeq + 1));
        } else if (receivedData.sequenceNum <= lastSeq && lastSeq != 0) {
            // Ignore duplicate or out-of-order packets for drop count,
            // but might indicate other issues.
            // A simple way is to just not update lastSequenceNum later for these.
            // If sequenceNum is 0, it might be a rollover/restart.
        }
    }

    
// verify checksum(CRC32)
uint8_t bufferForCrc[sizeof(receivedData.sequenceNum) + currentPayloadSize];
    memcpy(bufferForCrc, &receivedData.sequenceNum, sizeof(receivedData.sequenceNum));
    memcpy(bufferForCrc + sizeof(receivedData.sequenceNum), receivedData.payload, currentPayloadSize);

    uint32_t calculated_crc = calculateCRC32(bufferForCrc, sizeof(bufferForCrc));

    bool crcOk = (calculated_crc == receivedData.crc32);

    // 3. Update Statistics (atomically or carefully if using interrupts)
    // Using simple increments here, assuming callback runs sequentially
    packetsReceived++;
    packetsDropped += dropsDetected;
    if (!crcOk) {
        packetsCorrupted++;
    }

    // Update state variables
    if (isFirst) {
        firstPacket = false; // Mark that we've received the first packet
        lastSequenceNum = receivedData.sequenceNum; // Initialize sequence
        initialSequenceNum = receivedData.sequenceNum;//to reset success rate
        startTime = millis(); // Start timer on the *first successfully processed* packet
        if (crcOk) totalBytesReceived += currentPayloadSize; // Only count valid bytes
    }
    // Only update lastSequenceNum if CRC is OK AND it's a newer packet
    // This prevents corrupted sequence numbers messing up drop counts
    // and prevents duplicates from being counted as progress.
    else if (crcOk && receivedData.sequenceNum > lastSeq) {
        lastSequenceNum = receivedData.sequenceNum;
        totalBytesReceived += currentPayloadSize;
    }
     // Optional: Handle sequence number rollover (e.g., if transmitter restarts at 0)
    else if (crcOk && receivedData.sequenceNum == 0 && lastSeq > 1000) { // Simple rollover check
        lastSequenceNum = 0;
        totalBytesReceived += currentPayloadSize;
    }


    // --- Critical Section End ---

    // Keep serial printing outside the most critical part if possible
    // Serial.printf("Recv: Seq=%u, CRC=%s, Drops=%u\n", receivedData.sequenceNum, crcOk ? "OK" : "FAIL", dropsDetected);

} 

// 5. Setup Function
void setup() {
    Serial.begin(115200);
    Serial.println("ESP-NOW Receiver Profiler");

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    // This must match the Transmitter's channel exactly
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    delay(500);
    Serial.print("Receiver MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        ESP.restart();
        return;
    }

    // Register receive callback
    esp_now_register_recv_cb(OnDataRecv);

    Serial.println("Setup complete. Waiting for data...");
    lastStatTime = millis(); // Initialize stat timer
}

// 6. Main Loop
void loop() {
    // Print statistics periodically
    unsigned long now = millis();
    if (startTime > 0 && now - lastStatTime >= statInterval) { // Don't print stats until first packet arrives
        lastStatTime = now;

        // --- Read volatile variables carefully for statistics calculation ---
        noInterrupts(); // Briefly disable interrupts to get consistent snapshot
        uint32_t rxCount = packetsReceived;
        uint32_t dropCount = packetsDropped;
        uint32_t corruptCount = packetsCorrupted;
        uint32_t lastSeq = lastSequenceNum;
        bool first = firstPacket;
        interrupts(); // Re-enable interrupts quickly
        // --- End snapshot ---

        unsigned long elapsedTimeMs = now - startTime;
        float elapsedTimeSec = elapsedTimeMs / 1000.0f;

        // Calculate total expected packets based on the last *valid* sequence number received
        uint32_t totalExpected = 0;
        if (!first) {
             totalExpected =(lastSeq - initialSequenceNum) + 1; 
        }


        float lossRate = (totalExpected > 0) ? (float)(dropCount + corruptCount) / totalExpected * 100.0f : 0.0f;
        // Cap loss rate at 100%
        if (lossRate > 100.0f) lossRate = 100.0f;
        if (lossRate < 0.0f) lossRate = 0.0f; // Sanity check

        float successRate = (totalExpected > 0) ? (float)(rxCount - corruptCount) / totalExpected * 100.0f : 0.0f;
         if (successRate > 100.0f) successRate = 100.0f; // Sanity check
         if (successRate < 0.0f) successRate = 0.0f;

        // Throughput based on bytes in *successfully received, non-corrupted* packets' payloads
        float throughput_kbps = (elapsedTimeSec > 0.1) ? (float)(totalBytesReceived * 8.0f) / elapsedTimeSec / 1000.0f : 0.0f; // kbps

        Serial.println("-------------------------");
        Serial.printf("Time Elapsed: %.2f s\n", elapsedTimeSec);
        Serial.printf("Packets Received (Total): %u\n", rxCount);
        Serial.printf("Packets Dropped (Estimated): %u\n", dropCount);
        Serial.printf("Packets Corrupted (CRC Fail): %u\n", corruptCount);
        Serial.printf("Last Good Sequence #: %u\n", lastSeq);
        Serial.printf("Total Expected (Based on Last Seq): %u\n", totalExpected);
        Serial.printf("Success Rate (Good CRC / Expected): %.2f %%\n", successRate);
        Serial.printf("Packet Loss Rate (Drops+Corrupt / Expected): %.2f %%\n", lossRate);
        Serial.printf("Avg Throughput (Good Payloads): %.2f kbps\n", throughput_kbps);
        Serial.printf("Signal Strength (RSSI): %d dBm\n", currentRSSI);
        Serial.println("-------------------------");
    }

    // Add a small delay or yield to be nice to the system
    delay(10);
}
