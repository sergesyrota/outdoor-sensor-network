#include <EEPROMex.h>
#include <SPI.h>
#include <SyrotaAutomation1.h>
#include <DHT.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "config.h"

//
// Hardware configuration
//

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);
SyrotaAutomation net = SyrotaAutomation(RS485_CONTROL_PIN);
DHT dht;

const uint64_t receiverAddress = RADIO_ADDRESS;
char sensorsData[NUMBER_OF_SENSORS][32];
unsigned long sensorsDataTime[NUMBER_OF_SENSORS];
char buf[100];
dht_t dhtData;

struct configuration_t conf = {
  CONFIG_VERSION,
  // Default values for config
  9600UL, //unsigned long baudRate; // Serial/RS-485 rate: 9600, 14400, 19200, 28800, 38400, 57600, or 115200
};

void setup(void)
{
  readConfig();
  // Set device ID
  strcpy(net.deviceID, NET_ADDRESS);
  Serial.begin(conf.baudRate);

#ifdef DEBUG
  printf_begin(); // enable stdout to Arduino host serial stream
  printf("\n\rReceiver\n\r");
  sendSensorData(0);
#endif

  setupRadio();
  dht.setup(DHT11_PIN, DHT::DHT11);
  updateDht();
}

void setupRadio()
{
  radio.begin();
  radio.setRetries(15, 15); // set the number of retries and interval between retries to the maximum

  radio.openReadingPipe(1, receiverAddress);
  radio.startListening();
}

void readConfig()
{
  // Check to make sure config values are real, by looking at first 3 bytes
  if (EEPROM.read(0) == CONFIG_VERSION[0] &&
    EEPROM.read(1) == CONFIG_VERSION[1] &&
    EEPROM.read(2) == CONFIG_VERSION[2]) {
    EEPROM.readBlock(0, conf);
  } else {
    // Configuration is invalid, so let's write default to memory
    saveConfig();
  }
}

void saveConfig()
{
  EEPROM.writeBlock(0, conf);
}

void loop(void)
{
  // if there is data ready
  uint8_t pipe;
  if ( radio.available(&pipe))
  {
    readRadio();
  }
  
  // Process RS-485 commands
  if (net.messageReceived()) {
    if (net.assertCommandStarts("getSensor:", buf)) {
      sendSensorData((byte)atoi(buf));
    } else if (net.assertCommandStarts("getDht", buf)) {
      sprintf(buf, "%dC; %d%%RH; %ds ago", dhtData.temperature, dhtData.humidity, (millis() - dhtData.lastSuccessTime)/1000);
      net.sendResponse(buf);
    } else if (net.assertCommandStarts("set", buf)) {
      processSetCommands();
    } else {
      net.sendResponse("Unrecognized command");
    }
  }
  
  // Check DHT sensor
  if (millis() - dhtData.lastAttemptTime > DHT_UPDATE_INTERVAL) {
    updateDht();
  }
}

void updateDht()
{
  // Throw out result to make sure we don't have an error
  dht.getTemperature();
  dhtData.lastAttemptTime = millis();
  if (dht.getStatus() == dht.ERROR_NONE) {
    dhtData.lastSuccessTime = millis();
    dhtData.temperature = dht.getTemperature();
    dhtData.humidity = dht.getHumidity();
// In debug mode, print out data we just got if all is good, or details of what's wrong if not.
#ifdef DEBUG
    Serial.print("DHT: ");
    Serial.print(dhtData.temperature,1);
    Serial.print(",\t");
    Serial.println(dhtData.humidity,1);
  } else {
    Serial.print(millis());
    Serial.print(" DHT error: ");
    dht.getStatusString();
    Serial.println();
#endif
  }
}

// Write to the configuration when we receive new parameters
void processSetCommands()
{
  if (net.assertCommandStarts("setBaudRate:", buf)) {
    long tmp = strtol(buf, NULL, 10);
    // Supported: 9600, 14400, 19200, 28800, 38400, 57600, or 115200
    if (tmp == 9600 ||
      tmp == 14400 ||
      tmp == 19200 ||
      tmp == 28800 ||
      tmp == 38400 ||
      tmp == 57600 ||
      tmp == 115200
    ) {
      conf.baudRate = tmp;
      saveConfig();
      net.sendResponse("OK");
      Serial.end();
      Serial.begin(tmp);
    } else {
      net.sendResponse("ERROR");
    }
  } else {
    net.sendResponse("Unrecognized");
  }
}

void sendSensorData(byte sensorId) {
  // Check for sensor out of bounds
  if (sensorId >= NUMBER_OF_SENSORS) {
    net.sendResponse("ERROR: no such ID");
    return;
  }
#ifdef DEBUG
  Serial.println(millis()-sensorsDataTime[sensorId]);
  Serial.println(sensorsData[sensorId]);
#endif
  char dataBuff[16];
  char singleByte[2];
  // Send age first
  sprintf(buf, "%lus ago: ", (millis()-sensorsDataTime[sensorId])/1000);
  net.responseSendPart(buf);
  
  for (byte i=0; i<32; i++) {
    sprintf(singleByte, "%02X", (byte)sensorsData[sensorId][i]);
    dataBuff[(i%8)*2] = singleByte[0];
    dataBuff[((i%8)*2)+1] = singleByte[1];
    if (i%8 == 7) {
      net.responseSendPart(dataBuff);
    }
  }
  net.responseEnd();
}

void readRadio()
{
  char message[32];
  if( radio.read(&message, sizeof(message)) && (byte)message[0] < NUMBER_OF_SENSORS)
  {
    byte sensorId = (byte)message[0];
    memcpy(sensorsData[sensorId], message, sizeof(message));
    sensorsDataTime[sensorId] = millis();
#ifdef DEBUG
    printf("RECEIVED sensor %d data\r\n", sensorId);
    for (int i = 0; i < 32; i++) {
      printf("%02x ", (byte)sensorsData[sensorId][i]);
    }
    Serial.println(";");
    sendSensorData(sensorId);
#endif
  }
  else
  {
#ifdef DEBUG
    printf("Radio read failed or improper id\n\r");
//      radio.printDetails();
#endif
  }
}
