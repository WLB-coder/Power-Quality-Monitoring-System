#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <PZEM004Tv30.h>

// WiFi credentials
const char* ssid = "Your_SSID";
const char* password = "Your_Password";
  
// Fixed IP address configuration for the access point
IPAddress apIP(192, 168, 1, 10); // Define your desired fixed IP address for the AP
IPAddress subnet(255, 255, 255, 0); // Subnet mask

// Web server setup
AsyncWebServer server(80);

// Power quality analysis variables and constants
const int SAMPLES_PER_PERIOD = 100;
const unsigned long SAMPLE_INTERVAL_MICROS = 10000;

double voltageData[SAMPLES_PER_PERIOD];
double frequencyData[SAMPLES_PER_PERIOD];
unsigned int currentSampleIndex = 0;
unsigned long previousSampleTime = 0;

const int buzzerPin = 12;

// PZEM004Tv30 sensor setup
#if defined(ESP32)

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#endif

#define PZEM_SERIAL Serial2
#define CONSOLE_SERIAL Serial
PZEM004Tv30 pzem(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN);

#elif defined(ESP8266)

#define PZEM_SERIAL Serial
#define CONSOLE_SERIAL Serial1
PZEM004Tv30 pzem(PZEM_SERIAL);
#else

#define PZEM_SERIAL Serial2
#define CONSOLE_SERIAL Serial
PZEM004Tv30 pzem(PZEM_SERIAL);
#endif

double evaluateVoltageFluctuation(double voltageData[]);
double calculateRMS(double data[]);
double findMin(double data[]);
double evaluateFrequencyVariation(double frequencyData[]);

void setupWiFiAP() {
  // Configure ESP32 as an access point
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP("PowerQualityAP", "AP_Password"); // Set your desired AP SSID and password
}

void setup() {
  Serial.begin(115200);
  Serial.println("Power Quality Analysis");
  pinMode(buzzerPin, OUTPUT);

  setupWiFiAP(); // Configure the ESP32 as an access point

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String htmlContent = R"=====(<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, Helvetica, sans-serif;
      background-color: #21D4FD;
      background-image: linear-gradient(19deg, #21D4FD 0%, #B721FF 100%);
      text-align: center;
      margin: 0;
      padding: 0;
      color: black;
      background-repeat: no-repeat;
    }

    section {
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .header {
      background-color: #21D4FD;
      background-image: linear-gradient(19deg, #dee7e9 0%, #a795af 100%);
      color: black;
      padding: 20px;
      font-size: 36px; /* Augmentation de la taille ici */
      box-shadow: 0 3px 8px rgba(0, 0, 0, 25%);
      border-radius: 1em;
    }

    h3 {
      font-size: 19px;
    }

    .container {
      background-color: white;
      color: black;
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      padding: 20px;
      border-radius: 10px;
      margin: 20px;
      width: 1000px;
      height: 500px;
      box-shadow: 0 4px 8px 0 rgba(0, 0, 0, 0.2);
    }

    .data-item {
      font-size: 24px;
      width: 45%;
      padding: 20px;
      margin-bottom: 20px;
      border-radius: 10px;
      box-shadow: 0 4px 8px 0 rgba(0, 0, 0, 0.2);
    }

    .data-item:nth-child(odd) {
      background-color: #f2f2f2;
    }

    .data-value {
      font-weight: bold;
      font-size: 28px;
    }

    .data-label {
      color: #141414;
      font-size: 24px;
    }

    #o {
      margin-bottom: 100px;
      margin-top: 100px;
    }
    @media (max-width: 500px)
    {
      height:900px;
    }
  </style>
</head>
<body>
  <div class="header">
    <h3><span class="auto-type"></span></h3>
    <script src="https://unpkg.com/typed.js@2.0.16/dist/typed.umd.js"></script>
    <script>
      var typed = new Typed(".auto-type", {
        strings: ["Power Quality Analysis"],
        typeSpeed: 150,
        backSpeed: 150,
        loop: true,
      });
    </script>
  </div>
  <section>
    <div class="container">
      <div class="data-item">
        <span class="data-label">Voltage:</span>
        <span class="data-value" id="voltage">0 V</span>
      </div>
      <div class="data-item">
        <span class="data-label">Current:</span>
        <span class="data-value" id="current">0 A</span>
      </div>
      <div class="data-item">
        <span class="data-label">Power:</span>
        <span class="data-value" id="power">0 W</span>
      </div>
      <div class="data-item">
        <span class="data-label">Energy:</span>
        <span class="data-value" id="energy">0 kWh</span>
      </div>
      <div class="data-item">
        <span class="data-label">Frequency:</span>
        <span class="data-value" id="frequency">0 Hz</span>
      </div>
      <div class="data-item">
        <span class="data-label">Power Factor:</span>
        <span class="data-value" id="powerFactor">0</span>
      </div>
      <div class="data-item">
        <span class="data-label">Voltage Fluctuation:</span>
        <span class="data-value" id="voltageFluctuation">0 %</span>
      </div>
      <div class="data-item">
        <span class="data-label">Frequency Variation:</span>
        <span class="data-value" id="frequencyVariation">0 %</span>
      </div>
    </div>
  </section>
  <script>
    setInterval(function() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          document.getElementById('voltage').innerHTML = data.voltage + ' V';
          document.getElementById('current').innerHTML = data.current + ' A';
          document.getElementById('power').innerHTML = data.power + ' W';
          document.getElementById('energy').innerHTML = data.energy + ' kWh';
          document.getElementById('frequency').innerHTML = data.frequency + ' Hz';
          document.getElementById('powerFactor').innerHTML = data.powerfactor;
          document.getElementById('voltageFluctuation').innerHTML = data.voltageFluctuation + ' %';
          document.getElementById('frequencyVariation').innerHTML = data.frequencyVariation + ' %';
        }
      };
      xhr.open('GET', '/data', true);
      xhr.send();
    }, 1000);
  </script>
</body>
</html>
)=====";
    request->send(200, "text/html", htmlContent);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String data = "{\"voltage\":" + String(pzem.voltage()) +",\"current\":" + String(pzem.current()) +",\"power\":" + String(pzem.power()) +",\"energy\":" + String(pzem.energy()) +",\"frequency\":" + String(pzem.frequency()) +",\"powerfactor\":" + String(pzem.pf()) + ",\"voltageFluctuation\":" + String(evaluateVoltageFluctuation(voltageData), 2) + ",\"frequencyVariation\":" + String(evaluateFrequencyVariation(frequencyData), 2) + "}";
    request->send(200, "application/json", data);
  });

  server.begin();
}

void loop() {
  unsigned long currentTime = micros();
  if (currentTime - previousSampleTime >= SAMPLE_INTERVAL_MICROS) {
    previousSampleTime = currentTime;
    double voltageReading = pzem.voltage();
    double frequencyReading = pzem.frequency();
    voltageData[currentSampleIndex] = voltageReading;
    frequencyData[currentSampleIndex] = frequencyReading;
    currentSampleIndex = (currentSampleIndex + 1) % SAMPLES_PER_PERIOD;
    double voltageFluctuation = evaluateVoltageFluctuation(voltageData);
    double frequencyVariation = evaluateFrequencyVariation(frequencyData);
    float powerFactor = pzem.pf();
    Serial.print("Voltage Fluctuation: ");
    Serial.print(voltageFluctuation, 4);
    Serial.println(" %");
    Serial.print("Frequency Variation: ");
    Serial.print(frequencyVariation, 4);
    Serial.println(" %");
    Serial.print("Power Factor: ");
    Serial.println(powerFactor, 4);

    if (evaluateVoltageFluctuation(voltageData) > 0.2 || evaluateVoltageFluctuation(voltageData) < -0.2) {
      digitalWrite(buzzerPin, HIGH);
      delay(10);
      digitalWrite(buzzerPin, LOW);
    }
    delay(10);
  }
}

double evaluateVoltageFluctuation(double voltageData[]) {
  double Vrms = calculateRMS(voltageData);
  double Vmin = findMin(voltageData);
  return (Vmin - Vrms) / Vrms * 100.0;
}

double calculateRMS(double data[]) {
  double sum = 0.0;
  for (int i = 0; i < SAMPLES_PER_PERIOD; i++) {
    sum += data[i] * data[i];
  }
  double rms = sqrt(sum / SAMPLES_PER_PERIOD);
  return rms;
}

double findMin(double data[]) {
  double minVal = data[0];
  for (int i = 1; i < SAMPLES_PER_PERIOD; i++) {
    if (data[i] < minVal) {
      minVal = data[i];
    }
  }
  return minVal;
}

double evaluateFrequencyVariation(double frequencyData[]) {
  double sumFrequency = 0.0;
  for (int i = 0; i < SAMPLES_PER_PERIOD; i++) {
    sumFrequency += frequencyData[i];
  }
  double averageFrequency = sumFrequency / SAMPLES_PER_PERIOD;

  double sumSquaredDifferences = 0.0;
  for (int i = 0; i < SAMPLES_PER_PERIOD; i++) {
    double difference = frequencyData[i] - averageFrequency;
    sumSquaredDifferences += difference * difference;
  }

  double standardDeviation = sqrt(sumSquaredDifferences / SAMPLES_PER_PERIOD);
  return standardDeviation / averageFrequency * 100.0;
}