<template>
  <div v-if="show" class="telemetry-overlay" @click="closeModal">
    <div class="telemetry-modal" @click.stop :class="{ 'modal-victory': isVictory, 'modal-defeat': !isVictory }">
      
      <!-- Header con resultado -->
      <div class="result-header" :class="resultClass">
        <div class="result-icon-container">
          <div class="result-icon">{{ resultIcon }}</div>
          <div v-if="isVictory" class="shine-effect"></div>
        </div>
        <h2>{{ resultTitle }}</h2>
        <p class="result-subtitle">{{ resultSubtitle }}</p>
      </div>

      <!-- Estadísticas con animación stagger -->
      <div class="stats-container">
        <div class="stat-card" :style="{ animationDelay: '0.2s' }">
          <div class="stat-icon-bounce">🚧</div>
          <div class="stat-value" :class="{ 'pulse-victory': isVictory }">
            {{ telemetry.obstaclesAvoided }}
          </div>
          <div class="stat-label">Obstáculos Esquivados</div>
          <div class="stat-progress" v-if="telemetry.goalType === 'obstacles'">
            <div class="progress-bar" :style="{ width: obstacleProgress + '%' }"></div>
          </div>
        </div>

        <div class="stat-card" :style="{ animationDelay: '0.4s' }">
          <div class="stat-icon-bounce">⏱️</div>
          <div class="stat-value" :class="{ 'pulse-victory': isVictory }">
            {{ formattedTime }}
          </div>
          <div class="stat-label">Tiempo Sobrevivido</div>
          <div class="stat-progress" v-if="telemetry.goalType === 'time'">
            <div class="progress-bar" :style="{ width: timeProgress + '%' }"></div>
          </div>
        </div>

        <div class="stat-card" :style="{ animationDelay: '0.6s' }" v-if="telemetry.goalType">
          <div class="stat-icon-bounce">🎯</div>
          <div class="stat-value" :class="{ 'pulse-victory': isVictory }">
            {{ goalProgress }}
          </div>
          <div class="stat-label">Progreso del Objetivo</div>
        </div>
      </div>

      <!-- Mensaje adicional con animación -->
      <div class="result-message" v-if="resultMessage" :class="{ 'message-victory': isVictory }">
        <span class="message-emoji">{{ isVictory ? '🎉' : '💪' }}</span>
        {{ resultMessage }}
      </div>

      <!-- Botones con hover effects -->
      <div class="action-buttons">
        <button @click="playAgain" class="btn btn-primary">
          <span class="btn-icon">🔄</span>
          Jugar de Nuevo
        </button>
        <button @click="closeModal" class="btn btn-secondary">
          <span class="btn-icon">✖</span>
          Cerrar
        </button>
      </div>
    </div>
  </div>
</template>

<script>
import confetti from 'canvas-confetti'

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
        result: 'victory',
        goalType: null,
        goalValue: 0
      })
    }
  },
  emits: ['close', 'play-again'],
  computed: {
    isVictory() {
      return this.telemetry.result === 'victory'
    },
    resultClass() {
      return this.isVictory ? 'victory' : 'defeat'
    },
    resultIcon() {
      return this.isVictory ? '🏆' : '💀'
    },
    resultTitle() {
      return this.isVictory ? '¡Victoria!' : 'Game Over'
    },
    resultSubtitle() {
      return this.isVictory ? '¡Lo lograste!' : 'Inténtalo de nuevo'
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
    obstacleProgress() {
      if (this.telemetry.goalType !== 'obstacles') return 0
      return Math.min((this.telemetry.obstaclesAvoided / this.telemetry.goalValue) * 100, 100)
    },
    timeProgress() {
      if (this.telemetry.goalType !== 'time') return 0
      return Math.min((this.telemetry.survivalTime / this.telemetry.goalValue) * 100, 100)
    },
    resultMessage() {
      if (this.isVictory) {
        return '¡Felicidades! Has completado el nivel exitosamente.'
      } else {
        const reason = this.getDefeatReason()
        return `${reason} ¡Intenta de nuevo!`
      }
    }
  },
  watch: {
    show(newVal) {
      if (newVal && this.isVictory) {
        this.$nextTick(() => {
          this.launchConfetti()
        })
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
    },
    launchConfetti() {
      const duration = 3000
      const animationEnd = Date.now() + duration
      const defaults = { startVelocity: 30, spread: 360, ticks: 60, zIndex: 10001 }

      function randomInRange(min, max) {
        return Math.random() * (max - min) + min
      }

      const interval = setInterval(function() {
        const timeLeft = animationEnd - Date.now()

        if (timeLeft <= 0) {
          return clearInterval(interval)
        }

        const particleCount = 50 * (timeLeft / duration)
        
        confetti({
          ...defaults,
          particleCount,
          origin: { x: randomInRange(0.1, 0.3), y: Math.random() - 0.2 }
        })
        confetti({
          ...defaults,
          particleCount,
          origin: { x: randomInRange(0.7, 0.9), y: Math.random() - 0.2 }
        })
      }, 250)
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
  backdrop-filter: blur(4px);
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
  border-radius: 20px;
  padding: 2.5rem;
  max-width: 650px;
  width: 90%;
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
  animation: slideUp 0.5s cubic-bezier(0.68, -0.55, 0.265, 1.55);
  position: relative;
  overflow: hidden;
}

.modal-victory::before {
  content: '';
  position: absolute;
  top: -50%;
  left: -50%;
  width: 200%;
  height: 200%;
  background: linear-gradient(45deg, transparent, rgba(255, 215, 0, 0.1), transparent);
  animation: shimmer 3s infinite;
}

@keyframes shimmer {
  0% {
    transform: translateX(-100%) translateY(-100%) rotate(45deg);
  }
  100% {
    transform: translateX(100%) translateY(100%) rotate(45deg);
  }
}

@keyframes slideUp {
  from {
    opacity: 0;
    transform: translateY(100px) scale(0.8);
  }
  to {
    opacity: 1;
    transform: translateY(0) scale(1);
  }
}

.result-header {
  text-align: center;
  padding: 2.5rem 1rem;
  border-radius: 16px;
  margin-bottom: 2rem;
  position: relative;
  overflow: hidden;
}

.result-header.victory {
  background: linear-gradient(135deg, #ffd700 0%, #ffed4e 50%, #ffd700 100%);
  border: 3px solid #f9a825;
  box-shadow: 0 8px 24px rgba(249, 168, 37, 0.4);
}

.result-header.defeat {
  background: linear-gradient(135deg, #e53935 0%, #ef5350 50%, #e53935 100%);
  border: 3px solid #c62828;
  box-shadow: 0 8px 24px rgba(229, 57, 53, 0.4);
}

.result-icon-container {
  position: relative;
  display: inline-block;
  margin-bottom: 1rem;
}

.result-icon {
  font-size: 5rem;
  animation: bounceRotate 1s ease;
  display: inline-block;
  filter: drop-shadow(0 4px 8px rgba(0, 0, 0, 0.3));
}

@keyframes bounceRotate {
  0% {
    transform: rotate(0deg) scale(0);
  }
  50% {
    transform: rotate(180deg) scale(1.3);
  }
  75% {
    transform: rotate(270deg) scale(0.9);
  }
  100% {
    transform: rotate(360deg) scale(1);
  }
}

.shine-effect {
  position: absolute;
  top: 0;
  left: -100%;
  width: 100%;
  height: 100%;
  background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.8), transparent);
  animation: shine 2s infinite;
}

@keyframes shine {
  0% {
    left: -100%;
  }
  50%, 100% {
    left: 100%;
  }
}

.result-header h2 {
  margin: 0;
  font-size: 3rem;
  color: white;
  text-shadow: 3px 3px 6px rgba(0, 0, 0, 0.4);
  animation: titlePop 0.6s cubic-bezier(0.68, -0.55, 0.265, 1.55);
}

@keyframes titlePop {
  0%, 50% {
    transform: scale(0);
  }
  100% {
    transform: scale(1);
  }
}

.result-subtitle {
  margin: 0.5rem 0 0;
  font-size: 1.2rem;
  color: rgba(255, 255, 255, 0.9);
  text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
}

.stats-container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
  gap: 1.5rem;
  margin-bottom: 2rem;
}

.stat-card {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  padding: 1.75rem;
  border-radius: 16px;
  text-align: center;
  box-shadow: 0 6px 16px rgba(102, 126, 234, 0.3);
  animation: fadeInUp 0.6s ease forwards;
  opacity: 0;
  transform: translateY(30px);
  transition: transform 0.3s ease;
}

.stat-card:hover {
  transform: translateY(-5px) scale(1.02);
  box-shadow: 0 8px 20px rgba(102, 126, 234, 0.4);
}

@keyframes fadeInUp {
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.stat-icon-bounce {
  font-size: 2.5rem;
  margin-bottom: 0.75rem;
  animation: iconBounce 1s ease infinite;
}

@keyframes iconBounce {
  0%, 100% {
    transform: translateY(0);
  }
  50% {
    transform: translateY(-10px);
  }
}

.stat-value {
  font-size: 2.5rem;
  font-weight: bold;
  margin-bottom: 0.5rem;
  text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.2);
}

.pulse-victory {
  animation: pulseGlow 1.5s ease infinite;
}

@keyframes pulseGlow {
  0%, 100% {
    transform: scale(1);
    text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.2);
  }
  50% {
    transform: scale(1.1);
    text-shadow: 0 0 20px rgba(255, 255, 255, 0.8);
  }
}

.stat-label {
  font-size: 0.95rem;
  opacity: 0.95;
  margin-bottom: 0.75rem;
}

.stat-progress {
  width: 100%;
  height: 8px;
  background: rgba(255, 255, 255, 0.3);
  border-radius: 4px;
  overflow: hidden;
  margin-top: 0.75rem;
}

.progress-bar {
  height: 100%;
  background: linear-gradient(90deg, #4caf50, #8bc34a);
  border-radius: 4px;
  transition: width 1s ease;
  animation: progressFill 1s ease;
}

@keyframes progressFill {
  from {
    width: 0%;
  }
}

.result-message {
  text-align: center;
  font-size: 1.15rem;
  color: #555;
  margin-bottom: 2rem;
  padding: 1.25rem;
  background: #f5f5f5;
  border-radius: 12px;
  line-height: 1.6;
  animation: messageSlide 0.6s ease;
}

@keyframes messageSlide {
  from {
    opacity: 0;
    transform: translateX(-20px);
  }
  to {
    opacity: 1;
    transform: translateX(0);
  }
}

.message-victory {
  background: linear-gradient(135deg, #fff9c4 0%, #fff59d 100%);
  border: 2px solid #fdd835;
}

.message-emoji {
  font-size: 1.5rem;
  margin-right: 0.5rem;
}

.action-buttons {
  display: flex;
  gap: 1rem;
  justify-content: center;
}

.btn {
  padding: 1rem 2rem;
  border: none;
  border-radius: 12px;
  font-size: 1.05rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
  display: flex;
  align-items: center;
  gap: 0.5rem;
  position: relative;
  overflow: hidden;
}

.btn::before {
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  width: 0;
  height: 0;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.3);
  transform: translate(-50%, -50%);
  transition: width 0.6s, height 0.6s;
}

.btn:hover::before {
  width: 300px;
  height: 300px;
}

.btn-icon {
  font-size: 1.2rem;
  position: relative;
  z-index: 1;
}

.btn span:last-child {
  position: relative;
  z-index: 1;
}

.btn-primary {
  background: linear-gradient(135deg, #4caf50 0%, #66bb6a 100%);
  color: white;
  box-shadow: 0 4px 12px rgba(76, 175, 80, 0.3);
}

.btn-primary:hover {
  background: linear-gradient(135deg, #45a049 0%, #5cb85c 100%);
  transform: translateY(-3px);
  box-shadow: 0 6px 16px rgba(76, 175, 80, 0.4);
}

.btn-secondary {
  background: linear-gradient(135deg, #757575 0%, #9e9e9e 100%);
  color: white;
  box-shadow: 0 4px 12px rgba(117, 117, 117, 0.3);
}

.btn-secondary:hover {
  background: linear-gradient(135deg, #616161 0%, #757575 100%);
  transform: translateY(-3px);
  box-shadow: 0 6px 16px rgba(97, 97, 97, 0.4);
}
</style>