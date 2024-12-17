// Using the IRremote library: https://github.com/Arduino-IRremote/Arduino-IRremote
#include <Arduino.h>
#ifdef ESP8266
#define IR_SEND_PIN 5 /* D1 */
#define INPUT_PIN D3
#define LED_R D6 /* 12 */
#define LED_G 13 /* D7 */
#define LED_B 0  /* D3 */
#else
#define INPUT_PIN 5
#endif

const char *ssid = "internet";
const char *password = "password";

const char *ap_ssid = "ap";
const char *ap_password = "PASSWORD";

#include <TinyIRSender.hpp>
#include "PinDefinitionsAndMore.h" // Define macros for input and output pin etc.

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// #include <ezTime.h>
#include <NTPClient.h>

// #include <ConfigJson.h>
#include <EEPROM.h>
#define EEPROM_SIZE 512
#include <CRC8.h>

CRC8 crc8;

String crontab;
uint8_t tz_offset;

#define EEPROM_STR_LEN 64

typedef struct
{
  uint8_t tz_offset;
  char crontab[EEPROM_STR_LEN];
  uint8_t crc8;
} eeprom_data_s;

typedef union
{
  eeprom_data_s data;
  uint8_t raw[sizeof(eeprom_data_s)];
} eeprom_data_u;

eeprom_data_u EepromData;
bool eeprom_load(uint16_t start)
{
  bool res = true;
  EEPROM.begin(sizeof(EepromData));
  for (uint16_t i = 0; i < sizeof(EepromData); i++)
  {
    EepromData.raw[i] = EEPROM.read(i + start);
  };
  crc8.reset();
  crc8.restart();
  for (uint16_t i = 0; i < sizeof(EepromData) - 1; i++)
  {
    crc8.add(EepromData.raw[i]);
  };
  if (crc8.calc() == EepromData.data.crc8)
  {
    Serial.println("crc matched");
    tz_offset = EepromData.data.tz_offset;
    crontab = String(EepromData.data.crontab);
  }
  else
  {
    Serial.println("crc not matched; use defaults");
    res = false;
    tz_offset = 2;
    crontab = "#*/23 * * * * *";
  };
  EEPROM.end();
  return res;
}
void eeprom_save(uint16_t start)
{
  EepromData.data.tz_offset = tz_offset;
  strncpy(EepromData.data.crontab, crontab.c_str(), EEPROM_STR_LEN - 1);
  crc8.reset();
  crc8.restart();
  for (uint16_t i = 0; i < sizeof(EepromData) - 1; i++)
  {
    crc8.add(EepromData.raw[i]);
  };
  EepromData.data.crc8 = crc8.calc();
  EEPROM.begin(sizeof(EepromData));
  for (uint16_t i = 0; i < sizeof(EepromData); i++)
  {
    EEPROM.write(start + i, EepromData.raw[i]);
  };
  EEPROM.commit();
  EEPROM.end();
};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

#include "CronAlarms.h"



#include <GyverPortal.h>
GyverPortal ui;

void Alarm();
void SendToIrc(uint8_t cmd)
{
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
  Serial.print("Send cmd ");
  Serial.println(String(cmd, HEX));
  sendNEC(IR_SEND_PIN, 0x1, cmd, 5);
  digitalWrite(LED_B, HIGH);
}

void apply_crontab(String cron_str)
{
  Serial.print("apply_crontab '");
  Serial.print(cron_str);
  Serial.println("'");
  for (uint8_t id = 0; id < dtNBR_ALARMS; id++)
  {
    Cron.free(id); // ensure all Alarms are cleared and available for allocation
  };

  while (cron_str.length() > 0)
  {
    int end_of_str = cron_str.indexOf('\n');
    if (end_of_str < 0)
      end_of_str = cron_str.length();
    String cron_1_s = cron_str.substring(0, end_of_str);
    cron_str = cron_str.substring(end_of_str + 1);
    Serial.println("'" + cron_1_s + "' " + String(end_of_str));
    Serial.println(Cron.create((char *)cron_1_s.c_str(), Alarm, false));
  };
  Serial.println("apply_crontab done");
}

void build()
{
  GP.BUILD_BEGIN();
  GP.THEME(GP_DARK);

  if (ui.uri("/setup"))
  {
    GP.FORM_BEGIN("/setup");
    GP.TITLE("Setup crontab", "t1");
    GP.HR();
    GP.LABEL(String(ssid) + " " + WiFi.localIP().toString());
    GP.JQ_SUPPORT();
    GP.JQ_UPDATE_BEGIN(2000);
    GP.LABEL("Now: " + timeClient.getFormattedTime());
    GP.JQ_UPDATE_END();
    GP.HR();
    GP.LABEL("Alloc timers: " + String(Cron.count()));
    GP.HR();
    GP.LABEL("Crontab");
    GP.AREA("crontab", 3, crontab);
    GP.BREAK();
    GP.LABEL("TZ");
    GP.NUMBER("tz_offset", "timezone", tz_offset);
    GP.BREAK();

    GP.SUBMIT("Submit");
    GP.FORM_END();
  }
  else
  {
    GP.TITLE("Test IRC", "t1");
    GP.HR();
    M_BLOCK(
        M_BOX(GP.BUTTON("onoff", "On/Off");
              GP.BUTTON("light", "Light")););
    M_BLOCK(
        M_BOX(

            GP.BUTTON("flame_plus", "Flame +");
            GP.BUTTON("flame_minus", "Flame -");););
    M_BLOCK(
        M_BOX(
            GP.BUTTON("speed_plus", "Speed +");
            GP.BUTTON("speed_minus", "Speed -");););
    M_BLOCK(
        M_BOX(
            GP.BUTTON("temp_plus", "Temp +");
            GP.BUTTON("temp_minus", "Temp -");););

    M_BLOCK(
        M_BOX(
            GP.BUTTON("clock", "Clock");
            GP.BUTTON("sound", "Sound");););
    //);
  };
  GP.BUILD_END();
}
void form_action()
{
  // одна из форм была submit
  if (ui.form())
  {
    // проверяем, была ли это форма "/update"
    if (ui.form("/setup"))
    {
      // забираем значения и обновляем переменные
      // 1. получаем и присваиваем вручную
      crontab = ui.getString("crontab");
      tz_offset = ui.getInt("tz_offset");
      timeClient.setTimeOffset(3600 * tz_offset);

      // выводим для отладки
      Serial.print(crontab);
      Serial.print(',');
      Serial.println(tz_offset);
      apply_crontab(crontab);
      eeprom_save(0);
    }
  };
  if (ui.click())
  {
    Serial.println(ui.clickName());
    if (ui.click("onoff"))
    {
      SendToIrc(0x12);
    };
    if (ui.click("light"))
    {
      SendToIrc(0x14);
    };
    if (ui.click("flame_plus"))
    {
      SendToIrc(0x1A);
    };
    if (ui.click("flame_minus"))
    {
      SendToIrc(0x05);
    };
    if (ui.click("speed_plus"))
    {
      SendToIrc(0x1D);
    };
    if (ui.click("speed_minus"))
    {
      SendToIrc(0x19);
    };
    if (ui.click("temp_plus"))
    {
      SendToIrc(0x18);
    };
    if (ui.click("temp_minus"))
    {
      SendToIrc(0x1F);
    };
    if (ui.click("clock"))
    {
      SendToIrc(0x1C);
    };
    if (ui.click("sound"))
    {
      SendToIrc(0x1E);
    };
  };
}

time_t localtime_func()
{
  return timeClient.getEpochTime();
}

void Alarm()
{
  Serial.println("Alarm!");
  Serial.println(timeClient.getFormattedTime());
  //  Serial.println(localtime_func());
  // sendNEC(IR_SEND_PIN, 0x1, 0x12, 5);
  SendToIrc(0x12);
  // sendNEC(12, 0x1, 0x12, 5); // Send address 0 and command 11 on pin 3 with 2 repeats.
  delay(2000);
  //SendToIrc(0x1E); // enable sound
  //delay(1500);
  SendToIrc(0x1C); // timer on
}

void setup()
{
  Serial.begin(115200);
  pinMode(INPUT_PIN, INPUT_PULLUP); // SW1 connected to pin 2
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);

  delay(500);
  Cron.now_func = localtime_func;

  if (!eeprom_load(0))
  {
    Serial.println("No EEPROM config found");
  };

  WiFi.mode(WIFI_AP_STA);

  Serial.println("Configuring access point...");
  WiFi.softAP(ap_ssid, ap_password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  ui.attachBuild(build);
  ui.attach(form_action);
  ui.start();
  timeClient.setTimeOffset(3600 * tz_offset);

  timeClient.begin();
  WiFi.begin(ssid, password);
}

bool connected = false;

void do_blink()
{
  digitalWrite(LED_B, HIGH);
  uint32_t blink_period = 3000;
  uint32_t fade_time = 1500;
  if (!connected)
  {
    blink_period = 500;
    fade_time = 250;
  };

  uint32_t blink_phase = millis() % blink_period;
  uint8_t brightness;
  if (blink_phase < fade_time)
  {
    brightness = ((blink_phase * 255) / fade_time);
  }
  else if (blink_phase < 2 * fade_time)
  {
    brightness = (((2 * fade_time - blink_phase) * 255) / fade_time);
  }
  else
  {
    brightness = 0;
  };

  analogWrite(LED_BUILTIN, 255 - brightness);
  if (connected)
  {
    digitalWrite(LED_R, HIGH);
    analogWrite(LED_G, 255 - brightness);
  }
  else
  {
    digitalWrite(LED_G, HIGH);
    analogWrite(LED_R, 255 - brightness);
  };
}

bool apply_crontab_once = false;
void loop()
{
  do_blink();
  ui.tick();
  timeClient.update();

  Cron.delay();
  if (WiFi.status() != WL_CONNECTED)
  {

    Serial.print(".");
    connected = false;
  }
  else
  {
    if (!connected)
    {

      connected = true;
      Serial.println(WiFi.localIP());
    };
  };
  if (!timeClient.isTimeSet())
  {
    timeClient.forceUpdate();
  };
  if (!apply_crontab_once && timeClient.isTimeSet())
  {
    Serial.println(timeClient.getFormattedTime());
    Cron.delay();
    apply_crontab(crontab);
    apply_crontab_once = true;
  };

  if (digitalRead(INPUT_PIN) == LOW)
  { // When SW1 is pressed
    // IrSender.sendNEC(0x1, 0x12, 32);  // Replace with your own unique code
    Alarm();

    delay(30);
  };
}
