from flask import Blueprint, request, jsonify
import serial
import json
import time
from config import Config

api_bp = Blueprint('api', __name__)

# Variable global para la conexión serial
ser = None

def init_serial():
    """Inicializa la conexión serial con el PIC"""
    global ser
    try:
        ser = serial.Serial(
            port=Config.SERIAL_PORT,
            baudrate=Config.SERIAL_BAUDRATE,
            timeout=Config.SERIAL_TIMEOUT,
            write_timeout=2
        )
        time.sleep(2)  # Espera a que el PIC se inicialice
        print(f"✓ Puerto serial {Config.SERIAL_PORT} conectado")
        return True
    except serial.SerialException as e:
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
        # Limpia buffers antes de empezar
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Pequeña pausa para que el PIC esté listo
        time.sleep(0.2)
        
        # Convierte el diccionario a JSON compacto (sin espacios)
        json_str = json.dumps(data, separators=(',', ':'))
        
        # Envía el JSON byte por byte para evitar problemas de buffer
        for char in json_str:
            ser.write(char.encode('ascii'))
            ser.flush()
            time.sleep(0.001)  # 1ms entre caracteres
        
        print(f"→ Enviado al PIC: {json_str}")
        
        # Espera respuesta del PIC con timeout extendido
        response_timeout = 5  # 5 segundos para procesar
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
            # A veces el PIC responde pero el JSON está incompleto
            if "ok" in response_buffer.lower():
                return True, "Configuración cargada (respuesta parcial)", response_buffer
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

@api_bp.route('/health', methods=['GET'])
def health():
    """Endpoint de health check"""
    return jsonify({'status': 'ok'}), 200