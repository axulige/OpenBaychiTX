// **********************************************************
// Baychi soft 2013
// **      RFM22B/23BP/Si4432 Transmitter with Expert protocol **
// **      This Source code licensed under GPL            **
// **********************************************************
// Latest Code Update : 2013-10-22
// Supported Hardware : Expert Tiny, Orange/OpenLRS Tx/Rx boards (store.flytron.com)
// Project page       : https://github.com/baychi/OpenExpertTX
// **********************************************************

#define REGS_EERPON_ADR  4     /* first byte of eeprom */

#define FLASH_SIZE 16384         /* размер контроллируемой памяти программ. Dimension of memory */
#define FLASH_SIGN_ADR 64         /* адрес сигнатуры прошивки в EEPROM. Addres signature of firmware in EEPORM */
#define FLASH_KS_ADR 66           /* адрес контрольной суммы прошивки в EEPROM. Control summ address of firmware in EEPROM */
#define EEPROM_KS_ADR 68          /* адрес контрольной суммы настроек в EEPROM. Control summ setup of firmware in EEPROM */


unsigned int read_eeprom_uint(int address)
{
 return (EEPROM.read(address) * 256) + EEPROM.read(address+1); 
}

unsigned char read_eeprom_uchar(int address)
{
 return  EEPROM.read(address+REGS_EERPON_ADR); 
}


void write_eeprom_uint(int address,unsigned int value)
{
 EEPROM.write(address,value / 256);  
 EEPROM.write(address+1,value % 256);  
}

void write_eeprom_uchar(int address,unsigned char value)
{
  return  EEPROM.write(REGS_EERPON_ADR+address,value); 
}


// Проверка целостности прошивки. Checking firmware 
//

byte flash_check(void)
{
  unsigned int i,sign,ks=0;
  
  if(version[1] == 0) return 0;          // в режиме отладки не проверяем FLASH . Debug mode, don't check FLASH
  
  for(i=0; i<FLASH_SIZE; i++)            // считаем сумму . Account sum
      ks+=pgm_read_byte(i);
  
   sign=version[0] + (version[1]<<8);     // сигнатура из номера версии. Signature from version number
   i=read_eeprom_uint(FLASH_SIGN_ADR);    // прежняя сигнатура. Previus signature
   if(sign != i) {                        // при несовпадении, пропишем новые значения. If mismatch, wright new value
     write_eeprom_uint(FLASH_SIGN_ADR,sign); 
     write_eeprom_uint(FLASH_KS_ADR,ks);
     return 1;                            // новая программа. New programm
   } else {                               // в противном случае проверяем КС. In other way checking Control sum
     i=read_eeprom_uint(FLASH_KS_ADR);
     if(i != ks) return 255;               // признак разрушенной прошивки. Sign of damage firmware
   }
   
   return 0;                               // все в порядке. All OK
}    


// Чтение всех настроек. Read all settings
bool read_eeprom(void)
{
   byte i;
   unsigned int ks=0;
   
   for(i=0; i<sizeof(Regs4); i++)    ks+=Regs4[i] = read_eeprom_uchar(i);  // первые 5 регистров. Ferst 5 registers

   // hopping channels
   for(i=0; i<HOPE_NUM; i++)  ks+=hop_list[i] = read_eeprom_uchar(i+11);
  
   // Регистры управления мощностью (19-23): канал, мошность1 - мощность3. Power manage register (19-23): Channrel, Power1-power3
   for(i=0; i<sizeof(PowReg); i++)    ks+=PowReg[i] = read_eeprom_uchar(i+19);

   if(read_eeprom_uint(EEPROM_KS_ADR) != ks) return false;            // Checksum error

   return true;                                            // OK
} 

// Запись всех настроек. Wright all settings
void write_eeprom(void)
{
   byte i;
   unsigned int ks=0;
   
   for(i=0; i<sizeof(Regs4); i++) {
     write_eeprom_uchar(i,Regs4[i]);  
     ks+=Regs4[i];  
   }

   // hopping channels
   for(i=0; i<HOPE_NUM; i++) {
     write_eeprom_uchar(i+11,hop_list[i]);   
     ks+=hop_list[i];
   }
  
   // Регистры управления мощностью (19-23): канал, мошность1 - мощность3. Power manage register (19-23): Channrel, Power1-power3
   for(i=0; i<sizeof(PowReg); i++) {
     write_eeprom_uchar(i+19,PowReg[i]);  
     ks+=PowReg[i];  
   }
   write_eeprom_uint(EEPROM_KS_ADR,ks);        // Write checksum
} 

char etxt1[] PROGMEM = "FLASH ERROR!!! Can't work!";
char etxt2[] PROGMEM = "Error read settings!";

void eeprom_check(void)              // читаем и проверяем настройки из EEPROM, а также целостность программы. Read and check settings from EEPROM
{
  byte b=flash_check();

  if(b == 255) {             // совсем плохо, если программа разрушена, работать нельзя. All bad, program damage, forbiden work      
      printlnPGM(etxt1);
error_blink:
      Red_LED_Blink(59999);  // долго мигаем красным, если КС не сошлась. Red blinking long time if Conrtol sum wrong
      return;
  }    
  
  if(!read_eeprom()) {      // читаем настройки  . Read settings
    if(b == 1) {
       makeAutoBind(255);   // если это первый запуск программы, производим сброс в дефлот, и автонастройку. If first lunch of the program, drop to default, and autosettings
    } else {                   
       printlnPGM(etxt2);
       goto error_blink; 
    }
  }
}  
