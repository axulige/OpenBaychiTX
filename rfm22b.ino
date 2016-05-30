// **********************************************************
// Baychi soft 2013
// **      RFM22B/23BP/Si4432 Transmitter with Expert protocol **
// **      This Source code licensed under GPL            **
// **********************************************************
// Latest Code Update : 2013-10-22
// Supported Hardware : Expert Tiny, Orange/OpenLRS Tx/Rx boards (store.flytron.com)
// Project page       : https://github.com/baychi/OpenExpertTX
// **********************************************************
 
#define NOP() __asm__ __volatile__("nop") 

// Режимы RFM для 7-го регистра  RFM mode for 7 register
#define RF22B_PWRSTATE_READY    01 
#define RF22B_PWRSTATE_TX       0x09 
#define RF22B_PWRSTATE_RX       05 
#define RF22B_PWRSTATE_POWERDOWN  00 

// Режимы прерывания для 5-го регистра Interrupt mode for 5 register
#define RF22B_Rx_packet_received_interrupt   0x02 
#define RF22B_PACKET_SENT_INTERRUPT  04 

unsigned char ItStatus1, ItStatus2; 

unsigned char read_8bit_data(void); 
// void to_tx_mode(void); 
void to_ready_mode(void); 
void send_8bit_data(unsigned char i); 
void send_read_address(unsigned char i); 
void _spi_write(unsigned char address, unsigned char data); 
void RF22B_init_parameter(void); 

void port_init(void);   
unsigned char _spi_read(unsigned char address); 
void Write0( void ); 
void Write1( void ); 
void timer2_init(void); 
void Write8bitcommand(unsigned char command); 
void to_sleep_mode(void); 
 
 
//***************************************************************************** 

//-------------------------------------------------------------- 
void Write0( void ) 
{ 
    SCK_off;  
    NOP(); 
     
    SDI_off; 
    NOP(); 
     
    SCK_on;  
    NOP(); 
} 
//-------------------------------------------------------------- 
void Write1( void ) 
{ 
    SCK_off;
    NOP(); 
     
    SDI_on;
    NOP(); 
     
    SCK_on; 
    NOP(); 
} 
//-------------------------------------------------------------- 
void Write8bitcommand(unsigned char command)    // keep sel to low 
{ 
 unsigned char n=8; 
    nSEL_on;
    SCK_off;
    NOP(); 
    nSEL_off; 

    while(n--) 
    { 
         if(command&0x80) Write1(); 
         else Write0();    
         command = command << 1; 
    } 
    SCK_off;
}  

//-------------------------------------------------------------- 
void send_read_address(unsigned char i) 
{ 
  i &= 0x7f; 
  
  Write8bitcommand(i); 
}  
//-------------------------------------------------------------- 
void send_8bit_data(unsigned char i) 
{ 
  unsigned char n = 8; 
  SCK_off;
  NOP(); 

  while(n--)    { 
     if(i&0x80)  Write1(); 
     else  Write0();    
     i = i << 1; 
   } 
   SCK_off;
}  
//-------------------------------------------------------------- 

unsigned char read_8bit_data(void) 
{ 
  unsigned char Result, i; 
  
 SCK_off;
 NOP(); 

 Result=0; 
 for(i=0;i<8;i++)    {                    //read fifo data byte 
       Result=Result<<1; 
       SCK_on;
       NOP(); 
 
       if(SDO_1)  { 
         Result|=1; 
       } 
       SCK_off;
       NOP(); 
  } 
  return Result; 
}  

//-------------------------------------------------------------- 
unsigned char _spi_read(unsigned char address) 
{ 
  unsigned char result; 
  send_read_address(address); 
  result = read_8bit_data();  
  nSEL_on; 
  
  return(result); 
}  

//-------------------------------------------------------------- 
void _spi_write(unsigned char address, unsigned char data) 
{ 
  address |= 0x80; 
  Write8bitcommand(address); 
  send_8bit_data(data);  
  nSEL_on;
}  


//-------Defaults 7400 baud---------------------------------------------- 
void RF22B_init_parameter(void) 
{ 
   _spi_write(0x07, 0x80);      // сброс всех регистров RFM  / Dump (reset) all regigister for RFM
  delay(1);
  ItStatus1 = _spi_read(0x03); // read status, clear interrupt   
  ItStatus2 = _spi_read(0x04); 
  
  _spi_write(0x06, 0x00);    // no interrupt: no wakeup up, lbd, 
  _spi_write(0x07, RF22B_PWRSTATE_READY);      // disable lbd, wakeup timer, use internal 32768,xton = 1; in ready mode 
  if(Regs4[2]!=0)  _spi_write(0x09, Regs4[2]);     // точная подстройка частоты /  Precise adjust Frequency
  else _spi_write(0x09, 199);     // если сброшена, используем умолчание     If dump (reset) , using default
  _spi_write(0x0a, 0x05);    // выход CLK 2 МГц ?  output CLK 2 Mhz?

#ifdef SWAP_RXTX             // в Навке и Deluxe почему-то не так, как у остальных /  Swap RX-TX for HAwk and Delux
   _spi_write(0x0b, 0x15);    // gpio0 TX State
   _spi_write(0x0c, 0x12);    // gpio1 RX State 
#else 
   _spi_write(0x0b, 0x12);    // gpio0 TX State
   _spi_write(0x0c, 0x15);    // gpio1 RX State 
#endif

  _spi_write(0x0e, 0x00);    // gpio 0, 1,2 NO OTHER FUNCTION. 
  
// From Expert
  _spi_write(0x1F, 0x03);    //  Clock recovery
  _spi_write(0x1E, 0x0A);    //  AFC timing
  _spi_write(0x12, 0x60);    //  Режим измерения температуры -40..+85 C  Tempetature measured mode -40...+85C
  _spi_write(0x13, 0xF8);    //  Смещение температуры ?  Temperature shift?
  _spi_write(0x0F, 0x80);    //  АЦП для измерения температуры   ADС for measure temperature
  _spi_write(0x1D, 0x40);    //  AFC enable

//--------------------------
   
  _spi_write(0x1c, 0x26);    // IF filter bandwidth
  _spi_write(0x20, 0x44);    // clock recovery oversampling rate  
  _spi_write(0x21, 0x00);    // 0x21 , rxosr[10--8] = 0; stalltr = (default), ccoff[19:16] = 0; 
  _spi_write(0x22, 0xF2);    // 0x22   ncoff =5033 = 0x13a9 
  _spi_write(0x23, 0x7C);    // 0x23 
  _spi_write(0x24, 0x06);    // 0x24 
  _spi_write(0x25, 0x7D);    // 0x25 clock recovery ...
  _spi_write(0x2a, 0x0f);    // AFC limiter

  _spi_write(0x30, 0xAC);    // Mode: enable packet handler, msb first, enable crc CCITT, 
  _spi_write(0x32, 0x8C);    // Header: 0x32address enable for headere byte 0, 1,2,3, receive header check for byte 0, 1,2,3 
  _spi_write(0x33, 0x0A);    // header 3, 2, 1,0 used for head length, fixed packet length, synchronize word length 3, 2, 
  _spi_write(0x34, 0x04);    // 2 bytes preamble 
  _spi_write(0x35, 0x22);    // expect full preamble 
  _spi_write(0x36, 0x2d);    // synchronize word 3
  _spi_write(0x37, 0xd4);    // 2
  _spi_write(0x38, 0x00);    // 1
  _spi_write(0x39, 0x00);    // 0

  _spi_write(0x3e, RF_PACK_SIZE);    // total tx 16 byte 

//  _spi_write(0x6d, 0x58);    // 7 set power min TX power 
  _spi_write(0x6d, 0x0F);    // 7 set power min TX power 

// 7400 bps data rate
  _spi_write(0x6e, 0x3C); //  RATE_7400 
  _spi_write(0x6f, 0x9F); //   

  _spi_write(0x69, 0x60);    //  AGC enable
  _spi_write(0x70, 0x2E);    //  manchester enable!!!
  _spi_write(0x79, 0x00);    // no hopping (1-st channel)
  _spi_write(0x7a, HOPPING_STEP_SIZE);    // 60khz step size (10khz x value) // no hopping 

  _spi_write(0x71, 0x23);  // Gfsk,  fd[8] =0, no invert for Tx/Rx data, fifo mode, txclk -->gpio 
  _spi_write(0x72, 0x0E);  // frequency deviation setting to 8750

  //band 434.075
 _spi_write(0x75, 0x53);   // 433075 кГц  
 _spi_write(0x76, 0x4C);    
 _spi_write(0x77, 0xE0); 
}


//----------------------------------------------------------------------- 
void rx_reset(void) 
{ 
  _spi_write(0x07, RF22B_PWRSTATE_READY); 
  _spi_write(0x7e, 36);    // threshold for rx almost full, interrupt when 36 byte received 
  ppmLoop(); 

  _spi_write(0x08, 0x03);    // clear fifo disable multi packet 
  _spi_write(0x08, 0x00);    // clear fifo, disable multi packet 
  ppmLoop(); 

  _spi_write(0x07, RF22B_PWRSTATE_RX );  // to rx mode 
  _spi_write(0x05, RF22B_Rx_packet_received_interrupt); 
  ppmLoop(); 

  ItStatus1 = _spi_read(0x03);  //read the Interrupt Status1 register 
  ItStatus2 = _spi_read(0x04);  
}  
//-----------------------------------------------------------------------    

void to_rx_mode(void) 
{  
  to_ready_mode(); 
  Sleep(50); 
  rx_reset(); 
}  


#define ONE_BYTE_MKS 1055              // временнной интервал между байтами при скорости 74000 / Time time interval beetwin bytes on 74000 (speed)
#define TX_MAX_WAIT_TIME 31999         // предельное время ожидания при отправке пакета /   MAx wating time when sending packet

void sendOnFlyStd()
{
  byte i,j,k,m,b12;
  word pwm;
  unsigned long tx_start;

// Цикл формирования и отсылки данных на лету / Cycle making and sending Data (on the fly)
  tx_start=micros();
  i=1;  // первый байт мы уже отослали       First byte we sent  

  while(nIRQ_1 && ((micros()-tx_start) < TX_MAX_WAIT_TIME)) {  // ждем окончания, но не бесконечно / Wait finish, but not forever
    ppmLoop();

    if((i<RF_PACK_SIZE) && (micros()-tx_start) > (i+2)*ONE_BYTE_MKS) {   // ждем пока не подойдет реальное время отправки (запас 1 байт)/ Wait for get real sending time (1 byte  reserve)
        if(i == 1) {                                 // формируем байт старших бит и делаем предварительную подготовку пакета Make byte  High-order (senior) bit and make preliminary prepare of packet 
          for(m=j=k=b12=0; m<8; m++) {                   
            pwm=PPM[m];
            if(pwm >= 1024) j |= 1<<m;              // байт старших бит / Byte of High-order (senior) bits
            RF_Tx_Buffer[m+2]=((pwm>>2)&255);       // 8 средних бит первых 8 каналов / 8 avarage bits first 8 channels
            if(m<7) {
              if(pwm&2) k |= (2<<m);                 // байт младших бит (10 бит кодирование в упр. байте) / Byte of low-order bits (10 bits cod in control byte)
              if(pwm&1) b12 |= (1<<m);               // и байт дополнительных 11-х бит 
            }
          }
          _spi_write(0x7f,(RF_Tx_Buffer[1]=j));      // отсылаем байт старших бит  / Sending Byte of High-order bits
          RF_Tx_Buffer[RC_CHANNEL_COUNT+3]=k;        // запоминаем младшие биты  / Remember low-order bits
          if(Regs4[5]) RF_Tx_Buffer[RC_CHANNEL_COUNT+1]=b12; // и сверхмладшие   / And low - low-order bits
        } else if(i < RF_PACK_SIZE-2) {              // отправляем основные байты данных  / Send main byts of Data
           pwm=PPM[i-2];
           if(i < 10) {                              // для первых 8 байт нужна особая обработка, учитывая что байт старших бит  / For first 8 bytes need special treatment
             k=1<<(i-2); j=0;                        // уже отправлен и его не изменить. Sent and couldn't cheng
             if(pwm >= 1024) j=k;
             if((RF_Tx_Buffer[1]&k) == j) {          // если бит совпадает, с прежним, можно кодировать средние биты  If bits match with befor bits, we can coding average bits 
                RF_Tx_Buffer[i]=((pwm>>2)&255);      // 8 младших бит первых 8 каналов  / 8 low-order bits for 8 channels
                if(Regs4[5]) {                       // если надо, то еще и 11-е биты вместо последнего канала пакуем / If need , so another 11 bits packing instead last channel
                  if(pwm&1) RF_Tx_Buffer[RC_CHANNEL_COUNT+1] |= k;       // 11-е первых 8-ми за счет канала 12 / 11 first 8 at expense 12 cannel
                  else RF_Tx_Buffer[RC_CHANNEL_COUNT+1] &= ~k;            
                }
                if(i < 9) {
                  k=k+k;                             // маска младшего бита
                  if(pwm&2) RF_Tx_Buffer[RC_CHANNEL_COUNT+3] |= k;       // и не забываем уточнять младшие биты  / Don't forget clarify low-order bits
                  else RF_Tx_Buffer[RC_CHANNEL_COUNT+3] &= ~k;           // первых 7 ми каналов в управляющем байте  first 7 channels in control byte
                }
             }     
          } else {
            if(Regs4[5] == 0 || (i != RC_CHANNEL_COUNT && i != RC_CHANNEL_COUNT+1)) RF_Tx_Buffer[i]=(pwm>>3)&255;        // остальные каналы просто формируем на лету / Other channels just making on fly
            else if(i == RC_CHANNEL_COUNT) {         // в режиме 11 бит формируем последние младшие биты каналов, за счет канала 11 / 11 bit mode - make low-order bits channels, at expense 11 cannel
              pwm=PPM[i-2];
              RF_Tx_Buffer[i]=pwm&7;                 // 3 бита для байта 9 / 3 bits for byte 9
              pwm=PPM[i-1];
              RF_Tx_Buffer[i] |= (pwm&7)<<3;         // 3 бита для байта 10  / 3 bits for byte 10
              pwm=PPM[7];   
              RF_Tx_Buffer[i] |= (pwm&2)<<5;         // и один бит для байта 8  / and 1 bit for byte 8
            }
          }  
          _spi_write(0x7f,RF_Tx_Buffer[i]);           // отсылаем очередной байт  / send sequential byte
          
        } else if(i == RF_PACK_SIZE-2) {
          RF_Tx_Buffer[RC_CHANNEL_COUNT+2] = CRC8(RF_Tx_Buffer+2, RC_CHANNEL_COUNT); // формируем СRC8  / make CRC8
          _spi_write(0x7f,RF_Tx_Buffer[RC_CHANNEL_COUNT+2]);  // и отсылаем ее / and send it
        } else {
          if(FSstate == 2) RF_Tx_Buffer[RC_CHANNEL_COUNT+3]=0x01; // Нажатие кнопки == команде установки FS  / Push button ==FS
          _spi_write(0x7f,RF_Tx_Buffer[RC_CHANNEL_COUNT+3]);  // отсылаем последний, управляющий байт (или байт 10-х бит) / send last, control byte (or byte of 10 bits)
        } 
        i++;                                       // продвигаемся в буфере  / Mooving in buffer
    }
  }
}

//-------------------------------------------------------------- 
//
// Функция для подготовки отправляемых каналов на лету  / Function for prepare channels on fly
// Добавляет биты канала в очередные байты отправляемые в эфир / Add channeks bit in next sent byts 

static byte curB, bitMask, byteCntr, chCntr;

void prepFB(byte idx)
{
  if(idx < byteCntr) return;     // уже сформирванно / Already maked
  word pwm;
  byte i;
  
  if(Regs4[5] == 3) pwm=PPM[9-chCntr++];  // в режиме 3 каналы передаются в обратном порядке / Mode 3 channels send in reverse order
  else pwm=PPM[chCntr++];             // берем очередной канал / take sequential channel
  
  for(i=0; i<11; i++) {               // переносим его 11 бит в буфер отправки / move his 11 bits in buffer fo send
    if(pwm & 1) curB |= bitMask;
    if(bitMask == 0x80) {             // дошли до конца байта / get end of byte
       RF_Tx_Buffer[++byteCntr]=curB; // кладем его в буфер / put it in buffer
       curB=0; bitMask=1;             // и начинаем новый /  and sterting new
     } else bitMask+=bitMask;     // двигаем маску / move mask
     pwm >>= 1;
  }
}  

void sendOnFlyFutaba(void)                         // отправка на лету футабовского кадра / send on fly Futaba frame
{
  byte i,j;
  unsigned long tx_start;

// Цикл формирования и отсылки данных на лету / Cycle of making and sending data on fly
  tx_start=micros();
  i=1;  // первый байт мы уже отослали         
  curB=0, bitMask=1, byteCntr=0, chCntr=0;         // готовим контекст  / Prepare context

  while(nIRQ_1 && ((micros()-tx_start) < TX_MAX_WAIT_TIME)) {  // ждем окончания, но не бесконечно / Wating end but not forever
    ppmLoop();

    if((i<RF_PACK_SIZE) && (micros()-tx_start) > (i+2)*ONE_BYTE_MKS) {   // ждем пока не подойдет реальное время отправки (запас 1 байт) Wait befor get real time of sending (reserv 1 byte)
       if(i == RF_PACK_SIZE-1) {
          RF_Tx_Buffer[RF_PACK_SIZE-1] = CRC8(RF_Tx_Buffer+1, RF_PACK_SIZE-2); // формируем СRC8 / Make CRC8
       }  else {
         prepFB(i);
         if(i == RF_PACK_SIZE-2) {
           RF_Tx_Buffer[RF_PACK_SIZE-2] &= 0x3f;   // уберем лишнее из последнего байта / Remove excess from last byte
           if(FSstate == 2)                        // и добавим флаг кадра с FS / and add flag of frame with FS
             RF_Tx_Buffer[RF_PACK_SIZE-2] |= 0x80;
         }  
       }
        _spi_write(0x7f,RF_Tx_Buffer[i]);   // отсылаем байт / Sending byte
        i++;                                       // продвигаемся в буфере / Move in buffer
    }
  }
}

byte setPower(byte i=255)           // настройка мощности для FRMки / Set power output for RFM
{
  word pwm;

  to_ready_mode();
 
// Управление мощностью / Power control
//
  if(i > 7) {                       // если не задано явно / If not set 
    if(PowReg[0] > 0 && PowReg[0] <= 13) { // если задан канал 1-13 / if set ch 1-13
      pwm=PPM[PowReg[0]-1];                // берем длительность импульса / Take pulse duration
      if(pwm < 682) i=PowReg[1];           // и определяем, какую мощность требуют / and identify what Power we need
      else if(pwm >= 1364) i=PowReg[3];
      else i=PowReg[2];
    } 
#ifdef SW1_IS_ON                        // не для всех типов плат / Not for all boards
    else if(PowReg[0] == 0) {           // аппаратный переключатель на 3-х позиционном тумблере / 3 pos switch!!!
      if(SW1_IS_ON) i=PowReg[1];          // внизу - режим минмальной мощности / Low - min Power
      else if(SW2_IS_ON) i=PowReg[3];     // вверху- режим максимальной мощности / High - max Power
      else i=PowReg[2];                   // в середине - средняя мощность / Middle - average Power
    } 
#endif    
    else i=PowReg[3];                   // если не задано, используем макс. мощность  / If not set - Max Power
  }

#if(RFM_POWER_PIN != 0)    
   if(i&0x80) RFM_POWER_MIN;
   else RFM_POWER_MAX;
#endif

  i&=7; 
  _spi_write(0x6d, i+8);                // Вводим мощность в RFMку  / Input Power to RFM
  return i;
}

bool to_tx_mode(void)                  // Подготовка и отсылка пакета на лету / Prepare and send packet on fly
{ 
  byte i;

  i=setPower();
  if(++lastPower > (8-i)*3) {
    lastPower=0;                        // мигаем с частотой пропорциональной мощности / Blinking according Power output
    Green_LED_ON;
  }

  _spi_write(0x08, 0x03);               // disABLE AUTO TX MODE, enable multi packet clear fifo 
  _spi_write(0x08, 0x00);   
  
  _spi_write(0x7f,(RF_Tx_Buffer[0]=Regs4[1]));       // отсылаем номер линка в FIFO / send Number link in FIFO
  _spi_write(0x05, RF22B_PACKET_SENT_INTERRUPT);     // переводим  . Shift

  ItStatus1 = _spi_read(0x03);         //  read the Interrupt Status1 register 
  ItStatus2 = _spi_read(0x04); 

  while((micros() - lastSent) < 31350) ppmLoop(1);   // точная предварительная подгонка старта / Accurate pre start
//  while((micros() - lastSent) < 31300);             // точная окончательная подгонка старта / Accurate finish start
  _spi_write(0x07, RF22B_PWRSTATE_TX);              // старт передачи / Start sending
  lastSent += 31500;                                // формируем момент следующей отправки пакета / Make moment next send of packet

  if(Regs4[5] >= 2) sendOnFlyFutaba();
  else sendOnFlyStd(); 
  
  if(nIRQ_1) {                                     // Если не дождались отсылки / If not get sending
    Terminal.println("Timeout");
    return false;
  } 

  Green_LED_OFF;

  to_ready_mode();

  return true;
}  

//-------------------------------------------------------------- 
void to_ready_mode(void) 
{ 
  ItStatus1 = _spi_read(0x03);   
  ItStatus2 = _spi_read(0x04); 
  ppmLoop(); 
  _spi_write(0x07, RF22B_PWRSTATE_READY); 
}  

//-------------------------------------------------------------- 
void to_sleep_mode(void) 
{ 
  to_rx_mode();
} 

//############# FREQUENCY HOPPING FUNCTIONS #################
void Hopping(void)
{
    unsigned char hn;
    
    hn=Regs4[2]; 
    if(hn!=0) {                 // если разрешена точная подстройка / If allow accurate adjustment
      if(Regs4[3]) hn-=-freqCorr; // дополнительно учитываем поправку температуры / In addition, take account Temp correction
      _spi_write(0x09, hn);     // точная подстройка частоты . Accurate temp correction
      ppmLoop();
    }
    hopping_channel++;
    if (hopping_channel>=HOPE_NUM) hopping_channel = 0;
    hn=hop_list[hopping_channel];
    _spi_write(0x79, hn);
}

// Получение температуры и температурная коррекция кварца / Geting Temp and Temp quarts correction
void getTemper (void)
{
   _spi_write(0x0f, 0x80);               // запускаем измерение температуры / Meagure Temperature
   SleepMks(333);                             
   curTemperature=_spi_read(0x11)-0x40;  // читаем температуру из АЦП / Reed Temp from ADC
#if(TX_BOARD_TYPE == 1 || TX_BOARD_TYPE == 4)  // RFM23BP
   if(curTemperature > -40 && curTemperature < 85) freqCorr=-(curTemperature-25)/10;     // работаем только вреальном диапазоне / Work in real temp range
   else freqCorr=0;
#else
   if(curTemperature < 20) freqCorr=-(curTemperature-30)/10;            // область холода / Cold range
   else if(curTemperature > 30) freqCorr=-(curTemperature-30)/7;        // область жары / Hot range
   else freqCorr=0;
#endif
   if(Regs4[3] == 255) freqCorr=-freqCorr; // отрицательная поправка / Negative correction
   ppmLoop();
}  

// Процедуры автонастройеи передатчика / Auto adjust of TX
//
#define NOISE_POROG 222                   // порог шума, выше которого канал не рассматриваем  / Noise, upper not use
#define BUT_HOLD_TIME 4999                // время ожидания старта бинда по кнопке / Button hold time  for bind
#define LAST_FREQ_CHNL 232                // верхняя граница рассматриваемого диапазона / Upper range of Frequency range

static byte zw=29;                         // ширина одной частотной зоны / Width of one Frequency zone

byte findChnl(byte zn)                   // поиск лучшего чатотного канала в зоне zn или соседних / Find best Frequency in zone zn and near
{
  byte n=scanZone(zn);                   // сканируем свою зону / Scaning zone
  if(n > LAST_FREQ_CHNL) {
    if(zn) n=scanZone(zn-1);              // попробуем поискать в соседней /search in nearby zone
    if(n < LAST_FREQ_CHNL) return n;
    if(zn < 7) n=scanZone(zn-1);          // попробуем поискать в соседней / search in nearby zone
  }

  return n;
}  

byte scanZone(byte zn)                     // поиск лучшего чатотного канала в зоне zn / search best Chaneel in zn zone
{
    byte *buf=(byte *)pulseBuf;            // используем буфер SBUS для работы / Use buffer SBUS for work
    byte i,j,n,m;                             
    zn=zn*zw; m=zn+zw;                     // это границы нашей зоны / This is a border of our zone
    j=n=255;
    for(i=zn; i<m; i++) {
      if(buf[i] < NOISE_POROG) {
        if(buf[i] < j) { j=buf[i]; n=i; }   // ищем минимуум в зоне / Search minimun in zone
      }
    }
    if(j<NOISE_POROG) {                     // если нашли успешно  / if search sucsess
      Terminal.write(' '); Terminal.print(n);
      Terminal.print("/"); Terminal.print(j/4);
      if(n) buf[n-1]=255;                   // запрещаем соседние частоты / restrict neareble frequency
      if(n>1) buf[n-2]=255;
      buf[n]=buf[n+1]=buf[n+2]=255;        // и свою, что-б никто не полез / anв our
    }  
    return n;
}


char btxt1[] PROGMEM = "\r\nTry to new bind...";
char btxt2[] PROGMEM = "\n\rFreq/noise: ";
char btxt3[] PROGMEM = "Error: too noise!";
char btxt4[] PROGMEM = "Bind N=";

// Автонастройка передатчика  / Auto set of TX 
//
void makeAutoBind(byte p)                 // p=0 - c проверкой кнопки, p=255 - принудительный сброс настроек  / p=0 with button check , p=255 -forced reset of all settings          
{
  byte *buf=(byte *)pulseBuf;               // используем буфер SBUS для накопления / use SBUS buffer for accumulation
  byte i,j,k,n;
  unsigned long t=0;
  
  if(p == 0) {                   // при обычном вызове  / Usual bind
    t=millis()+BUT_HOLD_TIME;
    Green_LED_OFF;
    do {
      wdt_reset();               //  поддержка сторожевого таймера /Watchdog
      checkFS(0);                 // отслеживаем нажатие кнопочки / Check button
      
      if(millis() > t) Green_LED_ON;  // индициируем, что пора заканчивать / Have to finish
    } while(FSstate);
    if(millis() < t) return;     // не нажимали или держали мало / Don't push button or not enough
    if(!read_eeprom()) goto reset;    
  } else {
    if(p == 255) {               // принудительный вызов со сбросом регистров в дефолт  / Force reset all settings
reset:
      Regs4[3]=1; Regs4[4]=Regs4[5]=Regs4[6]=0;
      PowReg[0]=0; PowReg[1]=0; PowReg[2]=2; PowReg[3]=7;
    }
    Green_LED_ON;
  }

repeat:  
  if(Regs4[2] < 170 || Regs4[2] > 230) Regs4[2]=199;  // на всякий случай проверим поправку / Check correction
  RF22B_init_parameter();      // готовим RFMку  / Prepare RFM
  to_rx_mode(); 
  rx_reset();

  printlnPGM(btxt1,0);
  for(i=0; i<255; i++) buf[i]=0;
//     
// Постоянно сканируем эфир, на предмет выявления возможных частот  / Consoquently scaning 

  for(k=0; k<31; k++) {  
    wdt_reset();               //  поддержка сторожевого таймера / Watchdog
    for(i=0; i<LAST_FREQ_CHNL; i++) {
      _spi_write(0x79, i);      // ставим канал / Set channel
      delayMicroseconds(999);   // ждем пока перекинется / Wait
      j=_spi_read(0x26);        // читаем уровень сигнала / Read signal level
      j >>= 3;                  // делим на 8  / Devide to 8
      if(j > 7) j=8;            // что-бы не перебрать / For 
      buf[i] += j;              // накапливаем среднее / Accumulate avarage
    }
  }
  Green_LED_OFF;                // сигнализируем о завершении / LED - finished
  
  zw=(LAST_FREQ_CHNL+7)/8;      // ширина зоны  / Zone width
  
  printlnPGM(btxt2,0);          // раскидываем каналы прыжков / Hop Channel
  hop_list[0]=findChnl(0);
  hop_list[1]=findChnl(4);
  hop_list[2]=findChnl(1);
  hop_list[3]=findChnl(5);
  hop_list[4]=findChnl(2);
  hop_list[5]=findChnl(6);
  hop_list[6]=findChnl(3);
  hop_list[7]=findChnl(7);
  Terminal.println();
  
  for(i=0; i<8; i++) {         // проверяем каналы  / Check channels
   if(hop_list[i] > LAST_FREQ_CHNL) {
      printlnPGM(btxt3);
      Red_LED_Blink(5);
      goto repeat;
    }
  }    
  t=millis() - t;         // номер бинда формируется благодоря случайному всеремни отпускания кнопки / Bind number random time push button
  i=t&255; if(!i) i++;    // избегаем 0-ля / Avoid 0
  printlnPGM(btxt4,0);  Terminal.println(i); // отображаем / Show
  Regs4[1]=i;

  write_eeprom();         // запоминаем настройки  / remember settings
}

// Непрерывный режим излучения на заданной частоте / Continuous mode at a given frequency
char ftxt1[] PROGMEM = "Transmiting: F=4";
char ftxt2[] PROGMEM = " MHz, Power(0-7)=";
char ftxt3[] PROGMEM = " Fcorr(<>)= ";
char ftxt4[] PROGMEM = ". Press ESC to stop...";

void freqTest(char str[])             // отображаем уровень шумов по каналам / Show noise level
{
  word fCh=0;
  byte i,p=255;

  fCh=atoi(str+1);           // считаем параметры, если есть в виде Nbeg-end / Read parameters , if get in view Nbeg-end
  if(fCh >= 0 && fCh <= 255) {
    
    RF22B_init_parameter();     // подготовим RFMку / Prepare RFM
    _spi_write(0x70, 0x2C);    // disable manchester
    _spi_write(0x30, 0x00);    // disable packet handling

    _spi_write(0x71, 0x12);   // trclk=[00] no clock, dtmod=[01] direct using SPI, fd8=0 eninv=0 modtyp=[10] FSK
    _spi_write(0x72, 0x02);   // deviation 2*625Hz == 1.25kHz

    _spi_write(0x73, 0x00);
    _spi_write(0x74, 0x00);    // no offset
    _spi_write(0x79, fCh);     // freq hannel
    fCh=fCh*60+75;             // переведем в килогерцы / Shift to Khz

printMode:    
    printlnPGM(ftxt1,0);       // печатаем режим  / Print mode
    Terminal.print(fCh/1000+33); Terminal.write('.'); 
    if((fCh%1000) < 100) Terminal.write('0');
    Terminal.print(fCh%1000);
    p=setPower(p);              // берем можность по умолчанию / Take Power by default
    delay(10);
    Green_LED_ON;                // сигнализируем о начале / Inform about starting
    _spi_write(0x07, RF22B_PWRSTATE_TX);              // старт передачи / Start sending

    printlnPGM(ftxt2,0); Terminal.print(p);
    printlnPGM(ftxt3,0); Terminal.print(Regs4[2]);
    printlnPGM(ftxt4,0);

    while(Terminal.available() == 0) {
      wdt_reset();               //  поддержка сторожевого таймера / Watchdog
      delayMicroseconds(999);
      SDI_on;
      delayMicroseconds(999);
      SDI_off;
    }
    i=Terminal.read();
    Terminal.println();
    _spi_write(0x07, RF22B_PWRSTATE_READY);
    Green_LED_OFF;                // сигнализируем о завершении / Inform about finish

    if(i == 27)  return;          // выход по Esc / Out by Esc

    if(i == ',' || i == '.') {    // меняем поправку частоты / Cheng frequency correction
      if(i == '.') Regs4[2]++; 
      else Regs4[2]--;
      _spi_write(0x09, Regs4[2]);     // точная подстройка частоты / Accurate frequency correction
    } else  if(i >= '0' && i <= '7') p=i-'0'; // меняем мощность / measure Power 
    else if(i == 13) write_eeprom();        // по Enter запоминаем настройки / Remeber by Enter
    goto printMode;
  }
}
