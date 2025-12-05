from flask import Blueprint, request, jsonify
import serial
import json
import time
import logging
from datetime import datetime
from config import Config

api_bp = Blueprint('api', __name__)

# Configurar logging detallado
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('serial_communication.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

# Variable global para la conexión serial
ser = None

# Configuración de reintentos
MAX_RETRIES = 3
RETRY_DELAY = 1  # segundos
SEND_TIMEOUT = 5  # segundos

class SerialCommunicationError(Exception):
    """Excepción personalizada para errores de comunicación serial"""
    pass

def log_serial_event(event_type, message, data=None):
    """Registra eventos de comunicación serial con contexto adicional"""
    log_entry = {
        'timestamp': datetime.now().isoformat(),
        'event': event_type,
        'message': message
    }
    if data:
        log_entry['data'] = data
    
    logger.info(json.dumps(log_entry, indent=2))

def init_serial():
    """Inicializa la conexión serial con el PIC con reintentos"""
    global ser
    
    for attempt in range(1, MAX_RETRIES + 1):
        try:
            log_serial_event('INIT_ATTEMPT', f'Intento {attempt} de {MAX_RETRIES}')
            
            # Cerrar conexión previa si existe
            if ser is not None and ser.is_open:
                ser.close()
                time.sleep(0.5)
            
            ser = serial.Serial(
                port=Config.SERIAL_PORT,
                baudrate=Config.SERIAL_BAUDRATE,
                timeout=Config.SERIAL_TIMEOUT,
                write_timeout=2,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )
            
            # Espera a que el PIC se inicialice
            time.sleep(2)
            
            # Limpia buffers
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            
            log_serial_event(
                'INIT_SUCCESS',
                f'Puerto {Config.SERIAL_PORT} conectado exitosamente',
                {
                    'port': Config.SERIAL_PORT,
                    'baudrate': Config.SERIAL_BAUDRATE,
                    'attempt': attempt
                }
            )
            
            return True
            
        except serial.SerialException as e:
            log_serial_event(
                'INIT_ERROR',
                f'Error en intento {attempt}: {str(e)}',
                {'error': str(e), 'attempt': attempt}
            )
            
            if attempt < MAX_RETRIES:
                time.sleep(RETRY_DELAY)
            else:
                ser = None
                log_serial_event('INIT_FAILED', 'Todos los intentos fallaron')
                return False
    
    return False

def verify_connection():
    """Verifica que la conexión serial esté activa"""
    global ser
    
    if ser is None:
        return False
    
    try:
        return ser.is_open and ser.in_waiting >= 0
    except (serial.SerialException, OSError):
        return False

def send_to_pic_with_retry(data):
    """Envía datos al PIC con reintentos automáticos"""
    global ser
    
    for attempt in range(1, MAX_RETRIES + 1):
        try:
            log_serial_event(
                'SEND_ATTEMPT',
                f'Intento de envío {attempt} de {MAX_RETRIES}',
                {'data_summary': {
                    'goalType': data.get('goalType'),
                    'goalValue': data.get('goalValue')
                }}
            )
            
            # Verifica conexión
            if not verify_connection():
                log_serial_event('SEND_ERROR', 'Conexión no disponible, reintentando inicialización')
                if not init_serial():
                    if attempt < MAX_RETRIES:
                        time.sleep(RETRY_DELAY)
                        continue
                    raise SerialCommunicationError('No se pudo establecer conexión serial')
            
            # Limpia buffers
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            time.sleep(0.2)
            
            # Convierte a JSON compacto
            json_str = json.dumps(data, separators=(',', ':'))
            
            log_serial_event(
                'SENDING_DATA',
                'Enviando JSON al PIC',
                {'json_length': len(json_str), 'json': json_str}
            )
            
            # Envía byte por byte
            bytes_sent = 0
            for char in json_str:
                ser.write(char.encode('ascii'))
                ser.flush()
                bytes_sent += 1
                time.sleep(0.001)
            
            log_serial_event('DATA_SENT', f'{bytes_sent} bytes enviados correctamente')
            
            # Espera respuesta con timeout
            response_buffer = ""
            start_time = time.time()
            
            log_serial_event('WAITING_RESPONSE', 'Esperando confirmación del PIC')
            
            while (time.time() - start_time) < SEND_TIMEOUT:
                if ser.in_waiting > 0:
                    chunk = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                    response_buffer += chunk
                    
                    log_serial_event(
                        'RESPONSE_CHUNK',
                        'Datos recibidos del PIC',
                        {'chunk': chunk, 'buffer_length': len(response_buffer)}
                    )
                    
                    # Verifica respuesta completa
                    if '{"status":"ok"}' in response_buffer:
                        log_serial_event(
                            'SEND_SUCCESS',
                            'Confirmación recibida del PIC',
                            {
                                'attempt': attempt,
                                'response': response_buffer,
                                'elapsed_time': time.time() - start_time
                            }
                        )
                        return True, 'Configuración cargada exitosamente', response_buffer
                
                time.sleep(0.1)
            
            # Timeout - evalúa respuesta parcial
            log_serial_event(
                'RESPONSE_TIMEOUT',
                f'Timeout en intento {attempt}',
                {
                    'partial_response': response_buffer,
                    'elapsed_time': time.time() - start_time
                }
            )
            
            if response_buffer and 'ok' in response_buffer.lower():
                log_serial_event('PARTIAL_SUCCESS', 'Respuesta parcial pero parece exitosa')
                return True, 'Configuración cargada (respuesta parcial)', response_buffer
            
            # Si no es el último intento, reintentar
            if attempt < MAX_RETRIES:
                log_serial_event('RETRY', f'Reintentando en {RETRY_DELAY} segundos...')
                time.sleep(RETRY_DELAY)
            else:
                raise SerialCommunicationError(f'El PIC no respondió después de {MAX_RETRIES} intentos')
                
        except serial.SerialTimeoutException as e:
            log_serial_event(
                'TIMEOUT_ERROR',
                f'Timeout en intento {attempt}',
                {'error': str(e)}
            )
            if attempt >= MAX_RETRIES:
                raise SerialCommunicationError('Timeout al enviar datos')
                
        except serial.SerialException as e:
            log_serial_event(
                'SERIAL_ERROR',
                f'Error de comunicación en intento {attempt}',
                {'error': str(e)}
            )
            if attempt >= MAX_RETRIES:
                raise SerialCommunicationError(f'Error de comunicación: {str(e)}')
                
        except Exception as e:
            log_serial_event(
                'UNEXPECTED_ERROR',
                f'Error inesperado en intento {attempt}',
                {'error': str(e), 'type': type(e).__name__}
            )
            if attempt >= MAX_RETRIES:
                raise
    
    return False, 'Error desconocido en todos los intentos', None

@api_bp.route('/send_config', methods=['POST'])
def send_config():
    """Recibe configuración del frontend y la envía al PIC"""
    request_id = datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    
    try:
        log_serial_event('REQUEST_RECEIVED', f'Nueva solicitud de configuración', {'request_id': request_id})
        
        data = request.get_json()
        
        if not data:
            log_serial_event('VALIDATION_ERROR', 'No se recibieron datos', {'request_id': request_id})
            return jsonify({'error': 'No data provided'}), 400
        
        # Validaciones
        required_fields = ['character', 'obstacle', 'goalType', 'goalValue']
        for field in required_fields:
            if field not in data:
                log_serial_event(
                    'VALIDATION_ERROR',
                    f'Campo faltante: {field}',
                    {'request_id': request_id, 'missing_field': field}
                )
                return jsonify({'error': f'Missing field: {field}'}), 400
        
        # Valida sprites
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
        
        log_serial_event('VALIDATION_SUCCESS', 'Datos validados correctamente', {'request_id': request_id})
        
        # Prepara datos para el PIC
        pic_data = {
            'character': data['character'],
            'obstacle': data['obstacle'],
            'goalType': data['goalType'],
            'goalValue': int(data['goalValue'])
        }
        
        # Envía con reintentos
        success, message, pic_response = send_to_pic_with_retry(pic_data)
        
        if success:
            log_serial_event(
                'REQUEST_SUCCESS',
                'Solicitud completada exitosamente',
                {'request_id': request_id}
            )
            return jsonify({
                'status': 'success',
                'message': message,
                'data': data,
                'pic_response': pic_response,
                'request_id': request_id
            }), 200
        else:
            log_serial_event(
                'REQUEST_FAILED',
                'Solicitud fallida',
                {'request_id': request_id, 'reason': message}
            )
            return jsonify({
                'status': 'error',
                'message': message,
                'data': data,
                'pic_response': pic_response,
                'request_id': request_id
            }), 500
        
    except SerialCommunicationError as e:
        log_serial_event(
            'COMMUNICATION_ERROR',
            str(e),
            {'request_id': request_id}
        )
        return jsonify({
            'error': str(e),
            'request_id': request_id,
            'retry_info': f'Se realizaron {MAX_RETRIES} intentos'
        }), 500
        
    except Exception as e:
        log_serial_event(
            'UNEXPECTED_ERROR',
            f'Error inesperado: {str(e)}',
            {'request_id': request_id, 'type': type(e).__name__}
        )
        return jsonify({
            'error': str(e),
            'request_id': request_id
        }), 500

@api_bp.route('/serial/status', methods=['GET'])
def serial_status():
    """Verifica el estado de la conexión serial"""
    global ser
    
    if ser is None:
        init_serial()
    
    is_connected = verify_connection()
    
    status_info = {
        'connected': is_connected,
        'port': Config.SERIAL_PORT if is_connected else None,
        'baudrate': Config.SERIAL_BAUDRATE if is_connected else None,
        'timestamp': datetime.now().isoformat()
    }
    
    log_serial_event('STATUS_CHECK', 'Estado de conexión consultado', status_info)
    
    return jsonify(status_info), 200

@api_bp.route('/serial/reconnect', methods=['POST'])
def serial_reconnect():
    """Intenta reconectar el puerto serial"""
    global ser
    
    log_serial_event('RECONNECT_REQUEST', 'Solicitud de reconexión manual')
    
    # Cierra conexión existente
    if ser is not None and ser.is_open:
        try:
            ser.close()
            log_serial_event('CONNECTION_CLOSED', 'Conexión anterior cerrada')
        except Exception as e:
            log_serial_event('CLOSE_ERROR', f'Error al cerrar: {str(e)}')
    
    success = init_serial()
    
    return jsonify({
        'success': success,
        'message': 'Conectado exitosamente' if success else 'No se pudo conectar',
        'timestamp': datetime.now().isoformat()
    }), 200 if success else 500

@api_bp.route('/logs/recent', methods=['GET'])
def get_recent_logs():
    """Obtiene los logs recientes (últimas 50 líneas)"""
    try:
        with open('serial_communication.log', 'r', encoding='utf-8') as f:
            lines = f.readlines()
            recent_lines = lines[-50:] if len(lines) > 50 else lines
            
        return jsonify({
            'logs': ''.join(recent_lines),
            'total_lines': len(lines),
            'returned_lines': len(recent_lines)
        }), 200
    except FileNotFoundError:
        return jsonify({'error': 'Log file not found'}), 404
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@api_bp.route('/health', methods=['GET'])
def health():
    """Endpoint de health check con información detallada"""
    health_info = {
        'status': 'ok',
        'timestamp': datetime.now().isoformat(),
        'serial_connected': verify_connection(),
        'config': {
            'port': Config.SERIAL_PORT,
            'baudrate': Config.SERIAL_BAUDRATE,
            'timeout': Config.SERIAL_TIMEOUT,
            'max_retries': MAX_RETRIES
        }
    }
    
    return jsonify(health_info), 200