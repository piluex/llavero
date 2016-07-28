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

byte totp_secret[20];

volatile int rand_seed = 1;

volatile unsigned long last_push_high = 0;
volatile byte last_push_value = LOW;

volatile bool secret_set = false;
uint8_t secret[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
char data[17];

byte serial_buffer_ptr = 0;
char serial_last_byte = 0;
char serial_buffer[SERIAL_BUFFER_LEN + 1];

char* error_answer = "ERROR\n";

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
        cmd_confirmation();
      }
    }
    else
    {
      last_push_high = now + PUSH_FIRE_INTERVAL;
    }
  }
}


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
  byte j;
  
  if(reverse == true)
    j = (expected_size-1);
  else
    j = 0;

  while ((reverse == false && j < expected_size) || (reverse == true && j >= 0))
  {
    byte x = n_for_hex(ascii[i]);
    byte y = n_for_hex(ascii[i + 1]);
    if (x > 15 || y > 15)
      return false;
    
    dest[j] = (x << 4) | y;
    
    if(reverse == true)
      j--;
    else
      j++;
    i = i + 2;
  }
  memset(ascii,0,i);
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
  check_cmd();
}

enum LLAVERO_cmd
{
  GREETING, // Leave as first ALWAYS
  SET_SECRET,
  SET_PASSWORD,
  GET_PASSWORD,
  LIST_TAGS,
  INIT,
  TIME,
  SET_TOTP,
  NO_cmd // Leave as last ALWAYS
};
enum cmd_STATUS
{
  READY,
  WAITING_CONFIRMATION,
  WAITING_ARGUMENTS
};

char *cmd_txt[] = {"hi", "secret", "set", "get", "ls", "init", "time", "sett"};

typedef void (* cmd_func) ();
cmd_func cmd_recv_fun[] = {greeting_cmd_recv, set_secret_cmd_recv,
                                        set_password_cmd_recv, get_password_cmd_recv, list_tags_cmd_recv,
                                        init_cmd_recv, time_cmd_recv, set_totp_cmd_recv
                                       };
cmd_func cmd_arg_fun[] = {cmd_noop, set_secret_cmd_arg,
                                       set_password_cmd_arg, get_password_cmd_arg, cmd_noop,
                                       cmd_noop, time_cmd_arg, set_totp_cmd_arg
                                      };
cmd_func cmd_confirm_fun[] = {cmd_noop, cmd_noop,
                                           cmd_noop, get_password_cmd_confirm, list_tags_cmd_confirm,
                                           init_cmd_confirm, cmd_noop, cmd_noop
                                          };

LLAVERO_cmd current_cmd = NO_cmd;
cmd_STATUS cmd_status = READY;
byte current_argument = -1;
byte cmd_arg_len[MAX_ARGUMENTS];
char cmd_arg[MAX_ARGUMENTS][SERIAL_BUFFER_LEN + 1];

bool is_cmd(char* cmd, char* buff, byte buff_len)
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

byte which_cmd(char* buff, byte buff_len)
{
  for (byte i = GREETING; i < NO_cmd; i++)
  {
    if (is_cmd(cmd_txt[i], buff, buff_len))
      return i;
  }
  return NO_cmd;
}

bool read_argument()
{
  if ((current_argument >= 0) && (current_argument <= MAX_ARGUMENTS))
  {
    memcpy(cmd_arg[current_argument], serial_buffer, SERIAL_BUFFER_LEN);
    memset(serial_buffer, 0, SERIAL_BUFFER_LEN);
    cmd_arg[current_argument][SERIAL_BUFFER_LEN] = 0;
    cmd_arg_len[current_argument] = serial_buffer_ptr;
    return true;
  } else
  {
    return false;
  }
}

void wait_confirmation()
{
  led_status = LED_BLINK;
  cmd_status = WAITING_CONFIRMATION;
}

void wait_argument(byte n)
{
  cmd_status = WAITING_ARGUMENTS;
  current_argument = n;
}

void cmd_end()
{
  current_argument = -1;
  current_cmd = NO_cmd;
  cmd_status = READY;
  led_status = LED_OFF;
}

void cmd_noop()
{

}

void greeting_cmd_recv()
{
  Serial.print(F("HI HUMAN, I'M LLAVERO, A ROBOT FROM THE PAST MADE TO BURY YOUR SECRETS DEEP IN MY SILICON YARD."));
  print_time();
  cmd_end();
}

void set_secret_cmd_recv()
{
  Serial.print(F("ENTER THE SECRET.\n"));
  wait_argument(0);
}

void set_secret_cmd_arg()
{
  if (current_argument == 0)
  {
    if (set_secret(cmd_arg[current_argument]))
    {
      cmd_ack();
      cmd_end();
    }
    else
    {
      cmd_error();
    }
  } else
  {
    cmd_error();
  }
}

void set_password_cmd_recv()
{
  Serial.print(F("ENTER TAG (7 CHAR MAX).\n"));
  wait_argument(0);
}

bool save_password_for_tag(char* pass, byte pass_len, char* tag, byte tag_len)
{
  if(pass_len > 32 || tag_len > 7)
    return false;
    
  eeprom_new();
  EEPROM_ram_record* record = eeprom_current_record();
  
  record->flags = 0;
  memcpy(record->tag, cmd_arg[0],7);
  
  memcpy(data, cmd_arg[1],16);
  if(encrypt_data() == false)
    return false;
  memcpy(eeprom_current_record()->data1,data,16);
  
  if(cmd_arg_len[1] > 16)
  {
    record->flags = record->flags | EXTENDED;
    memcpy(data, &cmd_arg[1][16],16);
    if(encrypt_data() == false)
      return false;
    memcpy(eeprom_current_record()->data2,data,16);
  } 
  
  eeprom_write();
}

void set_password_cmd_arg()
{
  if (current_argument == 0)
  {
    int l = cmd_arg_len[0];
    if(l > 0 && l < 8)
    {
      cmd_ack();
      Serial.print(F("SEND THE SECRET.\n"));
      wait_argument(1);
    }else
    {
      cmd_error();
    }
  } else if (current_argument == 1)
  {
    bool result = save_password_for_tag(cmd_arg[1], cmd_arg_len[1], cmd_arg[0], cmd_arg_len[0]);
    if(result == false)
      cmd_error();
    else
    {
      cmd_ack();
      cmd_end();
    }
  } else
  {
    cmd_error();
  }
}

void get_password_cmd_recv()
{
  Serial.print(F("ENTER TAG.\n"));
  wait_argument(0);
}

void get_password_cmd_arg()
{
  if (current_argument == 0)
  {
    cmd_ack();
    int l = strlen(cmd_arg[current_argument]);
    if(l>0 && l <8)
    {
      Serial.print(F("FOCUS ON PROMPT AND PUSH THE BUTTON.\n"));
      wait_confirmation();
    }
    else
      cmd_error();
  } else
  {
    cmd_error();
  }
}

void print_totp(EEPROM_ram_record* record)
{
  memcpy(data, record->data1,16);
  decrypt_data();
  memcpy(totp_secret, data, 16);
  memcpy(data, record->data2,16);
  decrypt_data();
  memcpy(&totp_secret[16], data, 4);
  TOTP totp = TOTP(totp_secret, 20,30);
  long GMT = rtc.getTimestamp();
  char* code = totp.getCode(GMT);
  Keyboard.print(code);
  Keyboard.print('\n');
  memset(totp_secret,0,20);
}

bool access_record_by_tag(char* tag, byte tag_len)
{
  if(tag_len > 7)
    return false;
  bool found = eeprom_load_by_tag(tag);
  if(found == false)
    return false;
    
  EEPROM_ram_record* record = eeprom_current_record();
  
  //memcpy(data, record->data1,16);
  //decrypt_data();
  
  if((eeprom_current_record()->flags&TOTP_RECORD) == TOTP_RECORD)
  {
    print_totp(record);
  }else
  {
    Keyboard.print(data);
    Keyboard.print('\n');
    memset(data,0,16);
  }
}

void get_password_cmd_confirm()
{
  bool result = eeprom_load_by_tag(cmd_arg[0]);
  if(result)
  {
    memcpy(data, eeprom_current_record()->data1,16);
    decrypt_data();
    if((eeprom_current_record()->flags&TOTP_RECORD) == TOTP_RECORD)
    {
      memcpy(totp_secret, data, 16);
      memcpy(data, eeprom_current_record()->data2,16);
      decrypt_data();
      memcpy(&totp_secret[16], data, 4);
      TOTP totp = TOTP(totp_secret, 20,30);
      long GMT = rtc.getTimestamp();
      char* code = totp.getCode(GMT);
      Keyboard.print(code);
      Keyboard.print('\n');
      memset(totp_secret,0,20);
    }else
    {
      Keyboard.print(data);
      Keyboard.print('\n');
      memset(data,0,16);
    }
  }else
  {
    Serial.print(F("NOT FOUND.\n"));
  }
  cmd_end();
}

void list_tags_cmd_recv()
{
  Serial.print(F("PUSH THE BUTTON.\n"));
  wait_confirmation();
}

void list_tags_cmd_confirm()
{
  eeprom_print_all_tags();
  cmd_end();
}

void init_cmd_recv()
{
  Serial.print(F("THIS WILL RESET THE DEVICE, PLEASE CONFIRM.\n"));
  wait_confirmation();
}

void init_cmd_confirm()
{
  eeprom_reset();
  cmd_end();
}

void time_cmd_recv()
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

void time_cmd_arg()
{
  cmd_ack();
  ascii_hex_to_bin(cmd_arg[0],(byte*)&timestamp, 4, true);  
  if(rtc.getYear() < 1970)
  {
    rtc.stopRTC();
    rtc.setClockWithTimestamp(timestamp);
    rtc.startRTC();
  }  
  print_time();
  cmd_end();
}

void set_totp_cmd_recv()
{
  Serial.print(F("ENTER TAG.\n"));
  wait_argument(0);
}

void set_totp_cmd_arg()
{
  if (current_argument == 0)
  {
    cmd_ack();
    Serial.print(F("NOW THE SECRET.\n"));
    wait_argument(1);
  }else
  {
    cmd_ack();
    eeprom_current_record()->flags = TOTP_RECORD|EXTENDED;
    ascii_hex_to_bin(cmd_arg[1],totp_secret, 20);
    memcpy(data, totp_secret,16);
    encrypt_data();
    memcpy(eeprom_current_record()->data1,data,16);
    memcpy(data, &totp_secret[16],4);
    encrypt_data();
    memcpy(eeprom_current_record()->data2,data,16);
    memcpy(eeprom_current_record()->tag, cmd_arg[0], 7);
    eeprom_write();
    cmd_end();
  }
}

void print_totp()
{
  
}

void cmd_ack()
{
  Serial.write("ACK\n");
}

void cmd_error()
{
  Serial.write("ERROR\n");
  cmd_end();
}

void check_cmd()
{
  switch (cmd_status)
  {
    case READY:
      current_cmd = (LLAVERO_cmd) which_cmd(serial_buffer, serial_buffer_ptr);
      if (current_cmd != NO_cmd)
      {
        cmd_ack();
        cmd_recv_fun[current_cmd]();
      } else
      {
        cmd_error();
      }
      break;
    case WAITING_CONFIRMATION:
      Serial.print(F("CONFIRMATION CANCELED.\n"));
      cmd_error();
      break;
    case WAITING_ARGUMENTS:
      if (read_argument())
      {
        cmd_arg_fun[current_cmd]();
      } else
      {
        cmd_error();
      }
      break;
  }
}

void default_action()
{
  Serial.print(F("TOUCH THIS!\n"));
}

void cmd_confirmation()
{
  if (current_cmd != NO_cmd &&
      cmd_status == WAITING_CONFIRMATION)
  {
    delay(CONFIRMATION_TIME);
    if (digitalRead(PUSH_PIN) == HIGH)
    {
      led_status = LED_OFF;
      cmd_confirm_fun[current_cmd]();
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


