import type { Port } from '~/types/port'
import { LANGUAGES } from '~/data/portPackageMap'
import { isCleanQuestDropPath } from '../lib/dropPaths'

export const PORT_EXPANSIONS: Record<string, { id: string; fullPath: string; clean: boolean }[]> = {
  'game-a': LANGUAGES.map(language => ({
    id: language.id,
    fullPath: `/sdcard/languages/${language.name}/`,
    clean: isCleanQuestDropPath(`/sdcard/languages/${language.name}/`)
  })),
  'campaign-only': []
}

export function unused(port: Port) { return port.slug }
