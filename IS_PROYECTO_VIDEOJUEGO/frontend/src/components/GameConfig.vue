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
      <button @click="toggleLogs" class="btn-logs" :class="{ active: showLogs }">
        📋 Logs
      </button>
    </div>

    <!-- Panel de logs -->
    <transition name="slide">
      <div v-if="showLogs" class="logs-panel">
        <div class="logs-header">
          <h3>📊 Registro de Comunicación Serial</h3>
          <button @click="refreshLogs" class="btn-refresh-logs">Actualizar</button>
        </div>
        <div class="logs-content" ref="logsContent">
          <pre v-if="logs">{{ logs }}</pre>
          <p v-else class="no-logs">No hay logs disponibles</p>
        </div>
      </div>
    </transition>

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

    <!-- Panel de sugerencias inteligentes -->
    <SpriteSuggestions
      v-if="validationResult && !validationResult.valid"
      :suggestions="validationResult.suggestions"
      :difference="validationResult.difference"
      @close="dismissSuggestions"
    />

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
      <div v-if="lastRequestId" class="request-id">
        ID de solicitud: {{ lastRequestId }}
      </div>
    </div>

    <!-- Mensajes de respuesta -->
    <div v-if="responseMessage" class="response-message" :class="responseType">
      <div class="message-icon">{{ messageIcon }}</div>
      <div class="message-content">
        <strong>{{ messageTitle }}</strong>
        <p>{{ responseMessage }}</p>
        <small v-if="retryInfo" class="retry-info">{{ retryInfo }}</small>
      </div>
    </div>

    <!-- Loading overlay -->
    <LoadingSpinner 
      :show="isSending"
      message="Enviando configuración al PIC"
      :steps="loadingSteps"
      :current-step="currentLoadingStep"
    />
  </div>
</template>

<script>
import SpriteEditor from './SpriteEditor.vue'
import GoalConfig from './GoalConfig.vue'
import LoadingSpinner from './LoadingSpinner.vue'
import SpriteSuggestions from './SpriteSuggestions.vue'
import { validateSpriteDifference, isSpriteEmpty } from '../utils/spriteValidation.js'

export default {
  name: 'GameConfig',
  components: {
    SpriteEditor,
    GoalConfig,
    LoadingSpinner,
    SpriteSuggestions
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
      validationResult: null,
      loadingSteps: [
        'Validando configuración',
        'Preparando datos',
        'Enviando al dispositivo',
        'Esperando confirmación'
      ],
      currentLoadingStep: 0,
      lastRequestId: null,
      retryInfo: null,
      showLogs: false,
      logs: ''
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
    // Actualizar logs automáticamente cada 10 segundos si están visibles
    this.logsInterval = setInterval(() => {
      if (this.showLogs) {
        this.refreshLogs()
      }
    }, 10000)
  },
  beforeUnmount() {
    if (this.logsInterval) {
      clearInterval(this.logsInterval)
    }
  },
  methods: {
    checkSpriteSimilarity() {
      // No validar si alguno está vacío
      if (isSpriteEmpty(this.config.character) || isSpriteEmpty(this.config.obstacle)) {
        this.validationResult = null
        return
      }

      this.validationResult = validateSpriteDifference(this.config.character, this.config.obstacle)
    },

    dismissSuggestions() {
      // Opcionalmente ocultar sugerencias temporalmente
      this.validationResult = null
    },

    async checkSerialStatus() {
      try {
        const response = await fetch('http://localhost:5000/api/health')
        const data = await response.json()
        this.serialConnected = data.serial_connected || false
      } catch (error) {
        console.error('Error checking serial status:', error)
        this.serialConnected = false
      }
    },

    async reconnectSerial() {
      try {
        this.showMessage('Intentando reconectar con el dispositivo...', 'info')
        const response = await fetch('http://localhost:5000/api/serial/reconnect', {
          method: 'POST'
        })
        const data = await response.json()
        
        if (data.success) {
          this.serialConnected = true
          this.showMessage('Conexión restablecida correctamente', 'success')
        } else {
          this.serialConnected = false
          this.showMessage('No se pudo conectar. Verifica que el cable USB esté conectado y el dispositivo esté encendido.', 'error')
        }
      } catch (error) {
        console.error('Error reconnecting:', error)
        this.showMessage('Error al intentar reconectar. Por favor, verifica la conexión física.', 'error')
      }
    },

    async toggleLogs() {
      this.showLogs = !this.showLogs
      if (this.showLogs) {
        await this.refreshLogs()
      }
    },

    async refreshLogs() {
      try {
        const response = await fetch('http://localhost:5000/api/logs/recent')
        const data = await response.json()
        this.logs = data.logs
        
        // Auto-scroll al final
        this.$nextTick(() => {
          if (this.$refs.logsContent) {
            this.$refs.logsContent.scrollTop = this.$refs.logsContent.scrollHeight
          }
        })
      } catch (error) {
        console.error('Error fetching logs:', error)
        this.logs = 'Error al cargar logs'
      }
    },

    async sendConfiguration() {
      if (!this.validateConfig()) {
        return
      }

      this.isSending = true
      this.responseMessage = ''
      this.retryInfo = null
      this.lastRequestId = null
      this.currentLoadingStep = 0

      try {
        // Paso 1: Validando
        this.currentLoadingStep = 0
        await this.delay(300)

        // Paso 2: Preparando datos
        this.currentLoadingStep = 1
        await this.delay(300)

        // Paso 3: Enviando
        this.currentLoadingStep = 2
        
        const response = await fetch('http://localhost:5000/api/send_config', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify(this.config)
        })

        // Paso 4: Esperando confirmación
        this.currentLoadingStep = 3
        const data = await response.json()
        await this.delay(300)

        if (response.ok) {
          this.lastRequestId = data.request_id
          this.showMessage('Configuración cargada correctamente en el dispositivo', 'success')
          console.log('Respuesta del PIC:', data)
          
          // Actualizar logs si están visibles
          if (this.showLogs) {
            await this.refreshLogs()
          }
        } else {
          this.handleErrorResponse(data, response.status)
        }
      } catch (error) {
        console.error('Error:', error)
        this.showMessage('No se pudo conectar con el servidor. Verifica que el backend esté ejecutándose en http://localhost:5000', 'error')
      } finally {
        this.isSending = false
        this.currentLoadingStep = 0
      }
    },

    delay(ms) {
      return new Promise(resolve => setTimeout(resolve, ms))
    },

    handleErrorResponse(data, status) {
      this.lastRequestId = data.request_id
      
      if (data.retry_info) {
        this.retryInfo = data.retry_info
      }
      
      if (status === 400) {
        this.showMessage(`Datos inválidos: ${data.error || data.message}`, 'error')
      } else if (status === 500) {
        if (data.error && data.error.includes('serial')) {
          this.showMessage('El dispositivo no respondió. Verifica que el PIC esté conectado y encendido.', 'error')
        } else if (data.error && data.error.includes('timeout')) {
          this.showMessage('El dispositivo tardó demasiado en responder. Se realizaron varios intentos automáticamente.', 'error')
        } else if (data.error && data.error.includes('intentos')) {
          this.showMessage(`Error de comunicación: ${data.error}`, 'error')
        } else {
          this.showMessage(`Error del dispositivo: ${data.error || data.message}`, 'error')
        }
      } else {
        this.showMessage('Error desconocido. Por favor intenta nuevamente.', 'error')
      }
    },

    validateConfig() {
      if (isSpriteEmpty(this.config.character)) {
        this.showMessage('El personaje está vacío. Dibuja al menos 1 píxel en el editor de personaje.', 'warning')
        return false
      }

      if (isSpriteEmpty(this.config.obstacle)) {
        this.showMessage('El obstáculo está vacío. Dibuja al menos 1 píxel en el editor de obstáculo.', 'warning')
        return false
      }

      const validation = validateSpriteDifference(this.config.character, this.config.obstacle)
      if (!validation.valid) {
        this.showMessage(`Los sprites son muy similares (${validation.difference}% de diferencia). Se requiere al menos 20% de diferencia.`, 'warning')
        return false
      }

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

      if (type !== 'error') {
        setTimeout(() => {
          this.responseMessage = ''
          this.retryInfo = null
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

.btn-refresh, .btn-reconnect, .btn-logs {
  padding: 0.25rem 0.75rem;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  background: #007bff;
  color: white;
  font-size: 0.9rem;
  transition: all 0.2s;
}

.btn-logs {
  margin-left: auto;
}

.btn-logs.active {
  background: #0056b3;
  font-weight: 600;
}

.btn-refresh:hover, .btn-reconnect:hover, .btn-logs:hover {
  background: #0056b3;
}

.slide-enter-active, .slide-leave-active {
  transition: all 0.3s ease;
}

.slide-enter-from {
  opacity: 0;
  max-height: 0;
}

.slide-leave-to {
  opacity: 0;
  max-height: 0;
}

.logs-panel {
  background: #1e1e1e;
  border: 2px solid #007bff;
  border-radius: 8px;
  margin-bottom: 2rem;
  overflow: hidden;
}

.logs-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem;
  background: #007bff;
  color: white;
}

.logs-header h3 {
  margin: 0;
  font-size: 1.1rem;
}

.btn-refresh-logs {
  padding: 0.4rem 0.8rem;
  background: rgba(255, 255, 255, 0.2);
  border: 1px solid rgba(255, 255, 255, 0.3);
  border-radius: 4px;
  color: white;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-refresh-logs:hover {
  background: rgba(255, 255, 255, 0.3);
}

.logs-content {
  max-height: 300px;
  overflow-y: auto;
  padding: 1rem;
  background: #1e1e1e;
}

.logs-content pre {
  margin: 0;
  color: #d4d4d4;
  font-family: 'Courier New', monospace;
  font-size: 0.85rem;
  line-height: 1.5;
  white-space: pre-wrap;
  word-wrap: break-word;
}

.no-logs {
  color: #999;
  text-align: center;
  margin: 2rem 0;
}

.editors-container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 2rem;
  margin-bottom: 2rem;
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

.request-id {
  margin-top: 0.5rem;
  font-size: 0.85rem;
  color: rgba(255, 255, 255, 0.7);
  font-family: monospace;
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

.retry-info {
  display: block;
  margin-top: 0.5rem;
  font-style: italic;
  opacity: 0.9;
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