from flask import Blueprint, request, jsonify
import serial
import json
import time
import threading

try:
    from ..config import Config
except ImportError:
    import sys
    import os
    sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from config import Config

api_bp = Blueprint('api', __name__)

ser = None
watchdog_running = False
watchdog_thread = None
connection_status = {
    'is_connected': False,
    'last_check': None,
    'disconnection_count': 0,
    'reconnection_attempts': 0
}

latest_telemetry = None
serial_reader_thread = None
serial_reader_running = False

# NUEVO: Lock para sincronizar acceso al puerto serial
serial_lock = threading.Lock()

def check_connection():
    """Verifica si la conexión serial sigue activa"""
    global ser
    try:
        if ser is None:
            return False
        if not ser.is_open:
            return False
        return True
    except:
        return False

def watchdog_worker():
    """Thread que verifica la conexión serial cada 5 segundos"""
    global ser, watchdog_running, connection_status
    
    print("[WATCHDOG] Iniciado - Verificando conexión cada 5 segundos")
    
    while watchdog_running:
        time.sleep(5)
        
        connection_status['last_check'] = time.strftime('%Y-%m-%d %H:%M:%S')
        
        if not check_connection():
            if connection_status['is_connected']:
                connection_status['disconnection_count'] += 1
                print(f"[WATCHDOG] ⚠️ Desconexión detectada (#{connection_status['disconnection_count']})")
                connection_status['is_connected'] = False
            
            print(f"[WATCHDOG] Intentando reconectar...")
            connection_status['reconnection_attempts'] += 1
            
            if init_serial():
                print(f"[WATCHDOG] ✓ Reconexión exitosa")
                connection_status['is_connected'] = True
                start_serial_reader()
            else:
                print(f"[WATCHDOG] ✗ Reconexión fallida (intento #{connection_status['reconnection_attempts']})")
        else:
            connection_status['is_connected'] = True
    
    print("[WATCHDOG] Detenido")

def serial_reader_worker():
    """Thread que lee constantemente del puerto serial"""
    global ser, serial_reader_running, latest_telemetry, serial_lock
    
    print("[SERIAL_READER] Iniciado - Escuchando telemetría del PIC")
    
    buffer = ""
    
    while serial_reader_running:
        try:
            # NUEVO: Usar lock para evitar conflictos
            with serial_lock:
                if ser and ser.is_open and ser.in_waiting > 0:
                    chunk = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                    buffer += chunk
            
            # Procesar telemetría
            start_idx = buffer.find('{"obstacles"')
            if start_idx != -1:
                end_idx = buffer.find('}', start_idx)
                if end_idx != -1:
                    json_str = buffer[start_idx:end_idx+1]
                    
                    try:
                        telemetry_data = json.loads(json_str)
                        
                        if 'obstacles' in telemetry_data and 'time' in telemetry_data and 'result' in telemetry_data:
                            result = telemetry_data['result'].lower()
                            normalized_result = 'victory' if result in ['win', 'victory'] else 'defeat'
                            
                            latest_telemetry = {
                                'obstacles_avoided': int(telemetry_data['obstacles']),
                                'survival_time': int(telemetry_data['time']),
                                'result': normalized_result,
                                'timestamp': time.strftime('%Y-%m-%d %H:%M:%S')
                            }
                            
                            print(f"[SERIAL_READER] ✓ Telemetría recibida: {latest_telemetry}")
                            buffer = buffer[end_idx+1:]
                        else:
                            print(f"[SERIAL_READER] ⚠️ JSON incompleto: {json_str}")
                            buffer = buffer[end_idx+1:]
                            
                    except (json.JSONDecodeError, ValueError) as e:
                        print(f"[SERIAL_READER] ✗ Error: {e}")
                        buffer = buffer[end_idx+1:]
            
            # Limpiar mensajes de confirmación
            if '{"status":"loaded"' in buffer:
                conf_start = buffer.find('{"status":"loaded"')
                conf_end = buffer.find('}', conf_start)
                if conf_end != -1:
                    buffer = buffer[:conf_start] + buffer[conf_end+1:]
            
            # Mantener buffer pequeño
            if len(buffer) > 500:
                buffer = buffer[-500:]
                
            # Pausa si no hay datos
            time.sleep(0.1)
                
        except Exception as e:
            print(f"[SERIAL_READER] ✗ Error: {e}")
            time.sleep(0.5)
    
    print("[SERIAL_READER] Detenido")

def start_serial_reader():
    """Inicia el thread de lectura serial"""
    global serial_reader_running, serial_reader_thread
    
    if serial_reader_running:
        print("[SERIAL_READER] Ya está en ejecución")
        return
    
    if ser is None or not ser.is_open:
        print("[SERIAL_READER] No se puede iniciar - puerto serial no disponible")
        return
    
    serial_reader_running = True
    serial_reader_thread = threading.Thread(target=serial_reader_worker, daemon=True)
    serial_reader_thread.start()
    print("[SERIAL_READER] Thread iniciado")

def stop_serial_reader():
    """Detiene el thread de lectura serial"""
    global serial_reader_running
    
    if not serial_reader_running:
        return
    
    serial_reader_running = False
    time.sleep(0.3)  # NUEVO: Esperar más tiempo para asegurar que se detenga
    print("[SERIAL_READER] Detenido")

def start_watchdog():
    """Inicia el thread del watchdog"""
    global watchdog_running, watchdog_thread
    
    if watchdog_running:
        print("[WATCHDOG] Ya está en ejecución")
        return
    
    watchdog_running = True
    watchdog_thread = threading.Thread(target=watchdog_worker, daemon=True)
    watchdog_thread.start()
    print("[WATCHDOG] Thread iniciado")

def stop_watchdog():
    """Detiene el thread del watchdog"""
    global watchdog_running
    
    if not watchdog_running:
        return
    
    watchdog_running = False
    print("[WATCHDOG] Deteniendo...")

def init_serial():
    """Inicializa la conexión serial con el PIC"""
    global ser, connection_status
    try:
        if ser is not None and ser.is_open:
            ser.close()
        
        # NUEVO: Configuración mejorada con timeouts más largos
        ser = serial.Serial(
            port=Config.SERIAL_PORT,
            baudrate=Config.SERIAL_BAUDRATE,
            timeout=Config.SERIAL_TIMEOUT,
            write_timeout=3,  # Aumentado de 2 a 3 segundos
            # NUEVO: Parámetros adicionales para estabilidad
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False
        )
        
        # NUEVO: Esperar más tiempo para el FT232BL
        time.sleep(3)  # Aumentado de 2 a 3 segundos
        
        # Limpiar buffers múltiples veces
        for _ in range(3):
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            time.sleep(0.1)
        
        connection_status['is_connected'] = True
        print(f"✓ Puerto serial {Config.SERIAL_PORT} conectado")
        
        # NO iniciar el reader automáticamente
        # Se iniciará después del primer envío exitoso
        
        return True
    except serial.SerialException as e:
        connection_status['is_connected'] = False
        print(f"✗ Error al abrir puerto serial: {e}")
        ser = None
        return False

def send_to_pic(data):
    """Envía datos JSON al PIC vía serial y espera confirmación"""
    global ser, serial_reader_running, serial_lock
    
    if ser is None or not ser.is_open:
        if not init_serial():
            return False, "Puerto serial no disponible", None
    
    try:
        # NUEVO: Detener reader y esperar más tiempo
        reader_was_running = serial_reader_running
        if reader_was_running:
            print("[SEND_CONFIG] Deteniendo serial reader...")
            stop_serial_reader()
            time.sleep(0.5)  # Aumentado de 0.2 a 0.5 segundos
        
        json_str = json.dumps(data, separators=(',', ':'))
        
        # NUEVO: Usar lock para acceso exclusivo
        with serial_lock:
            # NUEVO: Limpieza AGRESIVA múltiple
            print("[SEND_CONFIG] Limpiando buffers...")
            for _ in range(5):  # Aumentado de 1 a 5 intentos
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                time.sleep(0.05)
            
            # Descartar datos residuales
            time.sleep(0.2)
            if ser.in_waiting > 0:
                garbage = ser.read(ser.in_waiting)
                print(f"[SEND_CONFIG] 🗑️ Descartados {len(garbage)} bytes: {garbage[:50]}")
            
            # Limpieza final
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            time.sleep(0.1)
            
            # NUEVO: Enviar con delay entre caracteres para FT232BL
            print(f"[SEND_CONFIG] → Enviando: {json_str}")
            for char in json_str:
                ser.write(char.encode('ascii'))
                time.sleep(0.001)  # 1ms entre caracteres
            ser.flush()
            
            # NUEVO: Esperar más tiempo antes de leer respuesta
            time.sleep(0.5)  # Dar tiempo al PIC para procesar
            
            # Espera respuesta del PIC con timeout extendido
            response_timeout = 8  # Aumentado de 5 a 8 segundos
            start_time = time.time()
            response_buffer = ""
            
            print("[SEND_CONFIG] Esperando respuesta del PIC...")
            
            while (time.time() - start_time) < response_timeout:
                if ser.in_waiting > 0:
                    chunk = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                    response_buffer += chunk
                    print(f"[SEND_CONFIG] ← Recibido: {chunk}")
                    
                    # Busca el JSON de respuesta completo
                    if '{"status":"loaded"' in response_buffer:
                        start_idx = response_buffer.find('{"status":"loaded"')
                        end_idx = response_buffer.find('}', start_idx)
                        
                        if end_idx != -1:
                            json_response = response_buffer[start_idx:end_idx+1]
                            print(f"[SEND_CONFIG] ✓ Respuesta completa: {json_response}")
                            
                            # NUEVO: Iniciar reader solo después del primer envío exitoso
                            if reader_was_running or not serial_reader_running:
                                time.sleep(0.5)
                                start_serial_reader()
                            
                            return True, "Configuración cargada exitosamente", json_response
                
                time.sleep(0.05)
        
        # Si llegamos aquí, no se recibió la confirmación
        print(f"[SEND_CONFIG] ⚠️ Timeout - Buffer recibido: {response_buffer}")
        
        # Reiniciar reader aunque haya fallado
        if reader_was_running:
            start_serial_reader()
        
        if response_buffer:
            return False, f"Respuesta incompleta del PIC: {response_buffer}", response_buffer
        else:
            return False, "El PIC no respondió (timeout)", None
        
    except serial.SerialTimeoutException:
        if reader_was_running:
            start_serial_reader()
        return False, "Timeout al enviar datos", None
    except Exception as e:
        if reader_was_running:
            start_serial_reader()
        return False, f"Error en comunicación serial: {str(e)}", None

@api_bp.route('/send_config', methods=['POST'])
def send_config():
    """Recibe configuración del frontend y la envía al PIC"""
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        # Valida campos requeridos
        required_fields = ['character', 'obstacle', 'goalType', 'goalValue']
        for field in required_fields:
            if field not in data:
                return jsonify({'error': f'Missing field: {field}'}), 400
        
        if not isinstance(data['character'], list) or len(data['character']) != 8:
            return jsonify({'error': 'character debe ser un array de 8 elementos'}), 400
        
        if not isinstance(data['obstacle'], list) or len(data['obstacle']) != 8:
            return jsonify({'error': 'obstacle debe ser un array de 8 elementos'}), 400
        
        if data['goalType'] not in ['time', 'obstacles']:
            return jsonify({'error': 'goalType debe ser "time" u "obstacles"'}), 400
        
        if not isinstance(data['goalValue'], (int, float)) or data['goalValue'] <= 0:
            return jsonify({'error': 'goalValue debe ser un número positivo'}), 400
        
        pic_data = {
            'character': data['character'],
            'obstacle': data['obstacle'],
            'goalType': data['goalType'],
            'goalValue': int(data['goalValue'])
        }
        
        # NUEVO: Intentar hasta 2 veces en caso de fallo
        max_attempts = 2
        for attempt in range(max_attempts):
            print(f"[API] Intento {attempt + 1} de {max_attempts}")
            
            success, message, pic_response = send_to_pic(pic_data)
            
            if success:
                return jsonify({
                    'status': 'success',
                    'message': message,
                    'data': data,
                    'pic_response': pic_response
                }), 200
            
            # Si falla el primer intento, esperar y reiniciar conexión
            if attempt < max_attempts - 1:
                print(f"[API] Intento {attempt + 1} falló, reiniciando conexión...")
                time.sleep(1)
                if ser and ser.is_open:
                    ser.close()
                    time.sleep(0.5)
                init_serial()
        
        # Si todos los intentos fallaron
        return jsonify({
            'status': 'error',
            'message': message,
            'data': data,
            'pic_response': pic_response
        }), 500
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@api_bp.route('/serial/status', methods=['GET'])
def serial_status():
    """Verifica el estado de la conexión serial"""
    global ser
    
    if ser is None:
        init_serial()
    
    is_connected = ser is not None and ser.is_open
    
    return jsonify({
        'connected': is_connected,
        'port': Config.SERIAL_PORT if is_connected else None,
        'baudrate': Config.SERIAL_BAUDRATE if is_connected else None
    }), 200

@api_bp.route('/serial/reconnect', methods=['POST'])
def serial_reconnect():
    """Intenta reconectar el puerto serial"""
    global ser
    
    if ser is not None and ser.is_open:
        ser.close()
    
    success = init_serial()
    
    return jsonify({
        'success': success,
        'message': 'Conectado' if success else 'No se pudo conectar'
    }), 200 if success else 500

@api_bp.route('/watchdog/start', methods=['POST'])
def start_watchdog_endpoint():
    """Inicia el watchdog de monitoreo de conexión serial"""
    start_watchdog()
    return jsonify({
        'success': True,
        'message': 'Watchdog iniciado'
    }), 200

@api_bp.route('/watchdog/stop', methods=['POST'])
def stop_watchdog_endpoint():
    """Detiene el watchdog de monitoreo de conexión serial"""
    stop_watchdog()
    return jsonify({
        'success': True,
        'message': 'Watchdog detenido'
    }), 200

@api_bp.route('/watchdog/status', methods=['GET'])
def watchdog_status():
    """Obtiene el estado del watchdog y estadísticas de conexión"""
    global watchdog_running, connection_status, serial_reader_running
    
    return jsonify({
        'watchdog_active': watchdog_running,
        'serial_reader_active': serial_reader_running,
        'connection': {
            'is_connected': connection_status['is_connected'],
            'last_check': connection_status['last_check'],
            'disconnections': connection_status['disconnection_count'],
            'reconnection_attempts': connection_status['reconnection_attempts']
        }
    }), 200

@api_bp.route('/telemetry', methods=['POST'])
def receive_telemetry():
    """Endpoint alternativo para recibir telemetría vía HTTP POST"""
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        required_fields = ['obstacles', 'time', 'result']
        for field in required_fields:
            if field not in data:
                return jsonify({'error': f'Missing field: {field}'}), 400
        
        if not isinstance(data['obstacles'], int) or data['obstacles'] < 0:
            return jsonify({'error': 'obstacles debe ser un entero no negativo'}), 400
        
        if not isinstance(data['time'], int) or data['time'] < 0:
            return jsonify({'error': 'time debe ser un entero no negativo'}), 400
        
        if data['result'] not in ['win', 'lose', 'victory', 'defeat']:
            return jsonify({'error': 'result debe ser "win", "lose", "victory" o "defeat"'}), 400
        
        normalized_result = 'victory' if data['result'] in ['win', 'victory'] else 'defeat'
        
        global latest_telemetry
        latest_telemetry = {
            'obstacles_avoided': data['obstacles'],
            'survival_time': data['time'],
            'result': normalized_result,
            'timestamp': time.strftime('%Y-%m-%d %H:%M:%S')
        }
        
        print(f"[TELEMETRY HTTP] Recibida: {latest_telemetry}")
        
        return jsonify({
            'status': 'success',
            'message': 'Telemetría recibida correctamente'
        }), 200
        
    except Exception as e:
        print(f"[TELEMETRY ERROR] {str(e)}")
        return jsonify({'error': str(e)}), 500

@api_bp.route('/telemetry/latest', methods=['GET'])
def get_latest_telemetry():
    """Obtiene la última telemetría recibida"""
    global latest_telemetry
    
    if latest_telemetry is None:
        return jsonify({
            'status': 'no_data',
            'message': 'No hay telemetría disponible'
        }), 200
    
    return jsonify({
        'status': 'ok',
        'data': latest_telemetry
    }), 200

@api_bp.route('/telemetry/clear', methods=['POST'])
def clear_telemetry():
    """Limpia la telemetría almacenada"""
    global latest_telemetry
    latest_telemetry = None
    
    print("[TELEMETRY] Buffer limpiado")
    
    return jsonify({
        'status': 'success',
        'message': 'Telemetría limpiada'
    }), 200

@api_bp.route('/health', methods=['GET'])
def health():
    """Endpoint de health check"""
    if not watchdog_running:
        start_watchdog()
    
    return jsonify({
        'status': 'ok',
        'watchdog_active': watchdog_running,
        'serial_reader_active': serial_reader_running
    }), 200