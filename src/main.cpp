#include <ETH.h>
#include <WiFiClient.h>
#include <Arduino.h>
#include <ModbusRTU.h>

// Настройки сети
IPAddress localIP(10, 190, 195, 202);
IPAddress gateway(10, 190, 195, 254);
IPAddress subnet(255, 255, 255, 0);

// Параметры сервера
const char* serverIP = "10.190.195.201";
const int serverPort = 80;
const char* payload = "QUESTION=30020130020330020830030130030230030330030430030630030730030830030930030a300701300702300703300704300707300708300709300714300715300716300501300502300503300504300505300506300507300508300509300e03300e2a31130131130331130431130531130731130831130931130a31130b31130c31130d31130e31130f31131031131131131231131331131431131531131631131731131831131931131a31131b31131c31131d31131e31131f31132031132131132231132331132431132531132631132731132831132931132a31132b31132c31132d31132e31132f31133031133131133231133331133431133531133631133731133831133931133a31133b31133c31133d31133e31133f31134031134131134231134331134431134531134631134731134831134931134a31134b31134c31134d31134e31134f31135031135131135231135331135431135531135631135731135831135931135a31135b31135c31135d31135e31135f31136031136131136231136331136431136531136631136731140131140231140331140431140531140631140731140831140931140a31140b31140c31140d31140e31140f311410311411311412300901300906300907300908300909300108"; // Полный payload из вашего запроса
// const char* payload = "QUESTION=3002013002"; // Полный payload из вашего запроса

// Holding Registers со сдвигом на 1 регистр меньше
const int maschineState = 16790; //статус
const int pressure = 16388; //давление
const int temperature = 16386; //темперетура

WiFiClient client;
unsigned long lastRequestTime = 0;
const long requestInterval = 10000; // 10 секунд

// Массив для хранения HEX-чисел в числовом формате
const int ARRAY_SIZE = 115;
uint32_t hexArray[ARRAY_SIZE] = {0};
int currentIndex = 0;

// настройки для modbus
#define SLAVE_ADDR 15
#define DE_RE_PIN 4  // Пин управления направлением RS485

ModbusRTU mb;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 5, 17);  // UART2: RX=5, TX=17
  // Инициализация Ethernet
  ETH.begin();
  ETH.config(localIP, gateway, subnet);
  
  while (!ETH.linkUp()) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nEthernet подключен!");

  mb.begin(&Serial2, DE_RE_PIN);  // Инициализация Modbus с управлением DE/RE
  mb.setBaudrate(115200);         // Установка битрейта
  mb.slave(SLAVE_ADDR);           // Установка адреса Slave

  // Регистрация обработчика для Holding Registers со сдвигом на 1 регистр меньше
  mb.addHreg(maschineState, 1234);     // Инициализация регистра значением 1234
  mb.addHreg(pressure, 1234);
  mb.addHreg(temperature, 1234);

  Serial.println("Modbus RTU Slave Started");
}

void sendRequest() {
  if (client.connect(serverIP, serverPort)) {
    client.println("POST /cgi-bin/mkv.cgi HTTP/1.1");
    client.println("Host: 10.190.195.201");
    client.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
    client.println("X-Requested-With: XMLHttpRequest");
    client.println("Cookie: LangName=Russian");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(strlen(payload));
    client.println();
    client.println(payload);
    
    Serial.println("Запрос отправлен");
  } else {
    Serial.println("Ошибка подключения");
  }
}

String readChunkedResponse() {
  String fullData;
  bool isChunked = false;

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Transfer-Encoding: chunked")) isChunked = true;
    if (line == "\r") break;
  }

  if (isChunked) {
    while (client.connected()) {
      String chunkSizeLine = client.readStringUntil('\n');
      chunkSizeLine.trim();
      int chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);
      
      if (chunkSize <= 0) break;
      
      for (int i = 0; i < chunkSize; i++) {
        fullData += (char)client.read();
      }
      client.readStringUntil('\n');
    }
  }
  return fullData;
}

bool parseHexBlock(const String &block, uint32_t &result) {
  char* endPtr;
  const char *hexStr = block.c_str();
  result = strtoul(hexStr, &endPtr, 16);
  
  // Проверка ошибок конвертации
  return (endPtr != hexStr) && (*endPtr == '\0');
}




void parseData(const String &rawData) {
  String cleanData = rawData;
  cleanData.replace("X", "");
  cleanData.trim();
  currentIndex = 0;

  // Обработка блоками по 8 символов
  for (int i = 0; i < cleanData.length() && currentIndex < ARRAY_SIZE; i += 8) {
    String block = cleanData.substring(i, i + 8);
    String hexPart = block.endsWith("0080") ? block.substring(0,4) : block;

    // Конвертация и запись в массив
    uint32_t value;
    if (parseHexBlock(hexPart, value)) {
      hexArray[currentIndex++] = value;
    } else {
      Serial.print("Ошибка конвертации: ");
      Serial.println(hexPart);
    }



  if (hexArray[114] == 28){
    mb.Hreg(maschineState, 9);
  }
  if (hexArray[114] == 21){
    mb.Hreg(maschineState, 5);
  }
  if (hexArray[114] == 8 || hexArray[114] == 9){
    mb.Hreg(maschineState, 12);
  }
  if (hexArray[114] == 16){
    mb.Hreg(maschineState, 7);
  }
  mb.Hreg(pressure, hexArray[0]);
  mb.Hreg(temperature, hexArray[1]);
  }

  // Вывод массива
  Serial.println("\nМассив HEX значений:");
  for (int i = 0; i < currentIndex; i++) {
    Serial.print("Index ");
    Serial.print(i);
    Serial.print(": 0x");
    Serial.print(hexArray[i], HEX); // Вывод в HEX-формате
    Serial.print(" (DEC: ");
    Serial.print(hexArray[i]);
    Serial.println(")");
  }


}



void loop() {
  mb.task();
  if (millis() - lastRequestTime >= requestInterval) {
    if (client.connected()) client.stop();
    
    sendRequest();
    lastRequestTime = millis();
  }

  if (client.available()) {
    String response = readChunkedResponse();
    parseData(response);
    client.stop();
  }
}