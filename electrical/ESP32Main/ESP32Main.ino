#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h> 

// --- Configurable Parameters ---
// Must match the Vehicle's currentPayloadSize to calculate CRC correctly!
const uint8_t currentPayloadSize = 64;

// Size of the payload buffer (must match Vehicle's MAX_PAYLOAD_SIZE)
//It is also something you can change for testing purposes
//do not go above 240 as that is best for ESPNOW protocol
#define MAX_PAYLOAD_SIZE 240
// -----------------------------

// 1. Data Structure Definition (must match Vehicle)
typedef struct struct_message {
    uint32_t sequenceNum;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint8_t crc8;
} struct_message;

// Create a variable to store received data
struct_message receivedData;

// 2. CRC8 Calculation Function (must match Vehicle)
uint8_t calculateCRC8(const uint8_t *data, size_t length) {
    uint8_t crc = 0xFF; // Initial value
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07; // Polynomial 0x07
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// 3. Statistics Variables
volatile uint32_t packetsReceived = 0;
volatile uint32_t packetsDropped = 0;
volatile uint32_t packetsCorrupted = 0;
volatile uint32_t lastSequenceNum = 0; // Use volatile as it's modified in ISR (callback)
volatile bool firstPacket = true;     // Use volatile
unsigned long startTime = 0;
unsigned long totalBytesReceived = 0; // For throughput calculation
const unsigned long statInterval = 2000; // Print stats every 2000 ms (2 seconds)
unsigned long lastStatTime = 0;


// 4. Callback function when data is received
void OnDataRecv(const esp_now_recv_info * info, const uint8_t *incomingData, int len) { 

    // Check if received data size matches our expected structure size
    if (len != sizeof(struct_message)) {
        // This is unlikely if sender sends fixed size, but good to check
        // Handle this case - maybe count as corrupted or just log an error
        packetsCorrupted++; // Example handling
        return; 
    }

    // Copy data into our structure
    memcpy(&receivedData, incomingData, sizeof(receivedData));

   
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

    // 2. Verify Checksum (CRC8)
    uint8_t bufferForCrc[sizeof(receivedData.sequenceNum) + currentPayloadSize];
    memcpy(bufferForCrc, &receivedData.sequenceNum, sizeof(receivedData.sequenceNum));
    memcpy(bufferForCrc + sizeof(receivedData.sequenceNum), receivedData.payload, currentPayloadSize);
    uint8_t calculated_crc = calculateCRC8(bufferForCrc, sizeof(receivedData.sequenceNum) + currentPayloadSize);

    bool crcOk = (calculated_crc == receivedData.crc8);

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
     // Optional: Handle sequence number rollover (e.g., if Vehicle restarts at 0)
     //last sequence check is 1000 to ensure that it wasn't simplly a jitter and was in fact a system reset from Vehicle transmission
    else if (crcOk && receivedData.sequenceNum == 0 && lastSeq > 1000) { // Simple rollover check
        lastSequenceNum = 0;
        totalBytesReceived += currentPayloadSize;
    }


    // Keep serial printing outside the most critical part if possible
    //uncomment if wanted to debug and see each time a data packet is sent
    // Serial.printf("Recv: Seq=%u, CRC=%s, Drops=%u\n", receivedData.sequenceNum, crcOk ? "OK" : "FAIL", dropsDetected);

} 

// 5. Setup Function
void setup() {
    Serial.begin(115200);
    Serial.println("ESP-NOW Receiver Profiler");

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
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
             totalExpected = lastSeq + 1; // Assuming sequence starts at 0
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
        Serial.println("-------------------------");
    }

    // Add a small delay or yield to be nice to the system
    delay(10);
}
