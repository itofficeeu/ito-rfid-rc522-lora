# ito-rfid-rc522-lora
A demonstration of how the Heltec WiFi Lora 32 can be connected with SPI to a sensor and sending it via Lora to thethingsnetwork.org. 

By coincidens do I use the sensor RFID RC522.

The program works in its basics. It does not send any important data except for the third and fourth byte read from the RFID unit. 

The bytes are send in the Cayenne channel 21 and 22 without making them pretty in any form. This means; They are put in as a single byte in the sender, and comes out as a double byte on thethingsnetwork.org. Both hexadecimal values converts to the same decimal value. Not pretty, but proves that it works to let the  Heltec WiFi Lora 32 collect data via SPI and send it via Lora, used the given pins. It is stable.
- Hence, it proves that the pins used for the SPI connection does not conflict with the pins used for sending Lora.

The data send, are picked from the last RFID card read.

The Lora sender is set to do frequenzy hopping. This means that only each third sending will hit SF7 (868.1MHz).

## Code Format
Arduino sketch .ino file type in mixed C/C++.

## Intended hardware
Heltec WiFi Lora 32, TTGO etc.
