import type { Unused } from '~/lib/doesNotExist'
export type PortWorkflowType = 'direct' | 'pc_assets' | 'emulator_roms'
export interface Port { slug: string; category: string; port_download_url?: string; base_game_url?: string; extra?: Unused }
