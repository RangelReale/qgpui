export function noPcInstall(slug: string | null | undefined) {
  return slug === 'game-direct' ? 'yes' : 'unknown'
}
