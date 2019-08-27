/*
 * This is a test program for collecting data from SPI and sending it via Lora 
 * to thethingsnetwork.org.
 * 
 * The program works in its basics. It does not send any data except for the 
 * third and fourth byte read from the RFID unit. 
 * 
 * The bytes are send in the Cayenne channel 21 and 22 without making them 
 * pretty in any form. This means; They are put in as a single byte in 
 * the sender, and comes out as a double byte on thethingsnetwork.org. 
 * Both hexadecimal values converts to the same decimal value. 
 * Not pretty, but proves that it works to let the  Heltec WiFi Lora 32 
 * collect data via SPI and send it via Lora, used the given pins. It is stable.
 * - Hence, it proves that the pins used for the SPI connection does 
 * not conflict with the pins used for sending Lora.
 * 
 * The data send, are picked from the last RFID card read.
 * 
 * The Lora sender is set to do frequenzy hopping. This means that only each 
 * third sending will hit SF7 (868.1MHz).
 */

#include <CayenneLPP.h>
#include <lmic.h>
#include <hal/hal.h>
#include <Wire.h>
#include <SPI.h>
#include <SSD1306.h>
#include <MFRC522.h>

/* Heltec WiFi Lora 32 V2 */
//const int RST_PIN = 33;           /* Reset pin */

/* Heltec WiFi Lora 32 V1 */
const int RST_PIN = 35;           /* Reset pin */

const int SS_PIN = 17;            /* Slave select pin */

MFRC522 mfrc522(SS_PIN, RST_PIN); /* Create MFRC522 instance */
int loop_count = 0;
static osjob_t sendjob;

/* LoRaWAN NwkSKey, network session key*/
static const PROGMEM u1_t NWKSKEY[16] = 
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* LoRaWAN AppSKey, application session key */
static const u1_t PROGMEM APPSKEY[16] = 
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* LoRaWAN end-device address (DevAddr) */
static const u4_t DEVADDR = 
{ 0x00000000 };

/* These folowing callbacks are only used in over-the-air activation, 
 * so they are left empty here (we cannot leave them out completely unless
 * DISABLE_JOIN is set in config.h, otherwise the linker will complain). */
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

/* Schedule data trasmission in every this many seconds 
 * (might become longer due to duty cycle limitations).
 * Fair Use policy of TTN requires update interval of at least several min. 
 * We set update interval here of 1 min for testing. */
const unsigned TX_INTERVAL = 60;

/* Pin mapping according to Cytron LoRa Shield RFM */
const lmic_pinmap lmic_pins = {
  .nss = 18,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 14,
  .dio = {26, 32, 33},
};
/*   .dio = {26, 32, 33}, for Heltec WiFi Lora 32 V1 */
/*   .dio = {26, 34, 35}, for Heltec WiFi Lora 32 V2 */

char DateAndTimeString[20]; /* 19 digits plus the null char */
uint32_t timer = millis();
uint8_t  ColumnPlus = 0;
CayenneLPP lpp(51);

/* I2C sensors can per default not use pins 21 and 22 on the Heltec when the display
 * is used with SSD1306.h. One solution to that can be to let the physical wiring of 
 * the I2C sensors use pin 4 and 15. Redefine the I2C to use these pins then. */
#define PIN_SDA 4
#define PIN_SCL 15

#include <AsyncDelay.h>
#include <SoftWire.h>
SoftWire sw(PIN_SDA, PIN_SCL);

/* Rewiring the OLED to use pin 4 and 15 instead of 21 and 22. */
#define DISPLAY_USE 1          // Use DISPLAY_USE to disable the display entirely.
#define DISPLAY_ON 1           // Turn the display off, even if it is initialized.
SSD1306 display(0x3C, PIN_SDA, PIN_SCL);

void onEvent (ev_t ev) 
{
  switch(ev) 
  {
    case EV_TXCOMPLETE:
      /* Schedule next transmission */
      os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
      LoopContent();
      break;  
    case EV_RXCOMPLETE:
      if (LMIC.dataLen)
      {
        Serial.printf("Received %d bytes\n", LMIC.dataLen);
      }
      break;
    case EV_SCAN_TIMEOUT:
        Serial.println(F("EV_SCAN_TIMEOUT"));
        break;
    case EV_BEACON_FOUND:
        Serial.println(F("EV_BEACON_FOUND"));
        break;
    case EV_BEACON_MISSED:
        Serial.println(F("EV_BEACON_MISSED"));
        break;
    case EV_BEACON_TRACKED:
        Serial.println(F("EV_BEACON_TRACKED"));
        break;
    case EV_JOINING:
        Serial.println(F("EV_JOINING"));
        break;
    case EV_JOINED:
        Serial.println(F("EV_JOINED"));
        break;
    case EV_RFU1:
        Serial.println(F("EV_RFU1"));
        break;
    case EV_JOIN_FAILED:
        Serial.println(F("EV_JOIN_FAILED"));
        break;
    case EV_REJOIN_FAILED:
        Serial.println(F("EV_REJOIN_FAILED"));
        break;
    case EV_LOST_TSYNC:
        Serial.println(F("EV_LOST_TSYNC"));
        break;
    case EV_RESET:
        Serial.println(F("EV_RESET"));
        break;
    case EV_LINK_DEAD:
        Serial.println(F("EV_LINK_DEAD"));
        break;
    case EV_LINK_ALIVE:
        Serial.println(F("EV_LINK_ALIVE"));
        break;
    default:
      Serial.printf("Unknown event\r\n");
      break;
  }
}

void do_send(osjob_t* j)
{
  /* Check if there is not a current TX/RX job running */
  if (LMIC.opmode & OP_TXRXPEND) 
  {
    Serial.printf("OP_TXRXPEND going on. => Will not send anything.\r\n");
  }
  else if (!(LMIC.opmode & OP_TXRXPEND)) 
  {
    lpp.reset();

    uint8_t wakeup_reason = 9;
    lpp.addAnalogInput(20, wakeup_reason);

    Serial.println("");
    Serial.println(mfrc522.uid.uidByte[2], HEX);
    Serial.println(mfrc522.uid.uidByte[3], HEX);
    Serial.println("");
    lpp.addAnalogInput(21, mfrc522.uid.uidByte[2]);
    lpp.addAnalogInput(22, mfrc522.uid.uidByte[3]);

    /* Prepare upstream data transmission at the next possible time. */
    LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), 0);
    //Serial.printf("Packet queued\r\n");
  }
  /* Next TX is scheduled after TX_COMPLETE event. */
}

/**
 * @author Andreas C. Dyhrberg
 */
void setup()
{
  Serial.begin(115200);
  while (!Serial);

  SPI.begin();                        // Init SPI bus
  delay(100);
  mfrc522.PCD_Init();                 // Init MFRC522
  delay(100);
  Serial.println("");
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  delay(100);

#if DISPLAY_USE
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW); // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH);
  delay(50);
  display.init();
  display.setFont(ArialMT_Plain_10);
  delay(50);
  display.drawString( 0, 0, "Starting up ...");
  display.drawString( 0,20, "- and initializing...");
  display.display();
#endif
  
  /* LMIC init */
  os_init();

  /* Reset the MAC state. Session and pending data transfers will be discarded. */
  LMIC_reset();

  /* Set static session parameters. Instead of dynamically establishing a session
   * by joining the network, precomputed session parameters are be provided. */
  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
  
  /* Select frequencies range */
  //LMIC_selectSubBand(0);
  
  /* Disable link check validation */
  LMIC_setLinkCheckMode(0);
  
  /* TTN uses SF9 for its RX2 window. */
  LMIC.dn2Dr = DR_SF9;
  
  /* Set data rate and transmit power for uplink
   * (note: txpow seems to be ignored by the library) */
  LMIC_setDrTxpow(DR_SF7,14);
  Serial.printf("LMIC setup done!\r\n");

  /* Start job */
  do_send(&sendjob);

  delay(500);
}
 
void loop()
{
  LoopContent();
  loop_count++;
  if (loop_count>15)
  {
    loop_count = 0;
    Serial.println("");
  }
  delay(1000); /* Delay between loops. */
}

#if DISPLAY_USE
#if DISPLAY_ON
/**
 * @author Andreas C. Dyhrberg
 */
int DisplayPrintUid(int x, int y, MFRC522 mfrc522){
  int indent = 0;
  int indent_uid = 48;
  display.drawString( x,y, "Card UID:              ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if(mfrc522.uid.uidByte[i] < 0x10)
    {
      display.drawString((x+indent_uid+indent),y, "0");
      indent = indent+5;
      display.drawString((x+indent_uid+indent),y, String(mfrc522.uid.uidByte[i],HEX));
      indent = indent+7;
    } else
    {
      display.drawString((x+indent_uid+indent),y, String(mfrc522.uid.uidByte[i],HEX));
      indent = indent+14;
    }
  }
}
#endif
#endif

/**
 * @author Andreas C. Dyhrberg
 */
void LoopContent()
{
  os_runloop_once();
  
  /* Look for new cards */
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    //Serial.print("iNCP-");
    Serial.print(".");
    return; /* Skip the rest of the code in this function called LoopContent() */
  }
  /* Select one of the cards */
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    Serial.print("RCS-");
    return; /* Skip the rest of the code in this function called LoopContent() */
  }

  /* Dump debug info about the card; PICC_HaltA() is automatically called */
  Serial.println("");
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  delay(100);

  // UID only
  Serial.print(F("Card UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if(mfrc522.uid.uidByte[i] < 0x10)
      Serial.print(F(" 0"));
    else
      Serial.print(F(" "));
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  } 
  Serial.println();

#if DISPLAY_USE
  digitalWrite(16, LOW); // set GPIO16 low to reset OLED
#if DISPLAY_ON
  delay(50);
  digitalWrite(16, HIGH);
  display.init();
  display.setFont(ArialMT_Plain_10);
  int prefill = DisplayPrefill(loop_count);
  display.drawString( 0,0, "Count:            : itoffice.eu");
  display.drawString((47+prefill),0, String(loop_count));

  DisplayPrintUid(0,10, mfrc522);

  display.display();
#endif
#endif

  //do_send(&sendjob);

  /* Make sure LMIC is ran too - only done if return is not called before now */
  //os_runloop_once();
}

/**
 * @author Andreas C. Dyhrberg
 */
int DisplayPrefill(int value) {
  int prefill = 0;
  if(value < 100){
     prefill = 6;
  }
  if(value < 10){
      prefill = prefill+6;
  }
  return prefill;
}
