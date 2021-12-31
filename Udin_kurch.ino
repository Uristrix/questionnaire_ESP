#include <DFRobotDFPlayerMini.h>
#include <SoftwareSerial.h>  
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <iarduino_RTC.h> 
#include <GyverOLED.h>
#include <ArduinoJson.h>

const char* ssid     = "S20 FE";
const char* password = "nsau0963";
const char* host = "bmstuvoting.herokuapp.com";
const int httpsPort = 443;

String winner;
DynamicJsonDocument times(256); // json, который хранит массив дат, в которые проигрывается музыка и звонок
DynamicJsonDocument music(4096); // json, который хранит структуру голосами, для выявляния победителя
SoftwareSerial MS(13, 15); // uart для dfplayer mini
String beginArr[] = {"08:40:00", "10:25:00", "12:50:00", "14:35:00"};// начало пар

GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;  
iarduino_RTC watch(RTC_DS3231); 
DFRobotDFPlayerMini Player;
//================================================================================================
//                                  Установка времени
//================================================================================================
void setDateTime()
{
  const char* strM="JanFebMarAprMayJunJulAugSepOctNovDec";    // Определяем массив всех вариантов текстового представления текущего месяца находящегося в предопределенном макросе __DATE__.
  const char* sysT=__TIME__;                                  // Получаем время компиляции скетча в формате "SS:MM:HH".
  const char* sysD=__DATE__;                                  // Получаем дату  компиляции скетча в формате "MMM:DD:YYYY", где МММ - текстовое представление текущего месяца, например: Jul.
  //  Парсим полученные значения в массив:                    // Определяем массив «i» из 6 элементов типа int, содержащий следующие значения: секунды, минуты, часы, день, месяц и год компиляции скетча.
  const int i[6] {(sysT[6]-48)*10+(sysT[7]-48), 
  (sysT[3]-48)*10+(sysT[4]-48), 
  (sysT[0]-48)*10+(sysT[1]-48), 
  (sysD[4]-48)*10+(sysD[5]-48), 
  ((int)memmem(strM,36,sysD,3)+3-(int)&strM[0])/3, 
  (sysD[9]-48)*10+(sysD[10]-48)};
  watch.settime(i[0]+40,i[1],i[2],i[3],i[4],i[5]);              // Устанавливаем время
}
//================================================================================================
//                                  Запросы к серверу
//================================================================================================

void getMusic(String tm)
{
  WiFiClientSecure client;
  client.setInsecure();//пропустить верификацию (из-за https)
  if (client.connect(host, httpsPort)) 
  {
    Serial.println("Get music at " + tm);
    // Make a HTTP request:
    client.println("GET https://" + String(host) + "/api_form/music_list/" + tm + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Connection: close");
    client.println();

    while (client.connected())
      if (client.readStringUntil('\n') == "\r") break; // пропускаем хедер

    deserializeJson(music, client); // сохраняем json
  }
  else Serial.println("Connection failed!");
  client.stop();
}

void getTime()
{
  WiFiClientSecure client;
  client.setInsecure();//пропустить верификацию(из-за https)
  if (client.connect(host, httpsPort)) 
  {
    Serial.println("Get time array");
    // Make a HTTP request:
    client.println("GET https://" + String(host) + "/api_form/time HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Connection: close");
    client.println();

    while (client.connected())
      if (client.readStringUntil('\n') == "\r") break; // пропускаем хедер

  deserializeJson(times, client); // сохраняем json
  }
  else Serial.println("Connection failed!");
  client.stop();
}

void resetVotes(String tm) // метод обнуляет голоса в бд в определённый перерыв
{
  WiFiClientSecure client;
  client.setInsecure();//пропустить верификацию (из-за https)
  if (client.connect(host, httpsPort)) 
  {
    Serial.println("reset votes at "+ tm);
    // Make a HTTP request:
    client.println("DELETE https://" + String(host) + "/api_form/erase/" + tm + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Connection: close");
    client.println();
  }
  else Serial.println("Connection failed!");
  client.stop();
}
//================================================================================================
//                                      Определение победителя
//================================================================================================

int Winner() // метод определяет победителя голосования
{
  int index = 0;
  int vote = 0;
  JsonArray Music = music["music"];
  for(int i = 0; i < Music.size(); i++)
  {
    if(Music[i]["vote"].as<int>() >= vote)
    {
      vote = Music[i]["vote"].as<int>();
      winner = Music[i]["name"].as<String>();
      index = i;
    }
  }
    return index + 1;
}

//================================================================================================
//                                        Работа с экраном
//================================================================================================

void drawLines()                 // рисует разметку
{
  oled.line(0, 20, 128, 20);
  oled.line(0, 41, 128, 41);
}
void printDate(String D)        // отображает дату
{
  oled.setScale(2);
  oled.setCursor(5, 0);
  oled.print(D);
}
void printTime(String T)       // отображает время
{
  oled.setScale(2);
  oled.setCursor(15, 3);
  oled.print(T);
}
void printLoading()            // отображает надпись loading
{
  oled.setScale(2);
  oled.setCursor(20, 3);
  oled.print("loading");
}
void printWinner()             // отображает победителя голосования
{
  int index = winner.indexOf(" - ");
  String au = winner.substring(0, index), comp = winner.substring(index + 3, winner.length()) ;
  //строка парсится на автора и композицию и центрируется
  oled.setScale(1);
  oled.setCursor((128 - 6 * au.length()) / 2, 6);
  oled.print(au);
  oled.setCursor((128 - 6 * comp.length()) / 2, 7);
  oled.print(comp);
}
//================================================================================================
//                                      Инициализация прибора
//================================================================================================

void setup()
{
  MS.begin(9600);
  Serial.begin(115200);
  oled.init();                      //инициализация экрана
  oled.clear();                     //очистка экрана 
  drawLines();
  printLoading();
  //setDateTime();  // выставляем время
  if (!Player.begin(MS))   // Проверяем, есть ли связь с плеером 
  {                               
    Serial.println(F("Please check dfplayer"));      
    while (true);                                              
  }
  Player.setTimeOut(500); //устанавливаем время отклика
  Player.volume(6);       //выставляем громкость (от 0 до 30)
  
  WiFi.mode(WIFI_OFF);    //Prevents reconnection issue (taking too long to connect)
  WiFi.mode(WIFI_STA);    //Only Station No AP, This line hides the viewing of ESP as wifi hotspot

  WiFi.begin(ssid, password);  //коннектимся к точке доступа
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED){ delay(500); Serial.print("."); }
  
  Serial.print("\nConnected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  getTime();      // получаем массив дат с БД, для голосования
  oled.clear();   // очищаем экран
}

//==============================================================================================
//                                       Основной цикл
//==============================================================================================
void loop()
{
  //выводим дату, время и победителя предыдущего голосования на экран
  drawLines();
  printDate(watch.gettime("d-m-Y"));
  printTime(watch.gettime("H:i:s")); 
  printWinner();

  //звонок на пару
  for(int i = 0; i < 4; i++)
    if(beginArr[i] == String(watch.gettime("H:i:s")) && (Player.readState() == 2 || Player.readState() == 0))
      Player.play(6);
      
  //пробегаемся по массиву перерывов. Если время настало, 
  //то воспроизводим звонок, узнаём победителя голосования, воспроизводим музыку
  //и обнуляем будущее голосование в БД
  JsonArray Times = times["time"].as<JsonArray>();
  for(int i = 0; i < Times.size(); i++)
  {
    if(String(watch.gettime("s")) == "00" && String(watch.gettime("H:i")) == (Times[i].as<String>()))
    {
      Player.play(6);
      oled.clear();
      drawLines();
      printLoading();
      
      if(i != Times.size() - 1)
        resetVotes(Times[i+1].as<String>());
      else
        resetVotes(Times[0].as<String>());
        
     getMusic(Times[i].as<String>());
     while(Player.readState() != 0); // ждём когда закончится звонок
     delay(5000);                    // задержка между звонком и композицией
     Player.play(Winner());
    }
  }  
delay(1);
}
//==============================================================================================
