import serial
import time

puerto = 'COM5'   # Ajusta al puerto COM que aparece en tu sistema

# Abrir puerto con DTR/RTS deshabilitados para evitar reset del PIC
ser = serial.Serial(
    port=puerto,
    baudrate=9600,
    timeout=2,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    rtscts=False,
    dsrdtr=False
)

print(f"Puerto {puerto} abierto")
time.sleep(2)  # Espera a que el PIC arranque

# Limpiar buffers
ser.reset_input_buffer()
ser.reset_output_buffer()

# Leer mensaje de inicio si existe
if ser.in_waiting > 0:
    inicio = ser.read(ser.in_waiting).decode(errors='ignore')
    print(f"Mensaje inicial del PIC: '{inicio.strip()}'")
    time.sleep(0.5)

# Limpiar buffer nuevamente
ser.reset_input_buffer()

# Enviar '1'
ser.write(b'1')
ser.flush()  # Asegurar que se envíe inmediatamente
print("Enviado: '1'")

# Esperar respuesta con timeout progresivo
respuesta_recibida = False
for intento in range(10):
    time.sleep(0.1)
    if ser.in_waiting > 0:
        bytes_disponibles = ser.in_waiting
        print(f"Bytes disponibles: {bytes_disponibles}")
        resp = ser.read(1).decode(errors='ignore')
        print(f"Respuesta PIC: '{resp}'")
        
        if resp == '1':
            print("[OK] Comunicacion exitosa: PIC devolvio '1'")
            respuesta_recibida = True
        else:
            print(f"[ADVERTENCIA] Se recibio '{resp}' en lugar de '1'")
        break

if not respuesta_recibida:
    print("[ERROR] No se recibio respuesta del PIC")
    print("El PIC puede estar bloqueado. Intenta:")
    print("1. Desconectar y reconectar la alimentacion del PIC")
    print("2. Verificar las conexiones TX/RX")
    print("3. Reprogramar el PIC")

ser.close()
print("\nPuerto cerrado")