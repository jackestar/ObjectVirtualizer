# Virtualizador de Objetos ESP32 - Logo

Este proyecto permite controlar y visualizar datos de un escáner basado en ESP32. Incluye un servidor en Python para la comunicación con el ESP32 y un código para el ESP32 que se puede compilar y subir utilizando Arduino IDE o Arduino CLI.

## Instalación del Servidor de Pruebas en Python

### Requisitos
- Python 3.8 o superior
- `pip` instalado

### Pasos
1. Clona este repositorio:
   ```bash
   git clone https://github.com/tu_usuario/ObjectVirtualizer.git
   cd ObjectVirtualizer/Server
   ```

2. Crea un entorno virtual:
   ```bash
   python3 -m venv venv
   source venv/bin/activate  # En Windows usa: venv\Scripts\activate
   ```

3. Instala las dependencias:
   ```bash
   pip install -r requirements.txt
   ```

4. Ejecuta el servidor:
   ```bash
   python app.py
   ```

5. Accede al panel de control en tu navegador en [http://localhost:5000](http://localhost:5000).

## Compilar y Subir el Código para el ESP32

### Usando Arduino IDE

1. Descarga e instala [Arduino IDE](https://www.arduino.cc/en/software).
2. Instala el soporte para ESP32:
   - Ve a `Archivo > Preferencias`.
   - En "Gestor de URLs Adicionales de Tarjetas", añade: `https://dl.espressif.com/dl/package_esp32_index.json`.
   - Ve a `Herramientas > Placa > Gestor de Tarjetas`, busca "esp32" e instálalo.
3. Abre el archivo `Arduino/AsDriver/AsDriver.ino` en Arduino IDE.
4. Configura la placa y el puerto:
   - Ve a `Herramientas > Placa > ESP32 Arduino > ESP32 Dev Module`.
   - Selecciona el puerto correspondiente a tu ESP32 en `Herramientas > Puerto`.
5. Compila y sube el código:
   - Haz clic en el botón de "Subir".

### Usando Arduino CLI

1. Descarga e instala [Arduino CLI](https://arduino.github.io/arduino-cli/installation/).
2. Instala el soporte para ESP32:
   ```bash
   arduino-cli core update-index
   arduino-cli core install esp32:esp32
   ```
3. Compila el código:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32dev Arduino/AsDriver
   ```
4. Sube el código al ESP32:
   ```bash
   arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32dev Arduino/AsDriver
   ```
   Reemplaza `/dev/ttyUSB0` con el puerto correspondiente a tu ESP32.

## Notas
- Asegúrate de configurar correctamente las credenciales WiFi en el archivo [`Arduino/AsDriver/AsDriver.ino`](Arduino/AsDriver/AsDriver.ino ) antes de compilar.
- El servidor debe estar ejecutándose para que el ESP32 pueda comunicarse con él.