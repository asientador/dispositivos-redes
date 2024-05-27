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
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <ThingsBoard.h>
#include <ArduinoJson.h> // Biblioteca para manipular JSON

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

Adafruit_BME280 bme; // I2C

#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

// Nombre del punto de acceso WiFi
#define WIFI_AP_NAME "wifiperuana"

// Contraseña del punto de acceso WiFi
#define WIFI_PASSWORD "abascalvox"

// Token para la conexión con ThingsBoard
#define TOKEN "1234"

// Dirección del servidor ThingsBoard
#define THINGSBOARD_SERVER "192.168.92.140"

// Velocidad de baudios para la comunicación serial
#define SERIAL_DEBUG_BAUD 115200

// Dirección y configuración de la pantalla LCD
#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

// Pin del botón
#define BUTTON_PIN 23

// Número máximo de categorías
const int MAX_CATEGORIAS = 7;

// Número máximo de ciudades
const int MAX_CIUDADES = 7;

// Variable para almacenar el tipo de noticia actual
int tipoNoticia = 0;
int prevTipoNoticia;

/**
 * @struct TablaCategoria
 * Estructura para gestionar categorías y sus estados.
 */
struct TablaCategoria
{
  // Array de cadenas de texto que almacena las categorías
  std::string categorias[MAX_CATEGORIAS];

  // Array de booleanos que almacena el estado de cada categoría
  bool estados[MAX_CATEGORIAS];

  /**
   * @brief Constructor para inicializar la tabla con categorías predeterminadas.
   */
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

    // Establecer todas las categorías como desactivadas por defecto
    for (int i = 0; i < MAX_CATEGORIAS; ++i)
    {
      estados[i] = false;
    }
  }

  /**
   * @brief Función para imprimir la tabla y el estado de activación de cada categoría.
   */
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

/**
 * @struct TablaCiudades
 * Estructura para gestionar ciudades y sus estados.
 */
struct TablaCiudades
{
  // Array de cadenas de texto que almacena las ciudades
  std::string ciudades[MAX_CIUDADES];

  // Array de booleanos que almacena el estado de cada ciudad
  bool estados[MAX_CIUDADES];

  // Número de ciudades activas
  int numeroActivo;

  /**
   * @brief Constructor para inicializar la tabla con ciudades predeterminadas.
   */
  TablaCiudades()
  {
    // Ciudades predeterminadas
    ciudades[0] = "x";
    ciudades[1] = "madrid";
    ciudades[2] = "tokyo";
    ciudades[3] = "paris";
    ciudades[4] = "new_york";
    ciudades[5] = "los_angeles";
    ciudades[6] = "rome";

    numeroActivo = 0;

    // Establecer todas las ciudades como desactivadas por defecto
    for (int i = 0; i < MAX_CIUDADES; ++i)
    {
      estados[i] = false;
    }
  }

  /**
   * @brief Función para imprimir la tabla y el estado de activación de cada ciudad.
   */
  void imprimirTabla()
  {
    Serial.println("Tabla de Ciudades:");
    for (int i = 0; i < MAX_CIUDADES; ++i)
    {
      Serial.printf("%s", ciudades[i].c_str());
      Serial.print(": ");
      Serial.println(estados[i] ? "Activada" : "Desactivada");
    }
  }
};

// Instancia de la estructura TablaCategoria para gestionar las categorías
TablaCategoria tablaCategorias;

// Instancia de la estructura TablaCiudades para gestionar las ciudades
TablaCiudades tablaCiudades;

// Instancia de la clase LiquidCrystal_I2C para controlar el LCD
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Cliente WiFi para la comunicación
WiFiClient espClient;

// Cliente WiFi para la comunicación de hora
WiFiClient espClientHora;

// Cliente PubSub para la comunicación MQTT
PubSubClient client(espClient);

// Cliente de ThingsBoard para la comunicación IoT
ThingsBoard tb(espClient);

// Cliente UDP para la sincronización NTP
WiFiUDP ntpUDP;

// Cliente NTP para obtener la hora de internet
NTPClient timeClient(ntpUDP, "time.google.com", 3600);

// Variable volátil para indicar si el botón ha sido presionado
volatile bool buttonPressed = false;

// Variable para indicar si se ha suscrito a un tópico MQTT
bool subscribed = false;


/**
 * @brief Manejador de interrupciones para el botón
 */
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

/**
 * @brief Maneja la alarma de humedad.
 * @param data Datos de la RPC.
 * @return Respuesta de la RPC.
 */
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

  delay(3000); // Mostrar el mensaje durante 3 segundos

  return RPC_Response(NULL, data);
}

/**
 * @brief Maneja la respuesta del estado del dispositivo.
 * @param data Datos de la RPC.
 * @return Respuesta de la RPC.
 */
RPC_Response estadoDispositivo(const RPC_Data &data)
{
  Serial.print("RPC ESTADO DISPOSITIVO\n");
  return RPC_Response(NULL, true);
}

/**
 * @brief Procesa el estado de la categoría establecida.
 * @param data Datos de la RPC.
 * @return Respuesta de la RPC.
 */
RPC_Response processSetCategoriaState(const RPC_Data &data)
{
  Serial.println("RPC SET CATEGORIAS\n");
  cambiandoCategoria();

  int indice = data["pin"];
  bool estado = data["enabled"];

  if (indice < MAX_CATEGORIAS)
  {
    tablaCategorias.estados[indice] = estado;
    tablaCategorias.imprimirTabla();
  }

  Serial.printf("Voy a devolver %d %d", data["pin"], data["enabled"]);
  tipoNoticia = indice;
  return RPC_Response(std::to_string(indice).c_str(), estado);
}

/**
 * @brief Procesa el estado de la categoría solicitada.
 * @param data Datos de la RPC.
 * @return Respuesta de la RPC.
 */
RPC_Response processGetCategoriaState(const RPC_Data &data)
{
  Serial.println("Received the set delay RPC method");

  return RPC_Response(NULL, true);
}
/**
 * @brief Procesa el estado de la ciudad establecida.
 * @param data Datos de la RPC.
 * @return Respuesta de la RPC.
 */
RPC_Response processSetCiudadState(const RPC_Data &data)
{
  Serial.println("RPC SET CIUDAD\n");

  int indice = data["pin"];
  bool estado = data["enabled"];

  Serial.printf("\n\n ESTADO -> %d, INDICE -> %d\n \n", estado, indice);

  // Verifica si el índice está dentro del rango permitido
  if (indice < MAX_CIUDADES)
  {
    // Actualiza el estado de la ciudad correspondiente
    tablaCiudades.estados[indice] = estado;
  }

  // Si el estado es 1 (activado), actualiza el número activo y desactiva las otras ciudades
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

  // Imprime la tabla de ciudades y sus estados
  tablaCiudades.imprimirTabla();
  Serial.printf("Voy a devolver %d %d", data["pin"], data["enabled"]);

  // Devuelve la respuesta de la RPC con el índice y el estado
  return RPC_Response(std::to_string(indice).c_str(), estado);
}

/**
 * @brief Procesa el estado de la ciudad solicitada.
 * @param data Datos de la RPC.
 * @return Respuesta de la RPC.
 */
RPC_Response processGetCiudadState(const RPC_Data &data)
{
  return RPC_Response(NULL, true);
}

/**
 * @brief Obtiene la hora de la red según la ciudad activa.
 */
void getNetworkTime_default()
{
  HttpClient client = HttpClient(espClientHora, "worldtimeapi.org", 80);

  // Seleccionar la zona horaria basada en la ciudad activa y realizar la solicitud HTTP
  // Por defecto se seleccionará la hora basada en la IP del dispositivo
  switch (tablaCiudades.numeroActivo)
  {
  case 1:
    lcd.setCursor(0, 0);
    lcd.printf("MA:");
    client.get("/api/timezone/Europe/Madrid");
    break;
  case 2:
    lcd.setCursor(0, 0);
    lcd.printf("TK:");
    client.get("/api/timezone/Asia/Tokyo");
    break;
  case 3:
    lcd.setCursor(0, 0);
    lcd.printf("PA:");
    client.get("/api/timezone/Europe/Paris");
    break;
  case 4:
    lcd.setCursor(0, 0);
    lcd.printf("NY:");
    client.get("/api/timezone/America/New_York");
    break;
  case 5:
    lcd.setCursor(0, 0);
    lcd.printf("LA:");
    client.get("/api/timezone/America/Los_Angeles");
    break;
  case 6:
    lcd.setCursor(0, 0);
    lcd.printf("RM:");
    client.get("/api/timezone/Europe/Rome");
    break;
  default:
    lcd.setCursor(0, 0);
    lcd.printf("IP:");
    client.get("/api/ip");
    break;
  }

  // Obtener el código de estado de la respuesta
  int statusCode = client.responseStatusCode();
  if (statusCode == 200)
  {
    // Procesar la respuesta
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
    lcd.setCursor(4, 0);
    lcd.print(formattedDateTime);
  }
  else
  {
    // Manejar error en la solicitud HTTP
    Serial.print("Error al obtener la hora: ");
    Serial.println(statusCode);
  }
}

/**
 * @brief Configuración inicial del sistema.
 */
void setup()
{
  // Inicializar la comunicación serial para depuración
  Serial.begin(SERIAL_DEBUG_BAUD);

  // Inicializar la comunicación I2C
  Wire.begin();

  // Inicializar la pantalla LCD
  lcd.init();
  lcd.backlight();

  // Inicializar el sensor BME
  bme.begin();

  // Configurar el pin del botón con resistencia pull-up interna
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Adjuntar la interrupción al pin del botón para detectar eventos de subida
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, RISING);

  // Conectar a la red WiFi
  connectWiFi();

  // Conectar a ThingsBoard
  connectThingsBoard();

  // Reconectar a ThingsBoard si es necesario
  if (!tb.connected())
  {
    subscribed = false;

    // Conectar a ThingsBoard
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

  // Suscribirse a RPC si es necesario
  if (!subscribed)
  {
    Serial.println("Subscribing for RPC...");

    // Realizar una suscripción. Todo el procesamiento de datos subsecuente ocurrirá en
    // las devoluciones de llamada indicadas por el array callbacks[].
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks)))
    {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    Serial.println("Subscribe to callbacks done");
    subscribed = true;
  }
}

/**
 * @brief Bucle principal del programa.
 */
void loop()
{
  // Reconectar al WiFi si es necesario
  if (WiFi.status() != WL_CONNECTED)
  {
    reconnect();
    return;
  }

  // Reconectar a ThingsBoard si es necesario
  if (!tb.connected())
  {
    subscribed = false;

    // Conectar a ThingsBoard
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

  // Suscribirse a RPC si es necesario
  if (!subscribed)
  {
    Serial.println("Subscribing for RPC...");

    // Realizar una suscripción. Todo el procesamiento de datos subsecuente ocurrirá en
    // las devoluciones de llamada indicadas por el array callbacks[].
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks)))
    {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    Serial.println("Subscribe done");
    subscribed = true;
  }

  // Mostrar el menú predeterminado
  showDefaultMenu();

  // Actualizar el cliente NTP
  timeClient.update();

  // Ejecutar el bucle de ThingsBoard
  tb.loop();
}

/**
 * @brief Muestra el menú predeterminado en la pantalla LCD.
 */
void showDefaultMenu()
{
  // Obtener la hora de la red según la ciudad activa y mostrarla en la pantalla
  getNetworkTime_default();

  // Mostrar la temperatura y la humedad en la pantalla
  showTempHum();

  // Mostrar las noticias del tipo especificado en la pantalla
  showNews();
}

/**
 * @brief Muestra un mensaje en la pantalla LCD indicando que se está cambiando la categoría.
 */
void cambiandoCategoria(void)
{
  // Limpiar la pantalla LCD
  lcd.clear();
  delay(200);

  // Mostrar el mensaje de cambio de categoría en la pantalla
  lcd.setCursor(0, 0);
  lcd.print("   //////////// ");
  lcd.setCursor(0, 1);
  lcd.print("  Cambiando");
  lcd.setCursor(0, 2);
  lcd.print(" Categoria");
  lcd.setCursor(0, 3);
  lcd.print("//////////");

  // Mantener el mensaje en la pantalla durante 2 segundos
  delay(2000);

  // Limpiar la pantalla LCD nuevamente
  lcd.clear();
}

/**
 * @brief Muestra la temperatura y la humedad en la pantalla LCD.
 */
void showTempHum()
{
  // Establecer el cursor en la segunda fila, primera columna
  lcd.setCursor(0, 1);

  // Mostrar la temperatura en grados Celsius
  lcd.print("T:");
  lcd.print(bme.readTemperature());
  lcd.print("C");

  // Mostrar la humedad relativa en porcentaje
  lcd.print("-H:");
  lcd.print(bme.readHumidity());
  lcd.print("%");
}

/**
 * @brief Muestra la categoría de noticias en la pantalla LCD según el tipo de noticia.
 * @param tipoNoticia El tipo de noticia a mostrar.
 */
void showCategoria(int tipoNoticia)
{
  lcd.setCursor(0, 2);
  lcd.printf("          ");

  // Determinar la categoría de noticias basada en el tipo de noticia
  switch (tipoNoticia)
  {
  case 1:
    lcd.setCursor(4, 2);
    lcd.printf("BUSINESS");
    break;
  case 2:
    lcd.setCursor(4, 2);
    lcd.printf("ENTERTAIMENT");
    break;
  case 3:
    lcd.setCursor(4, 2);
    lcd.printf("HEALTH");
    break;
  case 4:
    lcd.setCursor(4, 2);
    lcd.printf("SCIENCE");
    break;
  case 5:
    lcd.setCursor(4, 2);
    lcd.printf("SPORTS");
    break;
  case 6:
    lcd.setCursor(4, 2);
    lcd.printf("TECH");
    break;
  default:
    lcd.setCursor(4, 2);
    lcd.printf("GENERALES");
    break;
  }
}

/**
 * @brief Muestra las noticias en la pantalla LCD según el tipo de noticia.
 */
void showNews()
{
  int tipoNoticiaLocal;
gotoNews:
  Serial.printf("Tipo noticia: %d.\n", tipoNoticia);

  String peticionNoticias;
  if (WiFi.status() == WL_CONNECTED)
  {
    HttpClient client = HttpClient(espClient, "newsapi.org", 80);

    // Seleccionar la petición de noticias basada en el tipo de noticia
    switch (tipoNoticia)
    {
    case 1:
      Serial.println("Seleccionadas noticias business");
      peticionNoticias = "/v2/top-headlines/?category=business&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
      showCategoria(tipoNoticia);
      break;
    case 2:
      Serial.println("Seleccionadas noticias entertaiment");
      peticionNoticias = "/v2/top-headlines/?category=entertainment&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
      showCategoria(tipoNoticia);
      break;
    case 3:
      Serial.println("Seleccionadas noticias health");
      peticionNoticias = "/v2/top-headlines/?category=health&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
      showCategoria(tipoNoticia);
      break;
    case 4:
      Serial.println("Seleccionadas noticias science");
      peticionNoticias = "/v2/top-headlines/?category=science&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
      showCategoria(tipoNoticia);
      break;
    case 5:
      Serial.println("Seleccionadas noticias sport");
      peticionNoticias = "/v2/top-headlines/?category=sports&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
      showCategoria(tipoNoticia);
      break;
    case 6:
      Serial.println("Seleccionadas noticias tech");
      peticionNoticias = "/v2/top-headlines/?category=technology&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
      showCategoria(tipoNoticia);
      break;
    default:
      Serial.println("Seleccionadas noticias generales");
      peticionNoticias = "/v2/top-headlines/?category=general&pageSize=3&country=us&apiKey=8631a941dd2b40bba8bf5a56b9891e9f";
      showCategoria(tipoNoticia);
      break;
    }

    // Realizar la solicitud HTTP para obtener las noticias
    client.get(peticionNoticias);

    delay(1000); // Esperar un segundo para asegurar la recepción de datos
    String news; // Variable para almacenar la noticia

    tipoNoticiaLocal = tipoNoticia;

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

        Serial.printf("El title start es -> %d\n", titleStart);
        if (titleStart == -1)
        {
          break; // Salir del bucle si no se encuentra más titular
        }
        Serial.println("He pasado el title start");
        titleStart += 9;                               // Mover el índice de inicio al final del título actual
        int titleEnd = news.indexOf("\"", titleStart); // Encontrar el fin del titular

        if (titleEnd == -1)
        {
          break; // Salir del bucle si no se encuentra el fin del titular
        }

        // Extraer el titular entre las comillas
        String title = news.substring(titleStart, titleEnd);

        Serial.println("He pillado el título");

        // Mostrar el titular desplazándose en la cuarta línea del LCD
        int titleLength = title.length();
        int displayLength = LCD_COLS;

        for (int i = 0; i <= titleLength-2; i++)
        {
          String displayText;
          if (i + displayLength <= titleLength) {
            displayText = title.substring(i, i + displayLength);
          } else {
            displayText = title.substring(i);
            int spacesToAdd = displayLength - displayText.length();
            for (int j = 0; j < spacesToAdd; j++) {
              displayText += " ";
            }
          }

          lcd.setCursor(0, 3); // Establecer el cursor en la cuarta línea
          lcd.print(displayText);

          // Mostrar otros datos en las demás líneas sin afectarlas
          lcd.setCursor(0, 0);
          getNetworkTime_default();
          lcd.setCursor(0, 1);
          showTempHum();
          showCategoria(tipoNoticia);
          delay(10);
          enviarDatos();
          tb.loop();

          // En caso de que el tipo de noticia haya cambiado desde ThingsBoard volvemos a montarlas con el tipo correcto
          if(tipoNoticia!=tipoNoticiaLocal)
            goto gotoNews;
        }

        // En caso de que se acaben las noticias de una categoria cambiamos a la siguiente que este activa
        for( int i = 0; i<MAX_CATEGORIAS; i++){
          if( (i!=tipoNoticia) && (tablaCategorias.estados[i])){
              tipoNoticia = i;
              break;
          }
        }

        // Obtenemos las noticias generales otra vez y las mostramos
        goto gotoNews;
        
        // Pausa antes de pasar al siguiente título
        delay(2000);
      }

    }
  }
  else
  {
    Serial.println("No se recibió ninguna respuesta del servidor");
  }
}


/**
 * @brief Conecta el dispositivo a la red WiFi.
 */
void connectWiFi()
{
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);

  // Intentar conectar a la red WiFi
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");

    // Mostrar mensaje de fallo de conexión en la pantalla LCD
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

  // Indicar que la conexión a WiFi se ha establecido
  Serial.println("\nConnected to WiFi");
}

/**
 * @brief Conecta el dispositivo a ThingsBoard.
 */
void connectThingsBoard()
{
  Serial.println("Connecting to ThingsBoard...");
  client.setServer(THINGSBOARD_SERVER, 1883);

  // Intentar conectar a ThingsBoard
  while (!client.connected())
  {
    if (client.connect("ESP32 Device", TOKEN, NULL))
    {
      // Conexión exitosa
      Serial.println("Connected to ThingsBoard");
    }
    else
    {
      // Fallo al conectar
      Serial.print("Failed to connect to ThingsBoard, rc=");
      Serial.println(client.state());

      // Mostrar mensaje de fallo de conexión en la pantalla LCD
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

      // Esperar 5 segundos antes de reintentar
      delay(5000);
    }
  }
}

/**
 * @brief Envía datos de telemetría a ThingsBoard.
 */
void enviarDatos()
{
  // Reconectar al WiFi si es necesario
  if (WiFi.status() != WL_CONNECTED)
  {
    reconnect();
    return;
  }

  // Reconectar a ThingsBoard si es necesario
  if (!tb.connected())
  {
    subscribed = false;

    // Conectar a ThingsBoard
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

  // Suscribirse a RPC si es necesario
  if (!subscribed)
  {
    Serial.println("Subscribing for RPC...");

    // Realizar una suscripción. Todo el procesamiento de datos subsecuente ocurrirá en
    // las devoluciones de llamada indicadas por el array callbacks[].
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks)))
    {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    Serial.println("Subscribe done");
    subscribed = true;
  }

  // Actualizar el cliente NTP
  timeClient.update();

  // Leer la temperatura y la humedad del sensor BME
  float temperature = bme.readTemperature();
  int humidity = bme.readHumidity();

  // Publicar la temperatura y la humedad en ThingsBoard
  tb.sendTelemetryFloat("TemperaturaESP", temperature);
  tb.sendTelemetryInt("HumedadESP", humidity);
  Serial.printf("Enviada temperatura %f y humedad %d\n", temperature, humidity);
}

/**
 * @brief Reconectar al WiFi y ThingsBoard si es necesario.
 */
void reconnect()
{
  if (!client.connected())
  {
    connectWiFi();
    connectThingsBoard();
  }
}
