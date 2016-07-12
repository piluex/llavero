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
#ifndef LLEPROM_H
#define LLEPROM_H

enum EEPROM_record_flags
{
  FREE_RECORD = 0x1,
  EXTENDED = 0x2,
  TOTP = 0x4,
};

typedef struct 
{
  byte flags;
  char tag[8];//Last char is always 0 and not saved
  byte data1[16];
  byte data2[16];//Only for extended records
}EEPROM_record;

void eeprom_init();
void eeprom_reset();
EEPROM_record *eeprom_current_record();
void eeprom_new();
bool eeprom_load_by_tag(char* tag);
void eeprom_write();
void eeprom_print_all_tags();

#endif
