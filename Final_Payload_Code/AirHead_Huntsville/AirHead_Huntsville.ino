#include "DFRobot_OxygenSensor.h"
#include "SparkFun_SCD4x_Arduino_Library.h" //Click here to get the library: http://librarymanager/All#SparkFun_SCD4x
#include "Adafruit_MPL3115A2.h"
#include <SensirionI2cStc3x.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <Wire.h>

// Import memory API
#include "SPIFFS.h"
#include <EEPROM.h>//take

#define COLLECT_BYTES_NUMBER  10             // collect number, the collection range is 1-100.
#define BUZZER 5

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED false

#define NO_ERROR_STC31_CO2 0

// I2C  address On PCB #2
// #define Oxygen_IICAddress 0x73  //PCB
#define Oxygen_IICAddress ADDRESS_3 // Protoboard

// #define mainCO2_IICAddress 0x62 //PCB


// #define BMP_ADDRESS_PCB 0x76

// backup CO2 sensor
// #define STC31_C_I2C_ADDR_2C 0x29 //PCB
#define STC31_C_I2C_ADDR_2C 0x2C //(Protoboard)



SensirionI2cStc3x co2STC31Sensor;
static char errorMessageCo2STC31[64];
static int16_t errorCo2STC31;

DFRobot_OxygenSensor oxygen;
SCD4x co2MainSensor;
Adafruit_BMP3XX bmp;


// Configure this for the location where rocket will launch. Check preassure in that city and change here
// Huntswille https://forecast.weather.gov/data/obhistory/KHSV.html  1018.8 
// Seattle https://forecast.weather.gov/data/obhistory/KSEA.html.  1028
#define SEALEVELPRESSURE_HPA (1018)

bool debug = true;// false= wont print serial USE WHEN LAUNCHING, true = print on serial, USE WHEN TESTING

// FIX: Save after every 5 entry so at max we only loose five entries
int CLOSE_FILE_AFTER_ENTRIES =5; // 1000 for prod version

// Beep every 2 minute to indicte that system is ON
// Beeping ery minute does not give advantage(We can save battery by beeping in 2 minutes)

int DELAY_IN_BEEP = 120;
int nextBeep = 120; // Seconds
int DELAY_EACH_ENTRY = 1000; 
int entriesMade =0; // Each time we make an entry we increase this.
unsigned long timePassed; // since start of board.

// Boolean flag to indicate if there is error reading sensors.
bool errorO2 = false;
bool errorMainCo2 = false;
bool errorPreassureBMP = false;
bool errorFileSystem = false;
bool errorBackupCO2 = false;

// Using ID to indicate each sensor. We will use this to log error if happens 
int ID_BMP = 1;
int ID_MAIN_CO2 =2;
int ID_O2 = 3;
int ID_BACKUP_CO2 =4;
//previous CO2 value to use when it gives us 0
int preco2 =0;

// The file where the data will be stored.
File dataFile;
String fileName ="/data.txt";

//String to store previous data. We will use this to compare with new data.
String previousDataToLog = "";

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Wire.begin();

  pinMode(BUZZER, OUTPUT);
  Serial.println("Setup start..");

  // Memory Check now
  // SPIFFS Allows larger memory to use to store data up to 1.3 MB
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Memory Mount Failed !!! ");
    errorFileSystem = true;
    return;
  }

  // Open the file in append or new file in writing mode.
  if (SPIFFS.exists(fileName)) {
      dataFile = SPIFFS.open(fileName, "a"); // Open file for appending
      Serial.println("SPIFFS Memory: Opened existing file for appending.");
  } else {
      dataFile = SPIFFS.open(fileName, "w"); // Open file for writing
      Serial.println("SPIFFS Memory: Opened the new file for writing.");
  }

  if (!dataFile) {
    Serial.println("Failed to open file for data writing/appending.");
    errorFileSystem = true;
    return;
  }

  // Every time we switch on of off, this line will indicate we are doing a new tdata recording.
  // Because the file exists and we are recording the new data
  dataFile.println("Starting new data recording.");
 
  // if (!bmp.begin_I2C(BMP_ADDRESS_PCB)) {   // (PCB) hardware I2C mode, can pass in address & alt Wire
  if (!bmp.begin_I2C()){
    Serial.println("Could not find a valid BMP3 sensor, check wiring!");
    dataFile.println(String(ID_BMP)+"E:"+ "Could not connect in setup." );
    errorPreassureBMP = true;
  }else{
      Serial.println("BMP3 sensor working !!");
      bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
      bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
      bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
      bmp.setOutputDataRate(BMP3_ODR_50_HZ);//use flag here for test mode or not, dont need to use 50 Hz(this senses 50 times every sec)
  }
  
  
  // Backup CO2 Sensor
  co2STC31Sensor.begin(Wire, STC31_C_I2C_ADDR_2C);

  // Output the product identifier and serial number
  // You can check if the sensor is woorking fine or not.
  uint32_t productId = 0;
  uint64_t serialNumber = 0;
  delay(14);
  errorCo2STC31 = co2STC31Sensor.getProductId(productId, serialNumber);
  if (errorCo2STC31 != NO_ERROR_STC31_CO2) {
      Serial.print("STC31 Backup CO2:  Error trying to execute getProductId(), Check connection.");
      errorToString(errorCo2STC31, errorMessageCo2STC31, sizeof errorMessageCo2STC31);
      Serial.println(errorMessageCo2STC31);
      dataFile.println(String(ID_BACKUP_CO2)+"E:"+ errorMessageCo2STC31 );
      errorBackupCO2 = true;
  }else {
    Serial.print("STC31 Backup CO2 sensor is working !!");
  }
  Serial.println("productId:"+String(productId));
  delay(10);

  errorCo2STC31 = co2STC31Sensor.setBinaryGas(17);
  if (errorCo2STC31 != NO_ERROR_STC31_CO2) {
        Serial.print("STC31 Backup CO2: Error trying to execute setBinaryGas(): ");
        errorToString(errorCo2STC31, errorMessageCo2STC31, sizeof errorMessageCo2STC31);
        Serial.println(errorMessageCo2STC31);
        dataFile.println(String(ID_BACKUP_CO2)+"E:"+ errorMessageCo2STC31 );
        return;
  }

  // Enable weak smoothing filter for measurement.
  // This will decrease the signal noise but increase response time.
  errorCo2STC31 = co2STC31Sensor.enableWeakFilter();
  if (errorCo2STC31 != NO_ERROR_STC31_CO2) {
      Serial.print("STC31 Backup CO2: Error trying to execute enableWeakFilter(): ");
      errorToString(errorCo2STC31, errorMessageCo2STC31, sizeof errorMessageCo2STC31);
      Serial.println(errorMessageCo2STC31);        
      dataFile.println(String(ID_BACKUP_CO2)+"E:"+ errorMessageCo2STC31 );
  }

  // With the set relative humidity command, the sensor uses the set humidity. 
  // in the gas model to compensate the concentration results
  // Seattle https://www.weather.gov/wrh/timeseries?site=E7826  82%
  // Huntsville https://forecast.weather.gov/MapClick.php?w3u=1&w6=rh&w13u=0&w14u=1&w15u=1&pqpfhr=24&AheadHour=107&Submit=Submit&FcstType=graphical&textField1=34.7304&textField2=-86.5861&site=all&unit=0&dd=&bw=  
  //            10AM 68% 
  
  errorCo2STC31 = co2STC31Sensor.setRelativeHumidity(68.0);
  if (errorCo2STC31 != NO_ERROR_STC31_CO2) {
      Serial.print("STC31 Backup CO2: Error trying to execute setRelativeHumidity(): ");
      errorToString(errorCo2STC31, errorMessageCo2STC31, sizeof errorMessageCo2STC31);
      Serial.println(errorMessageCo2STC31);
      dataFile.println(String(ID_BACKUP_CO2)+"E:"+ errorMessageCo2STC31 );
  }
 
  // if (co2MainSensor.begin(mainCO2_IICAddress) == false) //pcb
  if (co2MainSensor.begin() == false)//(Protoboard)
  {
    Serial.println(F("Please check wiring. Co2 Sensor not detected."));
    dataFile.println(String(ID_MAIN_CO2)+"E:"+ "Could not connect in setup." );
    errorMainCo2 = true;
  } else {
    Serial.println("Detected main Co2 sensor. ");
  }
  
  
  while(!oxygen.begin(Oxygen_IICAddress)){
    Serial.println("I2c device: error oxygen sensor Not detected !");
    errorO2 = true;
    dataFile.println(String(ID_O2)+"E:"+ "Could not connect in setup." );
    delay(1000);
  }

  Serial.println("SPIFFS memory connected , Oxygen sensor  and all I2c connect success !");
  buzzerIt();
}

void loop() {
  // All the values are set to 0
  // If we have connection error with one sensor, we will save 0 in the file. 
  // A 0 in the file indicate that we have some connection error with sensor.

  int co2 =0;
  float temp =0;
  float co2BackUpSTC31 =0; // Value from Backup CO2 sensor
  float tempBackUpSTC31 =0;// Reading it but not using it.
  float oxygenData =0;
  float pressure =0;
  float altitude =0;
  timePassed = millis()/1000;
  
  // if file system not work then we should give up,
  if (errorFileSystem) {
    Serial.println("Exit from processing as file system not working.");
    delay(DELAY_EACH_ENTRY);
    return ;
  }

  // Read from sensors only if there were no error in connection during setup.
  if (!errorMainCo2 ) {  
    /*
    sometimes the co2 sensor gives us 0, messes up the excell shee( you cannot graph properly) 
    so we are taking out all the zero entries and replacing it
     with the previous entry until new entry given
    */
    if(co2MainSensor.readMeasurement()){
      co2 = co2MainSensor.getCO2();
      preco2 = co2; 
    }else{
      co2 = preco2;
    }
  }
  // Now read from back up co2 sensor
  if (!errorBackupCO2) {
    errorCo2STC31 = co2STC31Sensor.measureGasConcentration(co2BackUpSTC31, tempBackUpSTC31);
    if (errorCo2STC31 != NO_ERROR_STC31_CO2) {
         Serial.println(" NO_ERROR_STC31_CO2 : Backup CO2 Sensor Value :");
          // If Error in reading from backup sensor, we do not want to stop processing, we just print and continue
          if (debug) {
            Serial.print("STC31 Backup CO2:  Error trying to execute measureGasConcentration(): ");
            errorToString(errorCo2STC31, errorMessageCo2STC31, sizeof errorMessageCo2STC31);
            Serial.println(errorMessageCo2STC31);
            dataFile.println(String(ID_BACKUP_CO2)+"E:"+ errorMessageCo2STC31 );
          }
    } 
  }
  // Now read oxygen data
  if (!errorO2) {
    oxygenData = oxygen.getOxygenData(COLLECT_BYTES_NUMBER);//check with datasheet 
  }
  
  // Read from BMP
  if (!errorPreassureBMP) {
    pressure = bmp.pressure / 100.0;
    altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
  }

  // Write data to file
  String newDataToLog= String(co2)+","+String(co2BackUpSTC31*1000)+","+String(oxygenData)+","+String(altitude)+","+String(pressure)+"!";
  // If the data is exactly same from previous time, we do not save it on file.
  // if (!newDataToLog.equals(previousDataToLog)) {
    dataFile.print(String(timePassed)+","+newDataToLog);
    // Save to compare next time.
    previousDataToLog =newDataToLog;
    if (debug) Serial.println(newDataToLog);

    // How many entries we have made
    entriesMade++;
  // }

  // We close the file just as extra caution, we want to make sure the data we saved in file is not lost
  // Every CLOSE_FILE_AFTER_ENTRIES entries, we save the file and reopen it for appending
  if (entriesMade % CLOSE_FILE_AFTER_ENTRIES == 0 ) {
    dataFile.close();
    dataFile = SPIFFS.open(fileName, "a"); // Open file for writing
    if (debug) Serial.println("Open and Close the file.");

    // EEPROM have lifetime limit on how many times you can write, so we do not want to exceed that.
    // This number of write  on 1 register is around 100K.
    // Write in EEPROM: We are not using this information. This tells us how many entries we have made
    // This is extra informtion, just in case we need to use it. 
    EEPROM.writeString(0, String(entriesMade));
    EEPROM.commit();
  }

  // Delay in reading  in each  loop.
  delay(DELAY_EACH_ENTRY);

  // Beep if all the sensors are working or not.
  //To indicte things are ON
  if (timePassed>= nextBeep){
    buzzerIt();
    nextBeep+=DELAY_IN_BEEP;
  }
}

void buzzerIt() {
  digitalWrite(BUZZER,HIGH);
  delay(100);
  digitalWrite(BUZZER,LOW);
  
}
