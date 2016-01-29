
/* 
 * Programa para control de ordenes de marcha/paro de inversor Fronius y variador de velocidad de bombeo
 * 
 *
 * This sketch adds date string functionality to TimeSerial.pde
 * Miguel Alonso Abella - Ciemat - miguel.alonso@ciemat.es
 * 28 de enero de 2016 - versión con reloj Tyny RTC conectado SDA->A4 y SCL->A5
 */ 

 /* Comandos por puerto serie: T,A,G,M
  * T+unix time para poner la fecha y hora
  * A+ float para la atura solar limite
  * G+float para la irradiancia limit
  * MB manual bomba ON
  * MF manual fronius ON
  * MA manual change to automatic
  */
 
#include <TimeLib.h>
#include <Wire.h>
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <math.h>
#include <SunPos.h>
#include <Streaming.h> 
#include <EEPROM.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Software SPI (slower updates, more flexible pin options):
// pin 11 - Serial clock out (SCLK)
// pin 10 - Serial data out (DIN)
// pin 9 - Data/Command select (D/C)
// pin 8 - LCD chip select (CS)
// pin 7 - LCD reset (RST)
// pin  de backlight a pin 12 del nano
Adafruit_PCD8544 display = Adafruit_PCD8544(11, 10, 9, 8, 7);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2


#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16

static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };



 // definimos el lugar, sun y time de acuerdo con sunpos.h
 cLocation lugar;
 cSunCoordinates sun;
 cTime fecha_hora_PSA;
time_t t;
tmElements_t tm;
char time[20];
char buf [8];

// variables de control
  float altura_solar_limite=20; // valor de altura solar límite para encender la bomba
  float irradiancia_limite=400;
 
//A0 es la irradiancia
//A4 y A5 es el reloj tynyRTC
const int relePin=2;
const int ledPin=13;
const int manual_bombaPin=3;
const int manual_froniusPin=4;
const int pozoPin=5;
const int depositoPin=6;
const int backlightPin=12; // 11, 10, 9, 8, 7 son para el displau Nokia
 int pozo=0;
 int deposito=0;
 int manual_bomba=0;
 int manual_fronius=0;
 int manual=0;
 int manual_bomba_cmd=0; // comando por puerto serie
 int manual_fronius_cmd=0;
unsigned long  delayTime;

int buttonState = 0; 

bool releStatus=0;
float altura_solar;
int irradianciaPin = A0;    // select the input pin for the potentiometer
int irradianciaValue = 0;  // variable to store the value coming from the sensor
float voltage=0;
float irradiancia=0;
float calib=4.0; //valor en V de a 1000W/m2
const int numReadings = 30;
int readings[numReadings];      // the readings from the analog input
int index = 0;  
float Vref=5.0; //valor de referencia de tensión arduino
int total = 0;                  // the running total
int average = 0;                // the average


void setup()  {
  pinMode(relePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(manual_bombaPin, INPUT);
  pinMode(manual_froniusPin, INPUT);
  pinMode(pozoPin, INPUT);
  pinMode(depositoPin, INPUT);
  pinMode(backlightPin, OUTPUT);

  display.begin();
  display.setContrast(50);
  display.display(); // show splashscreen
  delay(1000);
  display.clearDisplay();   // clears the screen and buffer
  
  Serial.begin(9600);
  while (!Serial) ; // Needed for Leonardo only
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if (timeStatus() != timeSet) 
     Serial.println(F("Unable to sync with the RTC"));
  else
     Serial.println(F("RTC has set the system time"));  

     //coordenadas del lugar 
     lugar.dLongitude=-3.7336;
     lugar.dLatitude=40.4627;
     sun.dZenithAngle=0;
     sun.dAzimuth=0;
     for (int thisReading = 0; thisReading < numReadings; thisReading++){
    readings[thisReading] = 0;          
     }

      float valor; //lee los valores almacenados en eeprom
     EEPROM.get(0, valor);
     if (valor!=0){irradiancia_limite=valor;}
     EEPROM.get(5, valor);
     if (valor!=0){altura_solar_limite=valor;}
     
     
}

void loop(){    
  if (Serial.available()) {
     procesaSerie();
     }
  
  if(timeStatus()!= timeNotSet) 
  {
    digitalClockDisplay(); 
   //=======================================
   // fecha de sunpos PSA
  fecha_hora_PSA.iYear=year();
  fecha_hora_PSA.iMonth=month();
  fecha_hora_PSA.iDay=day();
  fecha_hora_PSA.dHours=hour();
  fecha_hora_PSA.dMinutes=minute();
  fecha_hora_PSA.dSeconds=second();
  sunpos(fecha_hora_PSA, lugar, &sun);
  altura_solar=90-sun.dZenithAngle;
  //========================================== 
  lee_irradiancia();
  control();
  }
  display_nokia511();
  
  delay(1000);
}

void digitalClockDisplay(){
   // digital clock display of the time
  Serial.println(F("--------------------------------->>>"));
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print("  ");
  Serial.print(day());
  Serial.print("-");
  Serial.print(month());
  Serial.print("-");
  Serial.print(year()); 
  Serial.println(); 
  
  Serial.print(F("sun.dZenithAngle: "));
  Serial.print(sun.dZenithAngle);
  Serial.print(F("  sun.dAzimuth:  "));
  Serial.print(sun.dAzimuth-180); 
  Serial.print(F("  sun.elevation:  "));
  Serial.print(altura_solar); 
  Serial.println(); 
  
  Serial.print(F("Irradiance: "));
  Serial.print(irradiancia);
  Serial.print(F("  irradianceValue:  "));
  Serial.print(irradianciaValue); 
  Serial.print(F("  voltage:  "));
  Serial.println(voltage);

  
  if (pozo){Serial.print(F("Water well sensor OK"));}else{Serial.print(F("No water in well!")); }
  if (deposito){Serial.print(F("  Water tank sensor OK"));}else{Serial.print(F("Water Tank full!")); }
  Serial.println(); 
   
  Serial.print(F("Manual:  "));
  Serial.print(manual);
  Serial.print(F("  Rele.status:  "));
  Serial.print(releStatus);
  if (releStatus){Serial.print(F("  PUMP ON  "));}else{Serial.print(F("  FRONIUS ON  ")); }
  Serial.println(); 
  
  Serial.print(F("Irradiance threshold: "));
  Serial.print(irradiancia_limite);
  Serial.print(F("  sun.elevation.angle threshold:  "));
  Serial.print(altura_solar_limite); 
  Serial.println();   
  
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/////////////////////////SERIAL READ COMMANDS //////////////////////////////////////////////////
/*  code to process time sync messages from the serial port   */

void procesaSerie(){
   char buf = '\0';
   
   buf = Serial.read();
    if (buf == 't' || buf == 'T'){
      while(Serial.available())
         buf = Serial.read();
          Serial.println( "________> TIME AND DATE SETTING <_______________" );
          Serial.println(F( " FORMAT  YYYY,MM,DD,HH,DD,SS,   DO NOT FORGET THE LAST ','" ));
          Serial.println(F( "Set the date and time by entering the current date/time in" ));
          Serial.println(F( "YYYY,MM,DD,HH,DD,SS," ));
          Serial.println(F("example: 2000,12,31,23,59,59, would be DEC 31st, 2000  11:59:59 pm") );
          
      // while(Serial.available())
       //  buf = Serial.read();
       setTimeFunction();
    }
  if (buf == 'g' || buf == 'G'){
      while(Serial.available())
         buf = Serial.read();
          Serial.println( F("_____________> IRRADIANCE THRESHOLD LEVEL <________________" ));
          Serial.println( F("Insert the new irradiance threshold value, in W/m2" ));
          Serial.println( F("format example , 234.45" ));
          
          setG();
    }
    if (buf == 'a' || buf == 'A'){
      while(Serial.available())
         buf = Serial.read();
          Serial.println( F("_____________> Solar height THRESHOLD LEVEL <________________" ));
          Serial.println( F("Insert the new solar height threshold value, in degress" ));
          Serial.println( F("format example , 20.45" ));
          
          setA();
    }

    if (buf == 'm' || buf == 'M'){
      while(Serial.available())
         buf = Serial.read();
          Serial.println( F("___________> MANUAL CONTROL <_______________________" ));
          Serial.println( F("insert P for pump or F for fronius, A for automatic run" ));
          Serial.println( F("format example , B, should put pump on" ));
          setM();
    }
    
}

void setTimeFunction(){
   delayTime = millis() + 45000UL;
  while (delayTime >= millis() && !Serial.available()) {
    delay(10);
  }
  if (Serial.available()) {
        //note that the tmElements_t Year member is an offset from 1970,
        //but the RTC wants the last two digits of the calendar year.
        //use the convenience macros from Time.h to do the conversions.
            int y = Serial.parseInt();
            tm.Year = CalendarYrToTm(y);
            tm.Month = Serial.parseInt();
            tm.Day = Serial.parseInt();
            tm.Hour = Serial.parseInt();
            tm.Minute = Serial.parseInt();
            tm.Second = Serial.parseInt();
            t = makeTime(tm);
     //use the time_t value to ensure correct weekday is set
            if (t != 0) {
                RTC.set(t);   // set the RTC and the system time to the received value
                setTime(t);          
              }
     else
       Serial.println(F("RTC set failed!") );

       Serial.println("RTC set OK!" );
       Serial.println(t);
       
       Serial.println(tm.Year);
       Serial.println(tm.Month);
       Serial.println(tm.Day);
       Serial.println(tm.Hour);
       Serial.println(tm.Minute);
       Serial.println(tm.Second);
            while (Serial.available() > 0) Serial.read();
  }
  else 
    Serial.println( F("timed out, please try again"));

    delay(1000);
}


void setG(){
    delayTime = millis() + 45000UL;
  while (delayTime >= millis() && !Serial.available()) {
    delay(10);
  }
  if (Serial.available()) {
            int y = Serial.parseInt();
             if (y>0 && y<1200) {
                irradiancia_limite=y;  
                EEPROM.put(0, irradiancia_limite);
                   }
     else
       Serial.println("G set failed!" );
       Serial.println("G set OK!" );
        while (Serial.available() > 0) Serial.read();
  }
  else 
    Serial.println( F("timed out"));
    delay(1000);
}
  
void setA(){
   delayTime = millis() + 45000UL;
  while (delayTime >= millis() && !Serial.available()) {
    delay(10);
  }
  if (Serial.available()) {
            int y = Serial.parseInt();
             if (y>0 && y<90) {
                altura_solar_limite=y;        
                EEPROM.put(5, altura_solar_limite);
              }
     else
       Serial.println("A set failed!" );
       Serial.println("A set OK!" );
        while (Serial.available() > 0) Serial.read();
  }
  else 
    Serial.println( F("timed out"));
    delay(1000);
}

  void setM(){
    char y = '\0';
    delayTime = millis() + 45000UL;
  while (delayTime >= millis() && !Serial.available()) {
    delay(10);
  }
  if (Serial.available()) {
            y = Serial.read();
             if (y=='p' || y == 'P') {
                Serial.println("pump ON!" );
                manual_bomba_cmd=1;       
              }
              if (y=='f' || y == 'F') {
                Serial.println("fronius inverter ON!" );
                manual_fronius_cmd=1;       
              }
              if (y=='a' || y == 'A') {
                Serial.println("Automatic running set!" );
                manual_fronius_cmd=0;  
                manual_bomba_cmd=0;     
              }
     
        while (Serial.available() > 0) Serial.read();
  }
  else 
    Serial.println( "timed out");
    delay(1000);
}



/////////////////////////////CONTROOOOOOOOOOLLLLLL///////////////////////////////////


void control (){
  
  manual_bomba = digitalRead(manual_bombaPin);
  manual_fronius = digitalRead(manual_froniusPin);
  pozo = digitalRead(pozoPin);
  deposito = digitalRead(depositoPin);

  if (manual_bomba || manual_fronius || manual_bomba_cmd || manual_fronius_cmd )
  {manual=1;}else{manual=0;}
  
  if (manual_bomba && manual_fronius){manual=0;}
  if (manual_bomba_cmd && manual_fronius_cmd){manual=0;}
  

  if (manual && (manual_bomba || manual_bomba_cmd)){
     digitalWrite(ledPin, HIGH);
     digitalWrite(relePin, LOW); //el rele va al reves
     releStatus=1;
  }

  
  if (manual && (manual_fronius || manual_fronius_cmd)){
      digitalWrite(ledPin, LOW);
      digitalWrite(relePin, HIGH); //el rele va al reves
      releStatus=0;
  }
  
  //if (altura_solar>altura_solar_limite || irradiancia>irradiancia_limite){
    if (altura_solar>altura_solar_limite && !manual ){
     digitalWrite(ledPin, HIGH);
     digitalWrite(relePin, LOW); //el rele va al reves
     releStatus=1; //BOMMBA on
    } 
    
    
    if (altura_solar<altura_solar_limite && !manual ){
      digitalWrite(ledPin, LOW);
      digitalWrite(relePin, HIGH);
      releStatus=0; // FRONIUS on
    }

    // se supone que hay un 1 cuando todo OK, cambiar si es al revés.
    if ((!pozo || !deposito) && !manual  ){
      digitalWrite(ledPin, LOW);
      digitalWrite(relePin, HIGH);
      releStatus=0; // FRONIUS on
    }
}

void lee_irradiancia(){
  total= total - readings[index];
  irradianciaValue = analogRead(irradianciaPin);
  readings[index] = irradianciaValue;
  total= total + readings[index];
  index = index + 1;  
   if (index >= numReadings)  {
          index = 0;   
          average = total / numReadings;     
          voltage = average * (Vref / 1023.0);
          irradiancia = (voltage*1000)/calib;     
    } 
 
}

///////////////DISPLAY

void display_nokia511(){
  
  //digitalWrite(backlightPin, HIGH);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  if (pozo){display.print("*");}else{display.print("!");}
  if (deposito){display.print("*");}else{display.print("!");}
  
  display.setCursor(30,0);
  displayHour(hour());
  displayDigits(minute());
  displayDigits(second());
 // display.println(); 
 
  display.setCursor(30,8);
  display.print(day());
  display.print("-");
  display.print(month());
  display.print("-");
  display.print(year()); 
  //display.println(); 

  display.print("A:");
  display.print(altura_solar);
  display.print(" Deg");
  display.println(); 
  display.print("G:");
  display.print(irradiancia);
  display.print(" W/m2");
  display.println();

  if (manual){display.print(F("Manual run"));}else{display.print(F("Automatic run")); }
  display.println();
  if (releStatus){display.print(F("PUMP ON"));}else{display.print(F("FRONIUS ON")); }
  display.println();
  
  display.display();
  
}

void displayDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  display.print(":");
  if(digits < 10)
    display.print('0');
  display.print(digits);
}
void displayHour(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
   if(digits < 10)
    display.print('0');
  display.print(digits);
}
