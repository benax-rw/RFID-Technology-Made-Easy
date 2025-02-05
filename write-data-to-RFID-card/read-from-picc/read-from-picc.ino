#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN  9  // Reset pin for MFRC522
#define SS_PIN  10  // Slave select pin for SPI communication

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
MFRC522::MIFARE_Key key;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  // Initialize authentication key (default 0xFFFFFFFFFFFF)
  for (byte i = 0; i < 6; i++) { 
    key.keyByte[i] = 0xFF;
  }

  Serial.println("🔍 RFID Reader Ready.");
  Serial.println("👉 Enter starting block to read data.");
  Serial.println("⚠️ Forbidden blocks (sector trailers): 3, 7, 11, 15, etc.");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return; // No card detected
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("[Bring PICC closer to PCD]");
    return;
  }

  Serial.println("\n🎤 Enter the starting block number:");
  while (Serial.available() == 0) {}  // Wait for user input

  String input = Serial.readStringUntil('\n');  
  input.trim(); // Remove extra spaces

  if (!isNumeric(input)) {
    Serial.println("❌ Invalid input! Please enter a valid block number.");
    return;
  }

  byte startBlock = input.toInt();

  if (isSectorTrailer(startBlock)) {
    Serial.print("⛔ Block ");
    Serial.print(startBlock);
    Serial.println(" is a sector trailer and cannot be read!");
    return;
  }

  String retrievedData = readTransblockData(startBlock);
  Serial.println("📜 Consolidated Data from Card:");
  Serial.println(retrievedData);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(500);
}

/*
 * Check if the input is numeric
 */
bool isNumeric(String str) {
  for (unsigned int i = 0; i < str.length(); i++) {
    if (!isDigit(str[i])) {
      return false;
    }
  }
  return true;
}

/*
 * Check if a block is a sector trailer (3, 7, 11, 15, etc.)
 */
bool isSectorTrailer(byte block) {
  return (block + 1) % 4 == 0;
}

/*
 * Read multi-block data safely while skipping forbidden blocks
 */
String readTransblockData(byte startBlock) {
  String data = "";
  byte currentBlock = startBlock; 

  Serial.print("🔍 Reading data from block ");
  Serial.println(startBlock);

  for (int i = 0; i < 10; i++) { // Read up to 10 blocks (adjustable)
    byte buffer[18];
    byte bufferSize = sizeof(buffer);

    // **Skip sector trailer blocks**
    while (isSectorTrailer(currentBlock)) {
      Serial.print("⚠️ Skipping sector trailer block ");
      Serial.println(currentBlock);
      currentBlock++; // Move to the next valid block
    }

    // Authenticate the block
    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, currentBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print("❌ Authentication failed for block ");
      Serial.print(currentBlock);
      Serial.print(": ");
      Serial.println(mfrc522.GetStatusCodeName(status));
      return "";
    }

    // Read the block
    status = mfrc522.MIFARE_Read(currentBlock, buffer, &bufferSize);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("❌ Read failed for block ");
      Serial.print(currentBlock);
      Serial.print(": ");
      Serial.println(mfrc522.GetStatusCodeName(status));
      return "";
    } else {
      Serial.print("✅ Block ");
      Serial.print(currentBlock);
      Serial.print(" contains: \"");
      
      String portion = "";
      for (int j = 0; j < 16; j++) {
        if (buffer[j] == 0) break; // Stop if null terminator found
        portion += (char)buffer[j];
      }
      
      Serial.print(portion);
      Serial.println("\"");
      
      data += portion;
    }

    currentBlock++; // Move to the next block
  }

  return data;
}