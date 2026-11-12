import type { Port } from '~/types/port'

export const ORIGIN_PLATFORMS = [
  { id: 'pc', label: 'PC' },
  { id: 'arcade', label: 'Arcade' }
] as const

export function portOriginPlatform(port: Pick<Port, 'title'> & { base_game_store?: string | null }) {
  const text = `${port.base_game_store ?? ''} ${port.title}`.toLowerCase()
  if (/arcade/.test(text)) return 'arcade'
  if (/steam|\bgog\b/.test(text)) return 'pc'
  return null
}
