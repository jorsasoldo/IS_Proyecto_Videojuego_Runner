<template>
  <div class="preset-gallery">
    <h3>{{ title }}</h3>
    <div class="presets-grid">
      <div 
        v-for="preset in presets" 
        :key="preset.id"
        class="preset-card"
        @click="selectPreset(preset)"
      >
        <div class="preset-preview">
          <canvas 
            :ref="el => setCanvasRef(preset.id, el)"
            width="100" 
            height="160"
            class="preset-canvas"
          ></canvas>
        </div>
        <div class="preset-info">
          <div class="preset-name">{{ preset.name }}</div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'PresetGallery',
  props: {
    title: {
      type: String,
      required: true
    },
    presets: {
      type: Array,
      required: true
    }
  },
  emits: ['select'],
  data() {
    return {
      canvasRefs: {}
    }
  },
  mounted() {
    this.$nextTick(() => {
      this.renderAllPreviews()
    })
  },
  methods: {
    setCanvasRef(id, el) {
      if (el) {
        this.canvasRefs[id] = el
      }
    },
    
    renderAllPreviews() {
      this.presets.forEach(preset => {
        this.renderPreview(preset)
      })
    },
    
    renderPreview(preset) {
      const canvas = this.canvasRefs[preset.id]
      if (!canvas) return
      
      const ctx = canvas.getContext('2d')
      const pixelSize = 20
      
      ctx.fillStyle = '#0000ff'
      ctx.fillRect(0, 0, canvas.width, canvas.height)
      
      for (let row = 0; row < 8; row++) {
        const byte = preset.data[row]
        
        for (let col = 0; col < 5; col++) {
          const isActive = (byte & (1 << (4 - col))) !== 0
          
          if (isActive) {
            ctx.fillStyle = '#ffff00'
            ctx.fillRect(
              col * pixelSize + 2,
              row * pixelSize + 2,
              pixelSize - 4,
              pixelSize - 4
            )
          }
        }
      }
      
      ctx.strokeStyle = '#ffffff'
      ctx.lineWidth = 1
      
      for (let i = 0; i <= 5; i++) {
        ctx.beginPath()
        ctx.moveTo(i * pixelSize, 0)
        ctx.lineTo(i * pixelSize, 160)
        ctx.stroke()
      }
      
      for (let i = 0; i <= 8; i++) {
        ctx.beginPath()
        ctx.moveTo(0, i * pixelSize)
        ctx.lineTo(100, i * pixelSize)
        ctx.stroke()
      }
    },
    
    selectPreset(preset) {
      this.$emit('select', preset.data)
    }
  }
}
</script>

<style scoped>
.preset-gallery {
  margin-bottom: 2rem;
}

h3 {
  color: #333;
  margin-bottom: 1rem;
  font-size: 1.1rem;
}

.presets-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 1rem;
}

.preset-card {
  background: white;
  border: 2px solid #ddd;
  border-radius: 12px;
  padding: 1rem;
  cursor: pointer;
  transition: all 0.3s ease;
  text-align: center;
}

.preset-card:hover {
  border-color: #2196f3;
  transform: translateY(-4px);
  box-shadow: 0 6px 16px rgba(33, 150, 243, 0.3);
}

.preset-preview {
  margin-bottom: 0.75rem;
  display: flex;
  justify-content: center;
}

.preset-canvas {
  border: 2px solid #333;
  border-radius: 4px;
  image-rendering: pixelated;
}

.preset-info {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}

.preset-name {
  font-weight: 600;
  color: #333;
  font-size: 1rem;
}
</style>