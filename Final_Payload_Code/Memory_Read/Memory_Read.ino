#include <EEPROM.h>
#include <Preferences.h>
#include "FS.h"
#include "SPIFFS.h"
/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED false

int DELAY_EACH_ENTRY = 5; // 1 Sec = 1000 
bool debug = false;// false= wont print serial USE WHEN LAUNCHING, true = print on serial, USE WHEN TESTING
String DATA_START_INDICATION = "start";
String DATA_END_INDICATION = "end";

File dataFile;
String fileName ="/data.txt";
// Are we using this progrms to read  entries only 
bool isThisReaderOnlyProgram = false;

// Separator for each entry
char eachDataEntrySeparator = '!';


void setup() {
  Serial.begin(115200);
  Serial.println("Setup start..");
  //Give it some time  for internal system to start
  delay(500);

  // SPIFFS Allows larger memory to use to store data up to 1.3 MB
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {//this variable should always be false to work
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  delay(5000);
} // setup

void loop() {

  // Read the data and send to serial
  readFileFromSPIFF(); 
  delay(30000);// dont have to read every 20, do a delay of 10 b4 the read then u read it so u dont have to print it every single time
}

void readFileFromSPIFF() {
  int entriesRead = 0;
  //  if file exists, open in append mode else in write mode
  dataFile = SPIFFS.open(fileName, "r"); 
  Serial.println("Opened the file for reading.");
  
  // Check if we have a valid data file or not.
  if (!dataFile) {
    Serial.println("Failed to open file for data writing/appending.");
    return;
  }

  Serial.println("Reading  data file.");
  int size = dataFile.size();
  Serial.println("Data File size Bytes :" + String(size));
  // Send indicator that data is starting
  Serial.println(String(DATA_START_INDICATION)); 
  
  Serial.println("Time Passed Seconds, CO2, CO2-STC31, O2, Altitude, Preasssure");
  while (dataFile.available()) {
    char value= dataFile.read(); // read one charactor each time
    if (value == eachDataEntrySeparator) {
      Serial.println(""); // make new line if we find end of one entry, so it makes new row in spreadsheet
      entriesRead++;
    }else{
      Serial.write(value);
    }
  }
  // Send indicator that data has completed
  Serial.println(String(DATA_END_INDICATION));
  dataFile.close();
  Serial.println("Total entries read from file = " + String(entriesRead));
  
}


