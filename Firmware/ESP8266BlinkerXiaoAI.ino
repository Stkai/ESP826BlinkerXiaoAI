#define BLINKER_PRINT Serial
#define BLINKER_WIFI
#define BLINKER_MIOT_LIGHT // Blinker 小爱同学接入

#include <Blinker.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <OneButton.h>
#include <Servo.h>
#include <EEPROM.h>

#define BUTTON_REST 0             // 重置按钮引脚  GPIO0 D3
#define SERVO_BUILTIN 3           // 舵机引脚     GPIO3 RX
#define EEPROM_START_ADDRESS 2448 // 存储起始位置  Blinker占用 0-2447

char ssid[32]; // 网络名称
char pswd[64]; // 网络密码
char auth[16]; // Blinker 密钥

int def_degrees = 90;  // 默认位置
int on_degrees = 45;   // 开灯位置
int off_degrees = 135; // 关灯位置

const IPAddress LocalIp(192, 168, 1, 1);

const char *ssid_config = "ESP8266_Setup"; //网络配置名称

DNSServer dnsServer;

boolean is_setting_mode; // 设置模式
String ssid_list;

Servo ServoLamp;

BlinkerButton ButtonLamp("btn-lamp");     // Blinker 组件键名
BlinkerNumber NumberCount("num-counter"); // 点击计数按键
int number_counter = 0;                   // 点击计数按键计数器

ESP8266WebServer WebServer(80);

OneButton ButtonRest = OneButton(BUTTON_REST, true); // 恢复设置按钮

void setup()
{
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  ButtonRest.reset();
  ButtonRest.setPressTicks(5000);
  ButtonRest.attachLongPressStop(HandleResetButtonPressStop);
  ButtonRest.attachDuringLongPress(HandleRestButtonDuringLongPress);

  if (restoreConfig())
  {
    is_setting_mode = false;

    // 初始化LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // 初始化blinker
    Blinker.begin(auth, ssid, pswd);
    Blinker.attachData(dataRead);
    ButtonLamp.attach(switch_callback);

    //舵机初始化
    ServoLamp.attach(SERVO_BUILTIN);
    ServoLamp.write(def_degrees);
    delay(100);
    ServoLamp.detach();
    // 小爱电源操作回调函数
    BlinkerMIOT.attachPowerState(switch_callback);

#if defined(BLINKER_PRINT)
    BLINKER_DEBUG.stream(BLINKER_PRINT);
#endif
    return;
  }
  is_setting_mode = true;
  setupMode();
}

void loop()
{
  ButtonRest.tick();

  if (is_setting_mode)
  {
    dnsServer.processNextRequest();
    WebServer.handleClient();
  }
  else
  {
    Blinker.run();
  }
}

// 按下按键即会执行该函数
void switch_callback(const String &state)
{
  BLINKER_LOG("电灯状态: ", state);
  number_counter++;
  NumberCount.print(number_counter);
  ServoLamp.attach(SERVO_BUILTIN);
  if (state == BLINKER_CMD_ON)
  {
    ServoLamp.write(on_degrees);
    ButtonLamp.print(BLINKER_CMD_ON);

    BlinkerMIOT.powerState(BLINKER_CMD_ON);
    BlinkerMIOT.print();

    LEDBlinker(100, 5);
    ServoLamp.write(def_degrees);
  }
  if (state == BLINKER_CMD_OFF)
  {
    digitalWrite(LED_BUILTIN, HIGH);

    ServoLamp.write(off_degrees);
    ButtonLamp.print(BLINKER_CMD_OFF);

    BlinkerMIOT.powerState(BLINKER_CMD_OFF);
    BlinkerMIOT.print();

    LEDBlinker(100, 5);

    ServoLamp.write(def_degrees);
  }
  ServoLamp.detach();
}

// 如果未绑定的组件被触发，则会执行其中内容
void dataRead(const String &data)
{
  BLINKER_LOG("Blinker readString: ", data);
  int index = data.lastIndexOf(" ");
  int value = data.substring(index).toInt();

  if (strstr(data.c_str(), "def") != NULL)
  {
    def_degrees = value;
    save_servo_degrees(EEPROM_START_ADDRESS + 112, value);
  }
  if (strstr(data.c_str(), "on") != NULL)
  {
    save_servo_degrees(EEPROM_START_ADDRESS + 113, value);
    on_degrees = value;
  }
  if (strstr(data.c_str(), "off") != NULL)
  {
    save_servo_degrees(EEPROM_START_ADDRESS + 114, value);
    off_degrees = value;
  }

  number_counter++;
  NumberCount.print(number_counter);
}

/**
 * @brief 恢复保存的配置
 *
 * @return true 读取成功
 * @return false 读取失败
 */
bool restoreConfig()
{
  EEPROM.begin(4096);
  Serial.println("读取EEPROM中....");
  if (EEPROM.read(EEPROM_START_ADDRESS + 0) != 255)
  {
    EEPROM.get(EEPROM_START_ADDRESS + 0, ssid);
    EEPROM.get(EEPROM_START_ADDRESS + 32, pswd);
    EEPROM.get(EEPROM_START_ADDRESS + 96, auth);
    def_degrees = EEPROM.read(EEPROM_START_ADDRESS + 112);
    on_degrees = EEPROM.read(EEPROM_START_ADDRESS + 113);
    off_degrees = EEPROM.read(EEPROM_START_ADDRESS + 114);

    Serial.printf("SSID:%s\n", ssid);
    Serial.printf("pswd:%s\n", pswd);
    Serial.printf("auth:%s\n", auth);
    EEPROM.end();
    return true;
  }
  else
  {
    Serial.println("读取配置失败!");
    return false;
  }
}

/**
 * @brief 设置模式
 *
 */
void setupMode()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("");
  for (int i = 0; i < n; ++i)
  {
    ssid_list += R"(<option value=")" + WiFi.SSID(i) + R"(">)" + WiFi.SSID(i) +
                 R"(</option>)";
  }
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(LocalIp, LocalIp, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid_config);
  dnsServer.start(53, "*", LocalIp);
  startWebServer();
  Serial.print("启动无线AP \"");
  Serial.print(ssid_config);
  Serial.println("\"");
}

/**
 * @brief 启动网络配置服务
 *
 */
void startWebServer()
{

  Serial.print("启动网络服务中.... ");
  Serial.println(WiFi.softAPIP());

  WebServer.onNotFound([]()
                       {
      String s =  R"(
        <h1>Wi-Fi 设置</h1>
        <p>请选择你的网络名称,输入密码和Blinker密钥</p>
        <form action="/setap" method="post">
        <label>网络名称: </label><select name="ssid">)" +
                 ssid_list +
                 R"(
        </select><br>
        网络密码: <input name="pass" length=64 type="pswd"><br>
        点灯密钥: <input name="auth" length=12 type="text">
        <input type="submit">
        </form>
        )";
      WebServer.send(200, "text/html", makePage("网络设置", s)); });
  WebServer.on("/setap", HTTP_POST, []()
               {
      strcpy(ssid,WebServer.arg("ssid").c_str());
      Serial.print("SSID: ");
      Serial.println(ssid);
      strcpy(pswd, WebServer.arg("pass").c_str());
      Serial.print("pswd: ");
      Serial.println(pswd);
      strcpy(auth,WebServer.arg("auth").c_str());
      Serial.printf("auth:%s\n", auth);

      Serial.println("开始写入配置信息.....");
      EEPROM.put(EEPROM_START_ADDRESS + 0,ssid);
      EEPROM.put(EEPROM_START_ADDRESS + 32,pswd);
      EEPROM.put(EEPROM_START_ADDRESS + 96,auth);
      EEPROM.end();
      String ssid_tmp = ssid;
      Serial.println("配置信息写入完成。");
      String s = R"(
        <h1>配置完成.</h1>
        <p>重启后连接到 ")" +
        ssid_tmp +
        R"(")";
      WebServer.send(200, "text/html", makePage("Wi-Fi 设置完成", s));
      
      ESP.restart(); });
  WebServer.begin();
}
String makePage(String title, String contents)
{
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

/**
 * @brief 重置按键释放
 *
 */
static void HandleResetButtonPressStop()
{
  Serial.println("正在重置设备......");
  EEPROM.begin(4096);
  EEPROM.write(EEPROM_START_ADDRESS + 0, 255);
  EEPROM.end();
  Serial.print("重置完成,设备正在重启......");
  ESP.restart();
}

/**
 * @brief 按键长按中
 *
 */
static void HandleRestButtonDuringLongPress()
{
  LEDBlinker(50, 1);
}

/**
 * @brief LED 闪烁
 *
 * @param intervel 时间间隔
 * @param number 闪烁次数
 */
static void LEDBlinker(int intervel, int number)
{
  for (int i = 0; i < number; i++)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(intervel);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(intervel);
  }
}

/**
 * @brief 保存舵机位置
 *
 * @param address 地址
 * @param degrees 角度
 */
static void save_servo_degrees(int address, int degrees)
{
  if (0 <= degrees <= 180)
  {
    EEPROM.begin(4096);
    EEPROM.write(address, degrees);
    EEPROM.end();
    Blinker.print("保存舵机位置:", degrees);
    ServoLamp.attach(SERVO_BUILTIN);
    ServoLamp.write(degrees);
    delay(1000);
    ServoLamp.write(def_degrees);
    ServoLamp.detach();
  }
  else
  {
    Blinker.print("参数错误,舵机位置保存失败!");
  }
}
