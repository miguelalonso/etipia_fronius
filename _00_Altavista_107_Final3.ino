/*
  Arduino con ethernet shield para control del aire acondicionado del salon
  Miguel Alonso  Octubre 2014
  Fuentes: varios de internet
  - CONNECTIONS: nRF24L01 Modules See:
 http://arduino-info.wikispaces.com/Nrf24L01-2.4GHz-HowTo
   1 - GND
   2 - VCC 3.3V !!! NOT 5V
   3 - CE to Arduino pin 9 ->6
   4 - CSN to Arduino pin 10 ->7
   5 - SCK to Arduino pin 13
   6 - MOSI to Arduino pin 11
   7 - MISO to Arduino pin 12
   8 - UNUSED
 */
 //https://api.xively.com/v2/feeds/130137.csv?key=WQ5gyvvWkWJ5oNLO2G7aOEb8Ex2EeoCWliPU4SjJB68ePyDO
 

#include <SPI.h>
#include <Ethernet.h>
#include <nRF24L01.h>
#include <RF24.h>

byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 177);
EthernetServer server(82);
EthernetClient client_emon;
IPAddress server_emon(163,117,157,189); //ordenador jaen-espectros2
const int  feedID          =    130137; // datos Xively
const int  streamCount     =      4; // Number of data streams to get
EthernetClient client_xiv;
char server_xiv[] = "api.xively.com"; 
int streamData[streamCount];    // change float to long if needed for your data
unsigned long lastConnectionTime_emon = 0;             // last time you connected to the server, in milliseconds
boolean lastConnected_emon = false;                    // state of the connection last time through the main loop
const unsigned long postingInterval_emon = 30000; // delay between updates, in milliseconds
unsigned long lastConnectionTime_xiv = 0;             // last time you connected to the server, in milliseconds
boolean lastConnected_xiv = false;                    // state of the connection last time through the main loop
const unsigned long postingInterval_xiv = 60000; // delay between updates, in milliseconds
float Pot_INV_auto; //potencia del inversor conectado a red Altavista 107 
float Pot_INV_red; //potencia del inversor conectado a red Altavista 107  
String cadena;
#define CE_PIN   6
#define CSN_PIN 7
const uint64_t pipes[2] = { 0xE8E8F0F0E1LL, 0xE8E8F0F0E2LL }; //pipes para RF24
RF24 radio(CE_PIN, CSN_PIN); // Create a Radio
float datos_NRF[5];  // 5 element array holding NRF24L01 readings
int led = 3;
int rele5 = 5;
bool estado_sensor=0;
bool manual=0;
float umbral_PAC=600; //umbral de potencia para arranque del aire acondicionado
int j;
 
void setup() {
  Serial.begin(9600);
  pinMode(led, OUTPUT);
  pinMode(rele5, OUTPUT);
  digitalWrite(led, HIGH);
  delay(5000);
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print(F("server IP "));
  Serial.println(Ethernet.localIP());
  radio.begin();
  radio.openReadingPipe(1,pipes[0]);
  radio.openReadingPipe(2,pipes[1]);
  radio.startListening();
  delay (2000);
  digitalWrite(led, LOW);
}


void loop() {
 // 0 - Primero lee los datos NRF RF24   
 uint8_t pipe;
  if ( radio.available(&pipe) )
  {
    if(pipe==1){
    // Read the data payload until we've received everything
    bool done = false;
    while (!done)
    {
      // Fetch the data payload
      done = radio.read( datos_NRF, sizeof(datos_NRF) );
      
      Serial.print("--------->Sensor: ");
      Serial.println(pipe);
      Serial.print("realPower = ");
      Serial.println(datos_NRF[0]);
      Serial.print(" apparentPower = ");      
      Serial.println(datos_NRF[1]);
      Serial.print(" powerFActor = ");      
      Serial.println(datos_NRF[2]);
      Serial.print(" supplyVoltage = ");      
      Serial.println(datos_NRF[3]);
      Serial.print(" Irms = ");      
      Serial.println(datos_NRF[4]);
      
    }
    }
    else{
      //analizar los datos de pipe==2;
      }
  }
  else
  {    
      //Serial.println("No radio available");
  }
  
  // fin de datos NRF
  
   // 1. listen for incoming Ethernet connections:
 listenForEthernetClients();
  //2. client Xively
  Xively2();
  //3. Datos a emoncms en leganes
   envio_emon();
   //fin envio emoncms
   
   if (!manual){
   if (estado_sensor==0 && Pot_INV_auto>=umbral_PAC+50){
             digitalWrite(led, HIGH);
             digitalWrite(rele5, HIGH);  
             estado_sensor=1;
            //   Aire_ON();
     }
     if (estado_sensor==1 && Pot_INV_auto<umbral_PAC-50){
             digitalWrite(led, LOW);
             digitalWrite(rele5, LOW);    
             estado_sensor=0;
             //  Aire_OFF();
     }
     }
  
}
/// fin de loop
void Xively2(void){
  
    if( (millis() - lastConnectionTime_xiv > postingInterval_xiv) ) {
    lastConnectionTime_xiv=millis();
      if( getFeed(feedID, streamCount) == true)
   {
      for(int id = 0; id < streamCount; id++){
        Serial.println( streamData[id]);
      }
     // Serial.println("--");
   }
   else
   {
      Serial.println(F("Unable feed"));
    }
    Pot_INV_red=streamData[1];
    Pot_INV_auto=streamData[3];
}
  lastConnected_xiv = client_xiv.connected();
}

// returns true if able to connect and get data for all requested streams
boolean getFeed(int feedId, int streamCount )
{
boolean result = false;
  if (client_xiv.connect(server_xiv, 80)>0) {
    client_xiv.print(F("GET /v2/feeds/"));
    client_xiv.print(130137);
    client_xiv.print(F(".csv HTTP/1.1\r\nHost: api.pachube.com\r\nX-PachubeApiKey: "));
    client_xiv.print(F("WQ5gyvvWkWJ5oNLO2G7aOEb8Ex2EeoCWliPU4SjJB68ePyDO"));
    client_xiv.print(F("\r\nUser-Agent: Arduino 1.0"));
    client_xiv.println(F("\r\n"));
  }
  else {
    Serial.println(F("failed"));
  }
  if (client_xiv.connected()) {
    Serial.println(F("Con"));
    if(  client_xiv.find("HTTP/1.1") && client_xiv.find("200 OK") )
       result = processCSVFeed(streamCount);
    else
       Serial.println(F("no 200 OK"));
  }
  else {
    Serial.println(F("Disc."));
  }
  client_xiv.stop();
  client_xiv.flush();
  return result;
}

int processCSVFeed(int streamCount)
{
  int processed = 0;
  client_xiv.find("\r\n\r\n"); // find the blank line indicating start of data
  for(int id = 0; id < streamCount; id++)
  {
    int id = client_xiv.parseFloat(); // you can use this to select a specific id
    client_xiv.find("Z,"); // skip past timestamp
    streamData[id] = client_xiv.parseFloat();
    processed++;      
  }
  return(processed == streamCount );  // return true if got all data
}


//===============================

void envio_emon(void){
  if (client_emon.available()) {
    char c_emon = client_emon.read();
    //Serial.print(c_emon);
    }
  if (!client_emon.connected() && lastConnected_emon) {
    Serial.println();
    //Serial.println("Disconnecting emoncms...");
    client_emon.stop();
  }
    if(!client_emon.connected() && (millis() - lastConnectionTime_emon > postingInterval_emon) ) {
    sendData_emon();
    }
  lastConnected_emon = client_emon.connected();
}

//envio de datos a emoncms
void sendData_emon(void){
  // if there's a successful connection:
  if (client_emon.connect(server_emon, 80)) {
    //Serial.println("Connecting emoncms...");
    // send the HTTP PUT request:
    client_emon.print(F("GET /emoncms/input/post.json?apikey=d3779ca1d8b1ae37c17b60012f585119&node=107&json={Pot_INV_red"));
    client_emon.print(F(":"));
    client_emon.print(Pot_INV_red);
    client_emon.print(F(",Pot_INV_auto:")); 
    client_emon.println(Pot_INV_auto);   
    client_emon.println(F("} HTTP/1.1"));
    //client_emon.println(" HTTP/1.1");
    //client_emon.println("Host: 163.117.157.189");
    //client_emon.println("User-Agent: Arduino-ethernet");
    client_emon.println(F("Connection: close"));
    client_emon.println();

    // note the time that the connection was made:
    lastConnectionTime_emon = millis();
  } 
  else {
    // if you couldn't make a connection:
    Serial.println(F("Fallo"));
//    Serial.println("Disconnecting emoncms...");
    client_emon.stop();
    delay(100);
  }
}
  
  
  //
  // web server
void listenForEthernetClients() {
   // listen for incoming clients
  EthernetClient client = server.available();
 client = server.available();
  if (client) {
    boolean currentLineIsBlank = true;
    while (client.connected()) {   
      if (client.available()) {
        char c = client.read();
    // Serial.print(c);
        //read char by char HTTP request//store characters to string
        if (cadena.length() < 50) {
          cadena += c;
          }

         //if HTTP request has ended
         if (c == '\n'&& currentLineIsBlank) {          
           //Serial.println(readString); //print to serial monitor for debuging
     
           client.println("HTTP/1.1 200 OK"); //send new page
           client.println("Content-Type: text/html");
            client.println("Refresh: 20");  // refresh the page automatically every 5 sec
           client.println();     
           client.println("<HTML>");
           client.println("<HEAD>");
           //client.println(F("<meta name='apple-mobile-web-app-capable' content='yes' />"));
           //client.println(F("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />"));
           client.println(F("<link rel='stylesheet' type='text/css' href='http://randomnerdtutorials.com/ethernetcss.css' />"));
           client.println(F("<TITLE>WebServer Arduino - Miguel Alonso</TITLE>"));
           client.println(F("</HEAD>"));
           client.println(F("<BODY>"));
           client.println(F("<H1>WebServer Arduino - Miguel Alonso</H1>"));
           client.println(F("<hr />"));
           client.println(F("<a href=\"/?button1on\"\">Aire ON</a>"));
           client.println(F("<a href=\"/?button1off\"\">Aire OFF</a><br />"));   
           client.println(F("<br />")); 
           client.println(F("<a href=\"/?manual\"\">Manual</a>"));
           client.println(F("<a href=\"/?auto\"\">Automat.</a><br />")); 
           client.println(F("<H2>"));
           client.println(F("Aire: ")); 
           client.println(estado_sensor);
           client.println(F("Manual: ")); 
           client.println(manual); 
           client.println(F("</H2>"));
           client.println(F("<FORM>Umbral arranque aire Acond. <input type=text name=maxwatts size=4 value="));
           client.println(umbral_PAC);
           client.println(F(" > <input type=submit value=Enter> </form> <br/><br/>"));
           // output the value of each analog input pin
          for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
            int sensorReading = analogRead(analogChannel);
            client.print(F("An input "));
            client.print(analogChannel);
            client.print(F(" is "));
            client.print(sensorReading);
            client.println(F("<br />"));       
          }
          // output the values read from remote sensor
          
          for (int k = 0; k < sizeof(datos_NRF); k++) {
            client.print(F("Datos remotos:"));
            client.print(k);
            client.print(" is ");
            client.print(datos_NRF[k]);
            client.println("<br />");       
          }
         
           
           client.print(F("Datos Xively:"));
           client.println(Pot_INV_red);
           client.println(Pot_INV_auto);
           client.println(millis());
           client.println(F("</BODY>"));
           client.println(F("</HTML>"));
          //controls the Arduino if you press the buttons
            
           break;
         }
         if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
        } 
       }
           delay(1);
           //stopping client
           client.stop();
           Ejecuta_webserver_actions();
          
            //clearing string for next read
            cadena=""; 
    }
}
 
  
  void Ejecuta_webserver_actions( ){
    
    //controls the Arduino if you press the buttons
           if (cadena.indexOf(F("?button1on")) >0){
               digitalWrite(led, HIGH);
               digitalWrite(rele5, HIGH);
                estado_sensor=1;
                //Aire_ON();
                Serial.println("SensorON");
           }
           if (cadena.indexOf(F("?button1off")) >0){
               digitalWrite(led, LOW);
               digitalWrite(rele5, LOW);
               estado_sensor=0;
               //Aire_OFF();
               Serial.println("SensorOFF");
           }
           if (cadena.indexOf(F("?manual")) >0){
              manual=1;
           }
           if (cadena.indexOf(F("?auto")) >0){
                 manual=0;
           }
           if (cadena.indexOf(F("?maxwatts=")) >0){
                int colonPosition = cadena.indexOf('=');
                int colonPosition2 = cadena.indexOf('HTTP');
               String valor = cadena.substring(colonPosition+1, colonPosition2);
               //char buf[valor.length()];
              //valor.toCharArray(buf,valor.length()+1);
               //umbral_PAC=atof(buf);
               umbral_PAC = double(valor.toFloat());
           }
  
  }
 
 
