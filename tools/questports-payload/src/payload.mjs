// Joins the database rows with the install data into one payload object.

export const PAYLOAD_FORMAT = 'qgpui-questports-payload'
export const PAYLOAD_VERSION = 1

// A slug becomes a folder name in QGPUI's files folder, so it must be a plain lower-case id, as
// data/recipe.schema.json requires of recipe slugs. The site's database is not ours to trust.
const SLUG_PATTERN = /^[a-z0-9]+(-[a-z0-9]+)*$/

// Every custom game's slug starts with this, so that it never clashes with a QuestPorts game.
export const CUSTOM_SLUG_PREFIX = 'qgpui-'

/**
 * Builds the payload.
 *
 * - `ports`: rows of the ports table, in the order they should appear.
 * - `install`: the loaded install data module (see repository.mjs).
 * - `sources`: what the payload records about where the data came from.
 * - `generatedAt`: a Date.
 * - `customGames`: QGPUI's own games, as custom-games.mjs reads them.
 *
 * Games are the database rows, then the custom games. Install configs whose slug has no row are
 * listed in `unmatched.installWithoutPort` and otherwise left out, because the site does not show
 * them.
 */
export function buildPayload({ ports, install, sources, generatedAt, customGames = [] }) {
  const configs = install.PORT_PACKAGE_CONFIGS
  const expansions = install.PORT_EXPANSIONS

  const seen = new Set()
  const games = ports.map(port => {
    if (typeof port.slug !== 'string' || port.slug === '') {
      throw new Error(`A ports row has no slug (id ${port.id ?? 'unknown'}).`)
    }
    if (!SLUG_PATTERN.test(port.slug)) {
      throw new Error(`The ports row ${port.id ?? 'unknown'} has the unsafe slug ${JSON.stringify(port.slug)}.`)
    }
    if (seen.has(port.slug)) {
      throw new Error(`Two ports rows have the slug "${port.slug}".`)
    }
    seen.add(port.slug)
    return gameEntry(install, port, own(configs, port.slug), own(expansions, port.slug))
  })
  const portSlugs = new Set(seen)

  // A custom game goes through the site's functions like a row; its file's `resolved` then
  // overrides what they compute.
  for (const custom of customGames) {
    if (typeof custom.slug !== 'string' || !SLUG_PATTERN.test(custom.slug) || !custom.slug.startsWith(CUSTOM_SLUG_PREFIX)) {
      throw new Error(`The custom game slug ${JSON.stringify(custom.slug)} must be a plain id that starts with "${CUSTOM_SLUG_PREFIX}".`)
    }
    if (seen.has(custom.slug)) {
      throw new Error(`The custom game "${custom.slug}" has the slug of another game.`)
    }
    seen.add(custom.slug)
    const port = { ...custom.port, slug: custom.slug }
    const entry = gameEntry(install, port, custom.install ?? null, custom.campaigns ?? null)
    games.push({ ...entry, resolved: { ...entry.resolved, ...custom.resolved }, custom: true })
  }

  return {
    format: PAYLOAD_FORMAT,
    version: PAYLOAD_VERSION,
    generatedAt: generatedAt.toISOString(),
    sources,
    // The site's origin platforms, in its order: { id, label }.
    originPlatforms: install.ORIGIN_PLATFORMS.map(({ id, label }) => ({ id, label })),
    // The site's VR styles, in its order: { id, label }.
    vrStyles: install.VR_STYLES.map(({ id, label }) => ({ id, label })),
    // The site's headset labels, in its order.
    headsets: [...install.CATALOG_HEADSETS],
    games,
    unmatched: {
      portsWithoutInstall: games.filter(game => !game.custom && game.install === null).map(game => game.slug),
      installWithoutPort: Object.keys(configs).filter(slug => !portSlugs.has(slug)).sort(),
      campaignsWithoutPort: Object.keys(expansions).filter(slug => !portSlugs.has(slug)).sort()
    }
  }
}

function gameEntry(install, port, config, campaigns) {
  return {
    slug: port.slug,
    port,
    install: config,
    campaigns,
    resolved: {
      installType: install.resolveInstallType(port),
      workflowType: install.resolveWorkflowType(port),
      requiresExternalFiles: install.portRequiresExternalFiles(config),
      selfContainedSideload: install.isSelfContainedSideload(config),
      // Where the game comes from (PC, Arcade, Nintendo 64...), as the site's catalog filter
      // derives it; null when nothing matches.
      originPlatform: install.portOriginPlatform(port),
      // How the game plays in VR, as the site's VR style filter derives it ("motion", "cockpit",
      // "mixed_reality" or "seated").
      vrStyle: install.portVrStyle(port),
      // The headsets the port runs on, as the site's catalog labels ("Quest 3").
      headsets: install.normalizeHeadsetList(port.supported_hardware),
      // "yes" when the headset alone can install it (the site's "No PC needed"), "no" or "unknown".
      noPcInstall: install.noPcInstall(port.slug)
    }
  }
}

function own(record, key) {
  return Object.prototype.hasOwnProperty.call(record, key) ? record[key] : null
}
