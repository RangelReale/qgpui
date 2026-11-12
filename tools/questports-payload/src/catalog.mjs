// Joins the QuestPorts payload with qgp's install recipes into the catalog qgp reads.

import { PAYLOAD_FORMAT, PAYLOAD_VERSION } from './payload.mjs'

export const CATALOG_FORMAT = 'qgp-catalog'
export const CATALOG_VERSION = 1

// Recipe keys that only matter in the recipe file, not to qgp.
const RECIPE_FILE_KEYS = ['$schema', 'format', 'version', 'slug', 'checkedAgainst', 'notes']

// Drift in these fields can make a recipe install to the wrong place or the wrong app.
export const SERIOUS_DRIFT = new Set(['packageName', 'targetPath'])

/**
 * Builds the catalog.
 *
 * - `payload`: the object that build-payload writes.
 * - `recipes`: checked recipes (see recipes.mjs).
 * - `generatedAt`: a Date.
 *
 * Every payload game is kept, with `recipe` set to its recipe or null, and `drift` listing the
 * payload values that changed since the recipe was written. Recipes without a game are listed in
 * `unmatched.recipesWithoutGame` and otherwise left out.
 */
export function buildCatalog({ payload, recipes, generatedAt }) {
  if (payload?.format !== PAYLOAD_FORMAT || payload.version !== PAYLOAD_VERSION) {
    throw new Error(`The payload is not ${PAYLOAD_FORMAT} version ${PAYLOAD_VERSION}.`)
  }

  const bySlug = new Map()
  for (const recipe of recipes) {
    if (bySlug.has(recipe.slug)) throw new Error(`Two recipes have the slug "${recipe.slug}".`)
    bySlug.set(recipe.slug, recipe)
  }

  const games = payload.games.map(game => {
    const recipe = bySlug.get(game.slug)
    return {
      ...game,
      recipe: recipe ? catalogRecipe(recipe) : null,
      drift: recipe ? findDrift(recipe, game) : []
    }
  })

  const slugs = new Set(payload.games.map(game => game.slug))
  return {
    format: CATALOG_FORMAT,
    version: CATALOG_VERSION,
    generatedAt: generatedAt.toISOString(),
    sources: {
      payload: { generatedAt: payload.generatedAt, sources: payload.sources },
      recipes: { count: recipes.length }
    },
    games,
    unmatched: {
      ...payload.unmatched,
      gamesWithoutRecipe: games.filter(game => game.recipe === null).map(game => game.slug),
      recipesWithoutGame: [...bySlug.keys()].filter(slug => !slugs.has(slug)).sort()
    }
  }
}

/** Lists the payload values that differ from the recipe's checkedAgainst. */
export function findDrift(recipe, game) {
  const now = {
    packageName: game.install?.packageName,
    version: game.port?.latest_version,
    targetPath: game.install?.targetPath
  }
  const drift = []
  for (const field of ['packageName', 'version', 'targetPath']) {
    const was = recipe.checkedAgainst[field]
    if ((was ?? null) !== (now[field] ?? null)) {
      drift.push({ field, recipe: was ?? null, payload: now[field] ?? null })
    }
  }
  return drift
}

function catalogRecipe(recipe) {
  const out = { ...recipe }
  for (const key of RECIPE_FILE_KEYS) delete out[key]
  return out
}
