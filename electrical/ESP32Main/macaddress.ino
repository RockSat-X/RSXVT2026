#include <WiFi.h> // Include the Wi-Fi library
//run this on either controller to find its macaddress, then plug in transmitter code
void setup() {
  Serial.begin(115200); // Start serial communication
  Serial.println(); // Print a blank line

  WiFi.mode(WIFI_STA); // Set Wi-Fi to Station mode (needed to easily get MAC)
delay(500);
  Serial.print("ESP32 MAC Address: ");
  Serial.println(WiFi.macAddress()); // Print the MAC address
}

void loop() {
  // Nothing needed here
  delay(1000);
}