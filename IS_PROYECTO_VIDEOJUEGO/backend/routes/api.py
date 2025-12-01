<<<<<<< HEAD
from flask import Blueprint, request, jsonify

api_bp = Blueprint('api', __name__)

@api_bp.route('/send_config', methods=['POST'])
def send_config():
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        required_fields = ['character', 'obstacle', 'goalType', 'goalValue']
        for field in required_fields:
            if field not in data:
                return jsonify({'error': f'Missing field: {field}'}), 400
        
        return jsonify({
            'status': 'success',
            'message': 'Configuration received',
            'data': data
        }), 200
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@api_bp.route('/health', methods=['GET'])
def health():
=======
from flask import Blueprint, request, jsonify

api_bp = Blueprint('api', __name__)

@api_bp.route('/send_config', methods=['POST'])
def send_config():
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        required_fields = ['character', 'obstacle', 'goalType', 'goalValue']
        for field in required_fields:
            if field not in data:
                return jsonify({'error': f'Missing field: {field}'}), 400
        
        return jsonify({
            'status': 'success',
            'message': 'Configuration received',
            'data': data
        }), 200
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@api_bp.route('/health', methods=['GET'])
def health():
>>>>>>> 234a90a8e71a7fdf874efb224fa92217ccda0f3c
    return jsonify({'status': 'ok'}), 200