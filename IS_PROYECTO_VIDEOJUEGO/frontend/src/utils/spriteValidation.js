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
 * @param {Array} character
 * @param {Array} obstacle
 * @returns {Object}
 */
export function validateSpriteDifference(character, obstacle) {
  const minDifferenceRequired = 20

  const difference = calculateSpriteDifference(character, obstacle)

  if (difference < minDifferenceRequired) {
    return {
      valid: false,
      difference: difference,
      message: `El personaje y el obstáculo son muy similares (${difference}% de diferencia). Se requiere al menos ${minDifferenceRequired}% de diferencia para garantizar jugabilidad.`
    }
  }

  return {
    valid: true,
    difference: difference,
    message: `Los sprites son suficientemente diferentes (${difference}% de diferencia).`
  }
}

/**
 * @param {Array} sprite
 * @returns {boolean}
 */
export function isSpriteEmpty(sprite) {
  return sprite.every(byte => byte === 0)
}