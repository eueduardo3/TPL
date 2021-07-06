//REMETENTE
#include <Arduino.h>


//FUNCOES E VARIAVEIS ESPNOW-------------------------------------------------------------
#include <esp_now.h>
#include <WiFi.h>
#include <esp_task_wdt.h> //Biblioteca do watchdog

//FUNÇOES DEBOUNCE, SERVE TANTO PARA BOTÃO QUE ESCOLHE MODO, COMO TAMBÉM PARA SEITURA DO SENSOR
#include "Bounce2.h"
Bounce debouncerA = Bounce();
RTC_DATA_ATTR bool anterior = true;
#define  botao 27

Bounce debouncer2 = Bounce();
RTC_DATA_ATTR bool anteriorSensor = true;
#define  SENSOR 4

//Armazena o valor (tempo) da ultima vez que executou a açao
unsigned long previousMillisFalhaEnvio = 0;
//Intervalo de tempo entre acionamentos
const long intervaloFalhaEnvio = 30000;


//Canal usado para conexão
#define CHANNEL 1

//Se MASTER estiver definido
//o compilador irá compilar o Master.ino
//Se quiser compilar o Slave.ino remova ou
//comente a linha abaixoa
//
//#define EXTERNO //*********************************************** LEO É NESSA LINHA QUE MUDA PARA O INTERNO OU EXTERNO
// Variáveis Globais ------------------------------------
char              id[30];       // Identificação do dispositivo
boolean           ledOn;        // Estado do LED
word              bootCount;    // Número de inicializações
char              EndMacA[30];     // Rede WiFi
char              EndMacB[30];       // Senha da Rede WiFi
int               NumeroNo;  //Indica se placa modulo A irá se conectar
boolean           moduloB;  //Indica se placa modulo A irá se conectar
boolean           moduloC;  //Indica se placa modulo A irá se conectar
uint8_t           UintEndMacA[] = {0xFF, 0xFF, 0xFF, 0XFF, 0XFF, 0X11}; // ENDERECO MAC PARA DISPOSITIVO QUE SERÁ PAREADO
uint8_t values[] = {0, 0, 0, 0};
//-----------------
long Tempo_anteriorBat = 0;
long intervaloBat = 60000;
bool ContrarioledState;
//Mac Address do peer para o qual iremos enviar os dados
//uint8_t peerMacAddress[] = {0XA4, 0xCF, 0x12, 0X99, 0XF2, 0XAC};
//A4:CF:12:9A:5D:5C
//10:52:1C:67:20:40
RTC_DATA_ATTR uint8_t peerMacAddress[] = {0X10, 0x52, 0x1C, 0X67, 0X20, 0X49};
bool autorizaEnviar = false;
bool enviado = false;
bool loopVolatil = true;
int contadorSucesso = 0;
bool autorizaDormir = false;

//sensor------------------------------
RTC_DATA_ATTR bool stable; // Guarda o último estado estável do botão;

RTC_DATA_ATTR bool primeiroLoop = true;
//------------------------------------


bool mudouEstado = false;
int ContNadaMudou = 0;
//----------------------
//Variaveis do botao que seta os modos ConfiguraWEB ou ESPNOW=============================================
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  2        /* Time ESP32 will go to sleep (in seconds) */
//RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool MODO = false;

int saida = 19;

//=============================================
//VARIAVEIS PARA PISCAR LED E AVISAR MODO QUE ESTÁ
int ledState = LOW;
unsigned long previousMillis = 0;
const long interval = 500;

//----------------------------------------------------------
#include "driver/adc.h"
//#include <esp_wifi.h>
//#include <esp_bt.h>

//-------------------------------------------------------------------------------------------------------

void goToDeepSleep()
{

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  adc_power_off();
  //  esp_wifi_stop();
  // esp_bt_controller_disable();
  delay(50);

}
void dorme() {
  
    goToDeepSleep();

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, ContrarioledState);
    delay(10);
    esp_deep_sleep_start();
    delay(2000);
  
}
//Estrutura com informações
//sobre o próximo peer
esp_now_peer_info_t peer;

//Função para inicializar o modo station
void modeStation() {
  //Colocamos o ESP em modo station
  WiFi.mode(WIFI_STA);
  //Mostramos no Monitor Serial o Mac Address
  //deste ESP quando em modo station
  Serial.print("Mac Address in Station: ");
  Serial.println(WiFi.macAddress());
}

//Função de inicialização do ESPNow
void InitESPNow() {
  //Se a inicialização foi bem sucedida
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  //Se houve erro na inicialização
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

//Função que adiciona um novo peer
//através de seu endereço MAC
void addPeer(uint8_t *peerMacAddress) {
  //Informamos o canal
  peer.channel = CHANNEL;
  //0 para não usar criptografia ou 1 para usar
  peer.encrypt = 0;
  //Copia o endereço do array para a estrutura
  memcpy(peer.peer_addr, peerMacAddress, 6);
  //Adiciona o slave
  esp_now_add_peer(&peer);
}

//Função que irá enviar o valor para
//o peer que tenha o mac address especificado
/*void send(const uint8_t *values[4], uint8_t *peerMacAddress) {
  esp_err_t result = esp_now_send(peerMacAddress, values, sizeof(values));
  Serial.print("Send Status: ");
  //Se o envio foi bem sucedido
  if (result == ESP_OK) {
    Serial.println("Success");
  }
  //Se aconteceu algum erro no envio
  else {
    Serial.println("Error");
  }
  }*/

//Função que serve de callback para nos avisar
//sobre a situação do envio que fizemos
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println("Rodou funcao OnDataSent ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Envio realizado com sucesso");
    enviado = true;
    contadorSucesso ++ ;
  } else {
    enviado = false;
    Serial.println("FALHA no Envio ");

  }


}

void AtivaESPNOW() {
  //Chama a função que inicializa o modo station
  modeStation();

  //Chama a função que inicializa o ESPNow
  InitESPNow();

  //Adiciona o peer
  addPeer(peerMacAddress);

  //Registra o callback que nos informará
  //sobre o status do envio.
  //A função que será executada é onDataSent
  //e está declarada mais abaixo
  esp_now_register_send_cb(OnDataSent);
}
//Função responsável pela
//leitura do pino e envio
//do valor para o peer
void readAndSend() {
  //Lê o valor do pino
  //uint8_t value = digitalRead(PIN);

  //uint8_t value = stable;


  Serial.print("values[3]=NumeroNo ->   ");
  values[3] = NumeroNo;
  Serial.println(values[3]);
  values[NumeroNo] = !stable; // mando o estado invertido pois dessa forma o sensor balin pode ficar de cabeça para baixo, e só setará o mag quando as esferas tocarem os fios
  Serial.println("Valores dentro do array values: ");
  for (int i = 0; i < 4; i++) {

    Serial.print(values[i]);
    Serial.print(" ");
  } Serial.println("");

  //Envia o valor para o peer
  //send(&value, peerMacAddress);
  //send(&values, peerMacAddress);
  //esp_err_t result = esp_now_send(peerMacAddress, values, sizeof(values));
  esp_err_t result = esp_now_send(
                       peerMacAddress,
                       (uint8_t *)&values,
                       sizeof(values));

  Serial.print("Send Status: ");
  //Se o envio foi bem sucedido
  if (result == ESP_OK) {
    Serial.println("Success");
  }
  //Se aconteceu algum erro no envio
  else {
    Serial.println("Error");
  }

 Serial.println(); Serial.println();
  Serial.println("###########################################################");
}


void BotaoModoLoop() {
  debouncerA.update(); // Executa o algorítimo de tratamento para verificar debouce;
  debouncer2.update();

  int value = debouncerA.read(); // Lê o valor tratado do botão;
  int value2 = debouncer2.read();

  //trecho para verificar se boltão alterou estado
  if (value != anterior) { // Se alterou o estado anterior por um certo tempo determinado executa a função abaixo
    anterior=value; 
    Serial.println("Alterou o estado do botão que verifica modo de operação");
    if (value == HIGH) {
      Serial.println("Botao SOLTO! MODO operando");
      MODO = false;
    } else {
      Serial.println("Botao Pressionado! MODO configuração");
      MODO = true;
      }

    delay(500);
    //aplico o reinicia aqui--------
    Serial.println("Going to sleep now");
    delay(10);
    Serial.flush();
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
    Serial.println("This will never be printed");
  }

//trecho para verificar se SENSOR alterou estado
  if (value2 != anteriorSensor) { // Se alterou o estado anterior por um certo tempo determinado executa a função abaixo
    anteriorSensor=value2; 
    Serial.println("Alterou o estado SENSOR");
    mudouEstado = true;
    autorizaEnviar = true;
    } 
    else {
      mudouEstado = false;
    }


  //TRECHO PARA PISCAR LED E AVISAR MODO QUE ESTÁ
  unsigned long tempo_atual = millis();
  if (MODO) {
    if (tempo_atual - previousMillis >= interval) {
      // save the last time you blinked the LED
      previousMillis = tempo_atual;

      // if the LED is off turn it on and vice-versa:
      if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
      }

      // set the LED with the ledState of the variable:
      digitalWrite(saida, ledState);
    }

  }else{
    digitalWrite(saida, LOW);
  }

}


void funcoesESPNOWloop() {
  
  stable=anteriorSensor;

  if (autorizaEnviar || primeiroLoop) {
    Serial.println();
    Serial.println();
    Serial.print("status stable = ");
    Serial.println(stable);

    do {
      Serial.println("Excutando funcao readAndSend)");

      if (loopVolatil) {
        AtivaESPNOW();
        loopVolatil = false;
      }

      int cont = 0;
      while (cont <= 5) {
        readAndSend();
        cont++;
      }
      esp_task_wdt_feed(); // essa linha adicionei ela em dezembro de 2020 se o programa do espnow bugar
      
      BotaoModoLoop();//verifica se enquanto esta preso nesse while o botao para configurar o modo nao foi pressionado

      if (!enviado) {
        unsigned long currentMillis = millis();
        //Verifica se o intervalo já foi atingido
        if (currentMillis - previousMillisFalhaEnvio >= intervaloFalhaEnvio)
        {
         //Armazena o valor da ultima vez que o led foi aceso
          previousMillis = currentMillis;
          Serial.println();Serial.println();Serial.println();Serial.println();Serial.println();       
          Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% não consegiu enviar atem 30 segundos %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%");
          Serial.println();Serial.println();Serial.println();Serial.println();Serial.println(); 
          autorizaDormir=true;
          break;
 
        }
      }
    } while (enviado == false);


  }

  if (contadorSucesso >= 5) {
    autorizaEnviar = false;
    contadorSucesso = 0;

  }

  //Serial.print("status da variavel stable = ");
  //    Serial.println(stable);
  primeiroLoop = false;

  if (autorizaDormir) {

    WiFi.mode(WIFI_OFF);
    btStop();

    Serial.println("Funcao Autoriza Dormir");
    ContrarioledState = ! stable;
    Serial.print(" variavel ContrarioledState = ");
    Serial.println(ContrarioledState);

    Serial.println(); Serial.println();
    Serial.println("-------------------------------irá dormir AGORA-----------------------------------------------");
    Serial.println(); Serial.println();
    dorme();
  }

  //  esp_task_wdt_reset();//Reseta o temporizador do watchdog

  
  if (mudouEstado == false) {
    ContNadaMudou++;
    
    if (ContNadaMudou >= 30) {
      autorizaDormir = true;
      ContNadaMudou = 0;
    }
  }

  //trecho para impedir que a bateria acabe caso ele não conseguir enviar a msg de jeito nenhum
  //ESSE TRECHO ABAIXO NAO FUNCIONOU PQ O WHATCHDOG reseta ele antes de dar o 1 minuto
  unsigned long Tempo_atualBat = millis();
  if (enviado == false) {
    if (Tempo_atualBat - Tempo_anteriorBat > intervaloBat)
    {
      Tempo_anteriorBat = Tempo_atualBat;
      dorme();
    }
  }
}

void fucoesESPNOWsetup() {
  (*(volatile uint32_t *)(0x3FF4803C)) &= ~(1 << 7); //desabilita alarme por energia baixa
  setCpuFrequencyMhz(80);// seta frequencia da cpu , colocar valor baixo para economizar energia

  esp_task_wdt_init(4, true);
  esp_task_wdt_add(NULL);



  Serial.print("Estado atual da variavel RTC primeiroLoop = "); Serial.println(primeiroLoop);
  Serial.println("MASTER - EXTERNO");
  Serial.print("Data gravação FIRMWARE   ");
  Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);

  Serial.print(" stable no setup, ou seja o valor dela que estava guradado do RTC = "); Serial.println(stable);

  pinMode(SENSOR,INPUT_PULLUP);
  // After setting up the button, setup the Bounce instance :
  debouncer2.attach(SENSOR);
  debouncer2.interval(2000); // interval in ms

}
//==================================================================================================================

//Variaveis para configurar modo de operacao entre WEB ou ESPNOW--------------------
// Bibliotecas ------------------------------------------
#ifdef ESP8266
// Bibliotecas para ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#else
// Bibliotecas para ESP32

#include <WebServer.h>
//#include <LITTLEFS.h>
#include "SPIFFS.h"

#endif
#include <DNSServer.h>
#include <TimeLib.h>
#include <ArduinoJson.h>



// Constantes ---------------asdf----------------------------

#define FORMAT_LITTLEFS_IF_FAILED false

// Porta Servidor Web
const byte        WEBSERVER_PORT          = 80;

// Headers do Servidor Web
const char*       WEBSERVER_HEADER_KEYS[] = {"User-Agent"};

// Porta Servidor DNS
const byte        DNSSERVER_PORT          = 53;

// Pino do LED
#ifdef ESP32
// ESP32 não possui pino padrão de LED
const byte      LED_PIN                 = 22;
#else
// ESP8266 possui pino padrão de LED
const byte      LED_PIN                 = LED_BUILTIN;
#endif

// Estado do LED
#ifdef ESP8266
// ESP8266 utiliza o estado inverso
const byte      LED_ON                  = LOW;
const byte      LED_OFF                 = HIGH;
#else
// ESP32 utilizam estado normal
const byte      LED_ON                  = HIGH;
const byte      LED_OFF                 = LOW;
#endif

// Tamanho do Objeto JSON
const   size_t    JSON_SIZE               = JSON_OBJECT_SIZE(5) + 130;

// Instâncias -------------------------------------------
// Web Server
#ifdef ESP8266
// Classe WebServer para ESP8266
ESP8266WebServer  server(WEBSERVER_PORT);
#else
// Classe WebServer para ESP32
WebServer  server(WEBSERVER_PORT);
#endif

// DNS Server
DNSServer         dnsServer;




// Funções Genéricas ------------------------------------
unsigned int hexToDec(String hexString) {

  unsigned int decValue = 0;
  int nextInt;

  for (int i = 0; i < hexString.length(); i++) {

    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);

    decValue = (decValue * 16) + nextInt;
  }

  return decValue;
}


void log(String s) {
  // Gera log na Serial
  Serial.println(s);
}

String softwareStr() {
  // Retorna nome do software
  return String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1);
}

String longTimeStr(const time_t &t) {
  // Retorna segundos como "d:hh:mm:ss"
  String s = String(t / SECS_PER_DAY) + ':';
  if (hour(t) < 10) {
    s += '0';
  }
  s += String(hour(t)) + ':';
  if (minute(t) < 10) {
    s += '0';
  }
  s += String(minute(t)) + ':';
  if (second(t) < 10) {
    s += '0';
  }
  s += String(second(t));
  return s;
}

String hexStr(const unsigned long &h, const byte &l = 8) {
  // Retorna valor em formato hexadecimal
  String s;
  s = String(h, HEX);
  s.toUpperCase();
  s = ("00000000" + s).substring(s.length() + 8 - l);
  return s;
}

String deviceID() {
  // Retorna ID padrão
#ifdef ESP8266
  // ESP8266 utiliza função getChipId()
  return "Nó - " + hexStr(ESP.getChipId());
#else
  // ESP32 utiliza função getEfuseMac()
  return "Nó - " + hexStr(ESP.getEfuseMac());
#endif
}

String ipStr(const IPAddress &ip) {
  // Retorna IPAddress em formato "n.n.n.n"
  String sFn = "";
  for (byte bFn = 0; bFn < 3; bFn++) {
    sFn += String((ip >> (8 * bFn)) & 0xFF) + ".";
  }
  sFn += String(((ip >> 8 * 3)) & 0xFF);
  return sFn;
}

//void ledSet() {
//  // Define estado do LED
//  //digitalWrite(LED_PIN, ledOn ? LED_ON : LED_OFF);
//}

// Funções de Configuração ------------------------------
void  configReset() {
  // Define configuração padrão
  strlcpy(id, "Nó - ", sizeof(id));
  //  strlcpy(ssid, "", sizeof(ssid));
  //  strlcpy(pw, "", sizeof(pw));
  ledOn     = false;
  bootCount = 0;
}

boolean configRead() {
  // Lê configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Config.json"), "r");
  if (deserializeJson(jsonConfig, file)) {
    // Falha na leitura, assume valores padrão
    configReset();

    log(F("Falha lendo CONFIG, assumindo valores padrão."));
    return false;
  } else {
    // Sucesso na leitura
    strlcpy(id, jsonConfig["id"]      | "", sizeof(id));
    strlcpy(EndMacA, jsonConfig["EndMacA"]  | "", sizeof(EndMacA));
    strlcpy(EndMacB, jsonConfig["EndMacB"]      | "", sizeof(EndMacB));
    NumeroNo   = jsonConfig["NumeroNo"];
    moduloB   = jsonConfig["moduloB"]     | false;

    ledOn     = jsonConfig["led"]     | false;
    bootCount = jsonConfig["boot"]    | 0;

    file.close();

    log(F("\nLendo config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    Serial.print("Variavel NumeroNó logo apos recuperar os valores spiffs: ");
    Serial.println(NumeroNo);
    // converte a string recebido na web em um uint8--------------------------------------

    String strEndMacA(EndMacA); // converte char para String

    String Astring = strEndMacA.substring(0, 2);
    uint8_t  Aint = hexToDec(Astring);
    Serial.println("Aint: ");
    Serial.println(Aint);

    String Bstring = strEndMacA.substring(2, 4);
    uint8_t  Bint = hexToDec(Bstring);
    Serial.println("Bint: ");
    Serial.println(Bint);

    String Cstring = strEndMacA.substring(4, 6);
    uint8_t  Cint = hexToDec(Cstring);
    Serial.println("Cint: ");
    Serial.println(Cint);

    String Dstring = strEndMacA.substring(6, 8);
    uint8_t  Dint = hexToDec(Dstring);
    Serial.println("Dint: ");
    Serial.println(Dint);

    String Estring = strEndMacA.substring(8, 10);
    uint8_t  Eint = hexToDec(Estring);
    Serial.println("Eint: ");
    Serial.println(Eint);

    String Fstring = strEndMacA.substring(10, 12);
    uint8_t  Fint = hexToDec(Fstring);
    Serial.println("Fint: ");
    Serial.println(Fint);

    uint8_t macSlaves[] = {Aint, Bint, Cint, Dint, Eint, Fint};
    peerMacAddress[0] = Aint;
    peerMacAddress[1] = Bint;
    peerMacAddress[2] = Cint;
    peerMacAddress[3] = Dint;
    peerMacAddress[4] = Eint;
    peerMacAddress[5] = Fint;
    //--------------------------------------------------------------------
    Serial.println("Variavel macSlaves");
    Serial.print(macSlaves[0]);
    Serial.print(macSlaves[1]);
    Serial.print(macSlaves[2]);
    Serial.print(macSlaves[3]);
    Serial.print(macSlaves[4]);
    Serial.println(macSlaves[5]);

    return true;
  }
}


boolean configSave() {
  // Grava configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Config.json"), "w+");
  if (file) {
    // Atribui valores ao JSON e grava
    jsonConfig["id"]    = id;
    jsonConfig["led"]   = ledOn;
    jsonConfig["boot"]  = bootCount;
    jsonConfig["EndMacA"]  = EndMacA;
    jsonConfig["EndMacB"]    = EndMacB;
    jsonConfig["NumeroNo"]    = NumeroNo;
    jsonConfig["moduloB"]    = moduloB;

    serializeJsonPretty(jsonConfig, file);
    file.close();

    log(F("\nGravando config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
  return false;
}

// Requisições Web --------------------------------------
void handleHome() {
  // Home
  File file = SPIFFS.open(F("/Home.htm"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Atualiza conteúdo dinâmico
    s.replace(F("#id#")       , id);
    s.replace(F("#led#")      , ledOn ? F("Ligado") : F("Desligado"));
    s.replace(F("#bootCount#"), String(bootCount));
#ifdef ESP8266
    s.replace(F("#serial#") , hexStr(ESP.getChipId()));
#else
    s.replace(F("#serial#") , hexStr(ESP.getEfuseMac()));
#endif
    s.replace(F("#software#") , softwareStr());
    s.replace(F("#sysIP#")    , ipStr(WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP()));
    s.replace(F("#clientIP#") , ipStr(server.client().remoteIP()));
    s.replace(F("#active#")   , longTimeStr(millis() / 1000));
    s.replace(F("#userAgent#"), server.header(F("User-Agent")));

    // Envia dados
    server.send(200, F("text/html"), s);
    log("Home - Cliente: " + ipStr(server.client().remoteIP()) +
        (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
    log(F("Home - ERRO lendo arquivo"));
  }
}

void handleConfig() {
  // Config
  //aqui so vai atualizar o que esta na pagina, ou seja vai puxar das variaveis locais e enviar pra pagina
  File file = SPIFFS.open(F("/Config.htm"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Atualiza conteúdo dinâmico
    s.replace(F("#id#")     , id);
    s.replace(F("#ledOn#")  ,  ledOn ? " checked" : "");
    s.replace(F("#ledOff#") , !ledOn ? " checked" : "");

    s.replace(F("#EndMacA#")   , EndMacA);
    s.replace(F("#EndMacB#")   , EndMacB);

    s.replace(F("#noA#")  ,  NumeroNo ? " checked" : "");
    s.replace(F("#noB#") , NumeroNo ? " checked" : "");
    s.replace(F("#noC#") , NumeroNo ? " checked" : "");

    //s.replace(F("#onA#")  ,  moduloA ? " checked" : "");
    //s.replace(F("#offA#") , !moduloA ? " checked" : "");

    // s.replace(F("#onB#")  ,  moduloB ? " checked" : "");
    // s.replace(F("#offB#") , !moduloB ? " checked" : "");

    // Send data
    server.send(200, F("text/html"), s);
    log("Config - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Config - ERROR 500"));
    log(F("Config - ERRO lendo arquivo"));
  }
}

void handleConfigSave() {
  // Grava Config
  // Verifica número de campos recebidos
  int numeroArg = 0;
  numeroArg = server.args();
  Serial.print("numeroArg: ");
  Serial.println(numeroArg);
  if (server.args() == 3) {

    String s;

    // Grava id
    s = server.arg("id");
    s.trim();
    if (s == "") {
      s = deviceID();
    }
    strlcpy(id, s.c_str(), sizeof(id));

    // Grava ssid
    s = server.arg("EndMacA");
    s.trim();
    strlcpy(EndMacA, s.c_str(), sizeof(EndMacA));

    // Grava pw
    s = server.arg("EndMacB");
    s.trim();
    if (s != "") {
      // Atualiza senha, se informada
      strlcpy(EndMacB, s.c_str(), sizeof(EndMacB));
    }

    // Grava placas conectadas
    NumeroNo = server.arg("no").toInt();
    //moduloB = server.arg("macB").toInt();

    // Grava ledOn
    ledOn = server.arg("led").toInt();

    // Grava configuração
    if (configSave()) {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva.');history.back()</script></html>"));
      log("ConfigSave - Cliente: " + ipStr(server.client().remoteIP()));
    } else {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha salvando configuração.');history.back()</script></html>"));
      log(F("ConfigSave - ERRO salvando Config"));
    }
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Erro de parâmetros.');history.back()</script></html>"));
  }
}

void handleReconfig() {
  // Reinicia Config
  configReset();

  // Grava configuração
  if (configSave()) {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração reiniciada.');window.location = '/'</script></html>"));
    log("Reconfig - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha reiniciando configuração.');history.back()</script></html>"));
    log(F("Reconfig - ERRO reiniciando Config"));
  }
}

void handleReboot() {
  // Reboot
  File file = SPIFFS.open(F("/Reboot.htm"), "r");
  if (file) {
    server.streamFile(file, F("text/html"));
    file.close();
    log("Reboot - Cliente: " + ipStr(server.client().remoteIP()));
    delay(100);
    ESP.restart();
  } else {
    server.send(500, F("text/plain"), F("Reboot - ERROR 500"));
    log(F("Reboot - ERRO lendo arquivo"));
  }
}

void handleCSS() {
  // Arquivo CSS
  File file = SPIFFS.open(F("/Style.css"), "r");
  if (file) {
    // Define cache para 3 dias
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("text/css"));
    file.close();
    log("CSS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("CSS - ERROR 500"));
    log(F("CSS - ERRO lendo arquivo"));
  }
}


void funcoesWebSetup() {
  //funcoes que devem rodar dentro do setup para abrir pagina web de configuração
  // WiFi Access Point

  // Configura WiFi para ESP32
  WiFi.setHostname("ConfigurarCentral");
  WiFi.softAP(deviceID().c_str());

  log("WiFi AP " + deviceID() + " - IP " + ipStr(WiFi.softAPIP()));

  // Habilita roteamento DNS
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(DNSSERVER_PORT, "*", WiFi.softAPIP());

  // WebServer
  server.on(F("/config")    , handleConfig);
  server.on(F("/configSave"), handleConfigSave);
  //server.on(F("/reconfig")  , handleReconfig);
  //server.on(F("/reboot")    , handleReboot);
  server.on(F("/css")       , handleCSS);
  server.onNotFound(handleConfig);
  server.collectHeaders(WEBSERVER_HEADER_KEYS, 1);
  server.begin();

  // Pronto
  log(F("Pronto"));
}

void funcoesWebLoop() {
  ////funcoes que devem rodar dentro do Loop para abrir pagina web de configuração
  // DNS ---------------------------------------------
  dnsServer.processNextRequest();

  // Web ---------------------------------------------
  server.handleClient();
}








//Funcoes do botao que seta os modos ConfiguraWEB ou ESPNOW-------------------------
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}
void BotaoModoSetup() {

  pinMode(botao, INPUT_PULLUP); // Configura pino 8 como entrada e habilita pull up interno;
  debouncerA.attach(botao); // Informa que o tratamento de debouce será feito no pino 8;
  debouncerA.interval(2000); // Seta o intervalo de trepidação;
   
  pinMode(saida, OUTPUT);
  //Increment boot number and print it every reboot
  //++bootCount;
  //Serial.println("Boot number: " + String(bootCount));
  Serial.println("MODO: " + String(MODO));
  //Print the wakeup reason for ESP32
  print_wakeup_reason();

}

//==================================================================================================================



void setup() {


  Serial.begin(115200);
  delay(10); //Take some time to open up the Serial Monitor
  Serial.println();
  Serial.println();
  Serial.print("Nome FIRMWARE   ");
  Serial.println(String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1));
  Serial.print("Data gravação FIRMWARE   ");
  Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);
  Serial.print("Mac Address in Station: ");
  Serial.println(WiFi.macAddress());
  
  // SPIFFS
  if (!SPIFFS.begin(true)) {
    log(F("LITTLEFS ERRO"));
    while (true);
  }
  configRead();

  BotaoModoSetup(); // AQUI VERIFICA QUAL O MODO ESTA SETADO PARA SABER QUAIS FUNCOES IRA RODAR NO SETUP

  if (MODO) {
    //coloque aqui codigos do modo WEB
    Serial.println("Modo WEB escolhido");
    // Lê configuração
    configRead();

    // Incrementa contador de inicializações
    bootCount++;

    // Salva configuração
    configSave();

    // LED
    pinMode(LED_PIN, OUTPUT);


    funcoesWebSetup();

  } else {  //coloque aqui codigos do modo ESPNOW
    Serial.println("MODO ESPNOW escolhido");
    fucoesESPNOWsetup();
  }
}

void loop() {
  //Serial.println(digitalRead(0));
  esp_task_wdt_feed();
  //esp_task_wdt_reset();//Reseta o temporizador do watchdog
  yield(); // WatchDog
  
  BotaoModoLoop();

  if (MODO) {
    //coloque aqui codigos do modo WEB
    funcoesWebLoop();
  } else {
    //coloque aqui codigos do modo ESPNOW
    funcoesESPNOWloop();
  }

}