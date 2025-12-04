<template>
  <div class="loading-overlay" v-if="show">
    <div class="loading-content">
      <div class="spinner"></div>
      <p class="loading-text">{{ message }}</p>
      <div class="loading-steps" v-if="steps.length > 0">
        <div 
          v-for="(step, index) in steps" 
          :key="index"
          class="step"
          :class="{ active: currentStep === index, completed: currentStep > index }"
        >
          <span class="step-icon">{{ currentStep > index ? '✓' : '●' }}</span>
          <span class="step-text">{{ step }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'LoadingSpinner',
  props: {
    show: {
      type: Boolean,
      default: false
    },
    message: {
      type: String,
      default: 'Enviando configuración...'
    },
    steps: {
      type: Array,
      default: () => []
    },
    currentStep: {
      type: Number,
      default: 0
    }
  }
}
</script>

<style scoped>
.loading-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.7);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 9999;
  animation: fadeIn 0.2s ease;
}

@keyframes fadeIn {
  from { opacity: 0; }
  to { opacity: 1; }
}

.loading-content {
  background: white;
  padding: 2rem;
  border-radius: 12px;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
  text-align: center;
  min-width: 300px;
  max-width: 400px;
}

.spinner {
  width: 60px;
  height: 60px;
  border: 5px solid #f3f3f3;
  border-top: 5px solid #2196f3;
  border-radius: 50%;
  animation: spin 1s linear infinite;
  margin: 0 auto 1.5rem;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.loading-text {
  color: #333;
  font-size: 1.1rem;
  font-weight: 500;
  margin: 0 0 1.5rem;
}

.loading-steps {
  text-align: left;
  margin-top: 1.5rem;
}

.step {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.5rem;
  margin-bottom: 0.5rem;
  border-radius: 4px;
  transition: all 0.3s;
}

.step.active {
  background: #e3f2fd;
  font-weight: 500;
}

.step.completed {
  color: #4caf50;
}

.step-icon {
  font-size: 1.2rem;
  width: 24px;
  text-align: center;
}

.step.active .step-icon {
  color: #2196f3;
  animation: pulse 1.5s ease infinite;
}

.step.completed .step-icon {
  color: #4caf50;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

.step-text {
  flex: 1;
}
</style>