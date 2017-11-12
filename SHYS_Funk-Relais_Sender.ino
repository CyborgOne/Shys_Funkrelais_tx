// NRF24L01_Relais
// Sender

#include <SPI.h>
#include <RF24.h>
#include <UIPEthernet.h>

//--------------------------------------
// Configuration Start
//--------------------------------------
#define RF_CE_PIN 8
#define RF_CSN_PIN 9

uint8_t clientNummer = 0;                    // Client-ID 0-5
static const uint8_t relaisAnzahl =  2;      // Anzahl der Relais

// Netzwerk
unsigned char _mac[]  = {0xB2, 0xAB, 0x32, 0x56, 0xFE, 0x0D  };
unsigned char _ip[]   = { 192, 168, 1, 130 };
unsigned char _dns[]  = { 192, 168, 1, 15  };
unsigned char _gate[] = { 192, 168, 1, 15  };
unsigned char _mask[] = { 255, 255, 255, 0  };

// Webseiten/Parameter
char*      rawCmdParam      = (char*)malloc(sizeof(char)*10);

const int  MAX_BUFFER_LEN           = 80; // max characters in page name/parameter 
char       buffer[MAX_BUFFER_LEN+1]; // additional character for terminating null
//--------------------------------------
// Configuration End
//--------------------------------------

const __FlashStringHelper * htmlHeader;
const __FlashStringHelper * htmlHead;
const __FlashStringHelper * htmlFooter;

// Netzwerkdienste
EthernetServer HttpServer(80); 
EthernetClient interfaceClient;

static const uint8_t pipes[6] = {0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL};

char* relaisStatusIn = "";
int    relaisIdIn = -1;
boolean relaisStatus[relaisAnzahl];

RF24 radio(RF_CE_PIN, RF_CSN_PIN); 

void setup() {  
  Serial.begin(9600);
  
  radio.begin();
  delay(20);
  radio.setChannel(1);               // Funkkanal - Mögliche Werte: 0 - 127
  radio.setAutoAck(0);
  radio.setRetries(15, 15);    
  radio.setPALevel(RF24_PA_LOW);     // Sendestärke darf die gesetzlichen Vorgaben des jeweiligen Landes nicht überschreiten! 
                                     // RF24_PA_MIN=-18dBm, RF24_PA_LOW=-12dBm, RF24_PA_MED=-6dBM, and RF24_PA_HIGH=0dBm
  radio.openWritingPipe(pipes[clientNummer]);
  radio.openReadingPipe(1,pipes[0]); 
  radio.startListening();
  delay(20);  
  
  Serial.println("Initialisiere Netzwerk");
  
  // Netzwerk initialisieren
  Ethernet.begin(_mac, _ip, _dns, _gate, _mask);
  HttpServer.begin();
  Serial.print( F("IP: ") );
  Serial.println(Ethernet.localIP());
  
  initStrings();
}
 
 
void loop() {
  EthernetClient client = HttpServer.available();
  if (client) {
    while (client.connected()) {
      if(client.available()){        
        Serial.println(F("Website anzeigen"));
        showWebsite(client);
        
        delay(100);
        client.stop();
      }
    }
  }
  delay(100);
  
  // Gecachte URL-Parameter leeren
  memset(rawCmdParam,0, sizeof(rawCmdParam));
}



void switchRelais(uint8_t relaisId, boolean relaisStatus){
  Serial.println("-------------------");
  Serial.print("SCHALTE: ");
  Serial.print(relaisId);
  Serial.print(" ");
  Serial.println(relaisStatus?"EIN":"AUS");
  Serial.println("-------------------");
  long message[2] = {relaisId, relaisStatus?1:0}; 
  radio.stopListening(); 
  radio.write(&message, sizeof(message));
  radio.startListening();
  delay(2000);
}



// ---------------------------------------
//     Webserver Hilfsmethoden
// ---------------------------------------
/**
 * Auswerten der URL Parameter
 */
void pruefeURLParameter(char* tmpName, char* value){
  if(strcmp(tmpName, "relaisId")==0 && strcmp(value, "")!=0){
    strcpy(rawCmdParam, value);
    relaisIdIn = atoi(rawCmdParam);
  }  
  if(strcmp(tmpName, "relaisStatus")==0 && strcmp(value, "")!=0){
    strcpy(rawCmdParam, value);
    relaisStatusIn = rawCmdParam;
  }  
}


/**
 *  URL auswerten und entsprechende Seite aufrufen
 */
void showWebsite(EthernetClient client){
  char * HttpFrame =  readFromClient(client);
  
 // delay(200);
  boolean pageFound = false;
  
  char *ptr = strstr(HttpFrame, "favicon.ico");
  if(ptr){
    pageFound = true;
  }
  ptr = strstr(HttpFrame, "rawCmd");
  if(!pageFound && ptr){
    runRawCmdWebpage(client, HttpFrame);
    pageFound = true;
  } 

  delay(200);

  ptr=NULL;
  HttpFrame=NULL;

 if(!pageFound){
    runRawCmdWebpage(client, HttpFrame);
  }
  delay(20);
}




// ---------------------------------------
//     Webseiten
// ---------------------------------------
/**
 * Startseite anzeigen
 */
void  runIndexWebpage(EthernetClient client){
  showHead(client);

  client.print(F("<h4>Navigation</h4><br/>"
    "<a href='/rawCmd'>Manuelle Schaltung</a><br>"));

  showFooter(client);
}


/**
 * rawCmd anzeigen
 */
void  runRawCmdWebpage(EthernetClient client, char* HttpFrame){
  if (relaisIdIn>=0 && strlen(relaisStatusIn)>=10) {
    switchRelais(relaisIdIn, strcmp(relaisStatusIn,"Einschalten") == 0);
  }
  delay(100);
  showHead(client);
  
  client.println(F(  "<form  method='GET' action='/rawCmd'>"));
  client.print   (F( "<table><tr>"));

  client.print   (F( "<td><b>Relais-Nr: (Max: "));
  client.print   ((relaisAnzahl-1));
  client.print   (F( ")</b></td> <td>" 
                     "<input type='input' name='relaisId' value='"));
  if(relaisIdIn>=0 && relaisIdIn<relaisAnzahl){
    client.print   (relaisIdIn);
  }
  client.println (F( "' maxlength='4' size='4'></td>"));

  client.print   (F( "</tr><tr>"));
                    
  client.println (F( "<td colspan=2><br/><input type='submit'  name='relaisStatus' value='Einschalten'/><input type='submit'  name='relaisStatus' value='Ausschalten'/></td>"));

  client.print   (F( "</tr></table></form>"));

  showFooter(client);
}





// ---------------------------------------
//     HTML-Hilfsmethoden
// ---------------------------------------

void showHead(EthernetClient client){
  client.println(htmlHeader);
  client.print("IP: ");
  client.println(Ethernet.localIP());
  client.println(htmlHead);
}


void showFooter(EthernetClient client){
  client.print(htmlFooter);
}


void initStrings(){
  htmlHeader = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
  
  htmlHead = F("<html><head>"
    "<title>SmartHome yourself - Relais-Switch Sender</title>"
    "<style type=\"text/css\">"
    "body{font-family:sans-serif}"
    "*{font-size:14pt}"
    "a{color:#abfb9c;}"
    "</style>"
    "</head><body text=\"white\" bgcolor=\"#494979\">"
    "<center>"
    "<hr><h2>SmartHome yourself - Relais-Switch Sender</h2><hr>") ;
    
    htmlFooter = F( "</center>"
    "<a  style=\"position: absolute;left: 30px; bottom: 20px; \"  href=\"/\">Zurueck zum Hauptmenue;</a>"
    "</body></html>");   
}



// ---------------------------------------
//     Ethernet - Hilfsmethoden
// ---------------------------------------
/**
 * Zum auswerten der URL des ÃƒÂ¼bergebenen Clients
 * (implementiert um angeforderte URL am lokalen Webserver zu parsen)
 */
char* readFromClient(EthernetClient client){
  char paramName[20];
  char paramValue[20];
  char pageName[20];
  
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        memset(buffer,0, sizeof(buffer)); // clear the buffer

        client.find("/");
        
        if(byte bytesReceived = client.readBytesUntil(' ', buffer, sizeof(buffer))){ 
          buffer[bytesReceived] = '\0';

          Serial.print(F("URL: "));
          Serial.println(buffer);
          
          if(strcmp(buffer, "favicon.ico\0")){
            char* paramsTmp = strtok(buffer, " ?=&/\r\n");
            int cnt = 0;
            
            while (paramsTmp) {
            
              switch (cnt) {
                case 0:
                  strcpy(pageName, paramsTmp);

                  Serial.print(F("Domain: "));
                  Serial.println(buffer);

                  break;
                case 1:
                  strcpy(paramName, paramsTmp);
                
                  Serial.print(F("Parameter: "));
                  Serial.print(paramName);

                  break;
                case 2:
                  strcpy(paramValue, paramsTmp);

                  Serial.print(F(" = "));
                  Serial.println(paramValue);
                  
                  pruefeURLParameter(paramName, paramValue);
                  break;
              }
              
              paramsTmp = strtok(NULL, " ?=&/\r\n");
              cnt=cnt==0?1:cnt==1?2:1;
            }
          }
        }
      }// end if Client available
      break;
    }// end while Client connected
  } 

  return buffer;
}

