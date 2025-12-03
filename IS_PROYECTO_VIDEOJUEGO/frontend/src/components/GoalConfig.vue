<template>
  <div class="goal-config">
    <h3>Configuración del Objetivo</h3>
    
    <div class="goal-type-section">
      <label class="goal-type-label">
        <input 
          type="radio" 
          value="obstacles" 
          v-model="localGoalType"
          @change="emitUpdate"
        />
        <span class="radio-text">
          <strong>Obstáculos Esquivados</strong>
          <small>Gana al esquivar cierta cantidad de obstáculos</small>
        </span>
      </label>

      <div v-if="localGoalType === 'obstacles'" class="goal-input-group">
        <label>Cantidad de obstáculos:</label>
        <input 
          type="number" 
          v-model.number="localGoalValue"
          @input="validateAndEmit"
          min="1"
          max="99"
          placeholder="Ej: 10"
        />
        <small class="input-hint">Rango válido: 1-99</small>
        <span v-if="obstaclesError" class="error-text">{{ obstaclesError }}</span>
      </div>
    </div>

    <div class="goal-type-section">
      <label class="goal-type-label">
        <input 
          type="radio" 
          value="time" 
          v-model="localGoalType"
          @change="emitUpdate"
        />
        <span class="radio-text">
          <strong>Tiempo de Supervivencia</strong>
          <small>Gana al sobrevivir cierto tiempo</small>
        </span>
      </label>

      <div v-if="localGoalType === 'time'" class="goal-input-group">
        <label>Tiempo en segundos:</label>
        <input 
          type="number" 
          v-model.number="localGoalValue"
          @input="validateAndEmit"
          min="1"
          max="255"
          placeholder="Ej: 60"
        />
        <small class="input-hint">Rango válido: 1-255 segundos</small>
        <span v-if="timeError" class="error-text">{{ timeError }}</span>
      </div>
    </div>

    <div v-if="localGoalType && localGoalValue" class="goal-summary">
      <strong>Meta configurada:</strong> 
      {{ goalSummaryText }}
    </div>
  </div>
</template>

<script>
export default {
  name: 'GoalConfig',
  props: {
    goalType: {
      type: String,
      default: 'obstacles',
      validator: (value) => ['obstacles', 'time'].includes(value)
    },
    goalValue: {
      type: Number,
      default: 10
    }
  },
  emits: ['update:goalType', 'update:goalValue'],
  data() {
    return {
      localGoalType: this.goalType,
      localGoalValue: this.goalValue,
      obstaclesError: '',
      timeError: ''
    }
  },
  computed: {
    goalSummaryText() {
      if (this.localGoalType === 'obstacles') {
        return `Esquivar ${this.localGoalValue} obstáculo${this.localGoalValue !== 1 ? 's' : ''}`
      } else {
        return `Sobrevivir ${this.localGoalValue} segundo${this.localGoalValue !== 1 ? 's' : ''}`
      }
    }
  },
  watch: {
    goalType(newVal) {
      this.localGoalType = newVal
    },
    goalValue(newVal) {
      this.localGoalValue = newVal
    }
  },
  methods: {
    validateAndEmit() {
      this.obstaclesError = ''
      this.timeError = ''

      if (this.localGoalType === 'obstacles') {
        if (this.localGoalValue < 1 || this.localGoalValue > 99) {
          this.obstaclesError = 'El valor debe estar entre 1 y 99'
          return
        }
      } else if (this.localGoalType === 'time') {
        if (this.localGoalValue < 1 || this.localGoalValue > 255) {
          this.timeError = 'El valor debe estar entre 1 y 255'
          return
        }
      }

      this.emitUpdate()
    },

    emitUpdate() {
      this.$emit('update:goalType', this.localGoalType)
      this.$emit('update:goalValue', this.localGoalValue)
    }
  }
}
</script>

<style scoped>
.goal-config {
  background: white;
  padding: 1.5rem;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

h3 {
  margin-top: 0;
  margin-bottom: 1.5rem;
  color: #333;
  font-size: 1.2rem;
}

.goal-type-section {
  margin-bottom: 1.5rem;
  padding: 1rem;
  border: 2px solid #e0e0e0;
  border-radius: 8px;
  transition: border-color 0.2s;
}

.goal-type-section:hover {
  border-color: #2196f3;
}

.goal-type-label {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
  cursor: pointer;
}

.goal-type-label input[type="radio"] {
  margin-top: 0.25rem;
  width: 18px;
  height: 18px;
  cursor: pointer;
}

.radio-text {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}

.radio-text strong {
  color: #333;
  font-size: 1rem;
}

.radio-text small {
  color: #666;
  font-size: 0.9rem;
}

.goal-input-group {
  margin-top: 1rem;
  margin-left: 2rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.goal-input-group label {
  font-weight: 500;
  color: #555;
  font-size: 0.95rem;
}

.goal-input-group input[type="number"] {
  padding: 0.6rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 1rem;
  width: 200px;
  transition: border-color 0.2s;
}

.goal-input-group input[type="number"]:focus {
  outline: none;
  border-color: #2196f3;
  box-shadow: 0 0 0 3px rgba(33, 150, 243, 0.1);
}

.input-hint {
  color: #999;
  font-size: 0.85rem;
}

.error-text {
  color: #f44336;
  font-size: 0.9rem;
  font-weight: 500;
}

.goal-summary {
  margin-top: 1.5rem;
  padding: 1rem;
  background: #e3f2fd;
  border-left: 4px solid #2196f3;
  border-radius: 4px;
  color: #1565c0;
}

.goal-summary strong {
  display: block;
  margin-bottom: 0.25rem;
}
</style>