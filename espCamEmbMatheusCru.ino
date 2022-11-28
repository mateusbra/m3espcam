#include <WiFi.h>
#include <HTTPClient.h>
HTTPClient http;
#include <esp32cam.h>
//#include "esp_camera.h"
//#include "soc/soc.h"
//#include "soc/rtc_cntl_reg.h"
#include <WebServer.h>
//#include "ArduinoJson.h"
#include "Arduino_JSON.h"


WebServer server(80);
WiFiClient client = server.client();
  
TaskHandle_t TaskHandleAll;

static auto hiRes = esp32cam::Resolution::find(800, 600);

const char* WIFI_SSID = "MultilaserPRO_ZTE_2.4G_SX3SSe";
const char* WIFI_PASS = "Zxt7Zh9x";

String post_url;
String server_name = "https://m3-esp-cam-backend.vercel.app/";

const int PIRsensor     = 13;
const int LDRsensor     = 15;
const int LEDsensor     = 12;
const int Buzzersensor  = 14;

const int ch = 2;

int PIRstate = LOW; // we start, assuming no motion detected
int val = 0;
int inputLDRVal = 0;
int modeState = 0; //O cioso, 1 Vigilante 

// the time we give the sensor to calibrate (approx. 10-60 secs according to datatsheet)
const int calibrationTime = 10; // 10 secs

void serveJpg()
{  
  auto frame = esp32cam::capture();
  if (frame == nullptr) 
  {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(), static_cast<int>(frame->size()));
  
  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgHi()
{
  if (!esp32cam::Camera.changeResolution(hiRes)) 
  {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  Serial.print("Conectando");
  
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(500);
  }

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);
 
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }
  
  String endpoint = "/cam-hi.jpg";
  String url = "http://" + WiFi.localIP().toString() + endpoint;
  Serial.println(url);

  post_url = server_name + "setServerUrl?serverUrl="+url;
  http.begin(post_url);
  int httpResponseCode = http.POST("");
  
  server.on("/cam-hi.jpg", handleJpgHi);
  server.begin();

  pinMode(Buzzersensor, OUTPUT);
  ledcAttachPin(Buzzersensor, ch);
  ledcSetup(ch, 1500, 10);
  
  xTaskCreatePinnedToCore(taskPhotoRequest,  "taskPhotoRequest", 10000, NULL, 3, &TaskHandleAll, 0);
  xTaskCreatePinnedToCore(taskPIR,          "taskPIR",           10000, NULL, 3, &TaskHandleAll, 0);
  //xTaskCreatePinnedToCore(taskLDR,          "taskLDR",            2600, NULL, 2, &TaskHandleAll, 0);
  xTaskCreatePinnedToCore(taskBuzzer,       "taskBuzzer",        10000, NULL, 2, &TaskHandleAll, 0);
}

static void taskPIR( void * pvParameters )
{
  while(1)
  {
    if(modeState)
    {
      val = digitalRead(PIRsensor);
  
      if (val == HIGH) 
      {
        if (PIRstate == LOW) 
        {
          delay(500);
          //Firebase.setBool("/presence",true);
          // /setPresence
          post_url = server_name + "setPresence?presence=true";
          http.begin(post_url);
          int httpResponseCode = http.POST("");
          // we have just turned on because movement is detected
          Serial.println("Motion detected!");
  
          delay(500);
          PIRstate = HIGH;
        }
      }
      else 
      {
        if (PIRstate == HIGH) 
        {
          Serial.println("Motion ended!");
          delay(500);
          //Firebase.setBool("/presence",false);
          // /setPresence
          post_url = server_name + "setPresence?serverUrl=false";
          http.begin(post_url);
          int httpResponseCode = http.POST("");
          PIRstate = LOW; 
        }
      }
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL); //Estado nunca atingido, apenas por segurança
}

static void taskBuzzer( void * pvParameters )
{
  float sinVal;
  int   toneVal;
  bool person_detected = false;
  
  while(1)
  {
    if(modeState)
    {
      http.begin(server_name + "getPersonDetected");
      http.GET();
      String payload = http.getString();
  
      Serial.print("PersonDetected: ");
      Serial.println(payload);
  
      if(payload == "true")
      {
        for (byte t = 0; t<10; t++)
        {
          for (byte x=0; x<180; x++)
          {
              //converte graus em radianos
              sinVal = (sin(x*(3.1412/180)));
              //agora gera uma frequencia
              toneVal = 2000+(int(sinVal*100));
              //toca o valor no buzzer
              ledcWriteTone(ch,toneVal);
              //Serial.print("*");
              //atraso de 2ms e gera novo tom
              delay(4);
          }
        }
      }
    }
    ledcWriteTone(ch, 0);
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL); //Estado nunca atingido, apenas por segurança
}

static void taskPhotoRequest( void * pvParameters )
{
  while(1)
  {
    if(modeState)
    {
      server.handleClient();
    }
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL); //Estado nunca atingido, apenas por segurança
}

static void taskMode( void * pvParameters )
{
  while(1)
  {
    http.begin(server_name + "getMode");
    http.GET();
    String payload = http.getString();

    Serial.print("PersonDetected: ");
    Serial.println(payload);

    if (payload == "true")
    {
      modeState = 1;
    } 
    else 
    {
      modeState = 0;
    }
    
    http.begin(post_url);
    int httpResponseCode = http.POST("");
      
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL); //Estado nunca atingido, apenas por segurança
}

void loop() { }
