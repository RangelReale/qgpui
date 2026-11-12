export const CATALOG_HEADSETS = ['Quest 2', 'Quest 3'] as const

function label(raw: string): string {
  const spaced = raw.toLowerCase().replace(/[_-]+/g, ' ')
  if (spaced.includes('quest 3')) return 'Quest 3'
  if (spaced.includes('quest 2')) return 'Quest 2'
  return raw.trim()
}

export function normalizeHeadsetList(values: string[] | null | undefined): string[] {
  if (!Array.isArray(values)) return []
  return [...new Set(values.map(label))].sort()
}
