#!/usr/bin/env python3
import time
import json
import random
import paho.mqtt.client as mqtt

# Configurações MQTT
MQTT_BROKER = "test.mosquitto.org"
MQTT_PORT = 1883
CLIENT_ID = "raspberry_simulator"

class Sensors:
    def __init__(self):
        # Valores base para simulação
        self.temperature = 25.0
        self.humidity = 60.0
        self.presence = False
        self.light = 1000
        
    def read_temperature(self):
        # Simula variação de temperatura
        self.temperature += random.uniform(-0.5, 0.5)
        return round(self.temperature, 1)
        
    def read_humidity(self):
        # Simula variação de umidade
        self.humidity += random.uniform(-2, 2)
        self.humidity = max(0, min(100, self.humidity))
        return round(self.humidity, 1)
        
    def read_presence(self):
        # Simula sensor de presença
        if random.random() < 0.1:  # 10% de chance de mudar
            self.presence = not self.presence
        return self.presence
        
    def read_light(self):
        # Simula sensor de luz
        hour = time.localtime().tm_hour
        if 6 <= hour <= 18:  # Dia
            base = 1000
        else:  # Noite
            base = 100
        return base + random.randint(-50, 50)

def main():
    # Inicializa sensores e MQTT
    sensors = Sensors()
    client = mqtt.Client(protocol=mqtt.MQTTv311)
    
    # Conecta ao broker MQTT
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        print("Conectado ao broker MQTT")
    except Exception as e:
        print(f"Erro ao conectar ao MQTT: {e}")
        return
    
    print("Iniciando simulação dos sensores...")
    
    while True:
        try:
            # Lê dados dos sensores
            temp = sensors.read_temperature()
            hum = sensors.read_humidity()
            pres = sensors.read_presence()
            light = sensors.read_light()
            
            # Prepara mensagens
            dht_data = {
                "temperature": temp,
                "humidity": hum
            }
            
            # Publica dados
            client.publish("esp32/dht", json.dumps(dht_data))
            client.publish("esp32/pir", json.dumps({"presence": pres}))
            client.publish("esp32/ldr", json.dumps({"light": light}))
            
            print(f"Dados enviados - Temp: {temp}°C, Hum: {hum}%, Presença: {pres}, Luz: {light}")
            time.sleep(5)
        except Exception as e:
            print(f"Erro: {e}")
            time.sleep(5)

if __name__ == "__main__":
    main()