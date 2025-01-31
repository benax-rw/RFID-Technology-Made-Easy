#include <SPI.h>
#include <MFRC522.h> 
#include <ESP8266WiFi.h>

#define SS_PIN 15  //D8
#define RST_PIN 16 //D0

MFRC522 mfrc522(SS_PIN, RST_PIN);   
MFRC522::MIFARE_Key key;
MFRC522::StatusCode card_status;
WiFiClient wifiClient;    
const char* host = "iot.benax.rw"; //domain or subdomain of your website

void setup(){
  Serial.begin(115200); // Start Serial Monitor with baud rate 115200
  connectToWiFi("Benax-POP8A", "ben@kushi");
  SPI.begin();                                                 
  mfrc522.PCD_Init();                                                
}

void loop(){
  byte block_number = 4;
  byte buffer_for_reading[18];
  for (byte i = 0; i < 6; i++){
    key.keyByte[i] = 0xFF;
  }

  if(!mfrc522.PICC_IsNewCardPresent()){
    return;
  }

  if(!mfrc522.PICC_ReadCardSerial()){
    return;
  }

  String initial_balance = readBalanceFromCard(block_number, buffer_for_reading);
  operateData(block_number, initial_balance); 
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();  
}

void connectToWiFi(const char* ssid, const char* passwd){
  WiFi.mode(WIFI_OFF); // Prevents reconnection issues
  delay(10);
  WiFi.mode(WIFI_STA); // Hides ESP from being a WiFi hotspot
  WiFi.begin(ssid, passwd); // Connect to WiFi router
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(".");
  }
  Serial.println();  
}

void connectToHost(const int httpPort){
  int retry_counter = 0;
  wifiClient.setTimeout(15000); // 15 seconds timeout
  delay(1000);
  Serial.printf("Connecting to \"%s\"\n", host);

  while((!wifiClient.connect(host, httpPort)) && (retry_counter <= 30)){
    delay(100);
    Serial.print(".");
    retry_counter++;
  }

  if(retry_counter == 31){
    Serial.println("\nConnection failed.");
    return;
  }
  else{
    Serial.printf("Connected to \"%s\"\n", host);
  }   
}

void transferData(String data, const char* filepath){
  Serial.println("Transferring data... ");
  wifiClient.println("POST "+(String)filepath+" HTTP/1.1");
  wifiClient.println("Host: " + (String)host);
  wifiClient.println("User-Agent: ESP8266/1.0");
  wifiClient.println("Content-Type: application/x-www-form-urlencoded");
  wifiClient.println("Content-Length: " +(String)data.length());
  wifiClient.println();
  wifiClient.print(data); 
  getFeedback("Transaction uploaded successfully!");
}

/*
 * GET FEEDBACK
*/
void getFeedback(String success_msg){
  String datarx;
  while (wifiClient.connected()){
    String line = wifiClient.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  while (wifiClient.available()){
    datarx += wifiClient.readStringUntil('\n');
  }

  if(datarx.indexOf(success_msg) >= 0){
    Serial.println("Data Transferred.\n");
  }
  else{
    Serial.println("Data Transfer Failed.\n"); 
  }
  datarx = "";  
}

/*
 * READ BALANCE
*/
String readBalanceFromCard(byte blockNumber, byte readingBuffer[]){
  card_status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(mfrc522.uid));
  if(card_status != MFRC522::STATUS_OK){
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(card_status));
    return "";
  }

  byte readDataLength = 18;
  card_status = mfrc522.MIFARE_Read(blockNumber, readingBuffer, &readDataLength);
  if(card_status != MFRC522::STATUS_OK){
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(card_status));
    return "";
  }

  String value = "";
  for (uint8_t i = 0; i < 16; i++){
    value += (char)readingBuffer[i];
  }
  value.trim();
  return value;
}

/*
 * SAVE BALANCE
 */
bool saveBalanceToCard(byte blockNumber, byte writingBuffer[]){
  card_status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNumber, &key, &(mfrc522.uid));
  if(card_status != MFRC522::STATUS_OK){
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(card_status));
    return false;
  }

  card_status = mfrc522.MIFARE_Write(blockNumber, writingBuffer, 16);
  if(card_status != MFRC522::STATUS_OK){
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(card_status));
    return false;
  }

  delay(3000);
  return true;
}

/*
 * OPERATE DATA
*/
void operateData(byte blockNumber, String initialBalance){
  int transportFare = 450;
  float newBalance = initialBalance.toInt() - transportFare;

  if(initialBalance.toInt() < transportFare){ 
    Serial.print("Insufficient Balance: ");
    Serial.println(initialBalance); 
    return;
  }

  String initial_balance_str;
  char writingBuffer[16];
  initial_balance_str = (String)newBalance;
  initial_balance_str.toCharArray(writingBuffer, 16);
  int strLeng = initial_balance_str.length() - 3;

  /*
   * Add spaces to fill up to 16 characters
  */
  for(byte i = strLeng; i < 30; i++){
    writingBuffer[i] = ' ';     
  }

  Serial.println("\n********************");
  Serial.println("Processing...");

  if(saveBalanceToCard(blockNumber, (unsigned char *)writingBuffer) == true){
    Serial.print("CustomerID: ");
    Serial.println(getUUID()); 
    Serial.print("Initial Balance: ");
    Serial.println(initialBalance);     
    Serial.print("Transport Fare: ");
    Serial.println(transportFare);
    Serial.print("New Balance: ");
    Serial.println(newBalance);  
    Serial.println("Transaction Succeeded.\n"); 

    String mData="";
    mData = "customer=" + String(getUUID()) + "&initial_balance=" + String(initialBalance) + "&transport_fare=" + String(transportFare);    
    connectToHost(80);
    transferData(mData, "/projects/4e8d42b606f70fa9d39741a93ed0356c/y2-2025/upload.php");   
  }
  else{
    Serial.print("Transaction Failed.\nPlease try again.\n"); 
  }
  Serial.println("********************\n");  
}

/*
 * GET UUID
*/
String getUUID(){
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++){
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  return content;
}