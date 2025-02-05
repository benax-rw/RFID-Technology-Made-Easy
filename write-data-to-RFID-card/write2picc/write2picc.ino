#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN  9  // Reset pin for MFRC522
#define SS_PIN  10  // Slave select pin for SPI communication

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
MFRC522::MIFARE_Key key;
MFRC522::StatusCode card_status;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  // Initialize the authentication key (default 0xFFFFFFFFFFFF)
  for (byte i = 0; i < 6; i++) { 
    key.keyByte[i] = 0xFF;
  }

  Serial.println("🔒 RFID Writer Ready.");
  Serial.println("👉 Enter data in the format: {data} -> {block}");
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

  Serial.println("\n🎤 Enter data in the format: {data} -> {block}");
  while (Serial.available() == 0) {}  // Wait for user input

  String input = Serial.readStringUntil('\n');  // Read user input
  input.trim(); // Remove extra spaces

  if (!validateInputFormat(input)) {
    Serial.println("❌ Invalid format! Use: {data} -> {block}");
    return;
  }

  String data = getDataFromInput(input);
  byte startBlock = getBlockFromInput(input);

  if (isSectorTrailer(startBlock)) {
    Serial.print("⛔ Block ");
    Serial.print(startBlock);
    Serial.println(" is a sector trailer and cannot be used as a starting block!");
    return;
  }

  writeTransblockData(startBlock, data);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(500);
}

/*
 * Validate input format {data} -> {block} (allowing flexible spaces)
 */
bool validateInputFormat(String input) {
  input.replace(" -> ", "->");
  input.replace(" > ", "->");  // Normalize different input formats

  int separatorIndex = input.indexOf("->");
  if (separatorIndex == -1) {
    return false; // No separator found
  }
  
  String dataPart = input.substring(0, separatorIndex);
  String blockPart = input.substring(separatorIndex + 2);
  
  dataPart.trim();
  blockPart.trim();

  if (dataPart.length() == 0 || blockPart.length() == 0) {
    return false; // Missing data or block number
  }

  for (int i = 0; i < blockPart.length(); i++) {
    if (!isDigit(blockPart[i])) {
      return false; // Block should be numeric
    }
  }

  return true;
}

/*
 * Extract data from input
 */
String getDataFromInput(String input) {
  int separatorIndex = input.indexOf("->");
  String data = input.substring(0, separatorIndex);
  data.trim();
  return data;
}

/*
 * Extract block number from input
 */
byte getBlockFromInput(String input) {
  int separatorIndex = input.indexOf("->");
  String blockStr = input.substring(separatorIndex + 2);
  blockStr.trim();
  return blockStr.toInt();
}

/*
 * Check if a block is a sector trailer (3, 7, 11, 15, etc.)
 */
bool isSectorTrailer(byte block) {
  return (block + 1) % 4 == 0; // Sector trailer formula
}

/*
 * Write long data across multiple blocks while skipping forbidden ones
 */
void writeTransblockData(byte startBlock, String data) {
  int dataLen = data.length();
  int numBlocks = (dataLen / 16) + ((dataLen % 16) ? 1 : 0); // Number of blocks needed

  Serial.print("📝 Writing ");
  Serial.print(dataLen);
  Serial.print(" bytes across ");
  Serial.print(numBlocks);
  Serial.println(" blocks.");

  byte currentBlock = startBlock; // Start writing from the given block

  for (int i = 0; i < numBlocks; i++) {
    byte buff[16] = {0}; // Buffer for each block

    // Copy up to 16 bytes from data into buffer
    String portion = "";
    for (int j = 0; j < 16; j++) {
      int dataIndex = (i * 16) + j;
      if (dataIndex < dataLen) {
        buff[j] = data[dataIndex];
        portion += (char)buff[j];
      } else {
        break; // Stop if data ends before 16 bytes
      }
    }

    // **Skip sector trailer blocks**
    while (isSectorTrailer(currentBlock)) {
      Serial.print("⚠️ Skipping sector trailer block ");
      Serial.println(currentBlock);
      currentBlock++; // Move to the next available block
    }

    // Authenticate block
    card_status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, currentBlock, &key, &(mfrc522.uid));
    if (card_status != MFRC522::STATUS_OK) {
      Serial.print("❌ Authentication failed for block ");
      Serial.print(currentBlock);
      Serial.print(": ");
      Serial.println(mfrc522.GetStatusCodeName(card_status));
      return;
    }

    // Write data to the block
    card_status = mfrc522.MIFARE_Write(currentBlock, buff, 16);
    if (card_status != MFRC522::STATUS_OK) {
      Serial.print("❌ Write failed for block ");
      Serial.print(currentBlock);
      Serial.print(": ");
      Serial.println(mfrc522.GetStatusCodeName(card_status));
      return;
    } else {
      Serial.print("✅ Block ");
      Serial.print(currentBlock);
      Serial.print(" written successfully: \"");
      Serial.print(portion);
      Serial.println("\"");
    }

    currentBlock++; // Move to the next block
  }
}