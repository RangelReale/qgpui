import type { Port, PortWorkflowType } from '~/types/port'
export type { PortWorkflowType }

export interface PortPackageConfig {
  slug: string
  packageName: string
  installType: 'direct_apk' | 'apk_and_assets'
  workflowType?: PortWorkflowType
  criticalFiles: string[]
  folders?: { id: string; targetPath: string; required: boolean }[]
}

export const LANGUAGES = [
  { id: 'english', name: 'English', base: true },
  { id: 'french', name: 'French', base: false }
] as const

function languageFolder(language: (typeof LANGUAGES)[number]) {
  return { id: `lang_${language.id}`, targetPath: `/sdcard/languages/${language.name}/`, required: language.base }
}

export const PORT_PACKAGE_CONFIGS: Record<string, PortPackageConfig> = {
  'game-a': {
    slug: 'game-a',
    packageName: 'com.example.a',
    installType: 'apk_and_assets',
    workflowType: 'pc_assets',
    criticalFiles: ['base/pak0.pk3'],
    folders: LANGUAGES.map(languageFolder)
  },
  'game-direct': {
    slug: 'game-direct',
    packageName: 'com.example.direct',
    installType: 'direct_apk',
    workflowType: 'direct',
    criticalFiles: []
  },
  'config-only': {
    slug: 'config-only',
    packageName: 'com.example.orphan',
    installType: 'direct_apk',
    criticalFiles: []
  }
}

export function resolveInstallType(port: any) {
  return PORT_PACKAGE_CONFIGS[port.slug]?.installType ?? 'pc_builder_required'
}

export function resolveWorkflowType(target: any): PortWorkflowType {
  return PORT_PACKAGE_CONFIGS[target.slug]?.workflowType ?? (target.category === 'emulator' ? 'emulator_roms' : 'pc_assets')
}

export function portRequiresExternalFiles(cfg?: PortPackageConfig | null): boolean {
  if (!cfg) return true
  return cfg.installType !== 'direct_apk' || cfg.criticalFiles.length > 0
}

export function isSelfContainedSideload(cfg?: PortPackageConfig | null): boolean {
  return !!cfg && !portRequiresExternalFiles(cfg)
}
