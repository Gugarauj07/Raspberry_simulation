# Sistema Embarcado com QEMU e Node-RED

Este projeto implementa um sistema embarcado que simula sensores em um ambiente QEMU, comunicando via MQTT com um dashboard Node-RED.

## Visão Geral

O sistema consiste em:
1. Ambiente de simulação usando QEMU
2. Distribuição Linux mínima usando Yocto
3. Aplicação Python para simulação de sensores
4. Dashboard Node-RED para visualização e controle

## Requisitos

- Windows 10/11 com WSL2
- 8GB RAM mínimo
- 50GB espaço em disco
- Conexão com internet

## Configuração do Ambiente

### 1. Instalação do WSL2

1. Abra o PowerShell como administrador e execute:
```powershell
wsl --install Ubuntu-22.04
```

2. Reinicie o computador

3. Após reiniciar, abra o Ubuntu 22.04 e configure:
- Nome de usuário
- Senha

### 2. Instalação das Dependências

No WSL (Ubuntu):
```bash
# Atualizar sistema
sudo apt update && sudo apt upgrade -y

# Instalar dependências do Yocto e QEMU
sudo apt install -y gawk wget git-core diffstat unzip texinfo gcc-multilib \
     build-essential chrpath socat cpio python3 python3-pip python3-pexpect \
     xz-utils debianutils iputils-ping python3-git python3-jinja2 libegl1-mesa \
     libsdl1.2-dev pylint xterm python3-subunit mesa-common-dev qemu-system-arm \
     lz4 zstd
```

### 3. Configuração do Yocto

1. Criar diretório e clonar Poky:
```bash
mkdir ~/embarcados
cd ~/embarcados
git clone git://git.yoctoproject.org/poky -b kirkstone
cd poky
source oe-init-build-env
```

2. Configurar build:
```bash
# No diretório build
cat > conf/local.conf << 'EOF'
MACHINE = "qemuarm"
DISTRO_FEATURES:append = " systemd"
IMAGE_INSTALL:append = " python3 python3-pip"
EXTRA_IMAGE_FEATURES += "debug-tweaks"
IMAGE_INSTALL:append = " dhcpcd"
IMAGE_FEATURES += "ssh-server-dropbear"
EOF
```

3. Construir imagem:
```bash
bitbake core-image-minimal
```

### 4. Configuração do QEMU

1. Iniciar QEMU:
```bash
runqemu qemuarm nographic slirp
```

2. Fazer login:
- Usuário: `root`
- Senha: (pressione Enter, sem senha)

3. Configurar rede:
```bash
dhcpcd eth0
ping -c 4 8.8.8.8  # Testar conexão
```

### 5. Desenvolvimento do Sistema Python

1. Criar diretório e instalar dependências:
```bash
mkdir -p /opt/embedded-system
cd /opt/embedded-system
pip3 install paho-mqtt
```

2. Criar arquivo main.py:
```bash
cat > main.py << 'EOF'
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
        self.ac_status = False
        self.room_light = False
        
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

    def read_ac_status(self):
        # Simula status do ar condicionado
        if random.random() < 0.05:  # 5% de chance de mudar
            self.ac_status = not self.ac_status
        return self.ac_status

    def read_room_light(self):
        # Simula status da luz da sala
        if random.random() < 0.05:  # 5% de chance de mudar
            self.room_light = not self.room_light
        return self.room_light

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
            ac_status = sensors.read_ac_status()
            room_light = sensors.read_room_light()
            
            # Prepara mensagens
            dht_data = {
                "temperature": temp,
                "humidity": hum
            }
            
            # Publica dados
            client.publish("esp32/dht", json.dumps(dht_data))
            client.publish("esp32/pir", json.dumps({"presence": pres}))
            client.publish("esp32/ldr", json.dumps({"light": light}))
            client.publish("esp32/ac_status", json.dumps({"status": "ligado" if ac_status else "desligado"}))
            client.publish("esp32/room_light", json.dumps({"status": "ligada" if room_light else "desligada"}))
            
            print(f"Dados enviados - Temp: {temp}°C, Hum: {hum}%, Presença: {pres}, Luz: {light}, AC: {'ligado' if ac_status else 'desligado'}, Luz Sala: {'ligada' if room_light else 'desligada'}")
            time.sleep(5)
        except Exception as e:
            print(f"Erro: {e}")
            time.sleep(5)

if __name__ == "__main__":
    main() 
EOF

# Tornar o arquivo executável
chmod +x main.py
```

3. Executar o programa:
```bash
python3 main.py
```

### 6. Teste do Sistema

1. Em outro terminal WSL, monitorar mensagens MQTT:
```bash
mosquitto_sub -h test.mosquitto.org -t "esp32/#" -v
```

2. Verificar se os dados estão sendo recebidos:
- Tópico `esp32/dht`: temperatura e umidade
- Tópico `esp32/pir`: presença
- Tópico `esp32/ldr`: luminosidade

### 6. Configuração do Node-RED

1. Instalar Node.js e npm:
```bash
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt-get install -y nodejs
```

2. Instalar Node-RED globalmente:
```bash
sudo npm install -g --unsafe-perm node-red
```

3. Instalar módulos necessários:
```bash
cd ~/.node-red
npm install node-red-dashboard
npm install node-red-contrib-mqtt-broker
```

4. Iniciar Node-RED:
```bash
node-red --host 0.0.0.0
```

5. Acessar a interface do Node-RED:
- Abra o navegador e acesse: `http://localhost:1880` ou `http://<IP_WSL>:1880`
- Para encontrar o IP do WSL, execute no PowerShell:
```powershell
wsl hostname -I
```

6. Importar o fluxo do dashboard:
- Clique no menu (três linhas) no canto superior direito
- Selecione "Import"
- Cole o seguinte JSON na aba "Clipboard":
```json
[{"id":"2fc36a874a2e9092","type":"tab","label":"Sala de Aula Inteligente","disabled":false,"info":"","env":[]},{"id":"ac8f699b4347d3ca","type":"ui_tab","name":"Sala de Aula","icon":"dashboard","disabled":false,"hidden":false},{"id":"a91d532e1ab68f50","type":"ui_group","name":"Minha sala de aula inteligente","tab":"ac8f699b4347d3ca","order":1,"disp":true,"width":"10","collapse":false,"className":""},{"id":"3c64433e6cb77a06","type":"mqtt-broker","name":"","broker":"test.mosquitto.org","port":1883,"clientid":"","autoConnect":true,"usetls":false,"protocolVersion":4,"keepalive":60,"cleansession":true,"autoUnsubscribe":true,"birthTopic":"","birthQos":"0","birthRetain":"false","birthPayload":"","birthMsg":{},"closeTopic":"","closeQos":"0","closeRetain":"false","closePayload":"","closeMsg":{},"willTopic":"","willQos":"0","willRetain":"false","willPayload":"","willMsg":{},"userProps":"","sessionExpiry":""},{"id":"a7fb41b0423b899a","type":"ui_text","z":"2fc36a874a2e9092","group":"a91d532e1ab68f50","order":1,"width":0,"height":0,"name":"PIR","label":"Presença","format":"{{msg.payload.presence}}","layout":"col-center","className":"","style":false,"font":"","fontSize":16,"color":"#000000","x":630,"y":320,"wires":[]},{"id":"866cbbf9444512d2","type":"ui_text","z":"2fc36a874a2e9092","group":"a91d532e1ab68f50","order":3,"width":0,"height":0,"name":"DHT Data","label":"Temperatura e Humidade","format":"{{msg.payload}}","layout":"col-center","className":"","style":false,"font":"","fontSize":16,"color":"#000000","x":620,"y":520,"wires":[]},{"id":"5ebc6e33d4349bcb","type":"ui_text","z":"2fc36a874a2e9092","group":"a91d532e1ab68f50","order":5,"width":0,"height":0,"name":"AC Status","label":"Ar Condicionado","format":"{{msg.payload.status}}","layout":"col-center","className":"","style":false,"font":"","fontSize":16,"color":"#000000","x":480,"y":460,"wires":[]},{"id":"6b62b2175b53279c","type":"mqtt in","z":"2fc36a874a2e9092","name":"temperatura e umidade","topic":"esp32/dht","qos":"0","datatype":"auto-detect","broker":"3c64433e6cb77a06","nl":false,"rap":true,"rh":0,"inputs":0,"x":220,"y":600,"wires":[["866cbbf9444512d2"]]},{"id":"bcae6e98906cec84","type":"mqtt in","z":"2fc36a874a2e9092","name":"ac status","topic":"esp32/ac_status","qos":"0","datatype":"auto-detect","broker":"3c64433e6cb77a06","nl":false,"rap":true,"rh":0,"inputs":0,"x":180,"y":520,"wires":[["5ebc6e33d4349bcb"]]},{"id":"1f195df8fd72ea14","type":"mqtt in","z":"2fc36a874a2e9092","name":"Luminosidade","topic":"esp32/ldr","qos":"0","datatype":"auto-detect","broker":"3c64433e6cb77a06","nl":false,"rap":true,"rh":0,"inputs":0,"x":190,"y":400,"wires":[["578c6b35cb4c531b"]]},{"id":"7b0c73d7f0c792a4","type":"mqtt in","z":"2fc36a874a2e9092","name":"Presença","topic":"esp32/pir","qos":"0","datatype":"auto-detect","broker":"3c64433e6cb77a06","nl":false,"rap":true,"rh":0,"inputs":0,"x":180,"y":300,"wires":[["a7fb41b0423b899a"]]},{"id":"578c6b35cb4c531b","type":"ui_gauge","z":"2fc36a874a2e9092","name":"","group":"a91d532e1ab68f50","order":4,"width":"6","height":"3","gtype":"gage","title":"Luminosidade","label":"lux","format":"{{msg.payload.light}}","min":0,"max":"500","colors":["#00b500","#fff76b","#ca3838"],"seg1":"100","seg2":"300","diff":false,"className":"","layout":"col-center","x":440,"y":400,"wires":[]},{"id":"7b60874f6b86b3f8","type":"mqtt in","z":"2fc36a874a2e9092","name":"Luz da Sala","topic":"esp32/room_light","qos":"0","datatype":"auto-detect","broker":"3c64433e6cb77a06","nl":false,"rap":true,"rh":0,"inputs":0,"x":210,"y":240,"wires":[["0148ab7bbce3ad04"]]},{"id":"0148ab7bbce3ad04","type":"ui_text","z":"2fc36a874a2e9092","group":"a91d532e1ab68f50","order":10,"width":0,"height":0,"name":"Luz da Sala","label":"Luz da Sala","format":"{{msg.payload.status}}","layout":"col-center","className":"","style":false,"font":"","fontSize":16,"color":"#000000","x":490,"y":200,"wires":[]}]
```
- Clique em "Import"

7. Configurar o broker MQTT:
- Clique duas vezes no nó MQTT (broker)
- Configure o servidor como `test.mosquitto.org`
- Porta: `1883`
- Clique em "Done"

8. Deploy do fluxo:
- Clique no botão "Deploy" no canto superior direito

9. Acessar o dashboard:
- Abra o navegador e acesse: `http://localhost:1880/ui` ou `http://<IP_WSL>:1880/ui`

10. Testar o sistema:
- Execute o programa Python no QEMU:
```bash
cd /opt/embedded-system
python3 main.py
```
- Verifique se os dados aparecem no dashboard:
  - Temperatura e umidade no medidor DHT
  - Presença no indicador PIR
  - Luminosidade no medidor de luminosidade (0-500 lux)
  - Status do ar condicionado
  - Status da luz da sala

11. Personalização do dashboard:
- Para alterar o tema, clique no ícone de engrenagem no canto superior direito
- Para reorganizar os widgets, arraste-os para a posição desejada
- Para editar um widget, clique duas vezes nele
- Para adicionar novos widgets, arraste-os da paleta lateral

12. Monitoramento de debug:
- Verifique a aba "Debug" no painel lateral direito para monitorar as mensagens MQTT
- Os dados de debug são exibidos em tempo real

## Comandos Úteis

### QEMU
- Iniciar: `runqemu qemuarm nographic slirp`
- Sair: `Ctrl + A`, depois `X`

### WSL
- Iniciar: `wsl`
- Desligar: `wsl --shutdown`

## Próximos Passos

1. Configuração do Node-RED
2. Desenvolvimento do dashboard
3. Implementação de controles
4. Testes de integração
