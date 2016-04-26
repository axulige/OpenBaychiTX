// **********************************************************
// Baychi soft 2013
// **      RFM22B/23BP/Si4432 Transmitter with Expert protocol **
// **      This Source code licensed under GPL            **
// **********************************************************
// Latest Code Update : 2013-10-22
// Supported Hardware : Expert Tiny, Orange/OpenLRS Tx/Rx boards (store.flytron.com)
// Project page       : https://github.com/baychi/OpenExpertTX
// **********************************************************

//-------------------------------------------------------
// 
// Реализация протокола SBUS через ICP/INTx(возможно) вход. Make SBUS via ICP/INTx input
// Драйвер ICP фиксирует моменты изменения лог уровня в буфере sbusbuf. Driver ICP take time of chenging logic levels in buffer sbusbuf
// Фоновый процесс sbusLoop анализирует первичный буфер и преобразует моменты перехода в байты протокола. background process sbusLoop, analyzed primary buffer and convert transition moments to byts of protocol 

#define SBUS_MIN_PWM 1760         // минимальна длительность импульса в представлении SBUS (мкс/2). Minimal pulse time in SBUS present

word PPM[RC_CHANNEL_COUNT+1];     // текущие длительности канальных импульсов. Current duration channels pulse
byte ppmAge = 0; // age of PPM data
byte ppmCounter = RC_CHANNEL_COUNT; // ignore data until first sync pulse
byte ppmDetecting = 0;            // counter & flag for PPM detection
byte ppmMicroPPM = 0;             // status flag for 'Futaba microPPM mode'

#define PULSE_BUF_SIZE 184                // размер буфера импульсов. Buffer dimension pulse
#define TICK_IN_BIT 20                    // количество тиков по 0.5 мкс на один бит. Tick in bit (0,5 microsecond)
#define SBUS_PKT_SIZE 25                  // размер пакета sbus SBUS packet size

volatile word pulseBuf[PULSE_BUF_SIZE];   // буфер фронтов импульсов принимаемых через PPM вход. Pulse buffer size geting through PPM input
volatile word *pbPtr=pulseBuf;            // указатель в буфере импульсов. pointer in buffer pulse 
byte eCntr1=0;                            // счетчик ошибок четности sbus. error counter SBUS
word eCntr2=0;                            // счетчик битых пакетов sbus. Damaged SBUS packet counter 
byte frontChange=0;                       // маска смены фронта для sbus. Mask for change SBUS front

// -----------------------------------------------------------
//
// Обработчики прерываний. Interrupt handlers 

#ifdef USE_ICP1 // Правильный режим -  использованиие ICP1 in input capture mode. Right mode! use ICP1 in input capture mode

ISR(TIMER1_CAPT_vect)
{
  *pbPtr=ICR1;                          // сохраняем зафиксированное значение . Save fixed value
//  TCCR1B ^= (1<<ICES1);                 // меняем тип фронта на противоположенный. Chang front type on opposite
  TCCR1B ^= frontChange;                 // для sbus меняем тип фронта на противоположенный. For SBUS Chang front type on opposite
  if(++pbPtr >= pulseBuf+PULSE_BUF_SIZE) pbPtr=pulseBuf;
}  

void setupPPMinput()
{
  ppmDetecting = 1;
  ppmMicroPPM = 0;
  TCCR1A = ((1 << WGM10) | (1 << WGM11));
  TCCR1B = ((1 << WGM12) | (1 << WGM13) | (1 << CS11));   // Setup timer1 for input capture (PSC=8 -> 0.5ms precision, falling edge)

  OCR1A = 65535;
  TIMSK1 |= (1 << ICIE1);   // Enable timer1 input capture interrupt
}

#else // sample PPM using pinchange interrupt

ISR(PPM_Signal_Interrupt)
{
    *pbPtr++=TCNT1;  // просто сохраняем значение и продвигаемся в буфере. Just save value and moved in buffer
    if(pbPtr >= pulseBuf+PULSE_BUF_SIZE) pbPtr=pulseBuf;
}

void setupPPMinput(void)
{
  ppmDetecting = 1;
  ppmMicroPPM = 0;
  // Setup timer1 for input capture (PSC=8 -> 0.5ms precision, top at 20ms)
  TCCR1A = ((1 << WGM10) | (1 << WGM11));
  TCCR1B = ((1 << WGM12) | (1 << WGM13) | (1 << CS11));
  OCR1A = 65535;
  TIMSK1 = 0;
  PPM_Pin_Interrupt_Setup
}
#endif

ISR(INT0_vect){                 // обработчик прерывания от RFMки НЕ используется. Interrupt handlers for RFM not use
}  

inline void mks2code(byte ch, word mks)      // преобразование 2*мкс в 11 битный код. Interrupt 2*mks in 11 bit code
{
  if(Regs4[5] >= 2) {                 // представление Futaba (880-1520-2160). Futaba represent 
    if(mks<1760) mks=0;
    else if(mks > 4319) mks=2047;
    else {
      mks-=1760;
      mks=(mks<<2)/5;
    } 
  } else {                            // представление Expert (JR) (988-1500-2012). JR represent
    if(mks < 1976) mks=0;
    else if(mks > 4023) mks=2047;
    else mks=mks-1976;
  }

  PPM[ch]=mks;
}  

inline void sbus2code(byte ch, word cod)  // преобразование 11 бит кода из пакета futaba в 11-битный код для отправки. convert 11 bit code from Futaba packet to 11 bit code for send
{
  if(Regs4[5] < 2) {                      // представление Expert. JR represent
     cod=((cod+cod+cod+cod+cod)>>2);      // умножаем на 5/4. multiply on 5/4
     if(cod < 216) cod=0;                 // и ограничиваем    . And limited
     else if(cod > 2263) cod=2047;
     else cod=cod-216;
  }
  PPM[ch]=cod;
}  

inline word code2mks(byte ch)      // преобразование в мкс для отображения. Converting in mks for showing
{
  word pwm=PPM[ch]+1;              // +1 для более точного округления. +1 for accurate round

  if(Regs4[5] >= 2)                // представление Futaba. Futaba represent
     pwm=((pwm+pwm+pwm+pwm+pwm)>>3) + 880;
  else pwm=(pwm>>1)+988;          // представление Expert. JR represent

  return pwm;
}

// Рвбота с SBUS протоколом.  Working with SBUS protocol

byte sbusPkt[SBUS_PKT_SIZE];          // используем общий буфер для sbus и импульсов. Use common buffer for SBUS and pulse 
static byte curByte=0, bitFlag=0, bitCntr=0, pktPtr=0;   // текущий байт, бит, счетчики бит и байт. Current byte, bit, counters bits and byts
static byte bit1Cntr=0, bMask=0;                  // счетчик единичных бит и маска бита. Counter single bit and bit mask
static word lastPulse=0;

void inline endPkt(void)        // завершаем текущий пакет, проверяем и переносим данные в массив PPM длительностей. Ask current packet, check, and transfer data to PPM duration array 
{
   byte i,j,k,m;
   word pwm,wm;
   
   if(pktPtr >= SBUS_PKT_SIZE && eCntr1 == 0 &&  // если набран кворум, нет ошибок по четности. If get quorum, no even mistake
      sbusPkt[0] == 0x0F && sbusPkt[24] == 0x00 ) {  // проверяем начало и конец. Cheak begining and end
      k=m=1;                                 // счетчик байт в sbus и маска бита. Counter byte in SBUS and bit mask
      for(i=0; i<RC_CHANNEL_COUNT+1; i++) {  // формируем наши PPM длительности  (13-й канал, для управления мощностью). Make PPM duration
        pwm=0; wm=1;
        for(j=0; j<11; j++) {          // счетчик бит в представлении. Bit counter 
          if(sbusPkt[k] & m) pwm |= wm;
          wm += wm;
          if(m == 0x80) { m=1; k++; } // обнуляем. null
          else m = m + m;             // или двигаем маскую . or move mask
        }
        sbus2code(i,pwm);            // формируем значение в PPM буфере. make value in PPM buffer
      }    
      ppmAge = 0;                    // признак обновления PPM. Sign of PPM update
   } else if(pktPtr) eCntr2++;       // считаем ошибки, если были пакеты в стадии формирования. Count errors, if packets was in making process

   curByte=bitFlag=bitCntr=pktPtr=bit1Cntr=eCntr1=0;
}

void inline sbusPulse(word val)    // обработка очередного принятого импульса. Handle sequential pulse 
{
  byte i=(val+TICK_IN_BIT/2)/TICK_IN_BIT;    // сколько бит формируем . Quantity bits making
  if(i > 10) { endPkt(); return; }           // паузы трактуем, как паузы между пакетами. Pause interpretб like pause between packets  
  
  if(bitCntr == 0) {                         // стартовый бит пропускаем. Start bit shift
     i--; bitCntr=1; 
     eCntr1+=bitFlag;                        // он должен быть нулевым. Start bit must be null
     bMask=1;
  }  

  for(;  i > 0; i--) {
    if(bitFlag) {
      bit1Cntr++;                 // считаем единичные биты  . Count single bits
      curByte |= bMask;           // формируем текущий байт. Make currant byte
    }
    bMask += bMask;
    
    if(++bitCntr == 12 || (bitCntr>9 && pktPtr >= 24))  {            // при поступлении стопового бита. if get stop bit
      sbusPkt[pktPtr] = curByte;         // запоминаем сформированный байт. Saving maked byte
      eCntr1+=(bit1Cntr&1);              // считаем ошибки по четности. Counting Even errors 
      curByte=bitCntr=bit1Cntr=0;
      if(++pktPtr >= SBUS_PKT_SIZE) {
        endPkt();
        break;
      }
    } else if(bitCntr == 11)   {       // стоповые биты должны быть единичными. Stop bits must be single
       if(!bitFlag) eCntr1++;    
    }
  }
  bitFlag=1-bitFlag;                      // меняем значение бита на противополеженное . Change bit value to opposite
}  
/*******************************************/

static byte pCntr=0;
static word lastP=0;

static void processPulse(word pulse)
{
#ifndef USE_ICP1                               // неправильный режим -  использованиие прерывания. Not good mode - use interrupt
    pCntr++;                                   // так как наш драйвер ловит оба фронта. as our driver catch two fronts
    if(!(pCntr&1)) {                           // половину нужно игнорировать  . Hаlf we have to ignore 
      lastP=pulse;
      return;
    }
    pulse+=lastP;                                  
#endif

  if(ppmDetecting) {                           // на стадии детектирования определяем наличие импульсов. in Detection stage defined pulse 
    if(ppmDetecting > 49) {
      ppmDetecting=0;

      if(eCntr1 > 9) {
        ppmMicroPPM=0xff;                        // признак работы в режиме SBUS. sign of SBUS mode 
        frontChange=(1<<ICES1);
      } else if(ppmMicroPPM>10) {
        ppmMicroPPM=1;                           // работа в режиме Futaba PPM12. work with Futaba PPM12 mode
      } else {
        ppmMicroPPM=0;
      }
    } else {
      if(pulse < 241)                              // импульсы короче 50 мкс трактуем, как SBUS. Pulse shorter 50 μs  - SBUS
        eCntr1++;                           
      else if (pulse<1600)                         // импульсы короче 800 мкс трактуем, как Футабские. Pulse shorter 800 μs - Futaba 12
        ppmMicroPPM++;

      ppmDetecting++;
    }
  } else {
    if(ppmMicroPPM) {
      pulse += pulse;                             // переводим в удвоенные микросекунды. transfer to double microsecond
    }

    if (pulse > 4999) {            // Если обнаружена пауза свыше 2.5 мс. If find pause more 2,5 ms
      ppmCounter = 0;              // переходим к первому канала. Jump to first channel
      ppmAge = 0;                  // призна поступления нового пакета. Sign of getting new packet
    } else if ((pulse > 1499) && (ppmCounter < RC_CHANNEL_COUNT)) { 
      mks2code(ppmCounter++,pulse);   // Запоминаем очередной импульс. Remember sequential pulse
    } else {
      ppmCounter = RC_CHANNEL_COUNT; // Короткие пички отключают цикл. Short picks - desable cycle 
    }
  }
}


word *ppmPtr= (word* ) pulseBuf;
unsigned long loopTime=0;
word avrLoop=0,ppmDif=0,mppmDif=0;

void ppmLoop(byte m)                       // Фоновый цикл обработки импульсов. Самая ресурсоемкая часть программы в sbus режиме. Backgroung cycle pulce handl.
{
  word i,j,n,*lastPb;
  unsigned long lt;

  cli();
  lastPb=(word *)pbPtr;       // берем текущиее положение указателя драйвера. Take current position of driver pointer
  sei();   

//---------------------------------- отладка для контроля времени и буфера импульсов . Debug for control time and pulse buffer
  if(Regs4[6]&2) {           // если отладка включена   . If debug inable
    lt=micros(); 
    i=lt-loopTime;  
    if(maxDif < i) maxDif=i;  // меряем максимальное время цикла ppmLoop. measure maximum cycle time ppnLoop
    loopTime=lt;

    avrLoop=avrLoop-(avrLoop>>5) + i;     // и его усреденение за 1 сек. and his averaging per 1 sec
    
    ppmDif=lastPb-ppmPtr;
    if(ppmDif >= PULSE_BUF_SIZE) ppmDif=PULSE_BUF_SIZE+ppmDif;
    if(mppmDif<ppmDif) mppmDif=ppmDif; // меряем максимальное количество необработанных времен в буфере. measure maximum quantity untreatment times in buffer (dificult to translate:(  ) 
  }
//-------------------------------
 
  for(n=0; ppmPtr != lastPb && n < m; n++) {  // пока есть новые данные в буфере, обрабатываем порцию размером до n . as we have new data in buffer, treatment portion sized to n.
     i=*ppmPtr++;
     if(ppmPtr >= pulseBuf+PULSE_BUF_SIZE) ppmPtr=(word *)pulseBuf;
     j=i-lastPulse; 
     lastPulse=i;
     if(ppmMicroPPM != 255) processPulse(j); 
     else sbusPulse(j); 
  }
}


// Отслеживание и отображение изменения состояния. tracking and showing state changes

word prevDif=0, prevErr=0, prevLat=0;
byte nchan=0, prevMode=0;
char prevTemp=0;
int ptAvr=0;
byte ptAvrCnt=0;
byte showNum=0;

bool checkTemp(void)            // проверяем, нужно ли отображать изменение температуры (что-бы не плясать на границе градуса). check, need to show temperature
{
  signed char d=prevTemp - curTemperature;

  if(d == 0) return false;

  if(abs(d) >= 2) {            // Более 1 градуса отображаем сразу. More 1 degree - showing
    ptAvr=0; 
    ptAvrCnt=0;
    return true;
  }
 
  if(ptAvrCnt >= 99) {        // Проверяем усреднение за 3 сек. Check averaging for 3 sec
     if((ptAvr/ptAvrCnt) != curTemperature) {
       ptAvr=0; 
       ptAvrCnt=0;
       return true;
     }
  } else {
    ptAvr += curTemperature;
    ptAvrCnt++;
  }

  return false;
}

byte showStage = 0;    // что-бы не напрягать проц, делаем все по частям. Make all parts by parts
byte prevFS,FSdetect = 0;     // Признак отключения выхода по FS ретранслятора. Sign of desable outputs FS retranslator

void showState(void)   // Отображение состояния после отправки пакета . Show state after transmit packet
{
  byte i; 
  if(maxDif > 3999) maxDif=0;      // обнуляем очевидное. Null
  
  if(checkTemp() || prevErr != eCntr2 ||  prevMode != ppmAge || prevDif != maxDif || prevLat != mppmDif || FSdetect != prevFS) {
     prevDif=maxDif;
     prevErr=eCntr2;
     prevTemp=curTemperature;
     prevMode = ppmAge;
     prevLat = mppmDif;
     prevFS=FSdetect;
     
     showStage =1;
  }   

  switch(showStage) {
  case 1:                       // вывод состояния. State outputs
     Terminal.write('\r');
     if(FSdetect) Terminal.print("Stop:");
     else if(ppmAge == 255) Terminal.print("Waiting start:");
     else if(ppmAge > 5) Terminal.print("Input lost:");
     else {
       if(!nchan) {            // один раз подстчитаем каналы PPM. once counting channels PPM
         for(i=0; i<RC_CHANNEL_COUNT; i++) {
            if(PPM[i]) nchan++;
         }
         ppmLoop();
       } 
       if(ppmMicroPPM == 255) Terminal.print("SBUS");
       else {
         if(ppmMicroPPM) Terminal.print("Fut750u ");
         Terminal.print("PPM");   ppmLoop();
         Terminal.print(nchan); 
       }
       Terminal.print(" mode:");
     }
     ppmLoop();
     showStage=2;
     break;

 case 2:                            // температура и ее поправка. Temperature and it correction
     Terminal.print(" T=");  Terminal.print((int)prevTemp);  // температура. Temperature
     Terminal.print(" Tc=");  Terminal.print((int)freqCorr);  // поправка частоты. Frequency correction
     ppmLoop();
     showStage=3;
     break;
     
 case 3: 
     if(Regs4[6]&2) {           // если требуется доп. информация. addition informatiom
       Terminal.print(" M=");  Terminal.print(prevDif);    // макс длительность цикла. Max cycle duration   
       ppmLoop();
       Terminal.print(" A=");  Terminal.print(avrLoop>>5); // средняя длительность цикла. Avarage cycle duration 
       ppmLoop();
       if(ppmMicroPPM == 255) {      // в режиме SBus 
         Terminal.print(" B=");  Terminal.print(prevLat);  // макс. запаздывание. Max delay 
         ppmLoop();
         Terminal.print(" E=");  Terminal.print(prevErr);  // ошибки пакетов. Packets error
         ppmLoop();
       }
     }
     showStage=4;
     break;

  case 4:
     if(Regs4[6]&1) {        // если включен вывод PPM импульсов. if inable PPM pulse output
       for(i=0; i<8; i++) { Terminal.print("    "); ppmLoop(2); }             // подчистим грязь. Cleaning dirty
     }
     Terminal.println();  
     showStage=showNum=0;
     break;
    
  case 0:                   // стадия вывода PPM импульсов, в свою очеред состоит из nchan стадий. PPM pulse output stage. consist from nchan stage.
     if((Regs4[6]&1) && nchan >2) {
       if(Regs4[6]&4) Terminal.print(PPM[showNum],HEX);  // выводим код. Show code
       else Terminal.print(code2mks(showNum));     // печатаем значение . Print value
       ppmLoop();
       Terminal.write(' ');
       if(++showNum >= nchan) {
         Terminal.write('\r');
         showNum=0;
       }
     }
     break;
  }
}

bool checkPPM(void)         // проверка PPM/SBUS на failSafe ретранслятора. Check PPM/SBUS on failsafe retranslator
{
  if(Regs4[4]) {                   // если проверка разрешена. If checking inable
    FSdetect=1;
    if(ppmMicroPPM == 255) {       // режим SBUS, FS = бит3 в управл. байте. Sbus mode, FS - bit 3 in control byte
      if(sbusPkt[23]&0x8) return false;
    } else {
      for(byte i=0; i<nchan; i++) {
        word pwm=code2mks(i); 
        if(pwm < 1000 || pwm >= 2000) return false;  // проверяем выход канала за диапазон. Chek channel output out of range
      }
    }

  }
  FSdetect=0;
  return true;            // PPM в порядке. PPM OK
}  
