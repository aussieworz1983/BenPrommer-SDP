#define SHIFT_DATA 2 //data
#define SHIFT_CLK 3  //clock 
#define SHIFT_LATCH 4 //latch
#define WRITE_EN A0
#define CHIP_EN A1
#define OUTPUT_EN A2
#define LED_DONE_PIN 13





#define EEPROM_D0 5
#define EEPROM_D7 12
#define EEPROM_SIZE 32768//256//48//2048

bool finished = false;
const byte numChars = 32768;
char receivedChars[numChars];   // an array to store the received data
int addr =0;
boolean reading = false;
void setDataPinMode(int mode)
{
  for (int pin = EEPROM_D7;pin >= EEPROM_D0; pin--) 
  {
    pinMode(pin, mode);
  }  
}

//Set the address in the shift registers
//WARNING: SHIFT_LATCH must be set high before calling
void setAddress(word address)
{
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address >> 8);
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);
   
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

//Read data at current address
//(So we can porgram faster by not having to redo address every time)
byte readEEPROMCurrent()
{
  setDataPinMode(INPUT);

  digitalWrite(OUTPUT_EN, LOW);
    
  digitalWrite(CHIP_EN, LOW);
  byte data = 0;
  for (int pin = EEPROM_D7;pin >= EEPROM_D0; pin--)
  {
    data = (data << 1) + digitalRead(pin);
  }
  digitalWrite(CHIP_EN, HIGH);
  digitalWrite(OUTPUT_EN, HIGH);
  
  return(data);  
}

//Write data to currently programmed address
//No verfiry/no write complete testing/No changing pinModes
void writeEEPROMCurrent(byte data)
{
  for(int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) 
  {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
 
  digitalWrite(WRITE_EN, LOW);
  digitalWrite(CHIP_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(CHIP_EN, HIGH);
  digitalWrite(WRITE_EN, HIGH);  
}

//Wait for the chip to finish, with a timeout
//The chip has finished writing when the data read back is the same as we wrote
bool writeWait(byte data)
{
  word timeout = 4000;
  while (timeout > 0 && readEEPROMCurrent() != data)
  {
    timeout--;
  }
  if (!timeout)
  {
    Serial.print("Write fail ");
  }
  return !timeout;
}

//   Programming Routines
//   ====================

//Read data at given address
byte readEEPROM(word address) 
{
  setAddress(address);

  return readEEPROMCurrent();
}

//Write a byte to the specified EPROM address
//SDP needs to be off (if chip has it)
bool writeEEPROM(word address, byte data) 
{
  setDataPinMode(OUTPUT);

  setAddress(address);

  writeEEPROMCurrent(data);

  return writeWait(data);
}

//     SDP related programming stuff
//     =============================

//Write a byte when SDP is enabled
//!!!(Some devices) This will enable SDP if is currently disabled
//(Some devices) you could adapt this to write multiple bytes in one go
//with only a single command sequence.
bool writeEEPROMSDP(word address, byte data) 
{
  setDataPinMode(OUTPUT);

  digitalWrite(CHIP_EN, LOW);
  setAddress(0x5555);
  writeEEPROMCurrent(0xaa);
  setAddress(0x2aaa);
  writeEEPROMCurrent(0x55);
  setAddress(0x5555);
  writeEEPROMCurrent(0xa0);

  setAddress(address);
  writeEEPROMCurrent(data);
  
  digitalWrite(CHIP_EN, HIGH);

  return writeWait(data);
}

//Send command sequence to disable SDP
void disableSDP()
{
  Serial.println("Disabling SDP");

  setDataPinMode(OUTPUT);

  digitalWrite(CHIP_EN, LOW);
  setAddress(0x5555);
  writeEEPROMCurrent(0xaa);
  setAddress(0x2aaa);
  writeEEPROMCurrent(0x55);
  setAddress(0x5555);
  writeEEPROMCurrent(0x80);
  setAddress(0x5555);
  writeEEPROMCurrent(0xaa);
  setAddress(0x2aaa);
  writeEEPROMCurrent(0x55);
  setAddress(0x5555);
  writeEEPROMCurrent(0x20);
  digitalWrite(CHIP_EN, HIGH);

  //(No delay needed)
}

//     Utilities
//     =========

//Clear entire EEPROM (every byte to 0xFF)
//Assumes SDP is disabled
void clearEeprom() 
{
  Serial.println("Clearing EEPROM");
  for (word address=0; address < EEPROM_SIZE;address++) 
  {
    if(address % 1028 == 0) 
    {
      Serial.print('c');
    }
    writeEEPROM(address, 255);
  }
  Serial.println();  
}
void fillEEPROM(byte data) 
{
  Serial.println("Clearing EEPROM");
  for (word address=0; address < EEPROM_SIZE;address++) 
  {
    if(address % 1028 == 0) 
    {
      Serial.print('c');
    }
    writeEEPROM(address, data);
  }
  Serial.println();  
}

//Dump the entire contents of the EEPROM to the serial port
void printContents() 
{
  int bufSize = 16;
   
  Serial.println("Reading EEPROM");
  for(word base=0; base < EEPROM_SIZE; base += 16) 
  {
    uint8_t data[bufSize];
    for(int offset=0; offset < bufSize; offset++) 
    {
      data[offset] = readEEPROM(base + offset);
    }

    char buf[bufSize*4];
    sprintf(buf, "%04x:  %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x",
      base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], 
      data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
    Serial.print(buf);
    Serial.print(' ');
    for (int i=0;i < bufSize;i++)
    {
      Serial.print(data[i] < 32 | data[i] > 127 ? '.' : (char) data[i]);
    }

    Serial.println();
  }
}





// Test routines
// =============

//Write every byte on the chip up to chipSize
//And verify it wrote correctly
void programROMTestPattern(word chipSize) {
  Serial.println("++Test Pattern without SDP");
  for (word address=0;address < chipSize;address++) 
  {
    int value = address % 256;
    writeEEPROM(address, value);

    if (readEEPROM(address) != value)
    {
      Serial.print("Not SDP Fail at ");
      Serial.println(address);
    }
    if (!(address % 1024)) 
    {
      Serial.print('t');
    }
  }
  Serial.println();
}

//Same as the previous routine but using SDP
void programROMTestPatternSDP(word chipSize) {
  Serial.println("++++Test Pattern with SDP");
  for (word address=0;address < chipSize;address++) 
  {
    int value = 255 - (address % 256);
    writeEEPROMSDP(address, value);

    if (readEEPROM(address) != value)
    {
      Serial.print("SDP Fail at ");
      Serial.println(address);
    }
    if (!(address % 1024)) 
    {
      Serial.print('T');
    }
  }
  Serial.println();
}

//Verify that SDP is disabled
//address = address to test
void testSDPOff(word address)
{
  //Backup old
  byte old = readEEPROM(address);
  //Write inverted
  writeEEPROM(address, ~old);
  //Read it back
  byte verify = readEEPROM(address);
  //Did we get back what we wrote?
  if (verify != (~old & 255))
  {
    Serial.println("Test SDP Off FAIL (1)");
    Serial.println(old, 2);
    Serial.println(~old & 255, 2);
    Serial.println(verify, 2);
  }
  //Is it still the old value?
  if (verify == old)
  {
    Serial.println("Test SDP Off FAIL (2)");
  }
  //Clean up - write original data back
  writeEEPROM(address,old);
  //Is it still the original data?
  if (readEEPROM(address) != old)
  {
    Serial.println("Test SDP Off FAIL (3)");
  }
}

//Verify that SDP is enabled
//address = address to test
void testSDPOn(word address)
{
  //Backup old
  byte old = readEEPROM(address);
  //Write inverted
  Serial.print("Expecting a write fail!!!: ");
  writeEEPROM(address, ~old);
  //Read it back
  byte verify = readEEPROM(address);
  //Did we change it?
  if (verify != old)
  {
    Serial.println("Test SDP On FAIL (1)");
  }
  //Clean up - write original data back in case we changed it
  writeEEPROM(address,old);
  //Is it still the original data?
  if (readEEPROM(address) != old)
  {
    Serial.println("Test SDP On FAIL (3)");
  }
}

//Run a series of tests
//Address is the address to use to test SDP enabled/disabled
//chipSize is passed to the programROMTestPattern routines
void autoTest(word address, word chipSize)
{
  disableSDP();
  testSDPOff(address);
  programROMTestPatternSDP(chipSize);
  testSDPOn(address);
  disableSDP();
  testSDPOff(address);
  programROMTestPattern(chipSize);
}
void Done(bool state){
  while(state==true){
      digitalWrite(LED_DONE_PIN,HIGH);
      delay(100);
      digitalWrite(LED_DONE_PIN, LOW);
      delay(100);
    }  
    if(digitalRead(LED_DONE_PIN)==HIGH){
      digitalWrite(LED_DONE_PIN, LOW);
      }
  }
void ReadAll(){
  byte ch[2];
 
  for (word address=0; address < EEPROM_SIZE;address++) 
  {
    
  byte ret = readEEPROM(address); 
   
    Serial.write(ret);
  
  }
  
  Done(true);
  ch[0]=1;
  ch[1]=1;//send back finished
  Serial.write(ch,sizeof(ch));
  delay(3000);
  Done(false);
  
  }
void WriteFromUsb(){
 reading=true; 
 byte ch[2];

 byte theBytes[64]; 
 
 ch[0]=1;
 ch[1]=1;//send back ready
 Serial.write(ch,sizeof(ch));
 
 
 
 while(reading==true){
  digitalWrite(LED_DONE_PIN,HIGH);
  if(Serial.available()>0){
    Serial.readBytes(theBytes,sizeof(theBytes));
     for(int j=0;j<sizeof(theBytes);j++){
        writeEEPROM(addr, theBytes[j]);
        addr++;
     } 
    
    }
     if(addr>32767){
      reading=false;
      }
  }

  digitalWrite(LED_DONE_PIN,LOW);
  
  
  }
void setup() {
   pinMode(LED_DONE_PIN, OUTPUT);
  

  digitalWrite(LED_DONE_PIN,LOW);
  // put your setup code here, to run once:
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(SHIFT_LATCH, LOW);

  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);
  digitalWrite(CHIP_EN, HIGH);
  pinMode(CHIP_EN, OUTPUT);
  digitalWrite(OUTPUT_EN, HIGH);
  pinMode(OUTPUT_EN, OUTPUT);

  Serial.begin(9600);
  Serial.println("ok");

 

}


//Flash the LED when we're done
void loop() {
byte ch[2];
   // if(Serial.available()){
    
     
    Serial.readBytes(ch,sizeof(ch));
    if(ch[0]==1&&ch[1]==1){
      ch[0]=0;
      ch[1]=0;
      Serial.write(ch,sizeof(ch));
      }
      else if(ch[0]==1&&ch[1]==0){
         
         ReadAll();
      }
      else if(ch[0]==0&&ch[1]==1){
        // Done(false);    
           
         WriteFromUsb();
         
      }else {
        // Serial.write(ch,sizeof(ch));
        
        }
    
   
  // }


  // put your main code here, to run repeatedly:
}
