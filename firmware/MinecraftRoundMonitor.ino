#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include "TouchDrvCSTXXX.hpp"

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *gfx = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
TouchDrvCST92xx touch;
const uint8_t TOUCH_ADDR = 0x5A;
int16_t tx[2], ty[2];
Preferences prefs;
WebServer setupServer(80);
DNSServer dns;
String wifiSsid, wifiPass, apiUrl;
bool configMode = false;
int page = 0;
unsigned long lastFetch = 0, lastTouch = 0;
bool serverOnline = false;
int playersOnline = 0, playersMax = 20;
float pingMs = 0;
String onlineNames[8]; int onlineCount = 0;
String recentPlayer[5], recentAction[5], recentTime[5]; int recentCount = 0;
String unknownName = "";
uint16_t C_BG=0x0861,C_CARD=0x10A2,C_WHITE=0xFFFF,C_MUTED=0xA514,C_GREEN=0x4E6A,C_RED=0xF986,C_YELLOW=0xFEC0,C_CYAN=0x4DFF;

void centerText(const String &s,int y,int size,uint16_t color){gfx->setTextSize(size);gfx->setTextColor(color);int16_t x1,y1;uint16_t w,h;gfx->getTextBounds(s,0,y,&x1,&y1,&w,&h);gfx->setCursor((LCD_WIDTH-w)/2,y);gfx->print(s);} 
void drawHeader(const String &title){gfx->fillScreen(C_BG);centerText("MINECRAFT",38,3,C_CYAN);centerText(title,78,2,C_WHITE);} 
void drawDots(){int cx=LCD_WIDTH/2;for(int i=0;i<3;i++)gfx->fillCircle(cx+(i-1)*22,425,5,(i==page)?C_WHITE:C_MUTED);} 
void drawStatus(){drawHeader("SERVER STATUS");gfx->fillRoundRect(70,125,326,205,28,C_CARD);gfx->fillCircle(128,180,18,serverOnline?C_GREEN:C_RED);gfx->setTextColor(serverOnline?C_GREEN:C_RED);gfx->setTextSize(3);gfx->setCursor(160,165);gfx->print(serverOnline?"ONLINE":"OFFLINE");centerText(String(playersOnline)+" / "+String(playersMax),225,5,C_WHITE);centerText("SPIELER",280,2,C_MUTED);if(serverOnline)centerText("Ping "+String((int)pingMs)+" ms",350,2,C_MUTED);drawDots();}
void drawPlayers(){drawHeader("SPIELER");if(onlineCount==0)centerText("Niemand online",215,3,C_MUTED);else{int y=145;for(int i=0;i<onlineCount&&i<5;i++){gfx->fillRoundRect(62,y-16,342,52,16,C_CARD);gfx->fillCircle(88,y+10,9,C_GREEN);gfx->setTextSize(2);gfx->setTextColor(C_WHITE);gfx->setCursor(112,y);gfx->print(onlineNames[i]);y+=62;}}drawDots();}
void drawHistory(){drawHeader("HISTORIE");int y=135;for(int i=0;i<recentCount&&i<5;i++){gfx->fillCircle(76,y+9,7,recentAction[i]=="JOIN"?C_GREEN:C_MUTED);gfx->setTextColor(C_WHITE);gfx->setTextSize(2);gfx->setCursor(96,y);gfx->print(recentPlayer[i]);gfx->setTextColor(C_MUTED);gfx->setTextSize(1);gfx->setCursor(96,y+24);String t=recentTime[i];if(t.length()>=19)t=t.substring(11,19);gfx->print((recentAction[i]=="JOIN"?"rein  ":"raus  ")+t);y+=58;}drawDots();}
void drawUnknown(){gfx->fillScreen(0x7800);centerText("WARNUNG",85,4,C_WHITE);centerText("UNBEKANNTER",165,3,C_YELLOW);centerText("SPIELER",205,3,C_YELLOW);centerText(unknownName,285,3,C_WHITE);centerText("Touch zum Schliessen",385,1,C_WHITE);} 
void redraw(){if(unknownName.length()){drawUnknown();return;}if(page==0)drawStatus();else if(page==1)drawPlayers();else drawHistory();}

String htmlPage(){return R"HTML(<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font-family:Arial;background:#111;color:#fff;max-width:520px;margin:40px auto;padding:20px}input{width:100%;padding:12px;margin:8px 0 18px;box-sizing:border-box}button{padding:14px;width:100%;font-size:18px}small{color:#aaa}</style></head><body><h2>Minecraft Monitor Setup</h2><form method="POST" action="/save"><label>WLAN-Name</label><input name="ssid"><label>WLAN-Passwort</label><input name="pass" type="password"><label>Monitor API</label><input name="api" value="http://192.168.178.115:8080/api/status"><button type="submit">Speichern & Neustarten</button></form><small>Nach dem Neustart verbindet sich das Display automatisch.</small></body></html>)HTML";}

void startConfigPortal(){configMode=true;WiFi.mode(WIFI_AP);WiFi.softAP("MinecraftMonitor-Setup");IPAddress ip=WiFi.softAPIP();dns.start(53,"*",ip);setupServer.on("/",[](){setupServer.send(200,"text/html",htmlPage());});setupServer.on("/save",HTTP_POST,[](){prefs.begin("mcmon",false);prefs.putString("ssid",setupServer.arg("ssid"));prefs.putString("pass",setupServer.arg("pass"));prefs.putString("api",setupServer.arg("api"));prefs.end();setupServer.send(200,"text/html","<h2>Gespeichert. Display startet neu...</h2>");delay(1200);ESP.restart();});setupServer.onNotFound([](){setupServer.sendHeader("Location","/",true);setupServer.send(302,"text/plain","");});setupServer.begin();gfx->fillScreen(C_BG);centerText("SETUP",115,4,C_CYAN);centerText("WLAN:",195,2,C_MUTED);centerText("MinecraftMonitor-Setup",230,2,C_WHITE);centerText("Dann Browser oeffnen",290,1,C_MUTED);centerText("192.168.4.1",320,2,C_WHITE);} 

bool connectWifi(){prefs.begin("mcmon",true);wifiSsid=prefs.getString("ssid","");wifiPass=prefs.getString("pass","");apiUrl=prefs.getString("api","http://192.168.178.115:8080/api/status");prefs.end();if(wifiSsid.isEmpty())return false;WiFi.mode(WIFI_STA);WiFi.begin(wifiSsid.c_str(),wifiPass.c_str());unsigned long start=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-start<15000)delay(250);return WiFi.status()==WL_CONNECTED;}

bool fetchStatus(){if(WiFi.status()!=WL_CONNECTED)return false;HTTPClient http;http.setTimeout(2500);http.begin(apiUrl);int code=http.GET();if(code!=200){http.end();return false;}String payload=http.getString();http.end();DynamicJsonDocument doc(8192);if(deserializeJson(doc,payload))return false;serverOnline=doc["server_online"]|false;playersOnline=doc["players_online"]|0;playersMax=doc["players_max"]|20;pingMs=doc["ping_ms"]|0.0;onlineCount=0;for(JsonVariant v:doc["online_names"].as<JsonArray>())if(onlineCount<8)onlineNames[onlineCount++]=v.as<String>();unknownName="";JsonArray unk=doc["unknown_players"].as<JsonArray>();if(!unk.isNull()&&unk.size()>0)unknownName=unk[0].as<String>();recentCount=0;for(JsonObject e:doc["recent_events"].as<JsonArray>()){if(recentCount>=5)break;recentTime[recentCount]=e["time"].as<String>();recentPlayer[recentCount]=e["player"].as<String>();recentAction[recentCount]=e["action"].as<String>();recentCount++;}return true;}

void handleTouch(){uint8_t n=touch.getPoint(tx,ty,2);if(!n)return;unsigned long now=millis();if(now-lastTouch<350)return;lastTouch=now;if(unknownName.length()){unknownName="";redraw();return;}int x=tx[0];if(x<155)page=(page+2)%3;else page=(page+1)%3;redraw();}

void setup(){Serial.begin(115200);Wire.begin(IIC_SDA,IIC_SCL);gfx->begin();gfx->setBrightness(180);gfx->fillScreen(C_BG);touch.setPins(TP_RESET,TP_INT);touch.begin(Wire,TOUCH_ADDR,IIC_SDA,IIC_SCL);touch.setMaxCoordinates(LCD_WIDTH,LCD_HEIGHT);touch.setMirrorXY(true,true);if(!connectWifi()){startConfigPortal();return;}fetchStatus();redraw();}
void loop(){if(configMode){dns.processNextRequest();setupServer.handleClient();delay(5);return;}handleTouch();if(millis()-lastFetch>3000){lastFetch=millis();bool ok=fetchStatus();if(!ok&&WiFi.status()!=WL_CONNECTED)WiFi.reconnect();redraw();}delay(15);}