#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN  9  
#define SS_PIN  10  
#define BUZZER_PIN  3 // Buzzer for success/error notification
#define LED_PIN  2  // LED Indicator

MFRC522 mfrc522(SS_PIN, RST_PIN);  
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  for (byte i = 0; i < 6; i++) {  
    key.keyByte[i] = 0xFF;
  }

  Serial.println("🛑 RFID Card Full Wipe Mode (Zeros)");
  Serial.println("🔄 Bring the card near the reader...");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println("\n🆔 Card Detected. Wiping started...");
  digitalWrite(LED_PIN, HIGH);

  bool success = wipeCard();

  digitalWrite(LED_PIN, LOW);
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  if (success) {
    Serial.println("✅ Wipe successful! (All writable blocks filled with 0x00)");
    alertSuccess();
  } else {
    Serial.println("❌ Wipe failed! Some blocks couldn't be erased.");
    alertError();
  }

  delay(1000);
}

/*
 * Wipes all writable blocks by filling them with zeros (0x00)
 */
bool wipeCard() {
  byte block;
  byte emptyBlock[16] = {0};  // 16 bytes of zeros

  bool allSuccess = true;

  for (block = 1; block < 64; block++) {
    if (isSectorTrailer(block)) {
      Serial.print("⛔ Skipping sector trailer block ");
      Serial.println(block);
      continue;
    }

    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print("❌ Authentication failed for block ");
      Serial.println(block);
      allSuccess = false;
      continue;
    }

    status = mfrc522.MIFARE_Write(block, emptyBlock, 16);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("❌ Failed to write block ");
      Serial.println(block);
      allSuccess = false;
    } else {
      Serial.print("✅ Block ");
      Serial.print(block);
      Serial.println(" wiped (filled with zeros).");
    }
  }

  return allSuccess;
}

/*
 * Checks if the block is a sector trailer (3, 7, 11, 15, etc.)
 */
bool isSectorTrailer(byte block) {
  return (block + 1) % 4 == 0;
}

/*
 * Success Melody (Ascending Beeps)
 */
void alertSuccess() {
  int melody[] = {1000, 1200, 1400, 1600};
  int duration = 150;

  for (int i = 0; i < 4; i++) {
    tone(BUZZER_PIN, melody[i], duration);
    delay(duration + 50);
  }
  noTone(BUZZER_PIN);
}

/*
 * Error Beep (3 Beeps)
 */
void alertError() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000, 200);
    delay(400);
  }
  noTone(BUZZER_PIN);
}