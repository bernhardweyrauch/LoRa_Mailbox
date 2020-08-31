/* Bernhard Weyrauch, v1.0, 31st August 2020
 *
 * Arduino program to receive LoRa packets about new incoming mailbox sent by the LoRa mailbox sender module.
 *
 * Hardware: - Heltec CubeCell HTCC-AB01 micro controller
 *
 * Received data will be printed via Serial port/communication and can be read by other microcontrollers like Raspberry PI.
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
static  RadioEvents_t                           RadioEvents;
#define RX_TIMEOUT_VALUE                        1000
#define RX_DATA_BUFFER_SIZE                     20        // Maximum payload size. 20 characters enough for this program
char    RX_PACKET[RX_DATA_BUFFER_SIZE];
int16_t rssi,rx_size;

// ----- Main program -----
void setup() {
  // init LoRa module
  boardInitMcu( );
  rssi = 0;
  RadioEvents.RxDone = OnRxDone;
  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                     LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                     LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                     0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

  // Serial monitor at 9600 baud
  Serial.begin( 9600 );
  Serial.println( "CubeCell started, initialized and waiting for incoming LoRa requests." );
}

void loop() {
  Radio.Rx( 0 );
  delay( 500 );
  Radio.IrqProcess( );
}

// ----- Help functions  -----
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr ) {
  rssi = rssi;
  rx_size = size;
  memcpy(RX_PACKET, payload, size);
  RX_PACKET[size] = '\0';
  Radio.Sleep( );
  Serial.printf("Received LoRa packet;rssi=%d;length=%d;data=%s\r\n", rssi, rx_size, RX_PACKET);
}
