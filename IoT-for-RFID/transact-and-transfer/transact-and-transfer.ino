#include <AES.h>
#include <SPI.h>
#include <MFRC522.h> 

#include <ESP8266WiFi.h>

#define SS_PIN 15  //D8
#define RST_PIN 16 //D0
#define AMBER_LED_PIN 4 //D2
#define GREEN_LED_PIN 2 //D4 
#define BUZZER_PIN 5 //D1

MFRC522 mfrc522(SS_PIN, RST_PIN);   
MFRC522::MIFARE_Key key;
MFRC522::StatusCode card_status;
WiFiClient wifiClient;    
const char* host = "iot.benax.rw"; //domain or subdomain of your website

void setup(){
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(AMBER_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(115200); //Start Serial Monitor with the baud rate 115200
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
  WiFi.mode(WIFI_OFF); //This prevents reconnection issue
  delay(10);
  WiFi.mode(WIFI_STA); //This hides the viewing of ESP as wifi hotspot
  WiFi.begin(ssid, passwd); //Connect to your WiFi router
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(".");
  }
  Serial.println();  
}

void connectToHost(const int httpPort){
  int retry_counter=0; //To be used while retrying to get connected
  wifiClient.setTimeout(15000); // 15 Seconds
  delay(1000);
  Serial.printf("Connecting to \"%s\"\n", host);

  while((!wifiClient.connect(host, httpPort)) && (retry_counter <= 30)){
    delay(100);
    Serial.print(".");
    retry_counter++;
  }

  if(retry_counter==31){
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
    digitalWrite(AMBER_LED_PIN, 0);
    blinkLEDWhileBuzzing(GREEN_LED_PIN, BUZZER_PIN, 100, 100, 4);   
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
    exit;
  }

  byte readDataLength = 18;
  card_status = mfrc522.MIFARE_Read(blockNumber, readingBuffer, &readDataLength);
  if(card_status != MFRC522::STATUS_OK){
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(card_status));
    exit;
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
    return false;  // ✅ Proper return statement
  }

  card_status = mfrc522.MIFARE_Write(blockNumber, writingBuffer, 16);
  if(card_status != MFRC522::STATUS_OK){
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(card_status));
    return false;  // ✅ Proper return statement
  }

  delay(3000);
  return true;  // ✅ Successful write, return true
}

/*
 * OPERATE DATA
*/
void operateData(byte blockNumber, String initialBalance){
  int transportFare = 450;
  float newBalance = initialBalance.toInt()-transportFare;
  if(initialBalance.toInt()<transportFare){ 
    blinkLEDWhileBuzzing(AMBER_LED_PIN, BUZZER_PIN, 600, 400, 4);   
    Serial.print("Insufficient Balance: ");
    Serial.println(initialBalance); 
    return;
  }

  String initial_balance_str;
  char writingBuffer[16];
  initial_balance_str = (String)newBalance;
  initial_balance_str.toCharArray(writingBuffer, 16);
  int strLeng = initial_balance_str.length()-3;

  /*
   * This servers to add spaces to the typed text in order to fill up to 16 characters
  */

  for(byte i = strLeng; i < 30; i++){
    writingBuffer[i] = ' ';     
  }

  Serial.println("\n********************");
  Serial.println("Processing...");
  digitalWrite(AMBER_LED_PIN, 1);

  if(saveBalanceToCard(blockNumber, (unsigned char *)writingBuffer)==true){
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
    mData = "customer="+String(getUUID())+"&initial_balance="+String(initialBalance)+"&transport_fare="+String(transportFare);    
    connectToHost(80);
    transferData(mData, "/projects/4e8d42b606f70fa9d39741a93ed0356c/y2-2025/upload.php");   
  }

  else{
    Serial.print("Transaction Failed.\nPlease try again.\n"); 
  }
  Serial.println("********************\n");  
}

void blinkLEDWhileBuzzing(int LED_Pin, int Buzzer_Pin, int t0, int t1, int n){
  for(int i=0; i<n; i++){
    digitalWrite(LED_Pin, 1);
    digitalWrite(Buzzer_Pin, 1);
    delay(t0);
    digitalWrite(LED_Pin, 0);
    digitalWrite(Buzzer_Pin, 0);
    delay(t1);    
  }
}

String getUUID(){
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++){
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  return content;
}