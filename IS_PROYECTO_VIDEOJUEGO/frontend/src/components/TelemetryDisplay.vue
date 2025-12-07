<template>
  <div v-if="show" class="telemetry-overlay" @click="closeModal">
    <div class="telemetry-modal" @click.stop>
      <!-- Header con resultado -->
      <div class="result-header" :class="resultClass">
        <div class="result-icon">{{ resultIcon }}</div>
        <h2>{{ resultTitle }}</h2>
      </div>

      <!-- Estadísticas -->
      <div class="stats-container">
        <div class="stat-card">
          <div class="stat-icon">🚧</div>
          <div class="stat-value">{{ telemetry.obstaclesAvoided }}</div>
          <div class="stat-label">Obstáculos Esquivados</div>
        </div>

        <div class="stat-card">
          <div class="stat-icon">⏱️</div>
          <div class="stat-value">{{ formattedTime }}</div>
          <div class="stat-label">Tiempo Sobrevivido</div>
        </div>

        <div class="stat-card" v-if="telemetry.goalType">
          <div class="stat-icon">🎯</div>
          <div class="stat-value">{{ goalProgress }}</div>
          <div class="stat-label">Progreso del Objetivo</div>
        </div>
      </div>

      <!-- Mensaje adicional -->
      <div class="result-message" v-if="resultMessage">
        {{ resultMessage }}
      </div>

      <!-- Botones -->
      <div class="action-buttons">
        <button @click="playAgain" class="btn btn-primary">
          🔄 Jugar de Nuevo
        </button>
        <button @click="closeModal" class="btn btn-secondary">
          ✖ Cerrar
        </button>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'TelemetryDisplay',
  props: {
    show: {
      type: Boolean,
      default: false
    },
    telemetry: {
      type: Object,
      default: () => ({
        obstaclesAvoided: 0,
        survivalTime: 0,
        result: 'victory', // 'victory' | 'defeat'
        goalType: null, // 'obstacles' | 'time'
        goalValue: 0
      })
    }
  },
  emits: ['close', 'play-again'],
  computed: {
    resultClass() {
      return this.telemetry.result === 'victory' ? 'victory' : 'defeat'
    },
    resultIcon() {
      return this.telemetry.result === 'victory' ? '🏆' : '💀'
    },
    resultTitle() {
      return this.telemetry.result === 'victory' ? '¡Victoria!' : 'Game Over'
    },
    formattedTime() {
      const seconds = this.telemetry.survivalTime || 0
      const mins = Math.floor(seconds / 60)
      const secs = seconds % 60
      return mins > 0 ? `${mins}m ${secs}s` : `${secs}s`
    },
    goalProgress() {
      if (!this.telemetry.goalType) return 'N/A'
      
      if (this.telemetry.goalType === 'obstacles') {
        return `${this.telemetry.obstaclesAvoided} / ${this.telemetry.goalValue}`
      } else if (this.telemetry.goalType === 'time') {
        return `${this.telemetry.survivalTime}s / ${this.telemetry.goalValue}s`
      }
      return 'N/A'
    },
    resultMessage() {
      if (this.telemetry.result === 'victory') {
        return '¡Felicidades! Has completado el nivel exitosamente.'
      } else {
        const reason = this.getDefeatReason()
        return `${reason} ¡Intenta de nuevo!`
      }
    }
  },
  methods: {
    getDefeatReason() {
      if (this.telemetry.goalType === 'obstacles') {
        const remaining = this.telemetry.goalValue - this.telemetry.obstaclesAvoided
        return `Te faltaron ${remaining} obstáculo${remaining !== 1 ? 's' : ''}.`
      } else if (this.telemetry.goalType === 'time') {
        const remaining = this.telemetry.goalValue - this.telemetry.survivalTime
        return `Te faltaron ${remaining} segundo${remaining !== 1 ? 's' : ''}.`
      }
      return 'Chocaste con un obstáculo.'
    },
    closeModal() {
      this.$emit('close')
    },
    playAgain() {
      this.$emit('play-again')
    }
  }
}
</script>

<style scoped>
.telemetry-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.85);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 10000;
  animation: fadeIn 0.3s ease;
}

@keyframes fadeIn {
  from {
    opacity: 0;
  }
  to {
    opacity: 1;
  }
}

.telemetry-modal {
  background: white;
  border-radius: 16px;
  padding: 2rem;
  max-width: 600px;
  width: 90%;
  box-shadow: 0 16px 48px rgba(0, 0, 0, 0.4);
  animation: slideUp 0.4s ease;
}

@keyframes slideUp {
  from {
    opacity: 0;
    transform: translateY(50px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.result-header {
  text-align: center;
  padding: 2rem 1rem;
  border-radius: 12px;
  margin-bottom: 2rem;
  animation: bounceIn 0.6s ease;
}

@keyframes bounceIn {
  0% {
    transform: scale(0.3);
    opacity: 0;
  }
  50% {
    transform: scale(1.05);
  }
  70% {
    transform: scale(0.9);
  }
  100% {
    transform: scale(1);
    opacity: 1;
  }
}

.result-header.victory {
  background: linear-gradient(135deg, #ffd700 0%, #ffed4e 100%);
  border: 3px solid #f9a825;
}

.result-header.defeat {
  background: linear-gradient(135deg, #e53935 0%, #ef5350 100%);
  border: 3px solid #c62828;
}

.result-icon {
  font-size: 4rem;
  margin-bottom: 0.5rem;
  animation: rotate 0.8s ease;
}

@keyframes rotate {
  0% {
    transform: rotate(0deg) scale(0);
  }
  50% {
    transform: rotate(180deg) scale(1.2);
  }
  100% {
    transform: rotate(360deg) scale(1);
  }
}

.result-header h2 {
  margin: 0;
  font-size: 2.5rem;
  color: white;
  text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
}

.stats-container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 1rem;
  margin-bottom: 2rem;
}

.stat-card {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  padding: 1.5rem;
  border-radius: 12px;
  text-align: center;
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.3);
  animation: fadeInUp 0.5s ease forwards;
  opacity: 0;
}

.stat-card:nth-child(1) {
  animation-delay: 0.2s;
}

.stat-card:nth-child(2) {
  animation-delay: 0.4s;
}

.stat-card:nth-child(3) {
  animation-delay: 0.6s;
}

@keyframes fadeInUp {
  from {
    opacity: 0;
    transform: translateY(20px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.stat-icon {
  font-size: 2rem;
  margin-bottom: 0.5rem;
}

.stat-value {
  font-size: 2rem;
  font-weight: bold;
  margin-bottom: 0.25rem;
}

.stat-label {
  font-size: 0.9rem;
  opacity: 0.9;
}

.result-message {
  text-align: center;
  font-size: 1.1rem;
  color: #555;
  margin-bottom: 2rem;
  padding: 1rem;
  background: #f5f5f5;
  border-radius: 8px;
  line-height: 1.5;
}

.action-buttons {
  display: flex;
  gap: 1rem;
  justify-content: center;
}

.btn {
  padding: 0.75rem 1.5rem;
  border: none;
  border-radius: 8px;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s;
}

.btn-primary {
  background: #4caf50;
  color: white;
}

.btn-primary:hover {
  background: #45a049;
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(76, 175, 80, 0.4);
}

.btn-secondary {
  background: #757575;
  color: white;
}

.btn-secondary:hover {
  background: #616161;
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(97, 97, 97, 0.4);
}
</style>