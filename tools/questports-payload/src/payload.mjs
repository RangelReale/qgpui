// Joins the database rows with the install data into one payload object.

export const PAYLOAD_FORMAT = 'qgp-questports-payload'
export const PAYLOAD_VERSION = 1

/**
 * Builds the payload.
 *
 * - `ports`: rows of the ports table, in the order they should appear.
 * - `install`: the loaded install data module (see repository.mjs).
 * - `sources`: what the payload records about where the data came from.
 * - `generatedAt`: a Date.
 *
 * Games are the database rows. Install configs whose slug has no row are listed in
 * `unmatched.installWithoutPort` and otherwise left out, because the site does not show them.
 */
export function buildPayload({ ports, install, sources, generatedAt }) {
  const configs = install.PORT_PACKAGE_CONFIGS
  const expansions = install.PORT_EXPANSIONS

  const seen = new Set()
  const games = ports.map(port => {
    if (typeof port.slug !== 'string' || port.slug === '') {
      throw new Error(`A ports row has no slug (id ${port.id ?? 'unknown'}).`)
    }
    if (seen.has(port.slug)) {
      throw new Error(`Two ports rows have the slug "${port.slug}".`)
    }
    seen.add(port.slug)

    const config = own(configs, port.slug)
    return {
      slug: port.slug,
      port,
      install: config,
      campaigns: own(expansions, port.slug),
      resolved: {
        installType: install.resolveInstallType(port),
        workflowType: install.resolveWorkflowType(port),
        requiresExternalFiles: install.portRequiresExternalFiles(config),
        selfContainedSideload: install.isSelfContainedSideload(config),
        // Where the game comes from (PC, Arcade, Nintendo 64...), as the site's catalog filter
        // derives it; null when nothing matches.
        originPlatform: install.portOriginPlatform(port)
      }
    }
  })

  return {
    format: PAYLOAD_FORMAT,
    version: PAYLOAD_VERSION,
    generatedAt: generatedAt.toISOString(),
    sources,
    // The site's origin platforms, in its order: { id, label }.
    originPlatforms: install.ORIGIN_PLATFORMS.map(({ id, label }) => ({ id, label })),
    games,
    unmatched: {
      portsWithoutInstall: games.filter(game => game.install === null).map(game => game.slug),
      installWithoutPort: Object.keys(configs).filter(slug => !seen.has(slug)).sort(),
      campaignsWithoutPort: Object.keys(expansions).filter(slug => !seen.has(slug)).sort()
    }
  }
}

function own(record, key) {
  return Object.prototype.hasOwnProperty.call(record, key) ? record[key] : null
}
