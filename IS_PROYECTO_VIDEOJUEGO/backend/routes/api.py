from flask import Blueprint, request, jsonify
import serial
import json
import time
import threading

# Importación relativa - el config está un nivel arriba
try:
    from ..config import Config
except ImportError:
    # Fallback si la importación relativa falla
    import sys
    import os
    sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from config import Config

api_bp = Blueprint('api', __name__)

# Variable global para la conexión serial
ser = None

# Variables del watchdog
watchdog_running = False
watchdog_thread = None
connection_status = {
    'is_connected': False,
    'last_check': None,
    'disconnection_count': 0,
    'reconnection_attempts': 0
}

# Variable global para telemetría
latest_telemetry = None

# Thread de lectura serial
serial_reader_thread = None
serial_reader_running = False

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
    """
    Thread que verifica la conexión serial cada 5 segundos
    y reconecta automáticamente si se detecta desconexión
    """
    global ser, watchdog_running, connection_status
    
    print("[WATCHDOG] Iniciado - Verificando conexión cada 5 segundos")
    
    while watchdog_running:
        time.sleep(5)
        
        connection_status['last_check'] = time.strftime('%Y-%m-%d %H:%M:%S')
        
        if not check_connection():
            if connection_status['is_connected']:
                connection_status['disconnection_count'] += 1
                print(f"[WATCHDOG] ⚠ Desconexión detectada (#{connection_status['disconnection_count']})")
                connection_status['is_connected'] = False
            
            print(f"[WATCHDOG] Intentando reconectar...")
            connection_status['reconnection_attempts'] += 1
            
            if init_serial():
                print(f"[WATCHDOG] ✓ Reconexión exitosa")
                connection_status['is_connected'] = True
                start_serial_reader()  # Reiniciar el lector serial
            else:
                print(f"[WATCHDOG] ✗ Reconexión fallida (intento #{connection_status['reconnection_attempts']})")
        else:
            connection_status['is_connected'] = True
    
    print("[WATCHDOG] Detenido")

def serial_reader_worker():
    """
    Thread que lee constantemente del puerto serial
    y detecta mensajes de telemetría del PIC
    """
    global ser, serial_reader_running, latest_telemetry
    
    print("[SERIAL_READER] Iniciado - Escuchando telemetría del PIC")
    
    buffer = ""
    
    while serial_reader_running:
        try:
            if ser and ser.is_open and ser.in_waiting > 0:
                # Leer datos disponibles
                chunk = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                buffer += chunk
                
                # IGNORAR mensajes de confirmación ({"status":"loaded"...})
                # Solo procesar telemetría de juego ({"obstacles":...)
                
                # Buscar JSON completo de telemetría
                # Formato esperado: {"obstacles":15,"time":45,"result":"win"}
                start_idx = buffer.find('{"obstacles"')
                if start_idx != -1:
                    end_idx = buffer.find('}', start_idx)
                    if end_idx != -1:
                        json_str = buffer[start_idx:end_idx+1]
                        
                        try:
                            # Parsear JSON
                            telemetry_data = json.loads(json_str)
                            
                            # Validar que tenga los campos esperados
                            if 'obstacles' in telemetry_data and 'time' in telemetry_data and 'result' in telemetry_data:
                                # Normalizar resultado
                                result = telemetry_data['result'].lower()
                                normalized_result = 'victory' if result in ['win', 'victory'] else 'defeat'
                                
                                # Guardar telemetría
                                latest_telemetry = {
                                    'obstacles_avoided': int(telemetry_data['obstacles']),
                                    'survival_time': int(telemetry_data['time']),
                                    'result': normalized_result,
                                    'timestamp': time.strftime('%Y-%m-%d %H:%M:%S')
                                }
                                
                                print(f"[SERIAL_READER] ✓ Telemetría recibida del PIC: {latest_telemetry}")
                                
                                # Limpiar buffer después de procesar
                                buffer = buffer[end_idx+1:]
                            else:
                                print(f"[SERIAL_READER] ⚠ JSON incompleto: {json_str}")
                                buffer = buffer[end_idx+1:]
                                
                        except json.JSONDecodeError as e:
                            print(f"[SERIAL_READER] ✗ Error al parsear JSON: {e}")
                            print(f"[SERIAL_READER] Contenido: {json_str}")
                            buffer = buffer[end_idx+1:]
                        except ValueError as e:
                            print(f"[SERIAL_READER] ✗ Error en valores: {e}")
                            buffer = buffer[end_idx+1:]
                
                # Limpiar mensajes de confirmación del buffer
                if '{"status":"loaded"' in buffer:
                    conf_start = buffer.find('{"status":"loaded"')
                    conf_end = buffer.find('}', conf_start)
                    if conf_end != -1:
                        # Remover este mensaje de confirmación del buffer
                        buffer = buffer[:conf_start] + buffer[conf_end+1:]
                        print("[SERIAL_READER] 🗑️ Mensaje de confirmación descartado")
                
                # Mantener buffer pequeño (últimos 500 caracteres)
                if len(buffer) > 500:
                    buffer = buffer[-500:]
            else:
                # Pequeña pausa si no hay datos
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
    print("[SERIAL_READER] Deteniendo...")

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
        # Cerrar conexión anterior si existe
        if ser is not None and ser.is_open:
            ser.close()
        
        ser = serial.Serial(
            port=Config.SERIAL_PORT,
            baudrate=Config.SERIAL_BAUDRATE,
            timeout=Config.SERIAL_TIMEOUT,
            write_timeout=2
        )
        time.sleep(2)  # Espera a que el PIC se inicialice
        
        # Limpiar buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        connection_status['is_connected'] = True
        print(f"✓ Puerto serial {Config.SERIAL_PORT} conectado")
        
        # Iniciar lector serial
        start_serial_reader()
        
        return True
    except serial.SerialException as e:
        connection_status['is_connected'] = False
        print(f"✗ Error al abrir puerto serial: {e}")
        ser = None
        return False

def send_to_pic(data):
    """Envía datos JSON al PIC vía serial y espera confirmación"""
    global ser, serial_reader_running
    
    # Intenta inicializar si no está conectado
    if ser is None or not ser.is_open:
        if not init_serial():
            return False, "Puerto serial no disponible", None
    
    try:
        # PAUSA TEMPORAL del serial_reader para evitar interferencias
        reader_was_running = serial_reader_running
        if reader_was_running:
            stop_serial_reader()
            time.sleep(0.2)  # Pequeña pausa para que termine de leer
        
        # Convierte el diccionario a JSON compacto (sin espacios)
        json_str = json.dumps(data, separators=(',', ':'))
        
        # Limpia AGRESIVAMENTE los buffers
        ser.reset_input_buffer()
        time.sleep(0.1)
        
        # Leer y descartar cualquier dato residual
        if ser.in_waiting > 0:
            garbage = ser.read(ser.in_waiting)
            print(f"[SEND_CONFIG] 🗑️ Descartados {len(garbage)} bytes residuales")
        
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Envía el JSON
        ser.write(json_str.encode('ascii'))
        ser.flush()
        
        print(f"→ Enviado al PIC: {json_str}")
        
        # Espera respuesta del PIC con timeout extendido
        response_timeout = 5  # Aumentado a 5 segundos
        start_time = time.time()
        response_buffer = ""
        
        while (time.time() - start_time) < response_timeout:
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                response_buffer += chunk
                
                # Busca el JSON de respuesta completo
                if '{"status":"loaded"' in response_buffer:
                    # Buscar el cierre del JSON
                    start_idx = response_buffer.find('{"status":"loaded"')
                    end_idx = response_buffer.find('}', start_idx)
                    
                    if end_idx != -1:
                        json_response = response_buffer[start_idx:end_idx+1]
                        print(f"← Respuesta del PIC: {json_response}")
                        
                        # REINICIAR serial_reader si estaba corriendo
                        if reader_was_running:
                            time.sleep(0.2)
                            start_serial_reader()
                        
                        return True, "Configuración cargada exitosamente", json_response
            
            time.sleep(0.05)  # Polling más frecuente
        
        # REINICIAR serial_reader aunque haya fallado
        if reader_was_running:
            start_serial_reader()
        
        # Si llegamos aquí, no se recibió la confirmación
        if response_buffer:
            print(f"← Respuesta parcial del PIC: {response_buffer}")
            return False, f"Respuesta incompleta del PIC: {response_buffer}", response_buffer
        else:
            return False, "El PIC no respondió (timeout)", None
        
    except serial.SerialTimeoutException:
        # Reiniciar reader
        if reader_was_running:
            start_serial_reader()
        return False, "Timeout al enviar datos", None
    except Exception as e:
        # Reiniciar reader
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
        
        # Valida que character y obstacle sean arrays de 8 elementos
        if not isinstance(data['character'], list) or len(data['character']) != 8:
            return jsonify({'error': 'character debe ser un array de 8 elementos'}), 400
        
        if not isinstance(data['obstacle'], list) or len(data['obstacle']) != 8:
            return jsonify({'error': 'obstacle debe ser un array de 8 elementos'}), 400
        
        # Valida goalType
        if data['goalType'] not in ['time', 'obstacles']:
            return jsonify({'error': 'goalType debe ser "time" u "obstacles"'}), 400
        
        # Valida goalValue
        if not isinstance(data['goalValue'], (int, float)) or data['goalValue'] <= 0:
            return jsonify({'error': 'goalValue debe ser un número positivo'}), 400
        
        # Prepara datos completos para el PIC
        pic_data = {
            'character': data['character'],
            'obstacle': data['obstacle'],
            'goalType': data['goalType'],
            'goalValue': int(data['goalValue'])
        }
        
        # Envía al PIC y espera confirmación
        success, message, pic_response = send_to_pic(pic_data)
        
        if success:
            return jsonify({
                'status': 'success',
                'message': message,
                'data': data,
                'pic_response': pic_response
            }), 200
        else:
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
    
    # Cierra conexión existente
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
    """
    Endpoint alternativo para recibir telemetría vía HTTP POST
    (útil para pruebas manuales)
    
    Espera JSON:
    {
        "obstacles": 15,
        "time": 45,
        "result": "win" o "lose"
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        # Validar campos requeridos
        required_fields = ['obstacles', 'time', 'result']
        for field in required_fields:
            if field not in data:
                return jsonify({'error': f'Missing field: {field}'}), 400
        
        # Validar tipos de datos
        if not isinstance(data['obstacles'], int) or data['obstacles'] < 0:
            return jsonify({'error': 'obstacles debe ser un entero no negativo'}), 400
        
        if not isinstance(data['time'], int) or data['time'] < 0:
            return jsonify({'error': 'time debe ser un entero no negativo'}), 400
        
        if data['result'] not in ['win', 'lose', 'victory', 'defeat']:
            return jsonify({'error': 'result debe ser "win", "lose", "victory" o "defeat"'}), 400
        
        normalized_result = 'victory' if data['result'] in ['win', 'victory'] else 'defeat'
        
        # Guardar telemetría en variable global
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
    """
    Obtiene la última telemetría recibida
    El frontend puede hacer polling a este endpoint
    """
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
    """
    Limpia la telemetría almacenada
    """
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
    # Iniciar watchdog automáticamente si no está corriendo
    if not watchdog_running:
        start_watchdog()
    
    return jsonify({
        'status': 'ok',
        'watchdog_active': watchdog_running,
        'serial_reader_active': serial_reader_running
    }), 200