#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
#define led 2
#define buzzer 3

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

bool ledState = HIGH; // Variable to track LED state (LOW = off, HIGH = on)
 
void setup() {
  Serial.begin(9600);   // Initiate a serial communication
  pinMode(led, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(led, ledState);
  SPI.begin();          // Initiate SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  Serial.println("RFID Ready!");
  Serial.println();
}

void loop(){
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()){
    return;
  }
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  // Show UID on serial monitor
  Serial.print("UID tag: ");
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message: ");
  content.toUpperCase(); // Convert to uppercase (modifies the string in place)

  // Replace with your card's UID
  if(content == "53D177F5"){
    alert(1);
    Serial.println("Authorized access");
    // Toggle LED state
    ledState = !ledState; // Change state: LOW -> HIGH or HIGH -> LOW
    digitalWrite(led, ledState);
    delay(3000);
  } else{
    Serial.print("Access denied.");
    alert(10);
  }

  if(ledState==HIGH){
      Serial.println("Intruder Detection System -> ON");
  }
  else{
    Serial.println("Intruder Detection System -> OFF");
  }
  Serial.println("*********************");
}

// Alert the buzzer
void alert(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
  }
}
