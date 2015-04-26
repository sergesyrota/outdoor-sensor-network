#include <JeeLib.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "config.h"
#include "Vcc.h"

ISR(WDT_vect) { Sleepy::watchdogEvent(); } // Setup the watchdog

//
// Hardware configuration
//

const uint64_t receiverAddress = 0xABCDABCD71LL;
Vcc vcc(3.41/3.38);
unsigned int sequenceId; // Just an increment to keep each packet different, and track if we missed any

void setup(void)
{
  pinMode(PROD_RADIO_POWER_PIN, OUTPUT);
  pinMode(SENSORS_ENABLE_PIN, OUTPUT);
  switchRadio(LOW); // disable by default
}

void loop(void)
{
  digitalWrite(SENSORS_ENABLE_PIN, HIGH);
  // Wait some time to charge capacitors on sensors
  Sleepy::loseSomeTime(100);
  sensors_t message;
  sequenceId += 1;
  message.address = RF_ADDRESS;
  // Sending the same data over and over again causes receiving side to stop.
  message.soil = getSoilData();
  message.internalLeak = getInternalLeak();
  message.batteryVoltage = getBatteryVoltage();
  message.seqId = sequenceId;
  sendMessage(message);
  digitalWrite(SENSORS_ENABLE_PIN, LOW);
  for (byte i=0; i<10; i++) {
    Sleepy::loseSomeTime(60000UL);
  }
}

int getSoilData()
{
  return analogRead(SENSOR_SOIL_PIN);
}

int getInternalLeak()
{
  return analogRead(SENSOR_INTERNAL_LEAK_PIN);
}

unsigned int getBatteryVoltage()
{ 
  return (unsigned int)(vcc.Read_Volts() * 1000);
}

void sendMessage(sensors_t msg)
{
  switchRadio(HIGH);
  RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  radio.setRetries(3, 15); // retry every 1 ms 15 times: http://nrqm.ca/nrf24l01/firmware/setup_retr-register/
  radio.stopListening();
  radio.powerUp();
  radio.openWritingPipe(receiverAddress);
  if( radio.write(&msg, sizeof(msg)) )
  {
    Serial.println("Success");
  }
  else
  {
    Serial.println("FAIL");
  }
  switchRadio(LOW);
}

void switchRadio(int mode)
{
  // Production radio
  digitalWrite(PROD_RADIO_POWER_PIN, mode);
}
