#LLAVERO (Alpha)

LLAVERO is a simple USB keychain designed using open hardware and open source software. The goal is to store your secrets in a offline and simple piece of hardware to really have control over it. 

LLAVERO uses Spark Fun Arduino Pro Micro as main board due it's form factor and USB emulation capabilities. The Pro Micro is based on the [ATmega32U4](http://www.atmel.com/devices/atmega32u4.aspx). Using ATmega32U4 USB capabilities LLAVERO is able to communicate with the target device using a serial link and directly type your secrets using a emulated HID keyboard avoiding eavesdroppers to see your secrets while typing them. LLAVERO is completely agnostic about the usage of the secrets and the target system.

To avoid unintended usage or retrieval of secrets LLAVERO encrypts all secrets using AES256 and will only decrypt them if a built-in push button is pressed. The AES256 key is stored in RAM and lost when LLAVERO is disconnected from the host device to protect your secrets if LLAVERO is stolen. 

Right now LLAVERO is a concept project with support for up to 16 characters secrets. TOTP support is on it's way but it will require specific software on the target device to sync the date. For simple secrets LLAVERO can be used by wiring a terminal to the USBtty. 

