#include "jsonStruct.h"

#include <SmcfJsonDecoder.h>

#include <WebMVC.h>

#include <Dhcp.h>
#include <Dns.h>
#include <ethernet_comp.h>
#include <UIPClient.h>
#include <UIPEthernet.h>
#include <UIPServer.h>
#include <UIPUdp.h>

 /**
 * Hardware:
 * 1 Arduino Nano (or compatible)
 * 1 ENC28J60 LAN Ethernet Network Board Module
 * 1 LED RGB
 * 1 resistor 100 ohm
 * 
 * Pins Connection:
 * Arduino        Ethernet Module
 *  pin              pin
 *  D13 (SCK)  ----  SCK (Atenção: não confunda com CLK!)
 *  D12 (MISO) ----  SO 
 *  D11 (MOSI) ----  ST (SI)
 *  D10 (SS)   ----  CS
 *  5V         ----  5V
 *  GND        ----  GND
 *  
 * Arduino      LED
 *  pin         pin
 *  D3   ----   Red
 *  D5   ----   Green
 *  D9   ----   Blue
 *  GND  ----   Common
 *  
 *  
 */

// data declaration for ethernet
const char VIEW_HOME_PAGE[] PROGMEM = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
 "<meta charset=\"utf-8\"/>\n"
 "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"/>\n"
 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>\n"
 "<title>Hello World</title>\n"
"</head>\n"
"<body>\n"
"<div class=\"content\">\n"
  "<h1>Hello World!</h1>\n"
  "<p>Welcome to the web world.</p>\n"
"</div>\n"
"</body>\n"
"</html>\n";

// Web Controllers
RedirectToViewCtrl redirectToHTMLCtrl(CONTENT_TYPE_HTML);

class RgbStateChangeCtrl: public WebController {
public:
  void execute(WebDispatcher &webDispatcher, WebRequest &request);
} rgbStateChangeCtrl;


// Web Routes
const char RESOURCE_HOME_PAGE[] PROGMEM = "/ ";
const char RESOURCE_API_STATE[] PROGMEM = "/api/state ";

#define NUM_ROUTES 2
const WebRoute routes[ NUM_ROUTES ] PROGMEM = {
  {RESOURCE_API_STATE,&rgbStateChangeCtrl,NULL},
  {RESOURCE_HOME_PAGE,&redirectToHTMLCtrl,VIEW_HOME_PAGE}
};

// start the server on port 80
EthernetServer server = EthernetServer(80);

//=====
unsigned long lastMillis;


// the setup routine runs once when you press reset:
void setup() {

  for (int i=0; i<3; i++) {
    pinMode(led[i].pin, OUTPUT);
  }

  uint8_t mac[6];

  Serial.begin(9600);

  //testJson();
  
  mac[0]=0x22; // Warning: the last two bits of first mac octet must be '10'. See https://en.wikipedia.org/wiki/MAC_address
  mac[1]=0x00;mac[2]=0x00;mac[3]=0xBC;mac[4]=0x00;mac[5]=0x32;
  
  IPAddress myIP(192,168,1,23);
  if (Ethernet.begin(mac)==0) {
    Serial.println(F("DHCP Fail"));
    Ethernet.begin(mac, myIP);
  }
  // print IP
  Serial.print(F("Local IP:"));
  myIP = Ethernet.localIP();
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(myIP[thisByte], DEC);
    Serial.print('.');
  }
  Serial.println();  
  // Server begin
  server.begin();  
  Serial.println("l2!");

  lastMillis = 0;
}

// the loop routine runs over and over again forever:
void loop() {
  updateRgbState();
  processWebRequests();
} // loop()


void updateRgbState() {
  /*  
   *  Essa função varia o brilho do LED entre <min_brightness> e <max_brightness>. 
   *  Os incrementos são definidos por <fadeAmount>. 
   *  O tempo entre os incrementos é definido por <max_delayCount> (em milisegundos).
   *  Cada cor do RGB tem definições independentes.
  */
  unsigned long currentMillis=millis();
  if (lastMillis!=currentMillis) { 
    // executa somente a cada 1 milisegundo!
    lastMillis=currentMillis;
    
    for (int i=0;i<3;i++) {
      ColorLed *l = &led[i];
      if (l->actual_delayCount==0) { 
        l->actual_delayCount = l->setup.max_delayCount;
       
        // set the brightness of pin:
        analogWrite(l->pin, l->actual_brightness);
      
        // change the brightness for next time through the loop:
        l->actual_brightness = l->actual_brightness + l->setup.fadeAmount;
      
        // reverse the direction of the fading at the ends of the fade:
        if (l->actual_brightness <= l->setup.min_brightness) {
           l->setup.fadeAmount= abs(l->setup.fadeAmount);
        }
        if (l->actual_brightness >= l->setup.max_brightness) {
          l->setup.fadeAmount= -abs(l->setup.fadeAmount);
        }
      }
      l->actual_delayCount--;
    } // for  
  
  } // a cada milisegundo...
}

void processWebRequests() {
  /* Crio o WebDispatcher local a este funcao para uso racional de memoria. Assim, todo o 
        MVC eh criado na memoria da pilha e liberado ao final desta funcao. */
  WebDispatcher webDispatcher(server);
  webDispatcher.setRoutes(routes,NUM_ROUTES);
  webDispatcher.process();
} // end processWebRequests()



void RgbStateChangeCtrl::execute(WebDispatcher &webDispatcher, WebRequest &request) {
#ifdef DEBUGGING
  Serial.println(F("StCtrl"));
#endif
  int err=0;
  if (request.method==METHOD_POST) {
    char jsonStr[150];
    if (webDispatcher.getNextLine(request.client,jsonStr,150)) {
      #ifdef DEBUGGING
        Serial.print(F("Processing relays: ")); Serial.println(jsonStr);
      #endif

      err=chgLedSetupFromJson(jsonStr);
      
    } else {
      err=3;//todo: corrigir codigo de erro
    }
  } // if POST
  sendJsonViaWeb(webDispatcher, request,err,printJsonRgbStatus);
}

void sendJsonViaWeb(WebDispatcher &webDispatcher, WebRequest &request, uint8_t err, void (*sendJson_fn) (EthernetClient&)) {
  request.response.contentType_P=CONTENT_TYPE_JSON;
  if (err) {
    request.response.httpStatus=RC_BAD_REQ;
  } else {
    request.response.httpStatus=RC_OK;
  }
  webDispatcher.sendHeader(request);
  sendJson_fn(request.client);
}

/**
 * Returns a JSON with the current values of the relays to the client. The
 * JSON will look like: {"r":[0,0,0,0,0,0,0,0]}
 * This one means all releays are turned off.
 */
void printJsonRgbStatus(EthernetClient &client) {
  #ifdef DEBUGGING
    Serial.print(F("stat..."));
  #endif
  client.print(F("{"));
  char sigla[]= {'r','g','b'};
  for (int i=0; i<3; i++) {
    if (i>0) {
      client.print(',');
    }
    client.print('\"');client.print(sigla[i]);client.print(F("\":{"));
    RgbSetup setup= led[i].setup;
    client.print(F("\"min\":"));   client.print(setup.min_brightness);
    client.print(F(",\"max\":"));   client.print(setup.max_brightness);
    client.print(F(",\"fade\":"));  client.print(setup.fadeAmount);
    client.print(F(",\"delay\":")); client.print(setup.max_delayCount);
    client.print('}');
  }
  client.println(F("}\n"));
}


