
#define RF_ADDRESS 1;

#define RADIO_CE_PIN 10
#define RADIO_CSN_PIN 9
#define PROD_RADIO_POWER_PIN 8

#define SENSOR_SOIL_PIN A4
#define SENSOR_INTERNAL_LEAK_PIN A5
#define SENSOR_BATTERY_PIN A1
#define SENSORS_ENABLE_PIN 2

struct sensors_t {
  byte address;
  int soil;
  int internalLeak;
  unsigned int batteryVoltage;
  unsigned int seqId;
};
