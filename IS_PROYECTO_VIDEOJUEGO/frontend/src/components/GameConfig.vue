<template>
  <div class="game-config">
    <h1>Configuración del Videojuego Runner</h1>

    <!-- Estado de conexión serial -->
    <div class="serial-status" :class="{ connected: serialConnected }">
      <span class="status-icon">{{ serialConnected ? '●' : '○' }}</span>
      <span>{{ serialConnected ? 'Conectado' : 'Desconectado' }}</span>
      <button @click="checkSerialStatus" class="btn-refresh">↻</button>
      <button v-if="!serialConnected" @click="reconnectSerial" class="btn-reconnect">
        Reconectar
      </button>
    </div>

    <!-- Editores de sprites -->
    <div class="editors-container">
      <SpriteEditor
        title="Personaje"
        v-model="config.character"
      />
      
      <SpriteEditor
        title="Obstáculo"
        v-model="config.obstacle"
      />
    </div>

    <!-- Configuración del objetivo -->
    <div class="goal-config">
      <h3>Configuración del Objetivo</h3>
      <div class="goal-inputs">
        <div class="input-group">
          <label>Tipo de Objetivo:</label>
          <select v-model="config.goalType">
            <option value="time">Tiempo</option>
            <option value="score">Puntaje</option>
            <option value="distance">Distancia</option>
          </select>
        </div>
        
        <div class="input-group">
          <label>Valor Objetivo:</label>
          <input 
            type="number" 
            v-model.number="config.goalValue"
            min="1"
            max="9999"
          />
        </div>
      </div>
    </div>

    <!-- Botón de envío -->
    <div class="send-section">
      <button 
        @click="sendConfiguration" 
        :disabled="isSending"
        class="btn-send"
      >
        {{ isSending ? 'Enviando...' : 'Enviar Configuración al PIC' }}
      </button>
    </div>

    <!-- Mensajes de respuesta -->
    <div v-if="responseMessage" class="response-message" :class="responseType">
      {{ responseMessage }}
    </div>
  </div>
</template>

<script>
import SpriteEditor from './SpriteEditor.vue'

export default {
  name: 'GameConfig',
  components: {
    SpriteEditor
  },
  data() {
    return {
      config: {
        character: [0, 0, 0, 0, 0, 0, 0, 0],
        obstacle: [0, 0, 0, 0, 0, 0, 0, 0],
        goalType: 'score',
        goalValue: 100
      },
      serialConnected: false,
      isSending: false,
      responseMessage: '',
      responseType: 'info'
    }
  },
  mounted() {
    this.checkSerialStatus()
  },
  methods: {
    async checkSerialStatus() {
      try {
        const response = await fetch('http://localhost:5000/api/serial/status')
        const data = await response.json()
        this.serialConnected = data.connected
      } catch (error) {
        console.error('Error checking serial status:', error)
        this.serialConnected = false
      }
    },

    async reconnectSerial() {
      try {
        this.showMessage('Intentando reconectar...', 'info')
        const response = await fetch('http://localhost:5000/api/serial/reconnect', {
          method: 'POST'
        })
        const data = await response.json()
        
        this.serialConnected = data.success
        this.showMessage(data.message, data.success ? 'success' : 'error')
      } catch (error) {
        console.error('Error reconnecting:', error)
        this.showMessage('Error al reconectar', 'error')
      }
    },

    async sendConfiguration() {
      if (!this.validateConfig()) {
        return
      }

      this.isSending = true
      this.responseMessage = ''

      try {
        const response = await fetch('http://localhost:5000/api/send_config', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(this.config)
        })

        const data = await response.json()

        if (response.ok) {
          this.showMessage('✓ Configuración enviada correctamente al PIC', 'success')
          console.log('Respuesta del PIC:', data.pic_response)
        } else {
          this.showMessage('✗ Error: ' + (data.error || data.message), 'error')
        }
      } catch (error) {
        console.error('Error:', error)
        this.showMessage('✗ Error de conexión con el servidor', 'error')
      } finally {
        this.isSending = false
      }
    },

    validateConfig() {
      if (this.config.character.every(byte => byte === 0)) {
        this.showMessage('⚠ El personaje está vacío', 'warning')
        return false
      }

      if (this.config.obstacle.every(byte => byte === 0)) {
        this.showMessage('⚠ El obstáculo está vacío', 'warning')
        return false
      }

      if (this.config.goalValue <= 0) {
        this.showMessage('⚠ El valor del objetivo debe ser mayor a 0', 'warning')
        return false
      }

      return true
    },

    showMessage(message, type = 'info') {
      this.responseMessage = message
      this.responseType = type

      setTimeout(() => {
        this.responseMessage = ''
      }, 5000)
    }
  }
}
</script>

<style scoped>
.game-config {
  max-width: 1200px;
  margin: 0 auto;
  padding: 2rem;
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
}

h1 {
  text-align: center;
  color: #333;
  margin-bottom: 2rem;
}

.serial-status {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.75rem 1rem;
  background: #fff3cd;
  border: 2px solid #ffc107;
  border-radius: 8px;
  margin-bottom: 2rem;
  font-weight: 500;
}

.serial-status.connected {
  background: #d4edda;
  border-color: #28a745;
}

.status-icon {
  font-size: 1.5rem;
  color: #ffc107;
}

.serial-status.connected .status-icon {
  color: #28a745;
}

.btn-refresh, .btn-reconnect {
  margin-left: auto;
  padding: 0.25rem 0.75rem;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  background: #007bff;
  color: white;
  font-size: 0.9rem;
}

.btn-refresh:hover, .btn-reconnect:hover {
  background: #0056b3;
}

.editors-container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 2rem;
  margin-bottom: 2rem;
}

.goal-config {
  background: white;
  padding: 1.5rem;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  margin-bottom: 2rem;
}

.goal-config h3 {
  margin-top: 0;
  color: #333;
}

.goal-inputs {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 1rem;
}

.input-group {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.input-group label {
  font-weight: 500;
  color: #555;
}

.input-group select,
.input-group input {
  padding: 0.5rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 1rem;
}

.send-section {
  text-align: center;
  margin-bottom: 1rem;
}

.btn-send {
  padding: 1rem 2rem;
  background: #28a745;
  color: white;
  border: none;
  border-radius: 8px;
  font-size: 1.1rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s;
}

.btn-send:hover:not(:disabled) {
  background: #218838;
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(40, 167, 69, 0.3);
}

.btn-send:disabled {
  background: #6c757d;
  cursor: not-allowed;
}

.response-message {
  padding: 1rem;
  border-radius: 8px;
  text-align: center;
  font-weight: 500;
  animation: slideDown 0.3s ease;
}

@keyframes slideDown {
  from {
    opacity: 0;
    transform: translateY(-10px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.response-message.success {
  background: #d4edda;
  color: #155724;
  border: 1px solid #c3e6cb;
}

.response-message.error {
  background: #f8d7da;
  color: #721c24;
  border: 1px solid #f5c6cb;
}

.response-message.warning {
  background: #fff3cd;
  color: #856404;
  border: 1px solid #ffeeba;
}

.response-message.info {
  background: #d1ecf1;
  color: #0c5460;
  border: 1px solid #bee5eb;
}
</style>