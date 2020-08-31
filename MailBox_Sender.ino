/* Bernhard Weyrauch, v1.0, 30th August 2020
 *
 * Arduino program to inform about new incoming mailbox containing the content weigth using the LoRa P2P network.
 *
 * Hardware: - Heltec CubeCell HTCC-AB01 micro controller
 *           - HX711 1kg Load Cell Amplifier
 * Interrupt GPIO pin GPIO0 to wakeup from deep sleep mode using a magnetic switch.
 * Uses GPIO1 and GPIO2 to measure the weight using HX711 1kg scale sensor.
 * Data will be sent using LoRa P2P network to another CubeCell arduino receiver module to process data.
 *
 * Helpful links:
 * - https://resource.heltec.cn/download/CubeCell/HTCC-AB01/HTCC-AB01_PinoutDiagram.pdf
 * - https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf
*/

// General library imports
#include "Arduino.h"
#include "LoRaWan_APP.h"

// LoRa related configs

#define LoraWan_RGB                             0         // Set LoraWan_RGB to 1,the RGB active in LoRaWAN. RGB red means sending, RGB green means received done
#define RF_FREQUENCY                            868000000 // Hz
#define TX_OUTPUT_POWER                         20        // dBm [1dBm least, 20dBm maximum]
#define LORA_BANDWIDTH                          0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR                   7         // [SF7..SF12]
#define LORA_CODINGRATE                         1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                    8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                     0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON              false
#define LORA_IQ_INVERSION_ON                    false
static RadioEvents_t                            RadioEvents;
#define TX_DATA_BUFFER_SIZE                     20        // Maximum payload size. 20 characters enough for this program
char TX_PACKET[TX_DATA_BUFFER_SIZE];

// Deep sleep configs
#define EXT_INTERRUPT                           GPIO0      // GPIO pin to listen for interrupts
#define TIME_AFTER_DEEP_SLEEP                   15000      // time in ms after wake up, until internal logic should be processed.
#define TIME_UNTIL_DEEP_SLEEP                   5000       // time in ms before going to deep sleep mode
static TimerEvent_t                             DeepSleep; // event type
uint8_t lowpower =                              1;         // default power mode

// Scale configs
#define SCALE_DATA_LINE_PIN                     GPIO2     // GPIO data line (DT) pin of scale
#define SCALE_SCK_PIN                           GPIO1     // GPIO SCK pin of scale
const int NUMBER_OF_CHECKS =                    30;       // how many data points will be measured
const int DELAY_BETWEEN_CHECKS =                50;       // time in ms between two weight measuring points
unsigned long scaleDataArray[NUMBER_OF_CHECKS];     // array to store measured weigth of scale

// ----- Main program -----
void setup() {
  // code in this function is executed once

  // init LoRa module
  boardInitMcu( );
  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                     LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                     true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

  // Serial monitor at 115200 baud
  Serial.begin(115200);

  // register interrupt
  pinMode(EXT_INTERRUPT,INPUT);
  attachInterrupt(EXT_INTERRUPT, onWakeUp, FALLING); // by default switch is closed/HIGH

  // set GPIO pins for scale
  pinMode(SCALE_DATA_LINE_PIN, INPUT);  // data line (DT)
  pinMode(SCALE_SCK_PIN, OUTPUT); // SCK line (SCK)

  // go to deep sleep mode
  TimerInit( &DeepSleep, onSleep );

  // power down HX711
  digitalWrite(SCALE_SCK_PIN, HIGH); // see data sheet for details
  Serial.println("Go to deep sleep mode NOW");
}

void loop() {
  if(lowpower){
    // lowPowerHandler() run six times the mcu into lowpower mode;
    lowPowerHandler();
  }
  // code below is executed repeatedly
}

// ----- Help functions  -----
// function called when going to deep sleep (ultra low power mode)
void onSleep() {
  Serial.println("Go to deep sleep mode.");
  // power down HX711
  digitalWrite(SCALE_SCK_PIN, HIGH); // see data sheet for details

  lowpower=1;
}

// function called when waking up from deep sleep (ultra low power mode)
void onWakeUp() {
  delay(10);
  if(digitalRead(EXT_INTERRUPT) == 0) {
    Serial.println("Interrupt detected and wake up.");
    lowpower=0;

    // wake-up HX711
    digitalWrite(SCALE_SCK_PIN, LOW); // see data sheet for details

    doMainAction();

    // go back to deep sleep mode again
    TimerSetValue( &DeepSleep, TIME_UNTIL_DEEP_SLEEP );
    TimerStart( &DeepSleep );
  }
}

// Logic triggered eveery time the program wakes up due to interrupt
void doMainAction() {
  // Waits some time to give postman enough time to complete inserting all letters, flyers and other content to my mailbox.
  delay(TIME_AFTER_DEEP_SLEEP);

  measureWeight();  // measure weigth of mailbox content
  //int avgWeigth = getAverageWeigth(); // average weigth
  //Serial.printf("Scale average weigth: %d\r\n", avgWeigth);
  int medianWeigth = getMedianWeigth(); // median weigth (maybe better value to exclude extrem values due to distortion)
  Serial.printf("Scale median weigth:  %d\r\n", medianWeigth);

  // retrieve current battery status/charge (voltage level)
  uint16_t batteryVoltage = getBatteryVoltage();
  Serial.printf("Battery voltage: %d\r\n",  batteryVoltage);

  // create data object
  String jsonData = createDataObject(medianWeigth, batteryVoltage);
  //Serial.println(jsonData);

  // send data via LoRa 2x
  sendDataViaLora(jsonData);
  delay(2000);
  sendDataViaLora(jsonData); // resubmit data for better liability
}

void clk() {
  digitalWrite(SCALE_SCK_PIN, HIGH);
  digitalWrite(SCALE_SCK_PIN, LOW);
}

void measureWeight() {
  memset(scaleDataArray, 0, sizeof(scaleDataArray)); // clear scaleDataArray
  unsigned long tmpVal = 0, value=0;
  for (int c = 0; c < NUMBER_OF_CHECKS; c++) {
    digitalWrite(SCALE_SCK_PIN, LOW);
    while (digitalRead(SCALE_DATA_LINE_PIN) != LOW) {
      // wait until Data Line goes LOW
    }
    tmpVal = 0;
    for (int i = 0; i < 24; i++) {  // read 24-bit data from HX711
      clk(); // generate CLK pulse to get MSB-it at GPIO2-pin
      bitWrite(tmpVal, 0, digitalRead(SCALE_DATA_LINE_PIN));
      tmpVal = tmpVal << 1; // process/store bit
    }
    clk();  // 25th pulse
    value = tmpVal;
    //Serial.printf("Scale check %i --> %d\r\n",(1+c), value);
    scaleDataArray[c] = value;
    delay(DELAY_BETWEEN_CHECKS);
  }
}

int getAverageWeigth() {
  unsigned long sum = 0;
  float avg = 0.0;
  for (int c = 0; c < NUMBER_OF_CHECKS; c++)
    sum += scaleDataArray[c];
  avg = sum / NUMBER_OF_CHECKS;
  return avg;
}

// qsort requires you to create a sort comparer function
int sortComparer(const void *cmp1, const void *cmp2) {
  // Need to cast the void * to int *
  int a = *((int *)cmp1);
  int b = *((int *)cmp2);
  // The comparison
  return a > b ? -1 : (a < b ? 1 : 0);
}

int getMedianWeigth() {
  int arrLength = sizeof(scaleDataArray) / sizeof(scaleDataArray[0]);
  qsort(scaleDataArray, arrLength, sizeof(scaleDataArray[0]), sortComparer); // sort data array using quick-sort
  int medianIndex = NUMBER_OF_CHECKS/2; // median index in the middle of the array
  return scaleDataArray[medianIndex];
}

String createDataObject(int weigth, uint16_t batteryVoltage) {
  String json =   "{"                              ;
         json +=    "w:" + String(weigth) + ","    ;
         json +=    "b:" + String(batteryVoltage)  ;
         json +=  "}"                              ;
  return json;
}

void sendDataViaLora(String payload) {
  payload.toCharArray(TX_PACKET,TX_DATA_BUFFER_SIZE);
  Serial.printf("Sending LoRa packet with payload '%s' and length %d.\r\n", TX_PACKET, strlen(TX_PACKET));
  Radio.Send( (uint8_t *)TX_PACKET, strlen(TX_PACKET) ); // send the package out
  memset(TX_PACKET, 0, sizeof(TX_PACKET)); // clear TX_PACKET buffer
}
