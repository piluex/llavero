/*
 * Copyright (c) 2016 Esteban Torre <esteban.torre@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this 
 * software and associated documentation files (the "Software"), to deal in the Software 
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following 
 * conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "Keyboard.h"
#include "AESLib.h"

#define BAUD_RATE 9600
#define LED_PIN 10
#define PUSH_PIN 7
#define PUSH_FIRE_INTERVAL 200
#define SERIAL_BUFFER_LEN 77
#define LINE_ENDING '\n'

enum LED_MODE {
  LED_OFF,
  LED_ON,
  LED_BLINK
};
#define LED_BLINK_INTERVAL 500

volatile LED_MODE led_status = LED_OFF;

void process_led()
{
  switch(led_status)
  {
    case LED_OFF:
      digitalWrite(LED_PIN,LOW);
      break;
    case LED_ON:
      digitalWrite(LED_PIN, HIGH);
      break;
    case LED_BLINK:
      unsigned long t = millis()%(LED_BLINK_INTERVAL*2);
      if(t<LED_BLINK_INTERVAL)
        digitalWrite(LED_PIN, LOW);
      else
        digitalWrite(LED_PIN, HIGH);
      break;
  }
}

volatile int rand_seed = 1;
void rand_init()
{
  for(int i = 0;i<10; ++i)
  {
    int x = analogRead(0);
    if(x != 0)
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
  if(value != last_push_value)
  {
    last_push_value = value;
    unsigned long now = millis();
    if(value == HIGH)
    {
      if(last_push_high < now)
      {
        Keyboard.println("Go touch yourself.\n");
        led_status = LED_ON;
      }
    }
    else
    {
      last_push_high = now + PUSH_FIRE_INTERVAL;
      led_status = LED_OFF;
    }
  }
}

volatile bool secret_set = false;
uint8_t secret[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
char* data[16];

bool encrypt_data()
{
  if(secret_set == false)
   return false;
  aes256_enc_single(secret,data);
  return true;
}

bool decrypt_data()
{
  if(secret_set == false)
   return false;
  aes256_dec_single(secret,data);
  return true;
}

char* error_answer = "ERROR\n";

byte serial_buffer_ptr = 0;
char serial_last_byte = 0;
char serial_buffer[SERIAL_BUFFER_LEN+1];

void check_serial()
{
  while(Serial.available() > 0)
  {
    if(serial_buffer_ptr < SERIAL_BUFFER_LEN)
    {
      serial_last_byte = Serial.read();
      if(serial_last_byte == LINE_ENDING)
      {
        serial_buffer[serial_buffer_ptr] = 0;
        discard_serial_data();
        process_serial();
        serial_buffer_ptr = 0;
      }
      else
      {
        serial_buffer[serial_buffer_ptr] = serial_last_byte;
        serial_buffer_ptr++;
      }
    }else
    {
      serial_buffer_ptr = 0;
      discard_serial_data();
      Serial.write(error_answer);
    }
  }
}

void discard_serial_data()
{
  while(Serial.available())
    Serial.read();
}

void process_serial()
{
  for(byte i = 0; i<serial_buffer_ptr; ++i)
  {
    char x = serial_buffer[i];
    if(x >= 'a' && x <= 'z')
    {
      serial_buffer[i] = x - ('a'-'A');
    }
  }
  //Echo in LLAVERO awesome uppercase for now
  Serial.write(serial_buffer, serial_buffer_ptr);
  Serial.write(LINE_ENDING);
}



void setup() 
{
  rand_init();
  pinMode(LED_PIN, OUTPUT);
  pinMode(PUSH_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PUSH_PIN), push_interrupt, CHANGE);
  Serial.begin(BAUD_RATE);
  Keyboard.begin();
  led_status = LED_BLINK;
}

void loop()
{
  process_led();
  check_serial();
}


