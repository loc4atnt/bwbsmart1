#define TINY_GSM_MODEM_SIM7600

#include <ArduinoJson.h>
#include <WiFi.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

#define SIM_RESET_PIN 15
#define SIM_AT_BAUDRATE 115200
#define GSM_PIN ""
#define RELAYS_AMOUNT 8

int relays[RELAYS_AMOUNT] = {7,8,33,25,39,36,22,21};

// GPRS config
const char apn[] = "m-wap";
// Wifi config
//const char* ssid     = "YourWifiSSID";
//const char* password = "YourWifiPassword";
const char* ssid     = "DUC NHAM";
const char* password = "0573558170";
// MQTT config
#define PORT_MQTT 1883
const char* broker = "bsmart2.cloud.shiftr.io";
const char* mqtt_client_name = "loc4atnt Nek";
const char* mqtt_user = "bsmart2";
const char* mqtt_pass = "34DOBprzncvolzM3";
const char* topic_relays = "/bsmart/relays";

TinyGsm modem(Serial2);
TinyGsmClient gsmClient(modem);
PubSubClient gsmMqtt(gsmClient);

WiFiClient wifiClient;
PubSubClient wifiMqtt(wifiClient);

StaticJsonDocument<200> jsonObj;// đối tượng sẽ lưu dữ liệu khi đọc chuỗi JSON (Độ dài tối đa: 200)

// hàm sẽ được gọi khi có dữ liệu gửi đến esp qua mqtt
void mqttCallback(char* topic, byte* payload, unsigned int len) {
 Serial.print("Du lieu gui den [");
 Serial.print(topic);
 Serial.print("]: ");
 Serial.write(payload, len);
 Serial.println();

 //
 if (String(topic) == topic_relays){
  DeserializationError error = deserializeJson(jsonObj, payload);// đọc dữ liệu từ chuỗi json (payload)
  if (error)
    Serial.println("Xay ra loi khi doc chuoi JSON");
  else{
    // Lấy dữ liệu từ đối tượng JSON
    int id = jsonObj["id"].as<int>();
    bool state = jsonObj["state"].as<bool>();
    digitalWrite(relays[id],state);
  }
 }
}

bool isWifiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

// Hàm dùng để kết nối tới Wifi
void connectToWifi() {
  Serial.println("Bat dau ket noi wifi");
  byte times = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while ( (WiFi.status() != WL_CONNECTED) && (times < 15) ) {
    delay(1000);
    Serial.print(".");
    times++;
  }
  if (isWifiConnected()) Serial.println("\nDa ket noi wifi");
  else Serial.println("\nKhong the ket noi wifi");
}

// Hàm dùng để thiết lập modem SIM tạo kết nối GPRS
void connectToGPRS() {
  // Unlock SIM (nếu có)
  if (GSM_PIN && modem.getSimStatus() != 3)
    modem.simUnlock(GSM_PIN);

  Serial.println("Ket noi toi nha mang...");
  while (!modem.waitForNetwork(60000L)) {
    Serial.println("._.");
    delay(5000);
  }

  // Nếu không thấy sóng từ nhà mạng thì thoát hàm
  if (!modem.isNetworkConnected())
    return;

  Serial.println("Ket noi GPRS");
  if (!modem.gprsConnect(apn)) {// Hàm kết nối tới gprs trả về true/false cho biết có kết nối được hay chưa
    Serial.print("Khong the ket noi GPRS - ");
    Serial.println(apn);
    return;
  }

  // Kiểm tra lại lần nữa để chắc cú
  if (modem.isGprsConnected())
    Serial.println("Da ket noi duoc GPRS!");
}

// Hàm khởi động module SIM
bool initModemSIM() {
  Serial.println("Bat dau khoi dong module SIM");

  // Đặt chân reset của module xuống LOW để chạy
  pinMode(SIM_RESET_PIN, OUTPUT);
  digitalWrite(SIM_RESET_PIN, LOW);
  delay(5000);

  Serial2.begin(SIM_AT_BAUDRATE);// Module SIM giao tiếp với ESP qua cổng Serial2 bằng AT cmds

  if (!modem.restart()) {
    Serial.println("Khong the khoi dong lai module SIM => Co loi");
    return false;
  }
  return true;
}

// Hàm setup cho MQTT Client
void setupMQTT(PubSubClient *mqtt) {
  mqtt->setServer(broker, PORT_MQTT);
  mqtt->setCallback(mqttCallback);
}

// Hàm kết nối tới MQTT Broker
boolean connectMQTT(PubSubClient *mqtt) {
  Serial.print("Ket noi broker ");
  Serial.print(broker);
  boolean status = mqtt->connect(mqtt_client_name, mqtt_user, mqtt_pass);
  if (status == false) {
    Serial.println(" khong thanh cong!");
    return false;
  }
  Serial.println(" thanh cong!");

  mqtt->subscribe(topic_relays);// Đăng ký nhận dữ liệu từ topic topic_relays
  return mqtt->connected();
}

// Hàm xử lý trong loop() dành cho MQTT Client dùng wifi
void mqttLoopWithWifi() {
  // Vì dùng wifi, nên nếu có kết nối GPRS với MQTT thì ngắt kết nối
  if (gsmMqtt.connected()) gsmMqtt.disconnect();

  if (!wifiMqtt.connected()) {
    Serial.println("Ket noi MQTT voi Wifi");
    while ( (!wifiMqtt.connected()) && isWifiConnected()) {
      connectMQTT(&wifiMqtt);
      delay(5000);
    }
  } else
    wifiMqtt.loop();// Hàm xử lý của thư viện mqtt
}

// Hàm xử lý trong loop() dành cho MQTT Client dùng GPRS
void mqttLoopWithGPRS() {
  // Vì dùng GPRS, nên nếu có kết nối Wifi với MQTT thì ngắt kết nối
  if (wifiMqtt.connected()) wifiMqtt.disconnect();

  if (!gsmMqtt.connected()) {
    Serial.println("Ket noi MQTT voi GPRS");
    while ( (!gsmMqtt.connected()) && modem.isGprsConnected()) {
      connectMQTT(&gsmMqtt);
      delay(5000);
    }
  } else
    gsmMqtt.loop();// Hàm xử lý của thư viện mqtt
}

void setup() {
  Serial.begin(115200);

  // pinmode, tắt các rơ le khi mới khởi động
  for (int i =0;i<RELAYS_AMOUNT;i++){
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i], LOW);
  }

  connectToWifi();

  if (initModemSIM())
    connectToGPRS();

  // Có 2 đối tượng MQTT Client: 1 dành cho kết nối Wifi, 1 dành cho GPRS
  setupMQTT(&wifiMqtt);
  setupMQTT(&gsmMqtt);
}

void loop() {
  if (isWifiConnected()) {// Ưu tiên đầu, nếu có wifi thì xử lý kết nối mqtt với wifi
    mqttLoopWithWifi();
  } else if (modem.isGprsConnected()) {// Còn khi không có wifi thì chuyển qua dùng GPRS
    mqttLoopWithGPRS();
  } else {
    Serial.println("Khong co ket noi ca wifi va GRPS!");
    delay(2000);
  }
}
