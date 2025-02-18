#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN  9  
#define SS_PIN  10  

#define LED_PIN  2  // LED Indicator
#define BUZZER_PIN  3 // Buzzer Indicator

MFRC522 mfrc522(SS_PIN, RST_PIN); 
MFRC522::MIFARE_Key key;
MFRC522::StatusCode card_status;

const int MAX_WRITABLE_BLOCKS = 48;  
const int MAX_DATA_SIZE = MAX_WRITABLE_BLOCKS * 16;  

void setup(){
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  for (byte i = 0; i < 6; i++) { 
    key.keyByte[i] = 0xFF;
  }

  Serial.println("RFID Writer Ready.");
  Serial.println("> Enter data in the format: {data} -> {block}");
  Serial.println("Forbidden blocks (sector trailers): 3, 7, 11, 15, etc.");
}

void loop(){
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Bring card closer to reader");
    return;
  }

  Serial.println("\n> Enter data in the format: {data} -> {block}");
  while (Serial.available() == 0) {}  

  String input = Serial.readStringUntil('#');  
  input.trim(); 

  // ✅ Extract block number BEFORE checking data length
  byte startBlock = getBlockFromInput(input);
  if(startBlock == 255) { // If block extraction failed
    Serial.println("Error: No valid block number detected!");
    alertError();
    return;
  }

  String data = getDataFromInput(input);

  if (isSectorTrailer(startBlock)) {
    Serial.print("Block ");
    Serial.print(startBlock);
    Serial.println(" is a sector trailer and cannot be written to!");
    alertError();
    return;
  }

  int availableBytes = MAX_DATA_SIZE - (startBlock * 16);
  int dataLen = data.length();

  if (dataLen > availableBytes) {
    Serial.print("Not enough space! Only ");
    Serial.print(availableBytes);
    Serial.println(" bytes can be stored.");
    Serial.println("Writing what fits...");
    data = data.substring(0, availableBytes);
    Serial.println("Data that WILL be written: " + data);
  }

  digitalWrite(LED_PIN, HIGH);
  writeTransblockData(startBlock, data);
  digitalWrite(LED_PIN, LOW);

  if (dataLen > availableBytes) {
    Serial.println("Error: Data too long! Some was left out.");
    alertError();
  } else {
    alertSuccessMelody();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(500);
}

/*
 * Extract block number FIRST before processing data
 */
byte getBlockFromInput(String input) {
  int separatorIndex = input.lastIndexOf("->");
  if (separatorIndex == -1) {
    return 255;  // Error: No valid block number detected
  }

  String blockStr = input.substring(separatorIndex + 2);
  blockStr.trim();

  for (int i = 0; i < blockStr.length(); i++) {
    if (!isDigit(blockStr[i])) {
      return 255; // Error: Block number is not numeric
    }
  }

  return blockStr.toInt();
}

/*
 * Extracts ONLY the data portion from input
 */
String getDataFromInput(String input) {
  int separatorIndex = input.lastIndexOf("->");
  if (separatorIndex == -1) {
    return "";  // Error case: No separator found
  }
  
  String data = input.substring(0, separatorIndex);  // Extract data
  data.trim();  // Trim properly before returning
  return data;
}

/*
 * Check if a block is a sector trailer (3, 7, 11, 15, etc.)
 */
bool isSectorTrailer(byte block) {
  return (block + 1) % 4 == 0;
}

/*
 * Write long data across multiple blocks while skipping forbidden ones
 */
void writeTransblockData(byte startBlock, String data) {
  int dataLen = data.length();
  int numBlocks = (dataLen / 16) + ((dataLen % 16) ? 1 : 0);

  Serial.print("Writing ");
  Serial.print(dataLen);
  Serial.print(" bytes across ");
  Serial.print(numBlocks);
  Serial.println(" blocks.");

  byte currentBlock = startBlock;
  int dataIndex = 0;

  for (int i = 0; i < numBlocks; i++) {
    byte buff[16] = {0};
    String portion = "";

    for (int j = 0; j < 16; j++) {
      if (dataIndex < dataLen) {
        buff[j] = data[dataIndex];
        portion += (char)buff[j];
        dataIndex++;
      } else {
        break;
      }
    }

    while (isSectorTrailer(currentBlock)) {
      Serial.print("Skipping sector trailer block ");
      Serial.println(currentBlock);
      currentBlock++;
    }

    card_status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, currentBlock, &key, &(mfrc522.uid));
    if (card_status != MFRC522::STATUS_OK) {
      Serial.print("Error: Authentication failed for block ");
      Serial.print(currentBlock);
      Serial.println(": Skipping further writing.");
      alertError();
      return;
    }

    card_status = mfrc522.MIFARE_Write(currentBlock, buff, 16);
    if (card_status != MFRC522::STATUS_OK) {
      Serial.print("Error: Write failed for block ");
      Serial.print(currentBlock);
      Serial.println(": Skipping further writing.");
      alertError();
      return;
    } else {
      Serial.print("✅ Block ");
      Serial.print(currentBlock);
      Serial.print(" written successfully: \"");
      Serial.print(portion);
      Serial.println("\"");
    }

    currentBlock++;
  }

  Serial.println("Writing complete! Check all blocks to verify data.");
}

/*
 * Alert on success - Play a melody
 */
void alertSuccessMelody() {
  int melody[] = {1000, 1200, 1400, 1600};
  int duration = 150;

  for (int i = 0; i < 4; i++) {
    tone(BUZZER_PIN, melody[i], duration);
    delay(duration + 50);
  }
  noTone(BUZZER_PIN);
}

/*
 * Alert on error (Three Beeps)
 */
void alertError() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000, 200);
    delay(400);
  }
  noTone(BUZZER_PIN);
}