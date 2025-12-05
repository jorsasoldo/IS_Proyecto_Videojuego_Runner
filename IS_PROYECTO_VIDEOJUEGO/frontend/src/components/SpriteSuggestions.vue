<template>
  <transition name="slide-down">
    <div v-if="suggestions.length > 0" class="suggestions-panel">
      <div class="panel-header">
        <div class="header-content">
          <span class="warning-icon">⚠️</span>
          <div>
            <h3>Sprites muy similares</h3>
            <p class="subtitle">{{ difference }}% de diferencia - Se requiere al menos 20%</p>
          </div>
        </div>
        <button @click="$emit('close')" class="btn-close" title="Cerrar">×</button>
      </div>

      <div class="suggestions-content">
        <div class="intro-text">
          <strong>💡 Sugerencias para mejorar la diferenciación:</strong>
        </div>

        <div 
          v-for="(suggestion, index) in sortedSuggestions" 
          :key="index"
          class="suggestion-card"
          :class="`priority-${suggestion.priority}`"
        >
          <div class="card-header">
            <span class="suggestion-icon">{{ suggestion.icon }}</span>
            <div class="card-title">
              <strong>{{ suggestion.title }}</strong>
              <span class="priority-badge" :class="`badge-${suggestion.priority}`">
                {{ priorityLabel(suggestion.priority) }}
              </span>
            </div>
          </div>

          <p class="suggestion-description">{{ suggestion.description }}</p>

          <div class="actions-list">
            <div 
              v-for="(action, actionIndex) in suggestion.actions" 
              :key="actionIndex"
              class="action-item"
            >
              <span class="action-bullet">•</span>
              <span>{{ action }}</span>
            </div>
          </div>
        </div>

        <div class="help-footer">
          <div class="help-icon">ℹ️</div>
          <p>
            <strong>Consejo:</strong> Los sprites diferentes hacen que el juego sea más fácil de jugar.
            Incluso pequeños cambios pueden marcar una gran diferencia.
          </p>
        </div>
      </div>
    </div>
  </transition>
</template>

<script>
export default {
  name: 'SpriteSuggestions',
  props: {
    suggestions: {
      type: Array,
      default: () => []
    },
    difference: {
      type: Number,
      default: 0
    }
  },
  emits: ['close'],
  computed: {
    sortedSuggestions() {
      const priorityOrder = { high: 1, medium: 2, low: 3 }
      return [...this.suggestions].sort((a, b) => {
        return priorityOrder[a.priority] - priorityOrder[b.priority]
      })
    }
  },
  methods: {
    priorityLabel(priority) {
      const labels = {
        high: 'Importante',
        medium: 'Recomendado',
        low: 'Opcional'
      }
      return labels[priority] || priority
    }
  }
}
</script>

<style scoped>
.slide-down-enter-active,
.slide-down-leave-active {
  transition: all 0.3s ease;
}

.slide-down-enter-from {
  opacity: 0;
  transform: translateY(-20px);
}

.slide-down-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}

.suggestions-panel {
  background: linear-gradient(135deg, #fff8e1 0%, #ffe0b2 100%);
  border: 3px solid #ff9800;
  border-radius: 12px;
  margin-bottom: 2rem;
  overflow: hidden;
  box-shadow: 0 4px 20px rgba(255, 152, 0, 0.2);
}

.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 1.25rem 1.5rem;
  background: linear-gradient(135deg, #ff9800 0%, #f57c00 100%);
  color: white;
}

.header-content {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.warning-icon {
  font-size: 2rem;
  animation: pulse 2s ease infinite;
}

@keyframes pulse {
  0%, 100% {
    transform: scale(1);
  }
  50% {
    transform: scale(1.1);
  }
}

.panel-header h3 {
  margin: 0;
  font-size: 1.3rem;
  font-weight: 700;
}

.subtitle {
  margin: 0.25rem 0 0;
  font-size: 0.9rem;
  opacity: 0.95;
}

.btn-close {
  background: rgba(255, 255, 255, 0.2);
  border: none;
  color: white;
  font-size: 2rem;
  width: 36px;
  height: 36px;
  border-radius: 50%;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
  line-height: 1;
  padding: 0;
}

.btn-close:hover {
  background: rgba(255, 255, 255, 0.3);
  transform: rotate(90deg);
}

.suggestions-content {
  padding: 1.5rem;
}

.intro-text {
  margin-bottom: 1.25rem;
  padding: 0.75rem;
  background: rgba(255, 152, 0, 0.1);
  border-left: 4px solid #ff9800;
  border-radius: 4px;
  color: #e65100;
}

.suggestion-card {
  background: white;
  border-radius: 8px;
  padding: 1.25rem;
  margin-bottom: 1rem;
  border-left: 4px solid #ff9800;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
  transition: all 0.2s;
}

.suggestion-card:hover {
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.12);
  transform: translateX(4px);
}

.suggestion-card.priority-high {
  border-left-color: #f44336;
  background: linear-gradient(to right, #ffebee 0%, white 10%);
}

.suggestion-card.priority-medium {
  border-left-color: #ff9800;
  background: linear-gradient(to right, #fff8e1 0%, white 10%);
}

.suggestion-card.priority-low {
  border-left-color: #2196f3;
  background: linear-gradient(to right, #e3f2fd 0%, white 10%);
}

.card-header {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
  margin-bottom: 0.75rem;
}

.suggestion-icon {
  font-size: 1.75rem;
  flex-shrink: 0;
}

.card-title {
  flex: 1;
  display: flex;
  align-items: center;
  gap: 0.75rem;
  flex-wrap: wrap;
}

.card-title strong {
  color: #333;
  font-size: 1.05rem;
}

.priority-badge {
  display: inline-block;
  padding: 0.2rem 0.6rem;
  border-radius: 12px;
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.badge-high {
  background: #ffcdd2;
  color: #c62828;
}

.badge-medium {
  background: #ffe0b2;
  color: #e65100;
}

.badge-low {
  background: #bbdefb;
  color: #1565c0;
}

.suggestion-description {
  color: #555;
  margin: 0 0 1rem 2.5rem;
  font-size: 0.95rem;
  line-height: 1.5;
}

.actions-list {
  margin-left: 2.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.action-item {
  display: flex;
  align-items: flex-start;
  gap: 0.5rem;
  color: #444;
  font-size: 0.9rem;
  line-height: 1.6;
  padding: 0.4rem 0.6rem;
  background: rgba(0, 0, 0, 0.02);
  border-radius: 4px;
  transition: background 0.2s;
}

.action-item:hover {
  background: rgba(255, 152, 0, 0.08);
}

.action-bullet {
  color: #ff9800;
  font-weight: bold;
  font-size: 1.2rem;
  line-height: 1.3;
}

.help-footer {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
  margin-top: 1.5rem;
  padding: 1rem;
  background: #e3f2fd;
  border-radius: 8px;
  border: 1px solid #90caf9;
}

.help-icon {
  font-size: 1.5rem;
  flex-shrink: 0;
}

.help-footer p {
  margin: 0;
  color: #1565c0;
  font-size: 0.9rem;
  line-height: 1.5;
}

.help-footer strong {
  color: #0d47a1;
}

@media (max-width: 768px) {
  .panel-header {
    padding: 1rem;
  }

  .panel-header h3 {
    font-size: 1.1rem;
  }

  .suggestion-card {
    padding: 1rem;
  }

  .suggestion-description,
  .actions-list {
    margin-left: 0;
  }
}
</style>