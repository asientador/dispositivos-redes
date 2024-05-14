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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

Adafruit_BME280 bme; // I2C

#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#define WIFI_AP_NAME "wifiperuana"
#define WIFI_PASSWORD "abascalvox"

#define TOKEN "1234"
#define THINGSBOARD_SERVER "192.168.137.125"
#define SERIAL_DEBUG_BAUD 115200

#define PINAZUL 33
#define PINROJO 32

#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

#define BUTTON_PIN 23         // Pin del botón
const int MAX_CATEGORIAS = 7; // Número máximo de categorías
const int MAX_CIUDADES = 7;   // Número máximo de categorías

struct TablaCategoria
{
  std::string categorias[MAX_CATEGORIAS];
  bool estados[MAX_CATEGORIAS];

  // Constructor para inicializar la tabla con categorías predeterminadas
  TablaCategoria()
  {
    // Categorías predeterminadas
    categorias[0] = "x";
    categorias[1] = "business";
    categorias[2] = "entertainment";
    categorias[3] = "health";
    categorias[4] = "science";
    categorias[5] = "sports";
    categorias[6] = "technology";

    // Establecer todas las categorías como activadas por defecto
    for (int i = 0; i < MAX_CATEGORIAS; ++i)
    {
      estados[i] = false;
    }
  }

  // Función para imprimir la tabla y el estado de activación de cada categoría
  void imprimirTabla()
  {
    Serial.println("Tabla de Categorías:");
    for (int i = 0; i < MAX_CATEGORIAS; ++i)
    {
      Serial.printf("%s", categorias[i].c_str());
      Serial.print(": ");
      Serial.println(estados[i] ? "Activada" : "Desactivada");
    }
  }
};
struct TablaCiudades
{
  std::string ciudades[MAX_CIUDADES];
  bool estados[MAX_CIUDADES];
  int numeroActivo;

  // Constructor para inicializar la tabla con categorías predeterminadas
  TablaCiudades()
  {
    // Categorías predeterminadas
    ciudades[0] = "x";
    ciudades[1] = "madrid";
    ciudades[2] = "tokyo";
    ciudades[3] = "paris";
    ciudades[4] = "new_york";
    ciudades[5] = "los_angeles";
    ciudades[6] = "rome";

    numeroActivo = 0;

    // Establecer todas las categorías como activadas por defecto
    for (int i = 0; i < MAX_CIUDADES; ++i)
    {
      estados[i] = false;
    }
  }

  // Función para imprimir la tabla y el estado de activación de cada categoría
  void imprimirTabla()
  {
    Serial.println("Tabla de Ciudades:");
    for (int i = 0; i < MAX_CIUDADES; ++i)
    {
      Serial.printf("%s", ciudades[i].c_str());
      Serial.print(": ");
      Serial.println(estados[i] ? "Activada" : "Desactivada");
      //Serial.println("numeroactivo -> " + numeroActivo);
    }
  }
};

TablaCategoria tablaCategorias;
TablaCiudades tablaCiudades;

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
WiFiClient espClient;
WiFiClient espClientHora;

PubSubClient client(espClient);
ThingsBoard tb(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 3600);
volatile bool buttonPressed = false;
bool subscribed = false;

int currentMenu = 0; // Variable para controlar el menú actual

void IRAM_ATTR handleButtonInterrupt()
{
  buttonPressed = true;
}

// RPC handlers
RPC_Callback callbacks[] = {
    {"estadoDispositivo", estadoDispositivo},
    {"setCategoria", processSetCategoriaState},
    {"getCategoria", processGetCategoriaState},
    {"setCiudad", processSetCiudadState},
    {"getCiudad", processGetCiudadState},
    {"alarmaHumedad", alarmaHumedad},
};

RPC_Response alarmaHumedad(const RPC_Data &data)
{

  Serial.println("\nMe ha llegado la alarma de humedad \n");
  lcd.clear();
  lcd.setCursor(0, 1);
  char message1[] = "  HA SALTADO LA";
  for (int i = 0; i < strlen(message1); i++)
  {
    lcd.print(message1[i]);
    delay(100); // Pequeño retraso para el efecto de animación
  }

  lcd.setCursor(0, 2);
  char message2[] = "    ALARMA";
  for (int i = 0; i < strlen(message2); i++)
  {
    lcd.print(message2[i]);
    delay(100); // Pequeño retraso para el efecto de animación
  }

  delay(3000); // Mostrar el mensaje durante 5 segundos

  return RPC_Response(NULL, data);
}

RPC_Response estadoDispositivo(const RPC_Data &data)
{
  Serial.print("RPC ESTADO DISPOSITIVO\n");
  return RPC_Response(NULL, true);
}
int tipoNoticia = 0;
RPC_Response processSetCategoriaState(const RPC_Data &data)
{
  Serial.println("RPC SET CATEGORIAS\n");

  int indice = data["pin"];
  bool estado = data["enabled"];

  if (indice < MAX_CATEGORIAS)
  {
    tablaCategorias.estados[indice] = estado;
    tablaCategorias.imprimirTabla();
  }

  Serial.printf("Voy a devolver %d %d", data["pin"], data["enabled"]);
  tipoNoticia=indice;
  return RPC_Response(std::to_string(indice).c_str(), estado);
}

RPC_Response processGetCategoriaState(const RPC_Data &data)
{
  Serial.println("Received the set delay RPC method");

  // Process data

  return RPC_Response(NULL, true);
}

RPC_Response processSetCiudadState(const RPC_Data &data)
{
  Serial.println("RPC SET CIUDAD\n");

  int indice = data["pin"];
  bool estado = data["enabled"];

  Serial.printf("\n\n ESTADO -> %d, INDICE -> %d\n \n", estado, indice);

  if (indice < MAX_CIUDADES)
  {

    tablaCiudades.estados[indice] = estado;
  }

  if (estado == 1)
  {
    tablaCiudades.numeroActivo = indice;

    Serial.println("\n HEMOS PASADO DEL IF \n");
    for (int i = 0; i < MAX_CIUDADES; i++)
    {
      if (i != indice)
      {
        tablaCiudades.estados[i] = false;
      }
    }
  }
  tablaCiudades.imprimirTabla();
  Serial.printf("Voy a devolver %d %d", data["pin"], data["enabled"]);
  return RPC_Response(std::to_string(indice).c_str(), estado);
}

RPC_Response processGetCiudadState(const RPC_Data &data)
{

  return RPC_Response(NULL, true);
}

String getNetworkTime_default()
{

  HttpClient client = HttpClient(espClientHora, "worldtimeapi.org", 80);
  switch (tablaCiudades.numeroActivo)
  {
  case 1:
    lcd.setCursor(0, 0);
    lcd.printf("MA:");
            client.get("/api/timezone/Europe/Madrid"); // Ajusta la zona horaria según sea necesario

    break;
  case 2:
    lcd.setCursor(0, 0);
    lcd.printf("TK:");
            client.get("/api/timezone/Asia/Tokyo"); // Ajusta la zona horaria según sea necesario

    break;
  case 3:
    lcd.setCursor(0, 0);
    lcd.printf("PA:");
            client.get("/api/timezone/Europe/Paris"); // Ajusta la zona horaria según sea necesario

    break;
  case 4:
    lcd.setCursor(0, 0);
    lcd.printf("NY");
            client.get("/api/timezone/America/New_York"); // Ajusta la zona horaria según sea necesario

    break;
  case 5:
    lcd.setCursor(0, 0);
    lcd.printf("LA:");
        client.get("/api/timezone/America/Los_Angeles"); // Ajusta la zona horaria según sea necesario

    break;
  case 6:
    lcd.setCursor(0, 0);
    lcd.printf("RM:");
        client.get("/api/timezone/Europe/Rome"); // Ajusta la zona horaria según sea necesario

    break;
  default:
    lcd.setCursor(0, 0);
    lcd.printf("IP:");
    client.get("/api/ip"); // Ajusta la zona horaria según sea necesario
    break;

  }
    int statusCode = client.responseStatusCode();
    if (statusCode == 200)
    {
      String response = client.responseBody();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      String datetime = doc["datetime"].as<String>();
      datetime = datetime.substring(0, 16); // Extrae solo la fecha y hora

      // Reorganizar el formato de "AAAA-MM-DDTHH:MM" a "DD-MM-AAAA HH:MM"
      String year = datetime.substring(0, 4);
      String month = datetime.substring(5, 7);
      String day = datetime.substring(8, 10);
      String time = datetime.substring(11, 16);

      String formattedDateTime = day + "-" + month + "-" + year + " " + time;
      lcd.setCursor(4,0);
      lcd.print(formattedDateTime);
      return formattedDateTime;
    }
    else
    {
      Serial.print("Error al obtener la hora: ");
      Serial.println(statusCode);
    }
  }


void ejemploThread(void *parameter) {
  while (true) {
    Serial.println("Este es un ejemplo de thread ejecutándose.");
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Espera de 1 segundo
  }
}

  void setup()
  {
    Serial.begin(SERIAL_DEBUG_BAUD);
    Wire.begin();
    lcd.init();
    lcd.backlight();
    bme.begin();

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, RISING);

    connectWiFi();
    connectThingsBoard();

  xTaskCreate(
    ejemploThread,    // Nombre de la función que implementa el thread
    "EjemploThread",  // Nombre del thread
    10000,            // Tamaño de la pila (stack size)
    NULL,             // Parámetro pasado a la función del thread
    1,                // Prioridad del thread
    NULL              // Handle del thread (opcional, puede ser NULL)
  );

    // Reconnect to ThingsBoard, if needed
    if (!tb.connected())
    {
      subscribed = false;

      // Connect to the ThingsBoard
      Serial.print("Connecting to: ");
      Serial.print(THINGSBOARD_SERVER);
      Serial.print(" with token ");
      Serial.println(TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN))
      {
        Serial.println("Failed to connect");
        return;
      }
    }

    // Subscribe for RPC, if needed
    if (!subscribed)
    {
      Serial.println("Subscribing for RPC...");

      // Perform a subscription. All consequent data processing will happen in
      // callbacks as denoted by callbacks[] array.
      if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks)))
      {
        Serial.println("Failed to subscribe for RPC");
        return;
      }

      Serial.println("Subscribe to callbacks done");
      subscribed = true;
    }

    tablaCategorias.imprimirTabla();
  }

  void loop()
  {

    // Reconnect to WiFi, if needed
    if (WiFi.status() != WL_CONNECTED)
    {
      reconnect();
      return;
    }

    // Reconnect to ThingsBoard, if needed
    if (!tb.connected())
    {
      subscribed = false;

      // Connect to the ThingsBoard
      Serial.print("Connecting to: ");
      Serial.print(THINGSBOARD_SERVER);
      Serial.print(" with token ");
      Serial.println(TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN))
      {
        Serial.println("Failed to connect");
        return;
      }
    }

    // Subscribe for RPC, if needed
    if (!subscribed)
    {
      Serial.println("Subscribing for RPC...");

      // Perform a subscription. All consequent data processing will happen in
      // callbacks as denoted by callbacks[] array.
      if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks)))
      {
        Serial.println("Failed to subscribe for RPC");
        return;
      }

      Serial.println("Subscribe done");
      subscribed = true;
    }

    if (buttonPressed)
    {
      Serial.println("¡Botón presionado!");
      buttonPressed = false;

      currentMenu = (currentMenu + 1) % 4; // Cambiar 4 por el número total de menús
    }

    // Muestra el menú actual en función del valor de currentMenu
    switch (currentMenu)
    {
    case 0:
      showDefaultMenu();
      delay(500);
      break;
    case 1:
      showGlobalTimeMenu();
      break;
    case 2:
      showTempHum();
      break;
    case 3:
      for (int i = 1; i < MAX_CATEGORIAS; i++)
      {
        if (tablaCategorias.estados[i] == true)
        {
          showNews(i);
        }
        cambiandoCategoria();
      }
      showNews(666);
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
    Serial.printf("Enviada temperatura %f y humedad %d\n", temperature, humidity);
    // delay(100); // Ajusta el retraso según sea necesario

    tb.loop();
  }

  void showDefaultMenu()
  {
    /*
    lcd.clear();
    lcd.setCursor(0, 0);
    char welcomeMessage[] = " BIENVENIDO!";
    for (int i = 0; i < strlen(welcomeMessage); i++)
    {
      lcd.print(welcomeMessage[i]);
      delay(100); // Pequeño retraso para el efecto de animación
    }

    lcd.setCursor(0, 1);
    char menu1[] = " Reloj";
    for (int i = 0; i < strlen(menu1); i++)
    {
      lcd.print(menu1[i]);
      delay(100); // Pequeño retraso para el efecto de animación
    }

    lcd.setCursor(0, 2);
    char menu2[] = " Temp/Hum";
    for (int i = 0; i < strlen(menu2); i++)
    {
      lcd.print(menu2[i]);
      delay(100); // Pequeño retraso para el efecto de animación
    }

    lcd.setCursor(0, 3);
    char menu3[] = " Noticias";
    for (int i = 0; i < strlen(menu3); i++)
    {
      lcd.print(menu3[i]);
      delay(100); // Pequeño retraso para el efecto de animación
    }
    */
    getNetworkTime_default();
    showTempHum();
    showNews(tipoNoticia);
  }

  

  void cambiandoCategoria(void)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   //////////// ");
    lcd.setCursor(0, 1);
    lcd.print("  Cambiando");
    lcd.setCursor(0, 2);
    lcd.print(" Categoria");
    lcd.setCursor(0, 3);
    lcd.print("//////////");
  }
  void showGlobalTimeMenu()
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("      RELOJ");
    lcd.setCursor(0, 1);
    lcd.print("Espania: ");
    lcd.print(getFormattedTime(3600)); // Hora de España (CET)
    lcd.setCursor(0, 2);
    lcd.print("Nueva York: ");
    lcd.print(getFormattedTime(-18000)); // Hora de Nueva York (EST)
    lcd.setCursor(0, 3);
    lcd.print("Tokio: ");
    lcd.print(getFormattedTime(31500)); // Hora de Tokio (JST) con una hora menos
    showNewsBueno(tipoNoticia);
    delay(1000);                        // Pequeño retraso para el efecto de animación
  }

  void showTempHum()
  {
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(bme.readTemperature());
    lcd.print("C");
    lcd.print("-H:");
    lcd.print(bme.readHumidity());
    lcd.print("%");
  }
  // business entertainment general health science sports technology

  /*
  https://newsapi.org/v2/top-headlines/?category=general&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f
  */
  
  
  
  void showNews(int tipoNoticia)
  {
    String peticionNoticias;
    if (WiFi.status() == WL_CONNECTED)
    {
      HttpClient client = HttpClient(espClient, "newsapi.org", 80);
      switch (tipoNoticia)
      {
      case 1:
        Serial.println("Seleccionadas noticias business");
        peticionNoticias = "/v2/top-headlines/?category=business&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
        lcd.setCursor(4,2);
        lcd.printf("BUSINESS");
        break;
      case 2:
        Serial.println("Seleccionadas noticias entertaiment");
        peticionNoticias = "/v2/top-headlines/?category=entertainment&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
                lcd.setCursor(4,2);
        lcd.printf("ENTERTAIMENT");
        break;

      case 3:
        Serial.println("Seleccionadas noticias health");
        peticionNoticias = "/v2/top-headlines/?category=health&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
                lcd.setCursor(4,2);
        lcd.printf("HEALTH");
        break;
      case 4:
        Serial.println("Seleccionadas noticias science");
        peticionNoticias = "/v2/top-headlines/?category=science&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
                lcd.setCursor(4,2);
        lcd.printf("SCIENCE");
        break;

      case 5:
        Serial.println("Seleccionadas noticias sport");
        peticionNoticias = "/v2/top-headlines/?category=sports&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
                lcd.setCursor(4,2);
        lcd.printf("SPORTS");
        break;

      case 6:
        Serial.println("Seleccionadas noticias tech");
        peticionNoticias = "/v2/top-headlines/?category=technology&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
                lcd.setCursor(4,2);
        lcd.printf("TECH");
        break;

      default:
        Serial.println("Seleccionadas noticias generales");
        peticionNoticias = "/v2/top-headlines/?category=general&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
                lcd.setCursor(4,2);
        lcd.printf("GENERALES");
        break;
      }


      // https://newsapi.org/v2/top-headlines/?category=general&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f

      client.get(peticionNoticias);

// https://newsapi.org/v2/top-headlines?country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f
delay(1000); // Esperar un segundo para asegurar la recepción de datos
String news; // Variable para almacenar la noticia

if (client.available())
{
  news = client.readString(); // Leer toda la respuesta
  Serial.println("Noticias recibidas");
  // Definir el índice de inicio de búsqueda del titular
  int titleStart = 0;

  while (true)
  {
    // Encontrar la próxima ocurrencia del titular
    titleStart = news.indexOf("\"title\":\"", titleStart); // Encontrar el inicio del titular

    Serial.printf("El title start es -> %d", titleStart);
    if (titleStart == -1)
    {
      break; // Salir del bucle si no se encuentra más titular
    }
    Serial.println("He pasado el title start");
    titleStart += 9; // Mover el índice de inicio al final del título actual
    int titleEnd = news.indexOf("\"", titleStart); // Encontrar el fin del titular

    if (titleEnd == -1)
    {
      break; // Salir del bucle si no se encuentra el fin del titular
    }

    // Extraer el titular entre las comillas
    String title = news.substring(titleStart, titleEnd);

    Serial.println("He pillado el título");

    // Mostrar el titular desplazándose en la cuarta línea del LCD
    for (int i = 0; i <= title.length(); i++) {
      String displayText = title.substring(i, i + LCD_COLS);
      lcd.setCursor(0, 3); // Establecer el cursor en la cuarta línea
      lcd.print(displayText);

      // Esperar un tiempo antes de pasar al próximo desplazamiento
      delay(300);

      // Mostrar otros datos en las demás líneas sin afectarlas
      lcd.setCursor(0, 0);
      lcd.print(getNetworkTime_default());
      lcd.setCursor(0, 1);
      showTempHum();
      delay(300);

      // Verificar si el botón fue presionado
      if (buttonPressed)
      {
        Serial.println("¡Botón presionado!");
        buttonPressed = false;
        currentMenu = (currentMenu + 1) % 4; // Cambiar 4 por el número total de menús
        break;
      }
    }

    // Actualizar el índice de inicio para la próxima búsqueda
    titleStart = titleEnd;
  }
}
else
{
  Serial.println("No se recibió ninguna respuesta del servidor");
}
    }
}

void showNewsBueno(int tipoNoticia)
{
  String peticionNoticias;
  if (WiFi.status() == WL_CONNECTED)
  {
    HttpClient client = HttpClient(espClient, "newsapi.org", 80);
    // Determinar la categoría de noticias basada en tipoNoticia
    String categories[] = {"business", "entertainment", "health", "science", "sports", "technology", "general"};
    String category = (tipoNoticia >= 1 && tipoNoticia <= 7) ? categories[tipoNoticia - 1] : "general";
    Serial.println("Seleccionadas noticias " + category);
    peticionNoticias = "/v2/top-headlines/?category=" + category + "&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";

    client.get(peticionNoticias);
    delay(1000); // Esperar un segundo para asegurar la recepción de datos

    if (client.available())
    {
      String news = client.readString(); // Leer toda la respuesta
      Serial.println("Noticias recibidas");

      // Mostrar noticias en el LCD
      scrollText(news, "title", lcd); // Función para desplazar el texto en el LCD
    }
    else
    {
      Serial.println("No se recibió ninguna respuesta del servidor");
    }
  }
  else
  {
    Serial.println("Fallo en la conexión WiFi");
  }
}

void scrollText(String data, String key, LiquidCrystal_I2C lcd)
{
  int titleStart = 0, titleEnd;
  while ((titleStart = data.indexOf("\"" + key + "\":\"", titleStart)) != -1)
  {
    titleStart += key.length() + 4; // Avanzar al contenido del título
    titleEnd = data.indexOf("\"", titleStart);
    if (titleEnd == -1) break;
    String title = data.substring(titleStart, titleEnd);

    for (int i = title.length(); i >= 0; i--)
    {
      lcd.setCursor(0, 3); // Establecer el cursor en la última línea
      lcd.print(title.substring(i)); // Imprimir texto desplazado
      delay(300); // Retardo para desplazamiento visual
      lcd.clear();
    }

    titleStart = titleEnd + 1; // Preparar el índice para el próximo título
    if (buttonPressed) // Salir si se presiona el botón
    {
      buttonPressed = false;
      break;
    }
  }
}


  String getLastMatchResult()
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      HttpClient client = HttpClient(espClient, "api.live-score-api.com", 80);
      client.get("/matches?team=Real%20Madrid&key=MSzbG1I0p4Urn0Q8");

      delay(1000);
      String result;

      if (client.available())
      {
        result = client.readString();
        Serial.println("Respuesta recibida:");

        // Analizar la respuesta JSON para extraer el resultado del partido
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, result);

        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          return "Error al analizar JSON";
        }

        // Obtener el resultado del partido
        JsonObject match = doc["data"]["matches"][0];
        String homeTeam = match["home_name"].as<String>();
        String awayTeam = match["away_name"].as<String>();
        String homeScore = match["home_score"].as<String>();
        String awayScore = match["away_score"].as<String>();

        String lastMatchResult = homeTeam + " " + homeScore + " - " + awayScore + " " + awayTeam;
        return lastMatchResult;
      }
      else
      {
        Serial.println("No se recibió ninguna respuesta del servidor");
      }
    }
    else
    {
      Serial.println("Fallo en la conexión WiFi");
    }

    return "No se pudo obtener el resultado del partido";
  }

  String getFormattedTime(long timeZoneOffset)
  {
    unsigned long currentTime = timeClient.getEpochTime() + timeZoneOffset;
    int hours = (currentTime % 86400L) / 3600;
    int minutes = (currentTime % 3600) / 60;
    String timeString = "";
    if (hours < 10)
    {
      timeString += "0";
    }
    timeString += hours;
    timeString += ":";
    if (minutes < 10)
    {
      timeString += "0";
    }
    timeString += minutes;
    return timeString;
  }

  void connectWiFi()
  {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      lcd.clear();
      lcd.setCursor(0, 1);
      char message1[] = "   Fallo conectar";
      for (int i = 0; i < strlen(message1); i++)
      {
        lcd.print(message1[i]);
        delay(100); // Pequeño retraso para el efecto de animación
      }

      lcd.setCursor(0, 2);
      char message2[] = "      WIFI";
      for (int i = 0; i < strlen(message2); i++)
      {
        lcd.print(message2[i]);
        delay(100); // Pequeño retraso para el efecto de animación
      }
    }
    Serial.println("\nConnected to WiFi");
  }

  void connectThingsBoard()
  {
    Serial.println("Connecting to ThingsBoard...");
    client.setServer(THINGSBOARD_SERVER, 1883);
    while (!client.connected())
    {
      if (client.connect("ESP32 Device", TOKEN, NULL))
      {
        Serial.println("Connected to ThingsBoard");
      }
      else
      {
        Serial.print("Failed to connect to ThingsBoard, rc=");
        Serial.println(client.state());
        lcd.clear();
        lcd.setCursor(0, 1);
        char message1[] = "   Fallo conectar";
        for (int i = 0; i < strlen(message1); i++)
        {
          lcd.print(message1[i]);
          delay(100); // Pequeño retraso para el efecto de animación
        }

        lcd.setCursor(0, 2);
        char message2[] = "    Thingsboard";
        for (int i = 0; i < strlen(message2); i++)
        {
          lcd.print(message2[i]);
          delay(100); // Pequeño retraso para el efecto de animación
        }

        delay(5000); // Mostrar el mensaje durante 5 segundos
      }
    }
  }

  void reconnect()
  {
    if (!client.connected())
    {
      connectWiFi();
      connectThingsBoard();
    }
  }