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

    <!-- Validación de similitud -->
    <div v-if="similarityWarning" class="warning-box">
      <strong>⚠ Advertencia:</strong> {{ similarityWarning }}
      <p class="suggestion">{{ similaritySuggestion }}</p>
    </div>

    <!-- Configuración del objetivo -->
    <GoalConfig
      v-model:goal-type="config.goalType"
      v-model:goal-value="config.goalValue"
    />

    <!-- Botón de envío -->
    <div class="send-section">
      <button 
        @click="sendConfiguration" 
        :disabled="isSending || !isConfigValid"
        class="btn-send"
        :class="{ disabled: !isConfigValid }"
      >
        {{ sendButtonText }}
      </button>
      <small v-if="!isConfigValid" class="validation-hint">
        Completa la configuración para enviar
      </small>
    </div>

    <!-- Mensajes de respuesta -->
    <div v-if="responseMessage" class="response-message" :class="responseType">
      <div class="message-icon">{{ messageIcon }}</div>
      <div class="message-content">
        <strong>{{ messageTitle }}</strong>
        <p>{{ responseMessage }}</p>
      </div>
    </div>
  </div>
</template>

<script>
import SpriteEditor from './SpriteEditor.vue'
import GoalConfig from './GoalConfig.vue'
import { validateSpriteDifference, isSpriteEmpty } from '../utils/spriteValidation.js'

export default {
  name: 'GameConfig',
  components: {
    SpriteEditor,
    GoalConfig
  },
  data() {
    return {
      config: {
        character: [0, 0, 0, 0, 0, 0, 0, 0],
        obstacle: [0, 0, 0, 0, 0, 0, 0, 0],
        goalType: 'obstacles',
        goalValue: 10
      },
      serialConnected: false,
      isSending: false,
      responseMessage: '',
      responseType: 'info',
      similarityWarning: '',
      similaritySuggestion: ''
    }
  },
  computed: {
    isConfigValid() {
      return !isSpriteEmpty(this.config.character) && 
             !isSpriteEmpty(this.config.obstacle) &&
             this.config.goalValue > 0
    },
    sendButtonText() {
      if (this.isSending) return 'Enviando...'
      if (!this.isConfigValid) return 'Configuración Incompleta'
      return 'Enviar Configuración al PIC'
    },
    messageIcon() {
      const icons = {
        success: '✓',
        error: '✗',
        warning: '⚠',
        info: 'ℹ'
      }
      return icons[this.responseType] || 'ℹ'
    },
    messageTitle() {
      const titles = {
        success: 'Éxito',
        error: 'Error',
        warning: 'Advertencia',
        info: 'Información'
      }
      return titles[this.responseType] || 'Mensaje'
    }
  },
  watch: {
    'config.character': {
      handler() {
        this.checkSpriteSimilarity()
      },
      deep: true
    },
    'config.obstacle': {
      handler() {
        this.checkSpriteSimilarity()
      },
      deep: true
    }
  },
  mounted() {
    this.checkSerialStatus()
  },
  methods: {
    checkSpriteSimilarity() {
      // No validar si alguno está vacío
      if (isSpriteEmpty(this.config.character) || isSpriteEmpty(this.config.obstacle)) {
        this.similarityWarning = ''
        this.similaritySuggestion = ''
        return
      }

      const validation = validateSpriteDifference(this.config.character, this.config.obstacle)
      
      if (!validation.valid) {
        this.similarityWarning = validation.message
        this.similaritySuggestion = 'Intenta cambiar la forma del obstáculo o personaje para que sean más distinguibles visualmente.'
      } else {
        this.similarityWarning = ''
        this.similaritySuggestion = ''
      }
    },

    async checkSerialStatus() {
      try {
        const response = await fetch('http://localhost:5000/api/health')
        const data = await response.json()
        this.serialConnected = data.status === 'ok'
      } catch (error) {
        console.error('Error checking serial status:', error)
        this.serialConnected = false
      }
    },

    async reconnectSerial() {
      try {
        this.showMessage('Intentando reconectar con el dispositivo...', 'info')
        // Lógica de reconexión
        await this.checkSerialStatus()
        
        if (this.serialConnected) {
          this.showMessage('Conexión restablecida correctamente', 'success')
        } else {
          this.showMessage('No se pudo conectar. Verifica que el cable USB esté conectado y el dispositivo esté encendido.', 'error')
        }
      } catch (error) {
        console.error('Error reconnecting:', error)
        this.showMessage('Error al intentar reconectar. Por favor, verifica la conexión física.', 'error')
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
          this.showMessage('Configuración cargada correctamente en el dispositivo', 'success')
          console.log('Respuesta del PIC:', data)
        } else {
          // Mensajes de error según el tipo
          this.handleErrorResponse(data, response.status)
        }
      } catch (error) {
        console.error('Error:', error)
        this.showMessage('No se pudo conectar con el servidor. Verifica que el backend esté ejecutándose en http://localhost:5000', 'error')
      } finally {
        this.isSending = false
      }
    },

    handleErrorResponse(data, status) {
      if (status === 400) {
        // Error de validación
        this.showMessage(`Datos inválidos: ${data.error || data.message}`, 'error')
      } else if (status === 500) {
        // Error del servidor o PIC
        if (data.error && data.error.includes('serial')) {
          this.showMessage('El dispositivo no respondió. Verifica que el PIC esté conectado y encendido.', 'error')
        } else if (data.error && data.error.includes('timeout')) {
          this.showMessage('El dispositivo tardó demasiado en responder. Intenta enviar la configuración nuevamente.', 'error')
        } else {
          this.showMessage(`Error del dispositivo: ${data.error || data.message}`, 'error')
        }
      } else {
        this.showMessage('Error desconocido. Por favor intenta nuevamente.', 'error')
      }
    },

    validateConfig() {
      // Validar personaje vacío
      if (isSpriteEmpty(this.config.character)) {
        this.showMessage('El personaje está vacío. Dibuja al menos 1 píxel en el editor de personaje.', 'warning')
        return false
      }

      // Validar obstáculo vacío
      if (isSpriteEmpty(this.config.obstacle)) {
        this.showMessage('El obstáculo está vacío. Dibuja al menos 1 píxel en el editor de obstáculo.', 'warning')
        return false
      }

      // Validar similitud
      const validation = validateSpriteDifference(this.config.character, this.config.obstacle)
      if (!validation.valid) {
        this.showMessage(`Los sprites son muy similares (${validation.difference}% de diferencia). Se requiere al menos 20% de diferencia.`, 'warning')
        return false
      }

      // Validar valor de meta
      if (this.config.goalValue <= 0) {
        this.showMessage('El valor del objetivo debe ser mayor a 0', 'warning')
        return false
      }

      if (this.config.goalType === 'obstacles' && (this.config.goalValue < 1 || this.config.goalValue > 99)) {
        this.showMessage('La cantidad de obstáculos debe estar entre 1 y 99', 'warning')
        return false
      }

      if (this.config.goalType === 'time' && (this.config.goalValue < 1 || this.config.goalValue > 255)) {
        this.showMessage('El tiempo debe estar entre 1 y 255 segundos', 'warning')
        return false
      }

      return true
    },

    showMessage(message, type = 'info') {
      this.responseMessage = message
      this.responseType = type

      // Auto-ocultar después de 8 segundos (excepto errores)
      if (type !== 'error') {
        setTimeout(() => {
          this.responseMessage = ''
        }, 8000)
      }
    }
  }
}
</script>

<style scoped>
.game-config {
  max-width: 1200px;
  margin: 0 auto;
  padding: 2rem;
}

h1 {
  text-align: center;
  color: white;
  margin-bottom: 2rem;
  text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
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
  transition: background 0.2s;
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

.warning-box {
  background: #fff3cd;
  border: 2px solid #ffc107;
  border-radius: 8px;
  padding: 1rem;
  margin-bottom: 2rem;
  color: #856404;
}

.warning-box strong {
  display: block;
  margin-bottom: 0.5rem;
  font-size: 1.05rem;
}

.suggestion {
  margin-top: 0.5rem;
  font-style: italic;
  color: #6c5400;
}

.send-section {
  text-align: center;
  margin: 2rem 0;
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
  transform: none;
}

.btn-send.disabled {
  background: #dc3545;
}

.validation-hint {
  display: block;
  margin-top: 0.5rem;
  color: #dc3545;
  font-weight: 500;
}

.response-message {
  display: flex;
  align-items: flex-start;
  gap: 1rem;
  padding: 1.25rem;
  border-radius: 8px;
  margin-top: 1.5rem;
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

.message-icon {
  font-size: 2rem;
  font-weight: bold;
  flex-shrink: 0;
}

.message-content {
  flex: 1;
}

.message-content strong {
  display: block;
  margin-bottom: 0.25rem;
  font-size: 1.05rem;
}

.message-content p {
  margin: 0;
}

.response-message.success {
  background: #d4edda;
  color: #155724;
  border: 2px solid #c3e6cb;
}

.response-message.success .message-icon {
  color: #28a745;
}

.response-message.error {
  background: #f8d7da;
  color: #721c24;
  border: 2px solid #f5c6cb;
}

.response-message.error .message-icon {
  color: #dc3545;
}

.response-message.warning {
  background: #fff3cd;
  color: #856404;
  border: 2px solid #ffeeba;
}

.response-message.warning .message-icon {
  color: #ffc107;
}

.response-message.info {
  background: #d1ecf1;
  color: #0c5460;
  border: 2px solid #bee5eb;
}

.response-message.info .message-icon {
  color: #17a2b8;
}
</style>