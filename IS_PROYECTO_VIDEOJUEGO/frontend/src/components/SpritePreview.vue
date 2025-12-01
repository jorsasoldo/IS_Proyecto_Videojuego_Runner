<template>
  <div class="sprite-preview">
    <h4>{{ title }}</h4>
    <canvas 
      ref="canvas" 
      :width="canvasWidth" 
      :height="canvasHeight"
      class="preview-canvas"
    ></canvas>
  </div>
</template>

<script>
export default {
  name: 'SpritePreview',
  props: {
    spriteData: {
      type: Array,
      required: true,
      validator: (val) => val.length === 8
    },
    title: {
      type: String,
      default: 'Vista Previa'
    },
    pixelSize: {
      type: Number,
      default: 40
    }
  },
  computed: {
    canvasWidth() {
      return 5 * this.pixelSize
    },
    canvasHeight() {
      return 8 * this.pixelSize
    }
  },
  watch: {
    spriteData: {
      handler() {
        this.$nextTick(() => {
          this.renderSprite()
        })
      },
      deep: true
    }
  },
  mounted() {
    this.renderSprite()
  },
  methods: {
    renderSprite() {
        const canvas = this.$refs.canvas
        if (!canvas) return

        const ctx = canvas.getContext('2d')
        
        ctx.fillStyle = '#0000ff'
        ctx.fillRect(0, 0, this.canvasWidth, this.canvasHeight)
        
        for (let row = 0; row < 8; row++) {
            const byte = this.spriteData[row]
            
            for (let col = 0; col < 5; col++) {
            const isActive = (byte & (1 << (4 - col))) !== 0
            
            if (isActive) {
                ctx.fillStyle = '#ffff00'
                ctx.fillRect(
                col * this.pixelSize + 2,
                row * this.pixelSize + 2,
                this.pixelSize - 4,
                this.pixelSize - 4
                )
                
                ctx.shadowColor = '#ffff00'
                ctx.shadowBlur = 10
                ctx.fillRect(
                col * this.pixelSize + 2,
                row * this.pixelSize + 2,
                this.pixelSize - 4,
                this.pixelSize - 4
                )
                ctx.shadowBlur = 0
            } else {
                ctx.fillStyle = '#000080'
                ctx.fillRect(
                col * this.pixelSize + 2,
                row * this.pixelSize + 2,
                this.pixelSize - 4,
                this.pixelSize - 4
                )
            }
            }
        }
        
        ctx.strokeStyle = '#ffffff'
        ctx.lineWidth = 1
        
        for (let i = 0; i <= 5; i++) {
            ctx.beginPath()
            ctx.moveTo(i * this.pixelSize, 0)
            ctx.lineTo(i * this.pixelSize, this.canvasHeight)
            ctx.stroke()
        }
        
        for (let i = 0; i <= 8; i++) {
            ctx.beginPath()
            ctx.moveTo(0, i * this.pixelSize)
            ctx.lineTo(this.canvasWidth, i * this.pixelSize)
            ctx.stroke()
        }
    }
  }
}
</script>

<style scoped>
.sprite-preview {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
  padding: 1rem;
  background: white;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

h4 {
  margin: 0;
  color: #333;
  font-size: 1rem;
}

.preview-canvas {
  border: 3px solid #333;
  border-radius: 4px;
  background: #1a1a1a;
  image-rendering: pixelated;
  image-rendering: crisp-edges;
}
</style>