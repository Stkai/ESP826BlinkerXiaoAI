#define BLINKER_PRINT Serial
#define BLINKER_WIFI
#define BLINKER_MIOT_LIGHT // Blinker 小爱同学接入

#include "../include/main.h"

#define BUTTON_REST 0             // 重置按钮引脚  GPIO0 D3
#define SERVO_BUILTIN 3           // 舵机引脚     GPIO3 RX
#define EEPROM_START_ADDRESS 2448 // 存储起始位置  Blinker占用 0-2447

char ssid[32]; // 网络名称
char pswd[64]; // 网络密码
char auth[16]; // Blinker 密钥

int defDegrees = 90;  // 默认位置
int onDegrees = 45;   // 开灯位置
int offDegrees = 135; // 关灯位置

const IPAddress LocalIp(192, 168, 1, 1);

const char *ssidConfig = "ESP8266_Setup"; //网络配置名称

DNSServer dnsServer;

boolean isSettingMode; // 设置模式
String ssidList;

Servo ServoLamp;

BlinkerButton buttonLamp("btn-lamp");     // Blinker 组件键名
BlinkerNumber numberCount("num-counter"); // 点击计数按键
int numberCounter = 0;                   // 点击计数按键计数器

ESP8266WebServer webServer(80);

OneButton buttonRest = OneButton(BUTTON_REST, true); // 恢复设置按钮

void setup() {
    Serial.begin(74880);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    buttonRest.reset();
    buttonRest.setPressTicks(5000);
    buttonRest.attachLongPressStop(handleResetButtonPressStop);
    buttonRest.attachDuringLongPress(HandleRestButtonDuringLongPress);

    if (restoreConfig()) {
        isSettingMode = false;

        // 初始化LED
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, HIGH);

        // 初始化blinker
        Blinker.begin(auth, ssid, pswd);
        Blinker.attachData(dataRead);
        buttonLamp.attach(switchCallback);

        //舵机初始化
        ServoLamp.attach(SERVO_BUILTIN);
        ServoLamp.write(defDegrees);
        delay(100);
        ServoLamp.detach();
        // 小爱电源操作回调函数
        BlinkerMIOT.attachPowerState(switchCallback);

#if defined(BLINKER_PRINT)
        BLINKER_DEBUG.stream(BLINKER_PRINT);
#endif
        return;
    }
    isSettingMode = true;
    setupMode();
}

void loop() {
    buttonRest.tick();

    if (isSettingMode) {
        dnsServer.processNextRequest();
        webServer.handleClient();
    } else {
        Blinker.run();
    }
}

// 按下按键即会执行该函数
void switchCallback(const String &state) {
    BLINKER_LOG("电灯状态: ", state);
    numberCounter++;
    numberCount.print(numberCounter);
    ServoLamp.attach(SERVO_BUILTIN);
    if (state == BLINKER_CMD_ON) {
        ServoLamp.write(onDegrees);
        buttonLamp.print(BLINKER_CMD_ON);

        BlinkerMIOT.powerState(BLINKER_CMD_ON);
        BlinkerMIOT.print();

        ledBlink(100, 5);
        ServoLamp.write(defDegrees);
    }
    if (state == BLINKER_CMD_OFF) {
        digitalWrite(LED_BUILTIN, HIGH);

        ServoLamp.write(offDegrees);
        buttonLamp.print(BLINKER_CMD_OFF);

        BlinkerMIOT.powerState(BLINKER_CMD_OFF);
        BlinkerMIOT.print();

        ledBlink(100, 5);

        ServoLamp.write(defDegrees);
    }
    ServoLamp.detach();
}

// 如果未绑定的组件被触发，则会执行其中内容
void dataRead(const String &data) {
    BLINKER_LOG("Blinker readString: ", data);
    int index = data.lastIndexOf(" ");
    int value = data.substring(index).toInt();

    if (strstr(data.c_str(), "def") != nullptr) {
        defDegrees = value;
        saveServoDegrees(EEPROM_START_ADDRESS + 112, value);
    }
    if (strstr(data.c_str(), "on") != nullptr) {
        saveServoDegrees(EEPROM_START_ADDRESS + 113, value);
        onDegrees = value;
    }
    if (strstr(data.c_str(), "off") != nullptr) {
        saveServoDegrees(EEPROM_START_ADDRESS + 114, value);
        offDegrees = value;
    }

    numberCounter++;
    numberCount.print(numberCounter);
}

/**
 * @brief 恢复保存的配置
 *
 * @return true 读取成功
 * @return false 读取失败
 */
bool restoreConfig() {
    EEPROM.begin(4096);
    Serial.println("读取EEPROM中....");
    if (EEPROM.read(EEPROM_START_ADDRESS + 0) != 255) {
        EEPROM.get(EEPROM_START_ADDRESS + 0, ssid);
        EEPROM.get(EEPROM_START_ADDRESS + 32, pswd);
        EEPROM.get(EEPROM_START_ADDRESS + 96, auth);
        defDegrees = EEPROM.read(EEPROM_START_ADDRESS + 112);
        onDegrees = EEPROM.read(EEPROM_START_ADDRESS + 113);
        offDegrees = EEPROM.read(EEPROM_START_ADDRESS + 114);

        Serial.printf("SSID:%s\n", ssid);
        Serial.printf("pswd:%s\n", pswd);
        Serial.printf("auth:%s\n", auth);
        EEPROM.end();
        return true;
    } else {
        Serial.println("读取配置失败!");
        return false;
    }
}

/**
 * @brief 设置模式
 *
 */
void setupMode() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int8_t n = WiFi.scanNetworks();
    delay(100);
    Serial.println("");
    for (int i = 0; i < n; ++i) {
        ssidList += R"(<option value=")" + WiFi.SSID(i) + R"(">)" + WiFi.SSID(i) +
                    R"(</option>)";
    }
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(LocalIp, LocalIp, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssidConfig);
    dnsServer.start(53, "*", LocalIp);
    startWebServer();
    Serial.print("启动无线AP \"");
    Serial.print(ssidConfig);
    Serial.println("\"");
}

/**
 * @brief 启动网络配置服务
 *
 */
void startWebServer() {

    Serial.print("启动网络服务中.... ");
    Serial.println(WiFi.softAPIP());

    webServer.onNotFound([]() {
        String s = R"(
        <h1>Wi-Fi 设置</h1>
        <p>请选择你的网络名称,输入密码和Blinker密钥</p>
        <form action="/setap" method="post">
        <label>网络名称: </label><select name="ssid">)" +
                   ssidList +
                   R"(
        </select><br>
        网络密码: <input name="pass" length=64 type="pswd"><br>
        点灯密钥: <input name="auth" length=12 type="text">
        <input type="submit">
        </form>
        )";
        webServer.send(200, "text/html", makePage("网络设置", s));
    });
    webServer.on("/setap", HTTP_POST, []() {
        strcpy(ssid, webServer.arg("ssid").c_str());
        Serial.print("SSID: ");
        Serial.println(ssid);
        strcpy(pswd, webServer.arg("pass").c_str());
        Serial.print("pswd: ");
        Serial.println(pswd);
        strcpy(auth, webServer.arg("auth").c_str());
        Serial.printf("auth:%s\n", auth);

        Serial.println("开始写入配置信息.....");
        EEPROM.put(EEPROM_START_ADDRESS + 0, ssid);
        EEPROM.put(EEPROM_START_ADDRESS + 32, pswd);
        EEPROM.put(EEPROM_START_ADDRESS + 96, auth);
        EEPROM.end();
        String ssid_tmp = ssid;
        Serial.println("配置信息写入完成。");
        String s = R"(
        <h1>配置完成.</h1>
        <p>重启后连接到 ")" +
                   ssid_tmp +
                   R"(")";
        webServer.send(200, "text/html", makePage("Wi-Fi 设置完成", s));

        EspClass::restart();
    });
    webServer.begin();
}

String makePage(String title, String contents) {
    String s = "<!DOCTYPE html><html><head>";
    s += R"(<meta name="viewport" content="width=device-width,user-scalable=0">)";
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
void handleResetButtonPressStop() {
    Serial.println("正在重置设备......");
    EEPROM.begin(4096);
    EEPROM.write(EEPROM_START_ADDRESS + 0, 255);
    EEPROM.end();
    Serial.print("重置完成,设备正在重启......");
    EspClass::restart();
}

/**
 * @brief 按键长按中
 *
 */
void HandleRestButtonDuringLongPress() {
    ledBlink(50, 1);
}

/**
 * @brief LED 闪烁
 *
 * @param intervel 时间间隔
 * @param number 闪烁次数
 */
void ledBlink(int intervel, int number) {
    for (int i = 0; i < number; i++) {
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
void saveServoDegrees(int address, int degrees) {
    if (0 <= degrees && degrees <= 180) {
        EEPROM.begin(4096);
        EEPROM.write(address, degrees);
        EEPROM.end();
        Blinker.print("保存舵机位置:", degrees);
        ServoLamp.attach(SERVO_BUILTIN);
        ServoLamp.write(degrees);
        delay(1000);
        ServoLamp.write(defDegrees);
        ServoLamp.detach();
    } else {
        Blinker.print("参数错误,舵机位置保存失败!");
    }
}
