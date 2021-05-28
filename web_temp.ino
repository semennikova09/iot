#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <Wire.h>
#include <Adafruit_MLX90614.h>

// Вставьте ниже SSID и пароль для своей WiFi-сети:
const char* ssid = "";
const char* password = "";

String ssidAP = "ESP_WiFi"; // SSID AP точки доступа
String passwordAP = ""; // пароль точки доступа


Adafruit_MLX90614 mlx = Adafruit_MLX90614();
const int pin = 34;
const int ledPin = 10;

// Создаем экземпляр класса «AsyncWebServer»
// под названием «server» и задаем ему номер порта «80»:
AsyncWebServer server(80);

int ledState = LOW;
long previousMillis = 0;
const long interval = 100; // интервал между измерениями
const int duration = 2000; // продолжительность измерений
int currentDuration = 0;
int currentNumOfReads = 0;
double sumOfTemp = 0.0;
bool flagStart = false;
bool flagFinishNormal = false;
bool flagFinishExceed = false;
bool flagWrongPosition = false;
const long ledInterval = 3000; //время задержки после измерения
double meanTemp;

const char index_html[] PROGMEM = R"rawliteral(
 	<!DOCTYPE HTML><html>
  <head>
  <title>Temperature screening</title>
  <meta name="viewport" content="width=device-width, initialscale=1">
  <link rel="icon" href="data:,">
  <style>
  topNavLine 
  {
   width: 100vw;
   height: 60px;
   background-color: #4286f4;
   position: fixed;
   top: 0px;
   left: 0px;
   z-index: 100; 
  }
  html {
  font-family: Arial;
  display: inline-block;
  margin: 0px auto;
  text-align: center;
  }
  h1 { 
        font-size: 3.0rem;
  text-align: center;
  padding: 10vh; 
        }
  p { 
        font-size: 3.0rem;
        text-align: center;
        }
  .units { font-size: 1.2rem; }
  .dht-labels{
  font-size: 1.5rem;
  vertical-align:middle;
  padding-bottom: 15px;
  }
  </style>
  </head>
  <body>
  <h1>Turnstile #1</h1>
  <p>
  <i class="fas fa-thermometer-half" style="color:#059e8a;"></i>
  <span class="dht-labels"></span>
  <span id="temperature">%TEMPERATURE%</span>
  <sup class="units"></sup>
  </p>
  <div id="topNavLine"></div>
  </body>
  <script>
  setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
  if (this.readyState == 4 && this.status == 200) {
  document.getElementById("temperature").innerHTML = this.responseText;
  }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
  }, 20 ) ;

</script>
</html>)rawliteral";

double distance_temp() {
  int voltage = analogRead(pin);
  if (voltage > 3000) {
    voltage = 3000;
  }
  if (voltage < 400) {
    voltage = 400;
  }
  return 12.08 * pow((double)voltage / 1000, -1.058);
}

double corrected_temp(double dis) {
  double err;
  double ambient_temp = mlx.readAmbientTempC();
  double read_temp = mlx.readObjectTempC();
  if (ambient_temp < 28.5) {
    err = 36.6 - (-0.250428721 * dis + 32.36017852);
  } else if (ambient_temp >= 28.5 and ambient_temp < 29.064) {
    err = 36.6 - (-0.315795205 * dis + 32.71691355);
  } else if (ambient_temp >= 29.064 and ambient_temp < 29.2) {
    err = 36.6 - (-0.249376943 * dis + 32.19206399);
  } else if (ambient_temp >= 29.2 and ambient_temp < 29.26) {
    err = 36.6 - (-0.212646938 * dis + 32.11829144);
  } else if (ambient_temp >= 29.26 and ambient_temp < 29.32) {
    err = 36.6 - (-0.170039206 * dis + 31.96051268);
  } else if (ambient_temp >= 29.32 and ambient_temp < 29.42) {
    err = 36.6 - (-0.250363072 * dis + 32.38807183);
  } else if (ambient_temp >= 29.42) {
    err = 36.6 - (-0.241557932 * dis + 32.92151829);
  }
  return read_temp + err;
}

String get_temp() {
  String tempState;
  double temp;
  double dis;

  if (flagFinishNormal == false and flagFinishExceed == false) {
    if (flagStart == false) {
      dis = distance_temp();
      if (dis <= 9 and dis >= 4) {
        flagStart = true;
      } else if (dis < 4) {
        tempState = "move your hand FURTHER ";
      } else {
        tempState = "move your hand CLOSER ";
      }
    }

    if (flagStart == true) {
      tempState = "...don't take your hand away...";
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        dis = distance_temp();
        temp = corrected_temp(dis);
        if (dis <= 9 and dis >= 4) {
          sumOfTemp += temp;
          currentNumOfReads ++;
        }
        currentDuration += 100; 
      }
      if (currentDuration == duration) {
        meanTemp = sumOfTemp / (double)currentNumOfReads;
        if (meanTemp < 37) {
          flagFinishNormal = true;
        } else {
          flagFinishExceed = true;
          ledState = HIGH;
          digitalWrite(ledPin, ledState);
        }
        sumOfTemp = 0.0;
        currentNumOfReads = 0;
        currentDuration = 0;
        flagStart = false;
      }
    }
  }

  if (flagFinishExceed == true) {
    tempState = "!EXCEEDED! Temperature in range [" + (String)(meanTemp - 0.2) + " - " + (String)(meanTemp + 0.2) + "]℃";
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > ledInterval) {
      previousMillis = currentMillis;
      flagFinishExceed = false;
      ledState = LOW;
      digitalWrite(ledPin, ledState);
    }
  }
  if (flagFinishNormal == true) {
    tempState = "Temperature in range [" + (String)(meanTemp - 0.2) + " - " + (String)(meanTemp + 0.2) + "]℃";
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > ledInterval) {
      previousMillis = currentMillis;
      flagFinishNormal = false;
    }
  }

  return tempState;
}


// Меняем заглушку на текущее:
String processor(const String& var) {
  // Создаем переменную для хранения состояния

  if (var == "TEMPERATURE") {
    return get_temp();
  }
  return String();
}


void setup() {
  // Включаем последовательную коммуникацию (для отладки):
  Serial.begin(115200);
  mlx.begin();
  pinMode(pin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Подключаемся к WiFi:
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
    //  "Подключаемся к WiFi..."
  }

  // Печатаем в мониторе порта локальный IP-адрес ESP32:
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  /*
    Serial.println("WiFi up AP");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, ,255, 0));
    WiFi.softAP(ssidAP.c_str(), passwordAP.c_str());
  */
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", get_temp().c_str());
  });
  // Запускаем сервер:
  server.begin();
}

void loop() {

}
