#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN  9  
#define SS_PIN  10  

#define LED_PIN  2  // LED Indicator
#define BUZZER_PIN  3 // Buzzer Indicator

MFRC522 mfrc522(SS_PIN, RST_PIN); 
MFRC522::MIFARE_Key key;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize authentication key (default 0xFFFFFFFFFFFF)
  for (byte i = 0; i < 6; i++) { 
    key.keyByte[i] = 0xFF;
  }

  Serial.println("🔍 RFID Reader Ready.");
  Serial.println("🔄 Bring a card close to scan all data...");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return; // No card detected
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("[Bring PICC closer to PCD]");
    return;
  }

  Serial.println("\n📡 Card Detected! Reading all accessible data...");

  digitalWrite(LED_PIN, HIGH); // Turn ON LED while reading
  String retrievedData = readAllData();
  digitalWrite(LED_PIN, LOW); // Turn OFF LED after reading

  Serial.println("\n📜 Consolidated Data from Card:");
  Serial.println(retrievedData);
  
  alertSuccessMelody(); // Play success melody

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(500);
}

/*
 * Reads ALL data blocks while skipping sector trailers
 */
String readAllData() {
  String data = "";

  for (byte currentBlock = 1; currentBlock < 64; currentBlock++) {
    if (isSectorTrailer(currentBlock)) {
      Serial.print("⛔ Skipping sector trailer block ");
      Serial.println(currentBlock);
      continue;
    }

    byte buffer[18];
    byte bufferSize = sizeof(buffer);

    // Authenticate before reading
    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, currentBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print("❌ Authentication failed for block ");
      Serial.print(currentBlock);
      Serial.print(": ");
      Serial.println(mfrc522.GetStatusCodeName(status));
      alertError();
      continue;
    }

    // Read the block
    status = mfrc522.MIFARE_Read(currentBlock, buffer, &bufferSize);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("❌ Read failed for block ");
      Serial.print(currentBlock);
      Serial.print(": ");
      Serial.println(mfrc522.GetStatusCodeName(status));
      alertError();
      continue;
    } else {
      Serial.print("✅ Block ");
      Serial.print(currentBlock);
      Serial.print(" contains: \"");
      
      String portion = "";
      for (int j = 0; j < 16; j++) {
        if (buffer[j] == 0) break;  // Stop reading at NULL terminator
        portion += (char)buffer[j];
      }
      
      Serial.print(portion);
      Serial.println("\"");
      
      data += portion;
    }
  }

  return data;
}

/*
 * Check if a block is a sector trailer (3, 7, 11, 15, etc.)
 */
bool isSectorTrailer(byte block) {
  return (block + 1) % 4 == 0;
}

/*
 * Success Melody (Ascending Beeps)
 */
void alertSuccessMelody() {
  int melody[] = {1000, 1200, 1400, 1600}; // Frequency of tones
  int duration = 150; // Duration of each tone in ms

  for (int i = 0; i < 4; i++) {
    tone(BUZZER_PIN, melody[i], duration);
    delay(duration + 50); // Add short pause
  }
  noTone(BUZZER_PIN); // Stop buzzer sound
}

/*
 * Error Beep (Three Beeps)
 */
void alertError() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000, 200);
    delay(400);
  }
  noTone(BUZZER_PIN);
}