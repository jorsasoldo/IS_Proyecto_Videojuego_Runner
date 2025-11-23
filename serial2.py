import serial
import time
import json

puerto = 'COM3'   # Ajusta al puerto COM que aparece en tu sistema

def comunicar_con_pic():
    try:
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
        time.sleep(0.5)
        if ser.in_waiting > 0:
            inicio = ser.read(ser.in_waiting).decode(errors='ignore')
            print(f"Mensaje inicial del PIC: '{inicio.strip()}'")
        
        # Crear y enviar JSON simple con comando
        comando = {"command": 1}
        json_str = json.dumps(comando) + '\n'  # Agregar newline como delimitador
        json_bytes = json_str.encode('utf-8')
        
        print(f"Enviando JSON: {json_str.strip()}")
        ser.write(json_bytes)
        ser.flush()  # Asegurar que se envíe inmediatamente
        
        # Esperar respuesta
        respuesta_recibida = False
        timeout = time.time() + 5  # 5 segundos de timeout
        
        while time.time() < timeout:
            if ser.in_waiting > 0:
                # Leer línea completa
                linea = ser.readline().decode('utf-8', errors='ignore').strip()
                if linea:
                    print(f"Respuesta recibida: '{linea}'")
                    
                    # Verificar respuesta simple
                    if "response" in linea:
                        if "1" in linea:
                            print("[OK] Comunicación JSON exitosa - LED encendido")
                        elif "0" in linea:
                            print("[OK] Comando 0 recibido - LED apagado")
                        respuesta_recibida = True
                    break
            
            time.sleep(0.1)
        
        if not respuesta_recibida:
            print("[ERROR] No se recibió respuesta JSON válida del PIC")
            print("Sugerencias:")
            print("1. Verificar que el PIC esté programado correctamente")
            print("2. Verificar conexiones TX/RX (cruzadas)")
            print("3. Verificar velocidad de baudios (9600)")
            print("4. Verificar alimentación del PIC")
        
        ser.close()
        print("Puerto cerrado")
        
    except Exception as e:
        print(f"Error: {e}")
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    comunicar_con_pic()