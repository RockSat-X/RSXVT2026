#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h> // Usually included automatically, but good practice

// --- Configurable Parameters ---
// REPLACE WITH THE MAC ADDRESS OF YOUR RECEIVER ESP32
// Find the receiver's MAC by uploading a simple sketch that prints WiFi.macAddress()
// Or use 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF for broadcast
uint8_t receiverAddress[] = {0xD8, 0x3B, 0xDA, 0x74, 0x81, 0xFC};//D8:3B:DA:74:81:FC

// Size of data payload (bytes). Max safe value is around 240.
const uint8_t currentPayloadSize = 64;
// -----------------------------

// Define maximum payload buffer size (should be >= currentPayloadSize)
#define MAX_PAYLOAD_SIZE 240

// 1. Data Structure Definition
typedef struct struct_message {
    uint32_t sequenceNum;          // Sequence number
    uint8_t payload[MAX_PAYLOAD_SIZE]; // Data payload buffer
    uint8_t crc8;                  // CRC8 checksum
} struct_message;

// Create a variable of the structure type
struct_message myData;
uint32_t currentSequenceNum = 0;
esp_now_peer_info_t peerInfo;
volatile bool sendNextPacket = true; // Flag to control sending

// 2. CRC8 Calculation Function
// Calculates CRC8-CCITT checksum
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

// 3. Callback function when data is sent
void OnDataSent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) { // <-- CORRECTED PARAMETERS
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.print("Send Status: Fail, Error: ");
        Serial.println(status); // Print error code if failed
    }
    // Set flag to allow the loop to send the next packet
    sendNextPacket = true;
}

// 4. Setup Function
void setup() {
    Serial.begin(115200);
    Serial.println("ESP-NOW Transmitter Profiler");

    // Seed the random number generator
    randomSeed(micros());

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    Serial.print("Transmitter MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        ESP.restart(); // Restart if initialization fails
        return;
    }

    // Register the send callback
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0; // Use default channel 0
    peerInfo.encrypt = false; // No encryption for this test

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        ESP.restart(); // Restart if adding peer fails
        return;
    }
    Serial.println("Setup complete. Starting transmission...");
}

// 5. Main Loop
void loop() {
    // Send data as fast as possible using the callback flag
    if (sendNextPacket) {
        sendNextPacket = false; // Prevent sending again until callback confirms
        sendData();
    }
    // A small yield or delay can sometimes help stability if watchdog timer issues arise
    // delay(1); // Optional: uncomment if needed
    yield(); // Preferred over delay(0) or delay(1) in tight loops
}

// 6. Send Data Helper Function
void sendData() {
    myData.sequenceNum = currentSequenceNum;

    // Fill payload with non-trivial data (random bytes)
    for (int i = 0; i < currentPayloadSize; i++) {
        myData.payload[i] = random(0, 256);
    }
    // Pad the rest of the buffer if needed (optional, depends on receiver handling)
    // for (int i = currentPayloadSize; i < MAX_PAYLOAD_SIZE; i++) {
    //    myData.payload[i] = 0; // Or some padding value
    // }

    // Calculate CRC8 only on sequence number and the *actual* payload size being sent
    uint8_t bufferForCrc[sizeof(myData.sequenceNum) + currentPayloadSize];
    memcpy(bufferForCrc, &myData.sequenceNum, sizeof(myData.sequenceNum));
    memcpy(bufferForCrc + sizeof(myData.sequenceNum), myData.payload, currentPayloadSize);
    myData.crc8 = calculateCRC8(bufferForCrc, sizeof(myData.sequenceNum) + currentPayloadSize);

    // Send message via ESP-NOW
    // Note: Send the *entire structure* size, even if payload isn't full.
    // The receiver expects a fixed size based on the struct definition.
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(struct_message));

    // Error check - primary check happens in callback, this is just immediate feedback
    if (result != ESP_OK) {
        Serial.print("Immediate Send Error: ");
        Serial.println(result);
        sendNextPacket = true; // Allow trying again immediately on error
    }

    currentSequenceNum++; // Increment for the next packet
}