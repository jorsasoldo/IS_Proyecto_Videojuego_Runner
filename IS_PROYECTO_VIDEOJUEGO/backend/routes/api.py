from flask import Blueprint, request, jsonify
import serial
import json
import time
import threading  # AGREGAR ESTE IMPORT
from config import Config

api_bp = Blueprint('api', __name__)

# Variable global para la conexión serial
ser = None
# AGREGAR ESTAS VARIABLES
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

def check_connection():
    """Verifica si la conexión serial sigue activa"""
    global ser
    try:
        if ser is None:
            return False
        if not ser.is_open:
            return False
        # Verificar que el puerto responda
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
        time.sleep(5)  # Verificar cada 5 segundos
        
        connection_status['last_check'] = time.strftime('%Y-%m-%d %H:%M:%S')
        
        if not check_connection():
            if connection_status['is_connected']:
                # Detectada desconexión
                connection_status['disconnection_count'] += 1
                print(f"[WATCHDOG] ⚠ Desconexión detectada (#{connection_status['disconnection_count']})")
                connection_status['is_connected'] = False
            
            # Intentar reconectar
            print(f"[WATCHDOG] Intentando reconectar...")
            connection_status['reconnection_attempts'] += 1
            
            if init_serial():
                print(f"[WATCHDOG] ✓ Reconexión exitosa")
                connection_status['is_connected'] = True
            else:
                print(f"[WATCHDOG] ✗ Reconexión fallida (intento #{connection_status['reconnection_attempts']})")
        else:
            # Conexión activa
            connection_status['is_connected'] = True
    
    print("[WATCHDOG] Detenido")

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
        return True
    except serial.SerialException as e:
        connection_status['is_connected'] = False
        print(f"✗ Error al abrir puerto serial: {e}")
        ser = None
        return False

def send_to_pic(data):
    """Envía datos JSON al PIC vía serial y espera confirmación"""
    global ser
    
    # Intenta inicializar si no está conectado
    if ser is None or not ser.is_open:
        if not init_serial():
            return False, "Puerto serial no disponible", None
    
    try:
        # Convierte el diccionario a JSON compacto (sin espacios)
        json_str = json.dumps(data, separators=(',', ':'))
        
        # Limpia el buffer de entrada antes de enviar
        ser.reset_input_buffer()
        
        # Envía el JSON
        ser.write(json_str.encode('ascii'))
        ser.flush()
        
        print(f"→ Enviado al PIC: {json_str}")
        
        # Espera respuesta del PIC con timeout extendido
        response_timeout = 3  # 3 segundos para procesar
        start_time = time.time()
        response_buffer = ""
        
        while (time.time() - start_time) < response_timeout:
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                response_buffer += chunk
                
                # Busca el JSON de respuesta
                if '{"status":"ok"}' in response_buffer:
                    print(f"← Respuesta del PIC: {response_buffer}")
                    return True, "Configuración cargada exitosamente", response_buffer
            
            time.sleep(0.1)  # Pequeña pausa para no saturar el CPU
        
        # Si llegamos aquí, no se recibió la confirmación
        if response_buffer:
            print(f"← Respuesta parcial del PIC: {response_buffer}")
            return False, f"Respuesta incompleta del PIC: {response_buffer}", response_buffer
        else:
            return False, "El PIC no respondió (timeout)", None
        
    except serial.SerialTimeoutException:
        return False, "Timeout al enviar datos", None
    except Exception as e:
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
    global watchdog_running, connection_status
    
    return jsonify({
        'watchdog_active': watchdog_running,
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
    Recibe telemetría del PIC al finalizar una partida
    
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
        
        # Guardar telemetría en variable global para que el frontend la pueda consultar
        global latest_telemetry
        latest_telemetry = {
            'obstacles_avoided': data['obstacles'],
            'survival_time': data['time'],
            'result': normalized_result,
            'timestamp': time.strftime('%Y-%m-%d %H:%M:%S')
        }
        
        print(f"[TELEMETRY] Recibida: {latest_telemetry}")
        
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
        'watchdog_active': watchdog_running
    }), 200