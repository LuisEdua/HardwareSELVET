import socket
import json
import RPi.GPIO as GPIO
import smbus2
from RPLCD.i2c import CharLCD
import requests

# Dirección I2C del módulo LCD
lcd_address = 0x27

GPIO.setwarnings(False)  # Desactivar advertencias de GPIO
api_url = "https://backselvet.viewdns.net/api/measures/"
# Resto de tu código aquí

# Configurar el LCD y el buzzer (asume que están conectados en los pines GPIO correspondientes)
lcd = CharLCD(i2c_expander='PCF8574', address=lcd_address, port=1, cols=16, rows=2)
buzzer_pin = 18
GPIO.setmode(GPIO.BCM)
GPIO.setup(buzzer_pin, GPIO.OUT)

# Función para configurar el LCD y mostrar el mensaje de bienvenida
def setup_lcd():
    lcd.clear()
    lcd.cursor_pos = (0, 0)
    lcd.write_string("Bienvenido al")
    lcd.cursor_pos = (1, 0)
    lcd.write_string("servidor webhook!")

# Configurar el servidor UDP
server_ip = "192.168.156.159"
server_port = 8080
buffer_size = 1024

udp_server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_server.bind((server_ip, server_port))

def process_data(data):
    print(data)

    # Decodificar los datos JSON recibidos
    data_dict = json.loads(data)

    # Mostrar los datos en el LCD
    lcd.clear()
    row = 0
    for key, value in data_dict.items():
        if row < 2:
            row = 0 
        lcd.cursor_pos = (row, 0)
        lcd.write_string(f"{key}: {value}")
        row += 1

    # Activar el buzzer si los valores superan los umbrales
    lp = data_dict["LP"]
    h2 = data_dict["H2"]
    co = data_dict["CO"]
    humidity = data_dict["humidity"]
    temperature = data_dict["temperature"]
    serialNumberESP32 = data_dict["SerialNumber"]

    if lp is not None and h2 is not None and co is not None and humidity is not None and temperature is not None:
        if lp > 5 or h2 > 20 or co > 50 or humidity < 55.6 or temperature < 15 or temperature > 25:
            GPIO.output(buzzer_pin, GPIO.HIGH)
        else:
            GPIO.output(buzzer_pin, GPIO.LOW)
    else:
        # Algunos valores son None, no se pueden verificar los umbrales, hacer algo apropiado aquí
        pass

    payload = {
            "lpg": lp,
            "hydrogen": h2,
            "co": co,
            "humidity": humidity,
            "temperature": temperature,
            "serial_number_esp32": serialNumberESP32,
            "error": 0,
            "serial_number_dispositive": "10000000f22d4746"
        }

    try:
        response = requests.post(api_url, json=payload)
        if response.status_code == 200:
            print("Datos enviados correctamente a la API.")
        else:
            print("Error al enviar los datos a la API:", response.status_code)
    except requests.exceptions.RequestException as e:
        print("Error de conexión con la API:", e)

try:
    print("Servidor UDP iniciado. Esperando datos...")
    
    # Configurar el LCD y mostrar mensaje de bienvenida
    setup_lcd()

    while True:
        data, addr = udp_server.recvfrom(buffer_size)
        data_str = data.decode("utf-8")
        process_data(data_str)
except KeyboardInterrupt:
    print("Apagando el servidor...")
    lcd.clear()
    GPIO.output(buzzer_pin, GPIO.LOW)
    GPIO.cleanup()
    udp_server.close()
