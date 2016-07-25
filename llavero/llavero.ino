/*
   Copyright (c) 2016 Esteban Torre <esteban.torre@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy of this
   software and associated documentation files (the "Software"), to deal in the Software
   without restriction, including without limitation the rights to use, copy, modify,
   merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to the following
   conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Coding guidelines: 
   1 - Try to keep it easy for regular humans to read.
   2 - Save EEPROM.
   3 - Save RAM.
   4 - Save Flash. < Quick note will optimize protocol strings when needed.
   5 - Keep it simple.
*/

#include "Keyboard.h"
#include "AESLib.h"
#include "swRTC/swRTC.h"
#include "TOTP.h"
#include "lleeprom.h"

#define BAUD_RATE 9600
#define LED_PIN 10
#define PUSH_PIN 7
#define PUSH_FIRE_INTERVAL 200
#define SERIAL_BUFFER_LEN 77
#define LINE_ENDING '\n'
#define MAX_ARGUMENTS 2
#define CONFIRMATION_TIME 100

enum LED_MODE {
  LED_OFF,
  LED_ON,
  LED_BLINK
};
#define LED_BLINK_INTERVAL 500

volatile LED_MODE led_status = LED_OFF;
swRTC rtc;

void process_led()
{
  switch (led_status)
  {
    case LED_OFF:
      digitalWrite(LED_PIN, LOW);
      break;
    case LED_ON:
      digitalWrite(LED_PIN, HIGH);
      break;
    case LED_BLINK:
      unsigned long t = millis() % (LED_BLINK_INTERVAL * 2);
      if (t < LED_BLINK_INTERVAL)
        digitalWrite(LED_PIN, LOW);
      else
        digitalWrite(LED_PIN, HIGH);
      break;
  }
}

volatile int rand_seed = 1;
void rand_init()
{
  for (int i = 0; i < 10; ++i)
  {
    int x = analogRead(0);
    if (x != 0)
      rand_seed *= x;
    delay(5);
  }
  randomSeed(rand_seed);
}

volatile unsigned long last_push_high = 0;
volatile byte last_push_value = LOW;
void push_interrupt()
{
  byte value = digitalRead(PUSH_PIN);
  if (value != last_push_value)
  {
    last_push_value = value;
    unsigned long now = millis();
    if (value == HIGH)
    {
      if (last_push_high < now)
      {
        command_confirmation();
      }
    }
    else
    {
      last_push_high = now + PUSH_FIRE_INTERVAL;
    }
  }
}

volatile bool secret_set = false;
uint8_t secret[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
char data[17];

byte n_for_hex(char x)
{
  if (x >= '0' && x <= '9')
    return x - '0';
  else if (x >= 'a' && x <= 'f')
    return x - 'a' + 10;
  else if (x >= 'A' && x <= 'F')
    return x - 'A' + 10;
  else
    return 16;
}

void wipe_secret()
{
  for (int i = 0; i < 32; i++)
  {
    secret[i] = 0;
  }
  secret_set = false;
}

bool ascii_hex_to_bin(char* ascii, byte* dest, byte expected_size, bool reverse = false)
{
  if (ascii[0] != '0' ||
      ascii[1] != 'x')
  {
    return false;
  }
  byte i = 2;
  byte j = 0;
  if(reverse == true)
    j = (expected_size-1);
  while ((reverse == false && j < expected_size) || (reverse == true && j >= 0))
  {
    byte x = n_for_hex(ascii[i]);
    byte y = n_for_hex(ascii[i + 1]);
    ascii[i] = 0; //Wipe key from buffer
    ascii[i + 1] = 0;
    i = i + 2;
    if (x > 15 || y > 15)
      return false;
    dest[j] = (x << 4) | y;
    if(reverse == true)
      j--;
    else
      j++;
    
  }
  if (ascii[i] != 0) //Extra data, not good.
    return false;
  else
    return true;
}

bool set_secret(char* buff)
{
  secret_set = ascii_hex_to_bin(buff, secret, 32);
  return secret_set;
}

bool encrypt_data()
{
  if (secret_set == false)
    return false;
  aes256_enc_single(secret, data);
  return true;
}

bool decrypt_data()
{
  if (secret_set == false)
    return false;
  aes256_dec_single(secret, data);
  data[16] = 0;
  return true;
}

char* error_answer = "ERROR\n";

byte serial_buffer_ptr = 0;
char serial_last_byte = 0;
char serial_buffer[SERIAL_BUFFER_LEN + 1];

void check_serial()
{
  while (Serial.available() > 0)
  {
    if (serial_buffer_ptr < SERIAL_BUFFER_LEN)
    {
      serial_last_byte = Serial.read();
      if (serial_last_byte == LINE_ENDING)
      {
        serial_buffer[serial_buffer_ptr] = 0;
        discard_serial();
        process_serial();
        serial_buffer_ptr = 0;
      }
      else
      {
        serial_buffer[serial_buffer_ptr] = serial_last_byte;
        serial_buffer_ptr++;
      }
    } else
    {
      serial_buffer_ptr = 0;
      discard_serial();
      Serial.write(error_answer);
    }
  }
}

void discard_serial()
{
  while (Serial.available())
    Serial.read();
}

void process_serial()
{
  check_command();
}

enum LLAVERO_COMMAND
{
  GREETING, // Leave as first ALWAYS
  SET_SECRET,
  SET_PASSWORD,
  GET_PASSWORD,
  LIST_TAGS,
  INIT,
  TIME,
  SET_TOTP,
  NO_COMMAND // Leave as last ALWAYS
};
enum COMMAND_STATUS
{
  READY,
  WAITING_CONFIRMATION,
  WAITING_ARGUMENTS
};

char *command_txt[] = {"hi", "secret", "set", "get", "ls", "init", "time", "sett"};

typedef void (* command_func) ();
command_func command_recv_function[] = {greeting_command_recv, set_secret_command_recv,
                                        set_password_command_recv, get_password_command_recv, list_tags_command_recv,
                                        init_command_recv, time_command_recv, set_totp_command_recv
                                       };
command_func command_arg_function[] = {command_noop, set_secret_command_arg,
                                       set_password_command_arg, get_password_command_arg, command_noop,
                                       command_noop, time_command_arg, set_totp_command_arg
                                      };
command_func command_confirm_function[] = {command_noop, command_noop,
                                           command_noop, get_password_command_confirm, list_tags_command_confirm,
                                           init_command_confirm, command_noop, command_noop
                                          };

LLAVERO_COMMAND current_command = NO_COMMAND;
COMMAND_STATUS command_status = READY;
byte current_argument = -1;
byte command_argument_len[MAX_ARGUMENTS];
char command_argument[MAX_ARGUMENTS][SERIAL_BUFFER_LEN + 1];

bool is_command(char* cmd, char* buff, byte buff_len)
{
  if (buff_len <= 0)
    return false;
  byte i = 0;
  while (i <= buff_len)
  {
    byte x = cmd[i];
    byte y = buff[i];
    if (x != y || (x == 0 && i != buff_len))
    {
      return false;
    }
    i++;
  }
  i = i - 1;
  return (i == buff_len && cmd[i] == 0);
}

byte which_command(char* buff, byte buff_len)
{
  for (byte i = GREETING; i < NO_COMMAND; i++)
  {
    if (is_command(command_txt[i], buff, buff_len))
      return i;
  }
  return NO_COMMAND;
}

bool read_argument()
{
  if ((current_argument >= 0) && (current_argument <= MAX_ARGUMENTS))
  {
    memcpy(command_argument[current_argument], serial_buffer, SERIAL_BUFFER_LEN);
    memset(serial_buffer, 0, SERIAL_BUFFER_LEN);
    command_argument[current_argument][SERIAL_BUFFER_LEN] = 0;
    command_argument_len[current_argument] = serial_buffer_ptr;
    return true;
  } else
  {
    return false;
  }
}

void wait_confirmation()
{
  led_status = LED_BLINK;
  command_status = WAITING_CONFIRMATION;
}

void wait_argument(byte n)
{
  command_status = WAITING_ARGUMENTS;
  current_argument = n;
}

void command_end()
{
  current_argument = -1;
  current_command = NO_COMMAND;
  command_status = READY;
  led_status = LED_OFF;
}

void command_noop()
{

}

void greeting_command_recv()
{
  Serial.print(F("HI HUMAN, I'M LLAVERO, A ROBOT FROM THE PAST MADE TO BURY YOUR SECRETS DEEP IN MY SILICON YARD."));
  print_time();
  command_end();
}

void set_secret_command_recv()
{
  Serial.print(F("ENTER THE SECRET.\n"));
  wait_argument(0);
}

void set_secret_command_arg()
{
  if (current_argument == 0)
  {
    if (set_secret(command_argument[current_argument]))
    {
      command_ack();
      command_end();
    }
    else
    {
      command_error();
    }
  } else
  {
    command_error();
  }
}

void set_password_command_recv()
{
  Serial.print(F("ENTER TAG (7 CHAR MAX).\n"));
  eeprom_new();
  wait_argument(0);
}

void set_password_command_arg()
{
  if (current_argument == 0)
  {
    int l = command_argument_len[0];
    if(l > 0 && l < 8)
    {
      command_ack();
      Serial.print(F("SEND THE SECRET.\n"));
      wait_argument(1);
    }else
    {
      command_error();
    }
  } else if (current_argument == 1)
  {
    if(command_argument_len[1] > 32)
    {
      command_error();
    }
    else
    {
      command_ack();
      eeprom_current_record()->flags = 0;
      memcpy(data, command_argument[1],16);
      encrypt_data();
      memcpy(eeprom_current_record()->data1,data,16);
      
      if(command_argument_len[1] > 16)
      {
        eeprom_current_record()->flags = EXTENDED;
        memcpy(data, &command_argument[15],16);
        encrypt_data();
        memcpy(eeprom_current_record()->data2,data,16);
      } 
      
      memcpy(eeprom_current_record()->tag, command_argument[0],7);
      eeprom_write();
      command_end();
    }
  } else
  {
    command_error();
  }
}

void get_password_command_recv()
{
  Serial.print(F("ENTER TAG.\n"));
  wait_argument(0);
}

void get_password_command_arg()
{
  if (current_argument == 0)
  {
    command_ack();
    int l = strlen(command_argument[current_argument]);
    if(l>0 && l <8)
    {
      Serial.print(F("FOCUS ON PROMPT AND PUSH THE BUTTON.\n"));
      wait_confirmation();
    }
    else
      command_error();
  } else
  {
    command_error();
  }
}

void get_password_command_confirm()
{
  bool result = eeprom_load_by_tag(command_argument[0]);
  if(result)
  {
    memcpy(data, eeprom_current_record()->data1,16);
    decrypt_data();
    Keyboard.print(data);
    Keyboard.print('\n');
    memset(data,0,16);
  }else
  {
    Serial.print(F("NOT FOUND.\n"));
  }
  command_end();
}

void list_tags_command_recv()
{
  Serial.print(F("PUSH THE BUTTON.\n"));
  wait_confirmation();
}

void list_tags_command_confirm()
{
  eeprom_print_all_tags();
  command_end();
}

void init_command_recv()
{
  Serial.print(F("THIS WILL RESET THE DEVICE, PLEASE CONFIRM.\n"));
  wait_confirmation();
}

void init_command_confirm()
{
  eeprom_reset();
  command_end();
}

void time_command_recv()
{
  Serial.print(F("I'D LOVE TO KNOW... WHAT TIME IS IT?\n"));
  wait_argument(0);
}

unsigned long timestamp;
void print_time()
{
  //rtc
  Serial.print(rtc.getYear());
  Serial.print("-");
  Serial.print(rtc.getMonth());
  Serial.print("-");
  Serial.print(rtc.getDay());
  Serial.print(" ");
  Serial.print(rtc.getHours());
  Serial.print(":");
  Serial.print(rtc.getMinutes());
  Serial.print(":");
  Serial.print(rtc.getSeconds());
  Serial.print("\n");
}

void time_command_arg()
{
  command_ack();
  ascii_hex_to_bin(command_argument[0],(byte*)&timestamp, 4, true);  
  if(rtc.getYear() < 1970)
  {
    rtc.stopRTC();
    rtc.setClockWithTimestamp(timestamp);
    rtc.startRTC();
  }  
  print_time();
  command_end();
}

void set_totp_command_recv()
{
  Serial.print(F("ENTER TAG.\n"));
  wait_argument(0);
}
byte totp_secret[20];
void set_totp_command_arg()
{
  if (current_argument == 0)
  {
    command_ack();
    Serial.print(F("NOW THE SECRET.\n"));
    wait_argument(1);
  }else
  {
    command_ack();
    eeprom_current_record()->flags = TOTP_RECORD|EXTENDED;
    ascii_hex_to_bin(command_argument[1],totp_secret, 20);
    memcpy(data, totp_secret,16);
    encrypt_data();
    memcpy(eeprom_current_record()->data1,data,16);
    memcpy(data, &totp_secret[15],4);
    encrypt_data();
    memcpy(eeprom_current_record()->data2,data,16);
    
    memcpy(eeprom_current_record()->tag, command_argument[0], 7);
    eeprom_write();
    command_end();
  }
}

void print_totp()
{
  
}

void command_ack()
{
  Serial.write("ACK\n");
}

void command_error()
{
  Serial.write("ERROR\n");
  command_end();
}

void check_command()
{
  switch (command_status)
  {
    case READY:
      current_command = (LLAVERO_COMMAND) which_command(serial_buffer, serial_buffer_ptr);
      if (current_command != NO_COMMAND)
      {
        command_ack();
        command_recv_function[current_command]();
      } else
      {
        command_error();
      }
      break;
    case WAITING_CONFIRMATION:
      Serial.print(F("CONFIRMATION CANCELED.\n"));
      command_error();
      break;
    case WAITING_ARGUMENTS:
      if (read_argument())
      {
        command_arg_function[current_command]();
      } else
      {
        command_error();
      }
      break;
  }
}

void default_action()
{
  Serial.print(F("TOUCH THIS!\n"));
}

void command_confirmation()
{
  if (current_command != NO_COMMAND &&
      command_status == WAITING_CONFIRMATION)
  {
    delay(CONFIRMATION_TIME);
    if (digitalRead(PUSH_PIN) == HIGH)
    {
      led_status = LED_OFF;
      command_confirm_function[current_command]();
    } else
    {
      Serial.print(F("ERROR: PUSH ME CORRECTLY!.\n"));
    }

  }
  else
    default_action();
}

void setup()
{
  rand_init();
  eeprom_init();
  pinMode(LED_PIN, OUTPUT);
  pinMode(PUSH_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PUSH_PIN), push_interrupt, CHANGE);
  Serial.begin(BAUD_RATE);
  Keyboard.begin();
}

void loop()
{
  process_led();
  check_serial();
}


