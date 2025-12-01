from flask import Flask
from flask_cors import CORS
from config import Config
from routes.api import api_bp

def create_app():
    app = Flask(__name__)
    app.config.from_object(Config)
    CORS(app)
    app.register_blueprint(api_bp, url_prefix='/api')
    return app

app = create_app()

if __name__ == '__main__':
    app.run(debug=True, port=5000)