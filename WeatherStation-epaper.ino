/**The MIT License (MIT)

Copyright (c) 2017 by Hui Lu

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <JsonListener.h>
#include "Wire.h"
#include "TimeClient.h"
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         
#include "heweather.h"
#include <EEPROM.h>
#include <SPI.h>
#include "EPD_drive.h"
#include "EPD_drive_gpio.h"
#include "bitmaps.h"
#include "lang.h"
/***************************
  Settings
 **************************/
//const char* WIFI_SSID = "";
//const char* WIFI_PWD = "";
const int sleeptime=60;     //updating interval 71min maximum
const float UTC_OFFSET = 8;
byte end_time=0;            //time that stops to update weather forecast
byte start_time=7;          //time that starts to update weather forecast
const char* server="duckduckweather.esy.es";

//modify language in lang.h

 /***************************
  **************************/
String city;
String lastUpdate = "--";
bool shouldsave=false;
bool updating=false; //is in updating progress
TimeClient timeClient(UTC_OFFSET,server);
heweatherclient heweather(server,lang);
//Ticker ticker;
Ticker avoidstuck;
WaveShare_EPD EPD = WaveShare_EPD();

void saveConfigCallback () {
   shouldsave=true;
}
void setup() {
 
  Serial.begin(115200);Serial.println();Serial.println();
   check_rtc_mem();
  pinMode(D3,INPUT);
  pinMode(CS,OUTPUT);
  pinMode(DC,OUTPUT);
  pinMode(RST,OUTPUT);
  pinMode(BUSY,INPUT);
  EEPROM.begin(20);
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  SPI.begin();
  EPD.EPD_init_Part();driver_delay_xms(DELAYTIME);
 
   /*************************************************
   wifimanager
   *************************************************/
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  WiFiManagerParameter custom_c("city","city","your city", 20);
  WiFiManager wifiManager;
   if (read_config()==126)
  {
     EPD.deepsleep(); ESP.deepSleep(60 * sleeptime * 1000000);
    }
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_c); 
  wifiManager.autoConnect("Weather widget");
  while (WiFi.status() != WL_CONNECTED)
  {
   check_config();
  Serial.println("failed to connect and hit timeout");
  EPD.clearshadows(); 
  EPD.clearbuffer();
  EPD.fontscale=1;
  EPD.SetFont(0x0);
  EPD.DrawUTF(0,0,16,16,config_timeout_line1);
  EPD.DrawUTF(18,0,16,16,config_timeout_line2);
  EPD.DrawUTF(36,0,16,16,config_timeout_line3); 
  EPD.DrawUTF(52,0,16,16,config_timeout_line4); 
  EPD.EPD_Dis_Part(0,127,0,295,(unsigned char *)EPD.EPDbuffer,1);
  EPD.deepsleep(); ESP.deepSleep(60 * sleeptime * 1000000);
  }
  city= custom_c.getValue();
   /*************************************************
   EPPROM
   *************************************************/
  if (city!="your city")
  {
    
         EEPROM.write(0,city.length());
         Serial.print("\n\n");
         Serial.println(String("eeprom_write_0:")+city.length());
         for(int i=0;i<city.length();i++)
         {
           EEPROM.write(i+1,city[i]);
        
         Serial.println(city[i]); Serial.print("\n\n");
          }
         EEPROM.commit(); 
    }
   
 byte city_length=EEPROM.read(0);
 Serial.println("EEPROM_CITY-LENGTH:");Serial.println(city_length);
 if (city_length>0)
 {
  String test="";
  for(int x=1;x<city_length+1;x++) 
  {test+=(char)EEPROM.read(x);
  Serial.print((char)EEPROM.read(x));
  }
  city=test;
  Serial.println("   test:");Serial.print(test);
  heweather.city=city;
  }
/*************************************************
   update weather
*************************************************/
//heweather.city="huangdao";
avoidstuck.attach(10,check);
updating=true;
updateData();
updating=false;
updatedisplay();

}
void check()
{
  if(updating==true)
  {EPD.deepsleep(); ESP.deepSleep(60 * sleeptime * 1000000);}
  avoidstuck.detach();
  return;
  }

void loop() {
 
 if(digitalRead(D3)==LOW)
 {
  WiFiManager wifiManager;
  wifiManager.resetSettings();
 }
 EPD.deepsleep();
 ESP.deepSleep(60 * sleeptime * 1000000);

}
void updatedisplay()
{
    
    EPD.clearshadows(); EPD.clearbuffer();EPD.fontscale=1; 
    
    EPD.SetFont(12);unsigned char code[]={0x00,heweather.getMeteoconIcon(heweather.now_cond_index.toInt())};EPD.DrawUnicodeStr(0,16,80,80,1,code);
    EPD.SetFont(13);unsigned char code2[]={0x00,heweather.getMeteoconIcon(heweather.today_cond_d_index.toInt())};EPD.DrawUnicodeStr(0,113,32,32,1,code2);
    EPD.SetFont(13);unsigned char code3[]={0x00,heweather.getMeteoconIcon(heweather.tomorrow_cond_d_index.toInt())};EPD.DrawUnicodeStr(33,113,32,32,1,code3);
    EPD.DrawXline(114,295,31);EPD.DrawXline(114,295,66);
    
    EPD.SetFont(0x0);
    Serial.println("heweather.citystr");
    Serial.println(heweather.citystr);
    EPD.DrawUTF(96,60,16,16,heweather.citystr);//城市名
    EPD.DrawUTF(112,60,16,16,heweather.date.substring(5, 10));
    EPD.DrawUTF(0,145,16,16,todaystr);EPD.DrawUTF(16,145,16,16,heweather.today_tmp_min+"°~"+heweather.today_tmp_max+"°"+heweather.today_txt_d);
    EPD.DrawUTF(33,145,16,16,tomorrowstr);EPD.DrawUTF(49,145,16,16,heweather.tomorrow_tmp_min+"°~"+heweather.tomorrow_tmp_max+"°"+heweather.tomorrow_txt_d);
    EPD.DrawUTF(70,116,16,16,airstr+heweather.qlty);
    EPD.DrawUTF(86,116,16,16,"RH:"+heweather.now_hum+"%"+" "+heweather.now_dir+heweather.now_sc);
    EPD.DrawUTF(102,116,16,16,tonightstr+heweather.today_txt_n);
    EPD.SetFont(0x2);
  //  EPD.DrawUTF(0,250,10,10,lastUpdate);//updatetime
   // float voltage=(float)(analogRead(A0))/1024;
   // String voltagestring=(String)((voltage+0.083)*13/3);
   // EPD.DrawUTF(10,270,10,10,voltagestring+"V");
    EPD.SetFont(0x1);
    EPD.DrawUTF(96,5,32,32,heweather.now_tmp+"°");//天气实况温度
    EPD.DrawYline(96,127,57);
    dis_batt(0,275);
   
    
    for(int i=0;i<1808;i++) EPD.EPDbuffer[i]=~EPD.EPDbuffer[i];
   EPD.EPD_Dis_Part(0,127,0,295,(unsigned char *)EPD.EPDbuffer,1);
    //EPD.EPD_Dis_Full((unsigned char *)EPD.EPDbuffer,1);
    driver_delay_xms(DELAYTIME);
      
  
  }



void updateData() {
  
  timeClient.updateTime();
  heweather.update();
  lastUpdate = timeClient.getHours()+":"+timeClient.getMinutes();

  byte rtc_mem[4];rtc_mem[0]=126;
  byte Hours=timeClient.getHours().toInt();
  Serial.println("hour");
  Serial.print(Hours);
  if (Hours==end_time)
  {
    if((start_time-end_time)<0)  rtc_mem[1]=(24-Hours+start_time)*60/sleeptime;
    else rtc_mem[1]=(start_time-Hours)*60/sleeptime;
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_mem, sizeof(rtc_mem));
    Serial.println("rtc_mem[1]");
    Serial.println(rtc_mem[1]);
    }
 
  Serial.print("heweather.rain");Serial.print(heweather.rain);Serial.print("\n");
  //delay(1000);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //进入配置模式
  
  EPD.clearshadows(); EPD.clearbuffer();EPD.fontscale=1;
  EPD.SetFont(0x0);
  EPD.fontscale=1;
  EPD.DrawXbm_P(6,80,32,32,one);
  EPD.DrawXbm_P(42,80,32,32,two);
  EPD.DrawXbm_P(79,80,32,32,three);
  EPD.fontscale=1;
  EPD.DrawUTF(6,112,16,16,config_page_line1);
  EPD.DrawUTF(22,112,16,16,config_page_line2);
  EPD.DrawXline(80,295,39);
  EPD.DrawUTF(42,112,16,16,config_page_line3);
  EPD.DrawUTF(58,112,16,16,config_page_line4);
  EPD.DrawXline(80,295,76);
  EPD.DrawUTF(79,112,16,16,config_page_line5);
  EPD.DrawUTF(94,112,16,16,config_page_line6);
  EPD.DrawXbm_P(6,0,70,116,phone);
  EPD.EPD_Dis_Part(0,127,0,295,(unsigned char *)EPD.EPDbuffer,1);
  driver_delay_xms(DELAYTIME);
}
void dis_batt(int16_t x, int16_t y)
{
  float voltage=(float)(analogRead(A0))/1024;
  float batt_voltage=(voltage+0.083)*13/3;
  if (batt_voltage<=3.2)  EPD.DrawXbm_P(x,y,20,10,(unsigned char *)batt_0);
  if (batt_voltage>3.2&&batt_voltage<=3.45)  EPD.DrawXbm_P(x,y,20,10,(unsigned char *)batt_1);
  if (batt_voltage>3.45&&batt_voltage<=3.7)  EPD.DrawXbm_P(x,y,20,10,(unsigned char *)batt_2);
  if (batt_voltage>3.7&&batt_voltage<=3.95)  EPD.DrawXbm_P(x,y,20,10,(unsigned char *)batt_3);
  if (batt_voltage>3.95&&batt_voltage<=4.2)  EPD.DrawXbm_P(x,y,20,10,(unsigned char *)batt_4);
  if (batt_voltage>4.2)  EPD.DrawXbm_P(x,y,20,10,(unsigned char *)batt_5);
  
  }
unsigned long read_config()
{
  byte rtc_mem[4];
  ESP.rtcUserMemoryRead(4, (uint32_t*)&rtc_mem, sizeof(rtc_mem));
  Serial.println("first time to run check config");
  return rtc_mem[0];
  
  }
unsigned long check_config()
{
  byte rtc_mem[4];
  ESP.rtcUserMemoryRead(4, (uint32_t*)&rtc_mem, sizeof(rtc_mem));
  if (rtc_mem[0]!=126)
  {
    Serial.println("first time to run check config");
    rtc_mem[0]=126;
    ESP.rtcUserMemoryWrite(4, (uint32_t*)&rtc_mem, sizeof(rtc_mem));
    return 180;
    }
  else
  {
     return 44;
    } 
  
  }
void check_rtc_mem()
{
  /*
  rtc_mem[0] sign for first run
  rtc_mem[1] how many hours left
  */
  byte rtc_mem[4];
  ESP.rtcUserMemoryRead(0, (uint32_t*)&rtc_mem, sizeof(rtc_mem));
  if (rtc_mem[0]!=126)
  {
    Serial.println("first time to run");
    rtc_mem[0]=126;
    rtc_mem[1]=0;
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_mem, sizeof(rtc_mem));
    }
  else
  {
    if(rtc_mem[1]>0) 
    {
       rtc_mem[1]--;
       Serial.println("don't need to update weather");
       Serial.println(rtc_mem[1]);
       ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_mem, sizeof(rtc_mem));
       EPD.deepsleep();
       ESP.deepSleep(60 * sleeptime * 1000000);
      }
    }
  
  }
