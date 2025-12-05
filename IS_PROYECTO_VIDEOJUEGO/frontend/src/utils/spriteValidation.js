/**
 * @param {Array} sprite1
 * @param {Array} sprite2
 * @returns {number}
 */
export function calculateSpriteDifference(sprite1, sprite2) {
  if (!sprite1 || !sprite2 || sprite1.length !== 8 || sprite2.length !== 8) {
    return 0
  }

  let totalPixels = 0
  let differentPixels = 0

  for (let row = 0; row < 8; row++) {
    for (let col = 0; col < 5; col++) {
      totalPixels++
      
      const bit1 = (sprite1[row] >> (4 - col)) & 1
      const bit2 = (sprite2[row] >> (4 - col)) & 1
      
      if (bit1 !== bit2) {
        differentPixels++
      }
    }
  }

  const differencePercentage = (differentPixels / totalPixels) * 100
  return Math.round(differencePercentage * 10) / 10
}

/**
 * Analiza las diferencias por región del sprite
 * @param {Array} sprite1
 * @param {Array} sprite2
 * @returns {Object}
 */
function analyzeRegionalDifferences(sprite1, sprite2) {
  const regions = {
    top: { rows: [0, 1, 2], diff: 0, total: 0 },
    middle: { rows: [3, 4], diff: 0, total: 0 },
    bottom: { rows: [5, 6, 7], diff: 0, total: 0 },
    left: { cols: [0, 1], diff: 0, total: 0 },
    center: { cols: [2], diff: 0, total: 0 },
    right: { cols: [3, 4], diff: 0, total: 0 }
  }

  // Analizar diferencias por región
  for (let row = 0; row < 8; row++) {
    for (let col = 0; col < 5; col++) {
      const bit1 = (sprite1[row] >> (4 - col)) & 1
      const bit2 = (sprite2[row] >> (4 - col)) & 1
      const isDiff = bit1 !== bit2

      // Regiones horizontales
      if (regions.top.rows.includes(row)) {
        regions.top.total++
        if (isDiff) regions.top.diff++
      }
      if (regions.middle.rows.includes(row)) {
        regions.middle.total++
        if (isDiff) regions.middle.diff++
      }
      if (regions.bottom.rows.includes(row)) {
        regions.bottom.total++
        if (isDiff) regions.bottom.diff++
      }

      // Regiones verticales
      if (regions.left.cols.includes(col)) {
        regions.left.total++
        if (isDiff) regions.left.diff++
      }
      if (regions.center.cols.includes(col)) {
        regions.center.total++
        if (isDiff) regions.center.diff++
      }
      if (regions.right.cols.includes(col)) {
        regions.right.total++
        if (isDiff) regions.right.diff++
      }
    }
  }

  // Calcular porcentajes
  for (const region in regions) {
    const r = regions[region]
    r.percentage = r.total > 0 ? (r.diff / r.total) * 100 : 0
  }

  return regions
}

/**
 * Analiza la densidad de píxeles en cada sprite
 * @param {Array} sprite
 * @returns {Object}
 */
function analyzeDensity(sprite) {
  let activePixels = 0
  const totalPixels = 40 // 5x8

  for (let row = 0; row < 8; row++) {
    for (let col = 0; col < 5; col++) {
      const bit = (sprite[row] >> (4 - col)) & 1
      if (bit) activePixels++
    }
  }

  return {
    activePixels,
    percentage: (activePixels / totalPixels) * 100
  }
}

/**
 * Genera sugerencias específicas basadas en el análisis
 * @param {Array} character
 * @param {Array} obstacle
 * @returns {Array}
 */
export function generateSmartSuggestions(character, obstacle) {
  const suggestions = []
  const regions = analyzeRegionalDifferences(character, obstacle)
  const charDensity = analyzeDensity(character)
  const obstDensity = analyzeDensity(obstacle)

  // 1. Sugerencias basadas en regiones con poca diferencia
  const lowDiffRegions = []
  
  if (regions.top.percentage < 30) lowDiffRegions.push('superior')
  if (regions.middle.percentage < 30) lowDiffRegions.push('medio')
  if (regions.bottom.percentage < 30) lowDiffRegions.push('inferior')

  if (lowDiffRegions.length > 0) {
    suggestions.push({
      type: 'region',
      priority: 'high',
      icon: '📍',
      title: 'Modificar regiones específicas',
      description: `Las zonas ${lowDiffRegions.join(', ')} son muy similares`,
      actions: [
        `Agrega o quita píxeles en la parte ${lowDiffRegions[0]}`,
        `Intenta crear formas diferentes en estas zonas`
      ]
    })
  }

  // 2. Sugerencias basadas en densidad
  const densityDiff = Math.abs(charDensity.percentage - obstDensity.percentage)
  
  if (densityDiff < 15) {
    if (charDensity.percentage > 50) {
      suggestions.push({
        type: 'density',
        priority: 'medium',
        icon: '🔲',
        title: 'Ajustar densidad',
        description: 'Ambos sprites tienen densidades similares',
        actions: [
          'Haz el personaje más compacto (menos píxeles)',
          'Haz el obstáculo más denso (más píxeles)',
          'O viceversa para mayor contraste'
        ]
      })
    } else {
      suggestions.push({
        type: 'density',
        priority: 'medium',
        icon: '🔳',
        title: 'Ajustar densidad',
        description: 'Ambos sprites están poco rellenos',
        actions: [
          'Agrega más píxeles a uno de los sprites',
          'Crea formas más sólidas y definidas'
        ]
      })
    }
  }

  // 3. Sugerencias de formas específicas
  if (regions.left.percentage < 25 && regions.right.percentage < 25) {
    suggestions.push({
      type: 'shape',
      priority: 'high',
      icon: '🎨',
      title: 'Modificar forma lateral',
      description: 'Los laterales son muy similares',
      actions: [
        'Agrega brazos o protuberancias al personaje',
        'Haz el obstáculo más ancho o irregular',
        'Crea asimetría en uno de los sprites'
      ]
    })
  }

  // 4. Sugerencias generales basadas en patrones comunes
  if (suggestions.length === 0) {
    suggestions.push({
      type: 'general',
      priority: 'medium',
      icon: '💡',
      title: 'Ideas generales',
      description: 'Prueba estas técnicas para diferenciar los sprites',
      actions: [
        'Usa formas geométricas diferentes (cuadrado vs triángulo)',
        'Varía la altura: uno alto y delgado, otro bajo y ancho',
        'Agrega detalles únicos: antenas, ruedas, picos, etc.',
        'Invierte uno de los sprites para crear contraste'
      ]
    })
  }

  // 5. Sugerencia de usar botones de utilidad
  suggestions.push({
    type: 'tools',
    priority: 'low',
    icon: '🛠️',
    title: 'Usa las herramientas',
    description: 'Prueba los botones de edición rápida',
    actions: [
      'Usa "Invertir" en uno de los sprites',
      'Prueba "Limpiar" y rediseña desde cero',
      'El botón "Llenar" puede servir como base'
    ]
  })

  return suggestions
}

/**
 * @param {Array} character
 * @param {Array} obstacle
 * @returns {Object}
 */
export function validateSpriteDifference(character, obstacle) {
  const minDifferenceRequired = 20

  const difference = calculateSpriteDifference(character, obstacle)
  const suggestions = generateSmartSuggestions(character, obstacle)

  if (difference < minDifferenceRequired) {
    return {
      valid: false,
      difference: difference,
      message: `El personaje y el obstáculo son muy similares (${difference}% de diferencia). Se requiere al menos ${minDifferenceRequired}% de diferencia para garantizar jugabilidad.`,
      suggestions: suggestions
    }
  }

  return {
    valid: true,
    difference: difference,
    message: `Los sprites son suficientemente diferentes (${difference}% de diferencia).`,
    suggestions: []
  }
}

/**
 * @param {Array} sprite
 * @returns {boolean}
 */
export function isSpriteEmpty(sprite) {
  return sprite.every(byte => byte === 0)
}