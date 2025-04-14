#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Configurações do Wi-Fi
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);

// Tópicos MQTT
const char* mqtt_topic_ldr = "esp32/ldr";
const char* mqtt_topic_pir = "esp32/pir";
const char* mqtt_topic_dht = "esp32/dht";
const char* mqtt_topic_ac = "esp32/ac_status";
const char* mqtt_topic_room_light = "esp32/room_light";

// Sensores e atuadores
#define LDR_ANALOG_PIN 34   // Pino analógico GPIO36 (ADC1_CH0) - Mais confiável para leitura analógica
#define LDR_DIGITAL_PIN 19  // Pino digital
#define PIR_PIN 18
#define DHT_PIN 4
#define ROOM_LIGHT_PIN 5
#define AC_LED_PIN 2

DHT dht(DHT_PIN, DHT22);
bool acState = false;
bool roomLightState = false;

// Função para realizar uma média das leituras do LDR com filtro para reduzir ruído
int readAverageLDR(int samples = 10) {
  // Descartar a primeira leitura (pode ser instável)
  analogRead(LDR_ANALOG_PIN);
  delay(10);
  
  long total = 0;
  for (int i = 0; i < samples; i++) {
    int reading = analogRead(LDR_ANALOG_PIN);
    total += reading;
    delay(20); // Pequeno delay entre leituras
  }
  return total / samples;
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  
  // Configuração dos pinos
  pinMode(LDR_ANALOG_PIN, INPUT);
  pinMode(LDR_DIGITAL_PIN, INPUT_PULLUP); // Usar pullup interno para evitar flutuações
  pinMode(PIR_PIN, INPUT);
  pinMode(ROOM_LIGHT_PIN, OUTPUT);
  pinMode(AC_LED_PIN, OUTPUT);
  dht.begin();
  
  // Configurar a resolução do ADC para 12 bits (0-4095)
  analogReadResolution(12);
  // Configurar a atenuação para 11dB (range completo 0-3.3V)
  analogSetAttenuation(ADC_11db);
  
  client.setServer(mqtt_server, 1883);
  reconnect();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Leitura do LDR com tratamento de ruído
  int ldrRaw = readAverageLDR();
  int digitalLDR = digitalRead(LDR_DIGITAL_PIN);
  
  // Leitura dos outros sensores
  int pirValue = digitalRead(PIR_PIN);
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Considera ambiente escuro se o sensor digital indicar HIGH
  bool ambienteEscuro = (digitalLDR == HIGH);
  
  // Lógica de controle:
  if (pirValue == HIGH) {
    roomLightState = ambienteEscuro;
    if (temp > 25) {
      acState = true;
    } else if (temp < 20) {
      acState = false;
    }
  } else {
    roomLightState = false;
    acState = false;
  }
  
  digitalWrite(ROOM_LIGHT_PIN, roomLightState ? HIGH : LOW);
  digitalWrite(AC_LED_PIN, acState ? HIGH : LOW);
  
  // Publica os dados via MQTT
  client.publish(mqtt_topic_ldr, String(ldrRaw).c_str());
  client.publish(mqtt_topic_pir, String(pirValue).c_str());
  String dhtData = "Temp: " + String(temp) + "C Hum: " + String(humidity) + "%";
  client.publish(mqtt_topic_dht, dhtData.c_str());
  client.publish(mqtt_topic_ac, acState ? "true" : "false");
  client.publish(mqtt_topic_room_light, roomLightState ? "true" : "false");
  
  // Imprime os valores no Serial Monitor com mais detalhes
  Serial.print("LDR (raw): "); Serial.print(ldrRaw);
  Serial.print(" (");
  if (ldrRaw < 1000) Serial.print("MUITA LUZ");
  else if (ldrRaw < 2000) Serial.print("LUZ MODERADA");
  else if (ldrRaw < 3000) Serial.print("POUCA LUZ");
  else Serial.print("ESCURO");
  Serial.print(") | LDR Digital: ");
  Serial.print(digitalLDR ? "ESCURO" : "CLARO");
  Serial.print(" | PIR: ");
  Serial.print(pirValue ? "PRESENCA" : "AUSENCIA");
  Serial.print(" | Temp: ");
  Serial.print(temp);
  Serial.print(" C | Hum: ");
  Serial.print(humidity);
  Serial.println("%");
  
  delay(2000);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando reconectar ao MQTT...");
    String clientId = "ESP32Client-" + String(random(1000, 9999));
    if (client.connect(clientId.c_str())) {
      Serial.println(" Conectado!");
    } else {
      Serial.print(" Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 10s");
      delay(10000);
    }
  }
}