<template>
  <div class="sprite-editor">
    <h3>{{ title }}</h3>
    
    <!-- Galería de presets -->
    <PresetGallery
      title="Diseños predefinidos"
      :presets="presets"
      @select="loadPreset"
    />

    <div class="editor-layout">
      <!-- Matriz 5x8 -->
      <div class="grid-container">
        <div class="grid" :style="gridStyle">
          <div
            v-for="rowIndex in 8"
            :key="`row-${rowIndex-1}`"
            class="grid-row"
          >
            <div
              v-for="colIndex in 5"
              :key="`cell-${rowIndex-1}-${colIndex-1}`"
              class="grid-cell"
              :class="{ active: matrix[rowIndex-1][colIndex-1] }"
              @mousedown.prevent="handleMouseDown(rowIndex-1, colIndex-1)"
              @mouseenter="dragCell(rowIndex-1, colIndex-1)"
              @mouseup="handleMouseUp"
            ></div>
          </div>
        </div>
      </div>

      <SpritePreview 
        :spriteData="matrixToArray()"
        :title="'Vista Previa LCD'"
      />
    </div>

    <div class="controls">
      <button @click="clearGrid" class="btn btn-clear">
        Limpiar
      </button>
      <button @click="fillGrid" class="btn btn-fill">
        Llenar
      </button>
      <button @click="invertGrid" class="btn btn-invert">
        Invertir
      </button>
    </div>

    <div class="array-preview">
      <strong>Array (8 bytes):</strong>
      <code>{{ arrayRepresentation }}</code>
    </div>
  </div>
</template>

<script>
import SpritePreview from './SpritePreview.vue'
import PresetGallery from './PresetGallery.vue'
import { characterPresets, obstaclePresets } from '../data/presets.js'

export default {
  name: 'SpriteEditor',
  components: {
    SpritePreview,
    PresetGallery
  },
  props: {
    title: {
      type: String,
      default: 'Sprite Editor'
    },
    modelValue: {
      type: Array,
      default: () => [0, 0, 0, 0, 0, 0, 0, 0]
    }
  },
  emits: ['update:modelValue'],
  data() {
    return {
      matrix: this.createEmptyMatrix(),
      isDragging: false,
      dragValue: null,
      clickedCell: null
    }
  },
  computed: {
    gridStyle() {
      return {
        gridTemplateRows: `repeat(8, 1fr)`,
        gridTemplateColumns: `repeat(5, 1fr)`
      }
    },
    arrayRepresentation() {
      return '[' + this.matrixToArray().join(', ') + ']'
    },
    presets() {
      return this.title.toLowerCase().includes('personaje') ? characterPresets : obstaclePresets
    }
  },
  watch: {
    modelValue: {
      handler(newVal) {
        if (newVal && newVal.length === 8) {
          this.arrayToMatrix(newVal)
        }
      },
      deep: true
    }
  },
  mounted() {
    if (this.modelValue && this.modelValue.length === 8) {
      this.arrayToMatrix(this.modelValue)
    }

    document.addEventListener('mouseup', this.stopDrag)
  },
  beforeUnmount() {
    document.removeEventListener('mouseup', this.stopDrag)
  },
  methods: {
    createEmptyMatrix() {
      return Array(8).fill(null).map(() => Array(5).fill(false))
    },
    
    handleMouseDown(row, col) {
      this.clickedCell = { row, col }
      this.isDragging = true
      this.dragValue = !this.matrix[row][col]
      
      const newMatrix = this.matrix.map(r => [...r])
      newMatrix[row][col] = this.dragValue
      this.matrix = newMatrix
      this.emitUpdate()
    },

    handleMouseUp() {
      this.isDragging = false
      this.dragValue = null
      this.clickedCell = null
    },

    dragCell(row, col) {
      if (this.isDragging && this.dragValue !== null) {
        const newMatrix = this.matrix.map(r => [...r])
        newMatrix[row][col] = this.dragValue
        this.matrix = newMatrix
        this.emitUpdate()
      }
    },

    stopDrag() {
      this.isDragging = false
      this.dragValue = null
    },

    clearGrid() {
      this.matrix = this.createEmptyMatrix()
      this.emitUpdate()
    },

    fillGrid() {
      this.matrix = Array(8).fill(null).map(() => Array(5).fill(true))
      this.emitUpdate()
    },

    invertGrid() {
      this.matrix = this.matrix.map(row => row.map(cell => !cell))
      this.emitUpdate()
    },

    loadPreset(presetData) {
      this.arrayToMatrix(presetData)
      this.emitUpdate()
    },

    matrixToArray() {
      return this.matrix.map(row => {
        let byte = 0
        for (let i = 0; i < 5; i++) {
          if (row[i]) {
            byte |= (1 << (4 - i))
          }
        }
        return byte
      })
    },

    arrayToMatrix(array) {
      this.matrix = array.map(byte => {
        const row = []
        for (let i = 0; i < 5; i++) {
          row.push((byte & (1 << (4 - i))) !== 0)
        }
        return row
      })
    },

    emitUpdate() {
      const array = this.matrixToArray()
      this.$emit('update:modelValue', array)
    }
  }
}
</script>

<style scoped>
.sprite-editor {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 1rem;
  padding: 1rem;
  background: #f5f5f5;
  border-radius: 8px;
}

h3 {
  margin: 0;
  color: #333;
  font-size: 1.2rem;
}

.editor-layout {
  display: flex;
  gap: 1.5rem;
  align-items: flex-start;
}

.grid-container {
  background: white;
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.grid {
  display: grid;
  gap: 2px;
  background: #ddd;
  border: 2px solid #999;
  user-select: none;
}

.grid-row {
  display: contents;
}

.grid-cell {
  width: 30px;
  height: 30px;
  background: white;
  cursor: pointer;
  transition: background-color 0.1s;
}

.grid-cell:hover {
  background: #e3f2fd;
}

.grid-cell.active {
  background: #2196f3;
}

.grid-cell.active:hover {
  background: #1976d2;
}

.controls {
  display: flex;
  gap: 0.5rem;
}

.btn {
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.9rem;
  transition: all 0.2s;
}

.btn-clear {
  background: #f44336;
  color: white;
}

.btn-clear:hover {
  background: #d32f2f;
}

.btn-fill {
  background: #4caf50;
  color: white;
}

.btn-fill:hover {
  background: #388e3c;
}

.btn-invert {
  background: #ff9800;
  color: white;
}

.btn-invert:hover {
  background: #f57c00;
}

.array-preview {
  width: 100%;
  padding: 0.75rem;
  background: white;
  border-radius: 4px;
  text-align: center;
  font-family: monospace;
}

.array-preview code {
  color: #1976d2;
  font-size: 0.9rem;
}
</style>