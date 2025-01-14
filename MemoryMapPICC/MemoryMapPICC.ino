#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN    9       
#define SS_PIN    10   

MFRC522 mfrc522(SS_PIN, RST_PIN);  

void setup() {
    Serial.begin(9600);     
    while (!Serial);   // wait as the Serial Interface is opening     
    SPI.begin();
    mfrc522.PCD_SetRegisterBitMask(mfrc522.RFCfgReg, (0x07<<4)); //Set to the highest sensitivity (48 dB)         
    mfrc522.PCD_Init();     
    delay(4);               
    mfrc522.PCD_DumpVersionToSerial();  //Print PCD Firmware version.
    Serial.println(F("DISPLAYING UID, SAK, TYPE, AND DATA BLOCKS:"));
}

void loop(){
  //Detect the card, otherwise exit the function if no card is detected.
    if(!mfrc522.PICC_IsNewCardPresent()){
        return;
    }
  //Read the Card, otherwise exit the function if the read operation fails
    if(!mfrc522.PICC_ReadCardSerial()){
        return;
    }
  /*
  dump detailed information about the detected card to the serial monitor. 
  UID, SAK, type, and the contents of its memory blocks.
  */
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
}