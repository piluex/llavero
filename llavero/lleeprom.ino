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
   4 - Save Flash.
   5 - Keep it simple.
*/
#include <EEPROM.h>
#include "lleeprom.h"

struct EEPROM_main
{
  unsigned long crc;
  byte count;
  unsigned int next;
};

EEPROM_main eeprom_main;
int current_address = sizeof(EEPROM_main);
EEPROM_record current_record;

//Written by Christopher Andrews.
//CRC algorithm generated by pycrc, MIT licence ( https://github.com/tpircher/pycrc )
unsigned long eeprom_crc(int start)
{
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };
  unsigned long crc = ~0L;

  for (int index = start ; index < EEPROM.length()  ; ++index) {
    crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}

void eeprom_init()
{
  EEPROM.get(0, eeprom_main);
  if(eeprom_main.crc == 0 && eeprom_main.count == 0)
  {
    //First use of eeprom
    eeprom_reset();
  }else
  {
    unsigned long crc = eeprom_crc(sizeof(eeprom_main.crc));
    if(eeprom_main.crc != crc)
    {
      bool ack = false;
      while(ack == false)
      {
        Serial.print(F("FATAL ERROR: EEPROM CRC MISSMATCH!\n"));
        delay(500);
        if(Serial.available())
        {
          ack = true;
          discard_serial();
          Serial.print(F("IF THIS IS LLAVERO IS NEW RUN THE INIT COMMAND, IF NOT BACKUP AND VERIFY EEPROM.\n"));
          eeprom_main.crc = crc;
          EEPROM.put(0, eeprom_main);
        }
      }
    }
  }
}

void eeprom_reset()
{
  eeprom_main.count = 0;
  eeprom_main.next = sizeof(eeprom_main);
  EEPROM.put(0,eeprom_main);
  eeprom_main.crc = eeprom_crc(sizeof(eeprom_main.crc));
  EEPROM.put(0,eeprom_main);
}

EEPROM_record *eeprom_current_record()
{
  return &current_record;
}

void eeprom_new()
{
  current_address = -1;
  current_record.flags = 0;
}
char eeprom_tag[7];
bool eeprom_load_by_tag(char* tag) //TODO: Fix this for new struct
{
  int c = 0;
  bool loaded = false;
  int ptr = sizeof(eeprom_main);
  current_record.data1[0] = 0;
  while(c<eeprom_main.count)
  {
    current_record.flags = EEPROM.read(ptr);
    EEPROM.get(ptr+1,eeprom_tag);
    memcpy(current_record.tag,eeprom_tag,7);
    current_record.tag[7] = 0;
    if(strcmp(tag,current_record.tag)==0)
    {
      EEPROM.get(ptr+8, current_record.data1);
      if(current_record.flags & EXTENDED)
      {
        EEPROM.get(ptr+8+16, current_record.data2);
      }
      current_address = ptr;
      return true;
    }
    c++;
    if(current_record.flags & EXTENDED)
    {
      ptr = ptr + sizeof(current_record);
    }else{
      ptr = ptr + sizeof(current_record)-16;
    }
  }
  return false;
}

void eeprom_write()
{
  if(current_address < 0)
  {
    current_address = eeprom_main.next;
    eeprom_main.next = eeprom_main.next + sizeof(current_record);
    if((current_record.flags & EXTENDED) == 0)
      eeprom_main.next = eeprom_main.next - 16;
    eeprom_main.count = eeprom_main.count + 1;
  }
  EEPROM.put(current_address, current_record);
  eeprom_main.crc = eeprom_crc(sizeof(eeprom_main.crc));
  EEPROM.put(0, eeprom_main);
}

void eeprom_print_all_tags()
{
  int c = 0;
  int ptr = sizeof(eeprom_main);
  while(c < eeprom_main.count)
  {
    current_record.flags = EEPROM.read(ptr);
    EEPROM.get(ptr+1,current_record.tag);
    Serial.write(current_record.tag);
    Serial.write('\n');
    c++;
    if(current_record.flags & EXTENDED)
    {
      ptr = ptr + sizeof(current_record);
    }else{
      ptr = ptr + sizeof(current_record)-16;
    }
  }
}


