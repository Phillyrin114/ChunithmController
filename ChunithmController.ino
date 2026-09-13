#include <CypressCY8CMBR3116.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_BusIO_Register.h>
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <ADCInput.h>
#include <Adafruit_VL53L0X.h>

#define I2C_ADDRESS_1 0x37
#define I2C_ADDRESS_2 0x38
#define I2C_ADDRESS_3 0x39
#define I2C_ADDRESS_4 0x3a

#define SENSOR1_WIRE Wire1
#define SENSOR2_WIRE Wire1
#define SENSOR3_WIRE Wire1
#define SENSOR4_WIRE Wire1
#define SENSOR5_WIRE Wire1
#define SENSOR6_WIRE Wire1

#define SDA1_GPIO 2u
#define SCL1_GPIO 3u

#define SDA_GPIO 0u
#define SCL_GPIO 1u
#define REQUEST_TIMEOUT 40  //After how many request is a Timeout triggered

// Setup mode for doing readsu
typedef enum {
  RUN_MODE_DEFAULT = 1,
  RUN_MODE_ASYNC,
  RUN_MODE_GPIO,
  RUN_MODE_CONT
} runmode_t;

runmode_t run_mode = RUN_MODE_CONT;
uint8_t show_command_list = 1;

//Create instances of the IC
CY8CMBR3116 touchIC_1(I2C_ADDRESS_1, REQUEST_TIMEOUT);
CY8CMBR3116 touchIC_2(I2C_ADDRESS_2, REQUEST_TIMEOUT);
CY8CMBR3116 touchIC_3(I2C_ADDRESS_3, REQUEST_TIMEOUT);
CY8CMBR3116 touchIC_4(I2C_ADDRESS_4, REQUEST_TIMEOUT);

typedef struct {
  Adafruit_VL53L0X *psensor; // pointer to object
  TwoWire *pwire;
  int id;            // id for the sensor
  int shutdown_pin;  // which pin for shutdown;
  int interrupt_pin; // which pin to use for interrupts.
  Adafruit_VL53L0X::VL53L0X_Sense_config_t
      sensor_config;     // options for how to use the sensor
  uint16_t range;        // range value used in continuous mode stuff.
  uint8_t sensor_status; // status from last ranging in continuous.
} sensorList_t;
Adafruit_VL53L0X sensor1;
Adafruit_VL53L0X sensor2;
Adafruit_VL53L0X sensor3;
Adafruit_VL53L0X sensor4;
Adafruit_VL53L0X sensor5;
Adafruit_VL53L0X sensor6;

sensorList_t sensors[] = {
    {&sensor1, &SENSOR1_WIRE, 0x30, 4, 10,
     Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED, 0, 0},
    {&sensor2, &SENSOR2_WIRE, 0x31, 5, 11,
     Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED, 0, 0},
    {&sensor3, &SENSOR3_WIRE, 0x32, 6, 12,
     Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED, 0, 0},
    {&sensor4, &SENSOR4_WIRE, 0x33, 7, 13,
     Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED, 0, 0},
    {&sensor5, &SENSOR5_WIRE, 0x34, 8, 14,
     Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED, 0, 0},
    {&sensor6, &SENSOR6_WIRE, 0x35, 9, 15,
     Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED, 0, 0}
};

const int COUNT_SENSORS = sizeof(sensors) / sizeof(sensors[0]);
const uint16_t ALL_SENSORS_PENDING = ((1 << COUNT_SENSORS) - 1);
uint16_t sensors_pending = ALL_SENSORS_PENDING;
uint32_t sensor_last_cycle_time;

uint8_t keys[38] = {};

uint8_t hidcode[] = {
 HID_KEY_A ,
 HID_KEY_B ,
 HID_KEY_C ,
 HID_KEY_D ,
 HID_KEY_E ,
 HID_KEY_F ,
 HID_KEY_G ,
 HID_KEY_H ,
 HID_KEY_I ,
 HID_KEY_J ,
 HID_KEY_K ,
 HID_KEY_L ,
 HID_KEY_M ,
 HID_KEY_N ,
 HID_KEY_O ,
 HID_KEY_P ,
 HID_KEY_Q ,
 HID_KEY_R ,
 HID_KEY_S ,
 HID_KEY_T ,
 HID_KEY_U ,
 HID_KEY_V ,
 HID_KEY_W ,
 HID_KEY_X ,
 HID_KEY_Y ,
 HID_KEY_Z ,
 HID_KEY_4 ,
 HID_KEY_5 ,
 HID_KEY_6 ,
 HID_KEY_7 ,
 HID_KEY_8 ,
 HID_KEY_9 ,
 HID_KEY_KEYPAD_1 ,
 HID_KEY_KEYPAD_2 ,
 HID_KEY_KEYPAD_3 ,
 HID_KEY_KEYPAD_4 ,
 HID_KEY_KEYPAD_5 ,
 HID_KEY_KEYPAD_6
};

uint8_t const desc_hid_report[] =
    {
        TUD_HID_REPORT_DESC_NKROKEYBOARD()
    };

//uint8_t hidcode[] = {HID_KEY_0, HID_KEY_1, HID_KEY_2, HID_KEY_3};
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, true);

hid_nkrokeyboard_report_t kb;

void setup() {
#if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
  TinyUSB_Device_Init(0);
#endif
  Wire.setSDA(SDA_GPIO);
  Wire.setSCL(SCL_GPIO);
  Serial.begin(9600);
  Wire.setClock(400000);
  Wire.begin();
  Wire1.setSDA(SDA1_GPIO);
  Wire1.setSCL(SCL1_GPIO);
  Wire1.begin();
  while (!Serial && millis() < 5000);
  Serial.println(F("VL53LOX_multi start, initialize IO pins"));
  for (int i = 0; i < COUNT_SENSORS; i++) {
    pinMode(sensors[i].shutdown_pin, OUTPUT);
    digitalWrite(sensors[i].shutdown_pin, LOW);

    if (sensors[i].interrupt_pin >= 0)
      pinMode(sensors[i].interrupt_pin, INPUT_PULLUP);
  }
  Serial.println(F("Starting..."));
  Initialize_sensors();
  stop_continuous_range();
  start_continuous_range(20);

  Serial.println("Start Programm");
  usb_hid.setBootProtocol(HID_ITF_PROTOCOL_KEYBOARD);
  usb_hid.setPollInterval(1);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("TinyUSB Keyboard");
  usb_hid.begin();
  //ir.begin(8000);
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}
void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
  // Manual call tud_task since it isn't called by Core's background
  TinyUSBDevice.task();
#endif
  if (!TinyUSBDevice.mounted()) {
    return;
  }
  //Request status of the first touch ic
  uint8_t touchStatusBuffer_1[1];
  uint8_t error_1 = touchIC_1.get_BUTTON_STAT(touchStatusBuffer_1);
  if (error_1 != 0) {
    Serial.println("Error for IC 1");
    printError(error_1);
  }

  //Request status of the second touch ic
  uint8_t touchStatusBuffer_2[1];
  uint8_t error_2 = touchIC_2.get_BUTTON_STAT(touchStatusBuffer_2);
  if (error_2 != 0) {
    Serial.println("Error for IC 2");
    printError(error_2);
  }
  uint8_t touchStatusBuffer_3[1];
  uint8_t error_3 = touchIC_3.get_BUTTON_STAT(touchStatusBuffer_3);
  if (error_3 != 0) {
    Serial.println("Error for IC 3");
    printError(error_3);
  }
  uint8_t touchStatusBuffer_4[1];
  uint8_t error_4 = touchIC_4.get_BUTTON_STAT(touchStatusBuffer_4);
  if (error_4 != 0) {
    Serial.println("Error for IC 4");
    printError(error_4);
  }
  //Combine both buffer into one
  uint8_t combinedTouchStatusBuffer[4] = { touchStatusBuffer_1[0], touchStatusBuffer_2[0], touchStatusBuffer_3[0], touchStatusBuffer_4[0] };
  //print the buffer
  printStatus(combinedTouchStatusBuffer);
  //Serial.print(combinedTouchStatusBuffer[0]);
  //Serial.print(combinedTouchStatusBuffer[1]);
  //Serial.print(combinedTouchStatusBuffer[2]);
  //Serial.println(combinedTouchStatusBuffer[3]);
  writereport();
  send();
  delay(1);
  releaseall();
  Process_continuous_range();
}
void hid_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) report_id;
  (void) bufsize;

  if (report_type != HID_REPORT_TYPE_OUTPUT) return;

  uint8_t ledIndicator = buffer[0];
}
  
void printStatus(uint8_t *statusBuffer) {
  //Serial.println("Status: ");
  //First 8 bit
  for (int i = 0; i < 8; i++) {
    uint8_t pressed = statusBuffer[0] >> i & 0b00000001;
    keys[i]=pressed;
    //Serial.print(pressed);
    //Serial.print(", ");
  }
  //Second 8 bit
  for (int i = 0; i < 8; i++) {
    uint8_t pressed = statusBuffer[1] >> i & 0b00000001;
    keys[i+8]=pressed;
    //Serial.print(pressed);
    //Serial.print(", ");
  }

  //Third 8 bit
  for (int i = 0; i < 8; i++) {
    uint8_t pressed = statusBuffer[2] >> i & 0b00000001;
    keys[i+16]=pressed;
    //Serial.print(pressed);
    //Serial.print(", ");
  }

  //forth 8 bit
  for (int i = 0; i < 8; i++) {
    uint8_t pressed = statusBuffer[3] >> i & 0b00000001;
    //Serial.print(pressed);
    keys[i+24]=pressed;
    //Serial.print(", ");
  }
  //6 Airs
  }


void printError(uint8_t errorCode) {
  Serial.println("Error in communication:");
  switch (errorCode) {
    case 1:
      Serial.println("Error 1: data too long to fit in transmit buffer");
      break;
    case 2:
      Serial.println("Error 2: received NACK on transmit of address");
      break;
    case 3:
      Serial.println("Error 3: received NACK on transmit of data");
      break;
    case 4:
      Serial.println("Error 4: undefined Error");
      break;
    case 5:
      Serial.println("Error 5: timeout Error");
      break;
    default:
      Serial.println("Error not a Wire Error");
      break;
  }
}

void send()
{
  usb_hid.sendReport(0, &kb, sizeof(kb));
}

void releaseall()
{
  kb.modifier = 0;
  kb.custom = 0;
  for (int i = 0; i < 13; i++)
  {
    kb.keys[i] = 0;
  }
}

void add(uint8_t key_value){
  kb.keys[key_value / 8] |= 1 << (key_value % 8);
}

void writereport(){
  for (int i = 0; i < 38; i++){
    if (keys[i]==1){
      add(hidcode[i]);
    }
  }
}
void Initialize_sensors() {
  bool found_any_sensors = false;
  // Set all shutdown pins low to shutdown sensors
  for (int i = 0; i < COUNT_SENSORS; i++)
    digitalWrite(sensors[i].shutdown_pin, LOW);
  delay(20);

  for (int i = 0; i < COUNT_SENSORS; i++) {
    // one by one enable sensors and set their ID
    digitalWrite(sensors[i].shutdown_pin, HIGH);
    delay(10); // give time to wake up.
    if (sensors[i].psensor->begin(sensors[i].id, false, sensors[i].pwire,
                                  sensors[i].sensor_config)) {
      found_any_sensors = true;
    } else {
      Serial.print(i, DEC);
      Serial.print(F(": failed to start\n"));
    }
  }
  if (!found_any_sensors) {
    Serial.println("No valid sensors found");
    while (1)
      ;
  }
}

void start_continuous_range(uint16_t cycle_time) {
  if (cycle_time == 0)
    cycle_time = 100;
  Serial.print(F("start Continuous range mode cycle time: "));
  Serial.println(cycle_time, DEC);
  for (uint8_t i = 0; i < COUNT_SENSORS; i++) {
    sensors[i].psensor->startRangeContinuous(cycle_time); // do 100ms cycle
  }
  sensors_pending = ALL_SENSORS_PENDING;
  sensor_last_cycle_time = millis();
}

void stop_continuous_range() {
  Serial.println(F("Stop Continuous range mode"));
  for (uint8_t i = 0; i < COUNT_SENSORS; i++) {
    sensors[i].psensor->stopRangeContinuous();
  }
  delay(100); // give time for it to complete.
}

void Process_continuous_range() {

  const uint16_t range_upper_bounds[6] = {110, 130, 150, 170, 190, 230};
  if (sensors_pending == ALL_SENSORS_PENDING) {
    for (uint8_t i = 0; i < 6; i++) {
      keys[32 + i] = 0;
    }
  }
  uint16_t mask = 1;
  for (uint8_t i = 0; i < COUNT_SENSORS; i++) {
    bool range_complete = false;
    if (sensors_pending & mask) {
      //if (sensors[i].interrupt_pin >= 0)
      //  range_complete = !digitalRead(sensors[i].interrupt_pin);
      //else
        range_complete = sensors[i].psensor->isRangeComplete();
      if (range_complete) {
        sensors[i].range = sensors[i].psensor->readRangeResult();
        sensors[i].sensor_status = sensors[i].psensor->readRangeStatus();
        if (sensors[i].sensor_status == VL53L0X_ERROR_NONE &&
            sensors[i].range > 90 && sensors[i].range <= 230) {
          for (uint8_t range_index = 0; range_index < 6; range_index++) {
            if (sensors[i].range <= range_upper_bounds[range_index]) {
              keys[32 + range_index] = 1;
              break;
            }
          }
        }
        sensors_pending ^= mask;
      }
    }
    mask <<= 1; // setup to test next one
  }
  // See if we have all of our sensors read OK
  uint32_t delta_time = millis() - sensor_last_cycle_time;
  if (!sensors_pending || (delta_time > 1000)) {
    Serial.print(delta_time, DEC);
    Serial.print(F("("));
    Serial.print(sensors_pending, HEX);
    Serial.print(F(")"));
    mask = 1;
    for (uint8_t i = 0; i < COUNT_SENSORS; i++) {
      Serial.print(F(" : "));
      if (sensors_pending & mask)
        Serial.print(F("TTT")); // show timeout in this one
      else {
        Serial.print(sensors[i].range, DEC);
        if (sensors[i].sensor_status == VL53L0X_ERROR_NONE)
          Serial.print(F("  "));
        else {
          Serial.print(F("#"));
          Serial.print(sensors[i].sensor_status, DEC);
        }
      }
    }
    // setup for next pass
    Serial.println();
    sensor_last_cycle_time = millis();
    sensors_pending = ALL_SENSORS_PENDING;
  }
}
