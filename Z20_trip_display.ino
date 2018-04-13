/*
Arduino based trip indicator for Toyota Soarer MZ21
Arduino Nano/Pro Mini
ST7735 1.8 TFT 160x128
DS18b20
*/

#include <SPI.h>    
#include <PDQ_GFX.h>        // PDQ: Core graphics library
#include "PDQ_ST7735_config.h"      // PDQ: ST7735 pins and other setup for this sketch
#include <PDQ_ST7735.h>     // PDQ: Hardware-specific driver library
#include <EEPROM.h>
#include <OneWire.h>
#include <Wire.h>
#include <Arduino.h> //not sure why it is here, but keep for further check

#include "gryphon.h"
#include "name.h"
#include "shifter.h"
#include "oil.h"
#include "od.h" //49x14
#include "norm.h"
#include "pwr.h"
#include "manu.h"

//no custom font yet, consider later
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/Buttons_7.h>
//#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/digital-7mono_12.h>


// pin definition 

//keep blocked pin 11 & pin 13!!! 10, 9. 
//секция с назначением пинов
#define IGNpin A0
//#define analogInput 0 //вход АЦП для измерения напряжения
#define Gear_input A1
//A2 - do not use. CS for TFT
#define BL A3
#define TFTreset A4
//A5 - empty for now
#define DSpin A6  // вход температурного датчика
//A7 empty for now
#define Power 2
#define BUTTON_RESET 3  // кнопка сброса (трип, прочее)
#define BUTTON_TRIP 4 //кнопка выбора трип-одометра А-В
#define SPisr 5  //input pin for speed sensor, Nano pin5
#define MANUpin 6
#define PWRpin 7
//8 ??
#define ODpin 9 // вход овердрайва на вх.9



//misc timings
#define UPDATE_TIME  1000 //частот обновления датчика температуры
#define debounce 20 // ms debounce period to prevent flickering when pressing or releasing the button
#define holdTime 2000 // ms hold period: how long to wait for press+hold event

//colors available
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0 
#define WHITE    0xFFFF
#define ORANGE   0xFA60
#define GRAY     0x8410


PDQ_ST7735 tft;     // PDQ: create LCD object (using pins in "PDQ_ST7735_config.h")
// These are used to get information about static SRAM and flash memory sizes
extern "C" char __data_start[];    // start of SRAM data
extern "C" char _end[];     // end of SRAM data (used to check amount of SRAM this program's variables use)
extern "C" char __data_load_end[];  // end of FLASH (used to check amount of Flash this program's code and data uses)


/* set of additional colors if I ever need some
#define ST7735_LIME    0x07FF
#define ST7735_CYAN    0x07FF
#define ST7735_AQUA    0x04FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_PINK    0xF8FF
 */

//debug 
  int gear_read = 0;
//end debug

  int off = 64; //this is an offset from the top of the screen to get screen positioned in the right place Vert
  unsigned long LastUpdateTime = 0; //переменная частоты опроса датчика температуры
  int idle = 0;
  int active = 0;
  
  int trip_razr [4] = {0, 0, 0, 0}; //массив для поразрядного выделения значения трип-километража
  int trip_draw [4] = {0, 0, 0, 0}; //массив для поразрядного вывода значения трип-километража
  unsigned long digit = 0;
  int tmp = 1; //временная переменная для чтения состояния входа
  int Shifter = 0;
  
  int OD = idle;    // устанавливаем дефолтное состояние 1 
  int Pattern = 3;  // выбор режима трансмиссии - PWR, Manu, Econ
  int PWR =idle;
  int MANU = idle;
  int TRIP = 0;     //переменная текущего trip-счетчика (A, B, Total?) 
  
  //переменные для расчета напряжения
  float vout = 0.0;
  float vin = 0.0;
  float R1 = 100000.0; // resistance of R1 (100K) -see text! верхнее плечо
  float R2 = 100000.0; // resistance of R2 (10K) - see text! нижнее плечо
  int value = 0;
  
  int IGN = 1;  
  int voltage = 0;
  int screen = 1; //переменная для определения текущего экрана: 1 - основной, 2 - Maintenance
  

  //float temp0 = 0;
  //odo variables call
  unsigned long m; //одометр, значение, по-идее, считывается из EEPROM при включении и потом туда же записывается при выключении МК 
  unsigned long m_osn;           //Пробег основной не обнуляемый
  unsigned long odo[3]  = {0,0,0};//Массив пробегов Суточный, Бак, Сервисный, Масло
  unsigned long odo_def[3] = {0,0,0}; //Массив значений по умолчанию для обнуления сервисных пробегов 
  byte odo_num = 0; //Переменная хранит номер отображаемого в данный момент одометра
  
  unsigned int i; 
  unsigned int counter;

  int timer1_counter;

  //variables to indicate that screen/data refresh required
  int screen_disp = 0;
  int Pattern_disp;
  int Shifter_disp = 0;
  int OD_disp = 1;
  int TRIP_status = 1;
  int trip_disp = 1; //переменная для определения необходимости обновления части экрана трип-километража, 1 - обновляем
  
  // TRIP Button variables
  int buttonVal; // value read from button
  int buttonLast = HIGH; // buffered value of the button's previous state
  long btnDnTime; // time the button was pressed down
  long btnUpTime; // time the button was released
  boolean ignoreUp = false; // whether to ignore the button release because the click+hold was triggered

  // RESET Button variables
  int buttonVal1; // value read from button
  int buttonLast1 = HIGH; // buffered value of the button's previous state
  long btnDnTime1; // time the button was pressed down
  long btnUpTime1; // time the button was released
  boolean ignoreUp1 = false; // whether to ignore the button release because the click+hold was triggered
  

  //переменные для автоматического возврата на активное окно
  long activity_timer = 0;
  long activity_pause = 5000;


   //переменные для состояния кнопок - A/B, Reset. Короткое и длинное нажатие 
  int reset_status = 0; //status of reset button for ODO, 0 - no reset required
  int select_status = 0; //status of A/B button for ODO, 0 - no trip change required

  int resetLong_status = 0;   //status of long press reset button for ODO, 0 - no long press
  int selectLong_status = 0;  //status of long press A/B button for ODO, 0 - no long press
  
  
void setup() {
   Serial.begin(115200);
  //basic pins setup 
  pinMode(ODpin, INPUT); // делаем подтяжку вверх, потом возможно изменим 
  pinMode(MANUpin, INPUT);
  pinMode(PWRpin, INPUT);

//BackLight setup, keep low until initial pic is ready. 
  pinMode(BL, OUTPUT);
  pinMode(Power, OUTPUT);
    digitalWrite(Power, LOW);

//TFT reset part. min 10us reset pulse required
  pinMode(TFTreset, OUTPUT);
    digitalWrite(TFTreset, LOW);
    delayMicroseconds(20);
    digitalWrite(TFTreset, HIGH);

//set interrupt 0 (pin 3 in Leonardo, pin 5 in Nano)
  pinMode(SPisr, INPUT_PULLUP);
  
//настройка входа для кнопки выбора одо А-В с подтяжкой вверх
  pinMode(BUTTON_TRIP,INPUT_PULLUP);  
  pinMode(BUTTON_RESET,INPUT_PULLUP);  
  

//EEPROM Read on start section
 /*
 m = EEPROM_ulong_read(1);
 m_osn = EEPROM_ulong_read(5);
 odo[0] = EEPROM_ulong_read(9);
 odo[1] = EEPROM_ulong_read(13);
 odo_def[0] = EEPROM_ulong_read(17);
 odo_def[1] = EEPROM_ulong_read(21);
 odo_num = EEPROM.read(25);
  */
 //just for debug - throw constant
 odo[0] = 20;
 odo[1] = 222;
 odo[2] = 5555;

//Interrupts setup section
  
  // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;

  // Set timer1_counter to the correct value for our interrupt interval
  //timer1_counter = 34286;   // preload timer 65536-16MHz/256/2Hz
  timer1_counter = 64262;   // setup for 1274 external ticks
  
  TCNT1 = timer1_counter;   // preload timer
  TCCR1B |= (1 << CS12);    // setup external event source 
  TCCR1B |= (1 << CS11);    // falling edge
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  interrupts();             // enable all interrupts

//end of interrupts setup section 


    tft.begin();      // initialize LCD
    //screen basic setups
    tft.setRotation(3);
    tft.fillScreen(ST7735_BLACK);
    //logo and name pictures draw
    tft.drawBitmap(3,3 + off,gryphon,64,56,ST7735_WHITE);
    tft.drawBitmap(70,3 + off,soarer,85,14,ST7735_WHITE);

    //frames draw section
    tft.drawFastHLine(0, 0 + off, 160, WHITE); 
    tft.drawFastHLine (0, 58 + off, 160, WHITE);
    tft.drawFastVLine (0, 0 + off, 58, WHITE);
    tft.drawFastVLine (159, 0 + off, 58, WHITE);
    
    tft.fillRoundRect(90, 105, 25, 15, 2, ORANGE);
    tft.fillRoundRect(120, 105, 38, 15, 2, GRAY);
    
    //misc texts on startup screen
    tft.setCursor(85,86);  
    tft.setTextSize(0);
    tft.println("4000 GT");
    tft.setCursor(70,95);  
    tft.println("SUPER LIMITED");
    tft.setCursor(122,42+off);  
    tft.setTextColor(ORANGE, GRAY);
    //tft.setFont(&FreeMonoBoldOblique12pt7b);
    //tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(1);
    tft.println("O/D");
    
    digitalWrite(BL, HIGH); //включение подсветки индикатора. включаем ее после полной отрисовки заставки. 
    
      delay(2000); //пауза после вывода заставки, 2 сек. 
      
      tft.fillRect(1, 1+off, 158, 57, BLACK); //заливка черным для очистки экрана. 
//здесь делаем секцию первоначального вывода данных на экран значения трип-компа. 
      tft.setFont(&FreeMonoBold12pt7b);
      tft.setCursor(63,16+off);  
      tft.setTextColor(ORANGE, BLACK);
      tft.setTextSize(1);
      tft.println("A");
      tft.setCursor(80,16+off);  
      tft.setTextColor(GREEN, BLACK);
      tft.setTextSize(1);
      tft.println("B");
/*
      //tft.setFont(&FreeSerif9pt7b);
      tft.setFont();
      tft.setCursor(63,52+off);  
      tft.setTextColor(ORANGE, BLACK);
      tft.setTextSize(2);
      tft.println("TEST test");
*/

      tft.setTextSize(1);
      tft.setFont(&digital_7__mono_12pt7b);
      tft.setTextColor(GREEN, BLACK);
      tft.setCursor(99, 17+off);
      tft.println(trip_draw[0]);   
      tft.setCursor(110, 17+off);
      tft.println(trip_draw[1]);   
      tft.setCursor(121, 17+off);
      tft.println(trip_draw[2]);   
      tft.setCursor(143, 17+off);
      tft.println(trip_draw[3]);   
      tft.setCursor(132, 17+off);
      tft.println("."); 

      //tft.drawBitmap(3,3 + off,oil,49,53,YELLOW);
      //odo[0] = 0; сделано для проверки
      
}

ISR(TIMER1_OVF_vect)        // interrupt service routine 
{
  TCNT1 = timer1_counter;   // preload timer
    m = m + 1;
    if (odo[0] < 9999) {
      odo[0]++;  
    }
    else {
      odo[0] = 0;
    }
    
    if (odo[1] < 9999) {
      odo[1]++;  
    }
    else {
      odo[1] = 0;
    }
        
    trip_disp = 1; //флаг означает, что изменились данные пробега
}

void loop() {
   
  
    PinStatus(); //проверяем статус всех пинов 
  //проверка зажигания. если нет IGN, то делаем процедуру шатдауна
  //if (IGN == LOW) {
  //  SHUTDOWN();
  //}
  switch (screen) {
    case 1: //рисуем основной рабочий экран
      // проверка нажатия кнопок A/B select - короткое и длинные нажатия 
      if (select_status == 1) { //A/B select - short press, change trip counter. 
        TRIP = TRIP + 1;
        TRIP_status = 1;
        if (TRIP > 1) {
          TRIP = 0;
          TRIP_status = 1;
        }
        select_status = 0;
      }
      if (selectLong_status == 1) { //A/B select - long press
        screen = 2;                 //switch to another screen - Maintenance
        screen_disp = 1;
        selectLong_status = 0;
      }
      // проверка нажатия кнопок Reset - здесь исользуем только короткое нажатие 
      if (reset_status == 1){
        odo[TRIP] = 0;
        trip_disp = 1;
        reset_status = 0;
      }
      if (resetLong_status == 1){
        resetLong_status = 0;
      }
      
   //секция рисования текущего положения шифтера.   
      if (Shifter != Shifter_disp) {
        if (Shifter == 1) {
         tft.fillRect(3, 3+off, 53, 54, BLACK);
         tft.drawBitmap(3,3 + off,p_big,53,52,WHITE);         
        }
        if (Shifter == 2) {
         tft.fillRect(3, 3+off, 53, 54, BLACK); 
         tft.drawBitmap(3,3 + off,r_big,53,52,RED);
        }
        if (Shifter == 3) {
         tft.fillRect(3, 3+off, 53, 54, BLACK);
         tft.drawBitmap(3,3 + off,n_big,53,52,GREEN);
        }
        if (Shifter == 4) {
         tft.fillRect(3, 3+off, 53, 54, BLACK);
         tft.drawBitmap(3,3 + off,d4_big,53,52,WHITE);
        }
        if (Shifter == 5) {
         tft.fillRect(3, 3+off, 53, 54, BLACK);
         tft.drawBitmap(3,3 + off,d4_big,53,52,YELLOW);
        }
        if (Shifter == 6) {
         tft.fillRect(3, 3+off, 53, 54, BLACK);
         tft.drawBitmap(3,3 + off,d4_big,53,52,YELLOW);
        }
        Shifter_disp = Shifter;
      } //конец секции рисования положения шифтера

    //секция росвания текущего трип-счетчика
      if (TRIP_status != 0){
       if (TRIP == 0){
        tft.setFont(&FreeMonoBold12pt7b);
        tft.setCursor(63,16+off);  
        tft.setTextColor(GREEN, BLACK);
        tft.setTextSize(1);
        tft.println("A");
        tft.setCursor(80,16+off);  
        tft.setTextColor(GRAY, BLACK);
        tft.setTextSize(1);
        tft.println("B"); 
        trip_disp = 1;
       }
       if (TRIP == 1){
        tft.setFont(&FreeMonoBold12pt7b);
        tft.setCursor(63,16+off);  
        tft.setTextColor(GRAY, BLACK);
        tft.setTextSize(1);
        tft.println("A");
        tft.setCursor(80,16+off);  
        tft.setTextColor(GREEN, BLACK);
        tft.setTextSize(1);
        tft.println("B"); 
        trip_disp = 1;
       }
       if (TRIP == 2){
        tft.fillRect(3, 3+off, 53, 54, BLACK);
        tft.drawBitmap(3,3 + off,oil,49,53,YELLOW);
        trip_disp = 1;
       }
       TRIP_status = 0; 
      }

    if (odo[0] > 10000) {
        odo[0] = 0;
        }
    if (odo[1] > 10000) {
        odo[1] = 0;
        }        

      //секция отображения текущего значения трип-километража.
    if (trip_disp != 0) { // если 1, то обновляем данные. ниже кусок для выделения сотен, десятков, целых, десятых
      digit = 0;
      Serial.println(odo[TRIP]);
      trip_razr[0] = odo[TRIP] / 1000; // целая часть от деления будет соответствовать количеству тысяч в числе кладем в массив под номером 0
      digit = trip_razr[0] * 1000;// вспомогательная переменная  
      trip_razr[1] = (odo[TRIP] - digit) / 100;    // получем количество сотен
      digit = digit + (trip_razr[1] * 100);             // суммируем сотни и тысячи
      trip_razr[2] = (odo[TRIP] - digit) / 10;     // получем десятки
      digit = digit + (trip_razr[2] * 10);              // сумма тысяч, сотен и десятков
      trip_razr[3] = odo[TRIP] - digit;            // получаем количество единиц
       
      tft.setFont(&digital_7__mono_12pt7b);
      tft.setTextColor(GREEN, BLACK);
      
      if (trip_draw[0] != trip_razr[0]) {
      tft.fillRect(99, 2+off, 10, 16, BLACK);  
      tft.setCursor(99, 17+off);
      trip_draw[0] = trip_razr[0];
      tft.println(trip_draw[0]);   
      }
      if (trip_draw[1] != trip_razr[1]) {
      tft.fillRect(110, 2+off, 10, 16, BLACK);  
      tft.setCursor(110, 17+off);
      trip_draw[1] = trip_razr[1];
      tft.println(trip_draw[1]);   
      }
      if (trip_draw[2] != trip_razr[2]) {
      tft.fillRect(121, 2+off, 10, 16, BLACK);  
      tft.setCursor(121, 17+off);
      trip_draw[2] = trip_razr[2];
      tft.println(trip_draw[2]);   
      }

      if (trip_draw[3] != trip_razr[3]) {
      tft.fillRect(143, 2+off, 10, 16, BLACK);  
      tft.setCursor(143, 17+off);
      trip_draw[3] = trip_razr[3];
      tft.println(trip_draw[3]);   
      }
      tft.setCursor(132, 17+off);
      tft.println("."); 

      //odometr total
      tft.setCursor(65, 37+off);  
      //tmp = odo[0];
      //tft.fillRect(99, 20+off, 60, 16, BLACK);  
      tft.println("145999.9");  
      trip_disp = 0;//обновление закончено, сбрасываем флаг в 0, пока не появяться новые данные

      
    }//окончание секции отображения трип-компа
      //секция рисования служебных индикаторов - O/D, PWR, MANU, etc

    //tft.fillRoundRect(90, 105, 25, 15, 2, ORANGE);
    if (Pattern =! Pattern_disp){
        if (Pattern == 1) {
          tft.fillRect(64, 42+off, 50, 14, BLACK); 
          tft.drawBitmap(64,42 + off,pwr,49,14,RED);
          
        }
        if (Pattern == 2) {
          tft.fillRect(64, 42+off, 50, 14, BLACK); 
          tft.drawBitmap(64,42 + off,manu,50,14,YELLOW);    
        }
        if (Pattern == 3) {
          tft.fillRect(64, 42+off, 50, 14, BLACK); 
          tft.drawBitmap(64,42 + off,norm,49,14,GREEN);
        }
        Pattern_disp = Pattern;
    }

    if (OD != OD_disp) {
      if (OD == 1) {
        tft.drawBitmap(118,42 + off,od,39,14,ORANGE);  
      }
      else {
        tft.drawBitmap(118,42 + off,od,39,14,GRAY);  
      }
      OD_disp = OD;
    }
 
    break; //окончание секции рисования основного скрина

    case 2: // секция рисования окна Maintenance
       // проверка нажатия кнопок A/B select - короткое и длинные нажатия 
      if (select_status == 1) { //A/B select - short press, change trip counter. 
        select_status = 0;
      }
      if (selectLong_status == 1) { //A/B select - long press
        selectLong_status = 0;
      }
      // проверка нажатия кнопок Reset - здесь исользуем только короткое нажатие 
      if (reset_status == 1){
        odo[TRIP] = 0;
        trip_disp = 1;
        reset_status = 0;
      }
      if (resetLong_status == 1){
        resetLong_status = 0;
      }
    
      if (screen_disp != 0){
      tft.fillRect(0, 0+off, 160, 58, BLACK);//заливка черным
      //tft.fillScreen(ST7735_BLACK);
      tft.drawFastHLine(0, 0 + off, 160, RED); 
      tft.drawFastHLine (0, 58 + off, 160, RED);
      tft.drawFastVLine (0, 0 + off, 58, RED);
      tft.drawFastVLine (159, 0 + off, 58, RED);

        tft.setFont(&FreeSerif9pt7b);
        tft.setCursor(3,16+off);  
        tft.setTextColor(GREEN, BLACK);
        tft.setTextSize(1);
        tft.println("Eng Oil");
        tft.setCursor(3,35+off);  
        tft.setTextColor(GREEN, BLACK);
        tft.setTextSize(1);
        tft.println("Trans Oil"); 
        tft.setCursor(3,54+off);  
        tft.setTextColor(GREEN, BLACK);
        tft.setTextSize(1);
        tft.println("T. Belt"); 

        tft.setCursor(100, 16+off);  
        tft.println("945999");  
        tft.setCursor(100, 35+off);  
        tft.println("745999");  
        tft.setCursor(100, 54+off);  
        tft.println("145999");  
      
      screen_disp = 0;
      }

      if ((millis () - activity_timer) > activity_pause){
        screen = 1;
        screen_disp = 1;
        Shifter_disp = 0;
        Pattern_disp = 0;
        OD_disp = OD_disp^1;
        TRIP_status = 1;
        trip_disp = 1;
        
        
      //костыль по заливке черным и первоначальным значениям для корректного обновления данных по возвращении в основной экран. 
          tft.fillRect(0, 0+off, 160, 58, BLACK);//заливка черным
          tft.drawFastHLine(0, 0 + off, 160, WHITE); 
          tft.drawFastHLine (0, 58 + off, 160, WHITE);
          tft.drawFastVLine (0, 0 + off, 58, WHITE);
          tft.drawFastVLine (159, 0 + off, 58, WHITE);

          //прориосвка пробега.. надо подумать как сделать обновление. 
              tft.setTextSize(1);
      tft.setFont(&digital_7__mono_12pt7b);
      tft.setTextColor(GREEN, BLACK);
      tft.setCursor(99, 17+off);
      tft.println(trip_draw[0]);   
      tft.setCursor(110, 17+off);
      tft.println(trip_draw[1]);   
      tft.setCursor(121, 17+off);
      tft.println(trip_draw[2]);   
      tft.setCursor(143, 17+off);
      tft.println(trip_draw[3]);   
      tft.setCursor(132, 17+off);
      tft.println("."); 
        
      }
      
    break;
  }

    

    

//debug - Analog inputs
      /*          
      //IGN voltage input
      tft.setCursor(40, 53+off);  
      tft.fillRect(40, 38+off, 60, 16, BLACK);  
      tft.println(voltage);  */

      /*gears input
      tft.setCursor(99, 53+off);  
      tft.fillRect(99, 38+off, 60, 16, BLACK);  
      tft.println(gear_read);  */
       




    /*
    tft.fillRoundRect(90, 105, 25, 15, 2, GREEN);
    //delay(500);
    tft.fillRoundRect(90, 105, 25, 15, 2, ORANGE);
    //delay(500);
    tft.fillRoundRect(120, 105, 38, 15, 2, ORANGE);
    tft.setCursor(122, 52+off);  
    tft.setTextColor(BLACK, ORANGE);
    //tft.setFont(&FreeSans9pt7b);
    tft.println("O/D");
    //delay(500);
    tft.fillRoundRect(120, 105, 38, 15, 2, GRAY);
    tft.setCursor(122, 52+off);  
    tft.setTextColor(ORANGE, GRAY);
    //tft.setFont(&FreeSans9pt7b);
    tft.println("O/D");
    //delay(500);
    */
    /*
    tft.fillRoundRect(120, 105, 25, 15, 2, ORANGE);
    delay(500);
    tft.fillRoundRect(120, 105, 25, 15, 2, GRAY);
    delay(500);*/ 

    
  ButtonRead();
  //процедура обработки кнопки сброса
  ButtonRead1();
}

/*
//секция процедуры опроса датчика температуры без делэй
float DS18B20(byte *adres)
{
  if (millis() - LastUpdateTime < 0) {
     LastUpdateTime = millis();
  }

  if (millis() - LastUpdateTime > UPDATE_TIME) // обращаемся к датчикам раз в 1000 мс
  {
    LastUpdateTime = millis();
    ds.reset();
    ds.select(adres);
    ds.write(0x44, 1); // start conversion, with parasite power on at the end

    value = analogRead(analogInput);
    
  }
  ds.reset();
  ds.select(adres);
  ds.write(0xBE); // Read Scratchpad

  for (byte i = 0; i < 9; i++) // можно увеличить точность измере��ия до 0.0625 *С (от 9 до 12 бит)
  { // we need 9 bytes
    data[i] = ds.read ();
  }
  raw =  (data[1] << 8) | data[0];//=======Пересчитываем в температуру
  float celsius =  (float)raw / 16.0;
  return celsius;
}*/

void PinStatus(){
  PWR = digitalRead(PWRpin);
  MANU = digitalRead(MANUpin);
  OD = digitalRead(ODpin);   // read the input pin
  if (PWR == 1) {
    Pattern = 1;
  }
  else if (MANU = 1) {
      Pattern = 2;
    }
  else {
      Pattern = 3;
  }
  
  
//debug  
  voltage = analogRead(IGNpin); //читаем напряжение IGN на входе A0
  
  gear_read = analogRead(Gear_input); 


  // считываем значения с аналогового входа(A1) 
  if (gear_read < 360) {
    Shifter = 1;
  }
  else if (gear_read < 400) {
    Shifter = 2;
  }
  else if (gear_read < 470){
    Shifter = 3;
  }
  else if (gear_read < 550){
    Shifter = 4;
  }
  else if (gear_read < 670){
    Shifter = 5;
  }
}

void ButtonRead()
{
// Read the state of the button
buttonVal = digitalRead(BUTTON_TRIP);

// Test for button pressed and store the down time
if (buttonVal == LOW && buttonLast == HIGH && (millis() - btnUpTime) > long(debounce))
{
btnDnTime = millis();
}

// Test for button release and store the up time
if (buttonVal == HIGH && buttonLast == LOW && (millis() - btnDnTime) > long(debounce))
{
  if (ignoreUp == false) {
    select_status = 1;
    activity_timer = millis();
  }
  else ignoreUp = false;
  btnUpTime = millis();
}

// Test for button held down for longer than the hold time
if (buttonVal == LOW && (millis() - btnDnTime) > long(holdTime))
{
  selectLong_status = 1;
  activity_timer = millis();
  ignoreUp = true;
  btnDnTime = millis();
}
buttonLast = buttonVal;
}

void ButtonRead1() //reset button check
{
  
  // Read the state of the button
  buttonVal1 = digitalRead(BUTTON_RESET);

  // Test for button pressed and store the down time
  if (buttonVal1 == LOW && buttonLast1 == HIGH && (millis() - btnUpTime1) > long(debounce))
  {
    btnDnTime1 = millis();
   }

  // Test for button release and store the up time
  if (buttonVal1 == HIGH && buttonLast1 == LOW && (millis() - btnDnTime1) > long(debounce))
  {
    if (ignoreUp1 == false) {
      reset_status = 1;
    }
    else ignoreUp1 = false;
    btnUpTime1 = millis();
  }
  buttonLast1 = buttonVal1;
}

void EEPROMWriteInt(int p_address, int p_value)
      {
      byte lowByte = ((p_value >> 0) & 0xFF);
      byte highByte = ((p_value >> 8) & 0xFF);

      EEPROM.write(p_address, lowByte);
      EEPROM.write(p_address + 1, highByte);
      }

unsigned int EEPROMReadInt(int p_address)
      {
      byte lowByte = EEPROM.read(p_address);
      byte highByte = EEPROM.read(p_address + 1);

      return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
      }

unsigned long EEPROM_ulong_read(int addr) {    
  byte raw[4];
  for(byte i = 0; i < 4; i++) raw[i] = EEPROM.read(addr+i);
  unsigned long &num = (unsigned long&)raw;
  return num;
}

void EEPROM_ulong_write(int addr, unsigned long num) {
  byte raw[4];
  (unsigned long&)raw = num;
  for(byte i = 0; i < 4; i++) EEPROM.write(addr+i, raw[i]);
}

void SHUTDOWN()
{
    
  //delay(1000);
  //if (digitalRead(12)==0) 
  //{ 
  // return;
  //} 
  //started = 0;
 //Сохраняем все пробеги и прочие счетчики в энергонезависимую память, перед отключением 
 EEPROM_ulong_write(1,m);
 EEPROM_ulong_write(5,m_osn);
 EEPROM_ulong_write(9,odo[0]);
 EEPROM_ulong_write(13,odo[1]);
 EEPROM.write(25,odo_num);
}
