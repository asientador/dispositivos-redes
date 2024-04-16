#include <ArduinoHttpClient.h>
#include <b64.h>
#include <HttpClient.h>
#include <URLEncoder.h>
#include <URLParser.h>
#include <WebSocketClient.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include <NTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <ThingsBoard.h>
#include <ArduinoJson.h> // Biblioteca para manipular JSON

Adafruit_BME280 bme; // I2C

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define WIFI_AP_NAME        "wifiperuana"
#define WIFI_PASSWORD       "abascalvox"

#define TOKEN               "1234"
#define THINGSBOARD_SERVER  "192.168.79.140"
#define SERIAL_DEBUG_BAUD   115200

#define PINAZUL             33
#define PINROJO             32

#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

#define BUTTON_PIN 23 // Pin del botón

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
WiFiClient espClient;
PubSubClient client(espClient);
ThingsBoard tb(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 3600);
volatile bool buttonPressed = false;
bool subscribed = false;

int currentMenu = 0; // Variable para controlar el menú actual

void IRAM_ATTR handleButtonInterrupt() {
  buttonPressed = true;
}

// RPC handlers
RPC_Callback callbacks[] = {
    { "estadoDispositivo", estadoDispositivo },
};

RPC_Response estadoDispositivo(const RPC_Data &data)
{
  Serial.print("CUIDAO QUE HA LLEGADO RPC\n");
  return RPC_Response(NULL,true);
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  bme.begin();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, RISING);

  connectWiFi();
  connectThingsBoard();


  // Reconnect to ThingsBoard, if needed
  if (!tb.connected()) {
    subscribed = false;

    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
  }

  // Subscribe for RPC, if needed
  if (!subscribed) {
    Serial.println("Subscribing for RPC...");

    // Perform a subscription. All consequent data processing will happen in
    // callbacks as denoted by callbacks[] array.
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    Serial.println("Subscribe to callbacks done");
    subscribed = true;
  }
}

void loop() {

  // Reconnect to WiFi, if needed
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }

  // Reconnect to ThingsBoard, if needed
  if (!tb.connected()) {
    subscribed = false;

    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
  }

  // Subscribe for RPC, if needed
  if (!subscribed) {
    Serial.println("Subscribing for RPC...");

    // Perform a subscription. All consequent data processing will happen in
    // callbacks as denoted by callbacks[] array.
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    Serial.println("Subscribe done");
    subscribed = true;
  }

  
  if (buttonPressed) {
    Serial.println("¡Botón presionado!");
    buttonPressed = false;
    
    // Cambia el menú cada vez que se presiona el botón
    currentMenu++;
    if (currentMenu > 3) { // Ajusta este valor si agregas más menús
      currentMenu = 0;
    }
  }

  // Muestra el menú actual en función del valor de currentMenu
  switch (currentMenu) {
    case 0:
      showDefaultMenu();
      break;
    case 1:
      showGlobalTimeMenu();
      break;
    case 2:
      showTempHum();
      break;
    case 3:
      showEconomyNews(); // Llamada a la función para mostrar noticias de economía
      break;
    default:
      showDefaultMenu();
      break;
  }

  timeClient.update();

  float temperature = bme.readTemperature();
  int humidity = bme.readHumidity();

  // Publish temperature and humidity to ThingsBoard
  char telemetry[100];
  
  tb.sendTelemetryFloat("TemperaturaESP", temperature);
  tb.sendTelemetryInt("HumedadESP", humidity);
  Serial.printf("Enviada temperatura %f y humedad %d\n",temperature,humidity);
  //delay(100); // Ajusta el retraso según sea necesario

  tb.loop();
}

void showDefaultMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("   MENU ROTATIVO");
    lcd.setCursor(0,1);
  lcd.print(" Reloj");
      lcd.setCursor(0,2);
  lcd.print(" Temp/Hum");


  delay(1000); // Mostrar el mensaje durante 2 segundos
}

void showGlobalTimeMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("      RELOJ");
  lcd.setCursor(0,1);
  lcd.print("Espania: ");
  lcd.print(getFormattedTime(3600)); // Hora de España (CET)
  lcd.setCursor(0,2);
  lcd.print("Nueva York: ");
  lcd.print(getFormattedTime(-18000)); // Hora de Nueva York (EST)
  lcd.setCursor(0,3);
  lcd.print("Tokio: ");
  lcd.print(getFormattedTime(31500)); // Hora de Tokio (JST) con una hora menos
  delay(1000);
}

void showTempHum() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("       TEMP/HUM");
  lcd.setCursor(0,1);
  lcd.print("Temp: ");
  lcd.print(bme.readTemperature());
  lcd.print(" C");
  lcd.setCursor(0,2);
  lcd.print("Humedad: ");
  lcd.print(bme.readHumidity());
  lcd.print("%");
  delay(1000); // Puedes ajustar este retraso según sea necesario
}

void showEconomyNews() {
  Serial.println("Requesting economy news...");
  if (WiFi.status() == WL_CONNECTED) {
    HttpClient client = HttpClient(espClient, "newsapi.org", 80);
    client.get("/v2/everything?q=economia&apiKey=6a8c3c809fa74e7b819d38ec1caeddef");
    delay(1000); // Esperar un segundo para asegurar la recepción de datos
    bool firstNews = true; // Flag para identificar la primera noticia
    unsigned long startTime = millis(); // Tiempo de inicio
    String news; // Variable para almacenar la noticia
    while (client.available()) {
      news = client.readStringUntil('\n'); // Leer la noticia completa
      if (firstNews) { // Si es la primera noticia
        // Parsear el JSON para obtener el titular
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, news);
        if (!error) {
          const char* title = doc["articles"][0]["title"]; // Obtener el titular
          lcd.clear(); // Limpiar la pantalla del LCD
          lcd.setCursor(0, 0); // Establecer el cursor en la primera fila
          lcd.print(title); // Mostrar el titular en el LCD
          startTime = millis(); // Reiniciar el tiempo de inicio
          firstNews = false; // Desactivar el flag de la primera noticia
        } else {
          Serial.println("Failed to parse JSON");
        }
      } else {
        Serial.println(news); // Imprimir la noticia por el puerto serial
      }
    }
    // Esperar 3 segundos para mostrar la primera noticia
    if (!firstNews) {
      while (millis() - startTime < 3000) {
        // Esperar
      }
      lcd.clear(); // Limpiar la pantalla del LCD
      lcd.setCursor(0, 0); // Establecer el cursor en la primera fila
    }
  } else {
    Serial.println("WiFi connection failed");
  }
}


String getFormattedTime(long timeZoneOffset) {
  unsigned long currentTime = timeClient.getEpochTime() + timeZoneOffset;
  int hours = (currentTime  % 86400L) / 3600;
  int minutes = (currentTime % 3600) / 60;
  String timeString = "";
  if (hours < 10) {
    timeString += "0";
  }
  timeString += hours;
  timeString += ":";
  if (minutes < 10) {
    timeString += "0";
  }
  timeString += minutes;
  return timeString;
}

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

void connectThingsBoard() {
  Serial.println("Connecting to ThingsBoard...");
  client.setServer(THINGSBOARD_SERVER, 1883);
  while (!client.connected()) {
    if (client.connect("ESP32 Device", TOKEN, NULL)) {
      Serial.println("Connected to ThingsBoard");
    } else {
      Serial.print("Failed to connect to ThingsBoard, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}


void reconnect() {
  if (!client.connected()) {
    connectWiFi();
    connectThingsBoard();
  }
}
