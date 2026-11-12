export function isCleanQuestDropPath(path: string): boolean {
  return /^\/sdcard\//.test(path)
}
