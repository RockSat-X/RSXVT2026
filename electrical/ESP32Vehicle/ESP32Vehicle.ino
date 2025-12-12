#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h> 
#include <esp_wifi.h>
// --- Configurable Parameters ---
// REPLACE WITH THE MAC ADDRESS OF YOUR RECEIVER ESP32
//Run the ESP32Main file to get the address of device. You can find it in the serial monitor of the Main code
// For unicast method Find the receiver's MAC by uploading a simple sketch that prints WiFi.macAddress()
// Or use 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF for broadcast
//unicast allows for more precise data transmission
uint8_t receiverAddress[] = {0xD8, 0x3B, 0xDA, 0x74, 0x81, 0xFC};//input MAC address of receiver here if using unicast

// Size of data payload (bytes). Max safe value is around 240.
//change if desired
const uint8_t currentPayloadSize = 64;

// Define maximum payload buffer size (should be >= currentPayloadSize)
#define MAX_PAYLOAD_SIZE 240

// 1. Data Structure Definition
// __attribute__((packed)) prevents invisible padding bytes
typedef struct __attribute__((packed)) struct_message {
    uint32_t sequenceNum;          // Sequence number
        uint32_t crc32;                  // CRC32 checksum
    uint8_t payload[MAX_PAYLOAD_SIZE]; // Data payload buffer
} struct_message;

// Create a variable of the structure type
struct_message myData;
uint32_t currentSequenceNum = 0;
//info for receiver 
esp_now_peer_info_t peerInfo;
volatile bool sendNextPacket = true; // Flag to control sending

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

// 3. Callback function when data is sent
//checks whether packet was sent succesfully and acts as a pit stop to prevent and buffer overload
void OnDataSent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) { 
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
    //added this wifi set channel function last minute, so if anythings off comment this out
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);//channel 1
    delay(500); //just added to give bootup more time 
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
    peerInfo.channel = 1; // Use channel 1
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
    yield(); 
}

// 6. Main function that sends data
void sendData() {
    myData.sequenceNum = currentSequenceNum;

    // Fill payload with non-trivial data (random bytes)
    for (int i = 0; i < currentPayloadSize; i++) {
        myData.payload[i] = random(0, 256);
    }
    // Pad the rest of the buffer if needed 
    // for (int i = currentPayloadSize; i < MAX_PAYLOAD_SIZE; i++) {
    //    myData.payload[i] = 0; // Or some padding value
    // }

// Calculate CRC32
    // We create a temporary buffer containing: [SequenceNum] + [Active Payload]
    uint8_t bufferForCrc[sizeof(myData.sequenceNum) + currentPayloadSize];
    memcpy(bufferForCrc, &myData.sequenceNum, sizeof(myData.sequenceNum));
    memcpy(bufferForCrc + sizeof(myData.sequenceNum), myData.payload, currentPayloadSize);
    // Store the result in the new 32-bit field
    myData.crc32 = calculateCRC32(bufferForCrc, sizeof(bufferForCrc));
    // Send message via ESP-NOW
// now only sends active data
size_t packetSize = sizeof(myData.sequenceNum) + sizeof(myData.crc32) + currentPayloadSize;
esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, packetSize);
    // Error check - primary check happens in callback, this is just immediate feedback
    if (result != ESP_OK) {
        Serial.print("Immediate Send Error: ");
        Serial.println(result);
        sendNextPacket = true; // Allow trying again immediately on error
    }

    currentSequenceNum++; // Increment for the next packet
}