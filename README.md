#LLAVERO (Alpha, which means: don't use it)
![LLAVERO v0](https://github.com/piluex/llavero/blob/master/prototype_pictures/connected_crop.jpg?raw=true)

LLAVERO is a simple USB keychain designed using open hardware and open source software. The goal is to store your secrets in a offline and simple piece of hardware to achieve real control over them. 

LLAVERO uses Spark Fun Arduino Pro Micro as main board due it's form factor and USB emulation capabilities. The Pro Micro is based on the [ATmega32U4](http://www.atmel.com/devices/atmega32u4.aspx). Using ATmega32U4 USB capabilities LLAVERO is able to communicate with the target device using a serial link and directly type your secrets using a emulated HID keyboard avoiding eavesdroppers to see your secrets while typing them. LLAVERO is completely agnostic about the usage of the secrets and the target system.

To avoid unintended usage or retrieval of secrets LLAVERO encrypts all secrets using AES256 and will only decrypt them if a built-in push button is pressed, when the button is pressed the selected secret is decrypted and typed with the emulated keyboard. The AES256 key is stored in RAM and lost when LLAVERO is disconnected from the host device to protect your secrets if LLAVERO is stolen. 

All secrets are stored in the ATmega32U4 EEPROM memory which is rated to last up to 100k read/write cycles and up to 20 years at 80C. LLAVERO does a CRC to the EEPROM memory and warns you if your EEPROM is failing. In addition all write operations are verified. 

Right now LLAVERO is a concept project with support for up to 16 characters secrets. TOTP support is on it's way but it will require specific software on the target device to sync the date. For simple secrets LLAVERO can be used by wiring a terminal to the USBtty. 

##Hardening notes

The ATmega32U4 has a protection bit which should be set to avoid possible reprogramming/EEPROM retrieval attacks from the host device or if your LLAVERO is stolen. For further reading check page 346 on the [datasheet](http://cdn.sparkfun.com/datasheets/Dev/Arduino/Boards/ATMega32U4.pdf).

##Building your own

I promise to put the schematics and a guide as soon as I learn to use the software to create them :). LLAVERO is just a Arduino Pro Micro with one led and one push button. If you have money please buy the original from SparkFun, if it's too expensive for you just grab a clone from your local dealer or directly from aliexpress (they run for about 3 usd).