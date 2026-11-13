// Reads QGPUI's custom games (data/custom-games/<slug>.json): games that are not on QuestPorts,
// which build-payload adds to the payload. See data/custom-games/README.md.

import { readFile, readdir } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import Ajv2020 from 'ajv/dist/2020.js'

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..')
export const DEFAULT_CUSTOM_GAMES_DIR = path.join(repositoryRoot, 'data', 'custom-games')
export const DEFAULT_CUSTOM_GAME_SCHEMA_FILE = path.join(repositoryRoot, 'data', 'custom-game.schema.json')

/** Returns a function that checks one custom game against the JSON Schema and returns a list of errors. */
export async function loadCustomGameValidator(schemaFile = DEFAULT_CUSTOM_GAME_SCHEMA_FILE) {
  const schema = JSON.parse(await readFile(schemaFile, 'utf8'))
  const ajv = new Ajv2020({ allErrors: true, strict: true })
  const validate = ajv.compile(schema)
  return (game) =>
    validate(game) ? [] : validate.errors.map(e => `${e.instancePath || '/'} ${e.message}`)
}

/**
 * Reads every custom game in a folder. A missing folder has none. Returns { games, errors },
 * where errors maps a file name to its errors. Each game is { slug, port, install, campaigns,
 * resolved }, without the keys that matter only in the file.
 */
export async function loadCustomGames(dir = DEFAULT_CUSTOM_GAMES_DIR, schemaFile = DEFAULT_CUSTOM_GAME_SCHEMA_FILE) {
  const validate = await loadCustomGameValidator(schemaFile)
  let files
  try {
    files = (await readdir(dir)).filter(f => f.endsWith('.json')).sort()
  } catch (e) {
    if (e.code === 'ENOENT') return { games: [], errors: {} }
    throw e
  }
  const games = []
  const errors = {}
  for (const file of files) {
    let game
    try {
      game = JSON.parse(await readFile(path.join(dir, file), 'utf8'))
    } catch (e) {
      errors[file] = [`not valid JSON: ${e.message}`]
      continue
    }
    const found = validate(game)
    if (found.length === 0 && file !== `${game.slug}.json`) found.push(`file name ${file} does not match slug ${game.slug}`)
    if (found.length) {
      errors[file] = found
      continue
    }
    const { slug, port, install = null, campaigns = null, resolved = {} } = game
    games.push({ slug, port, install, campaigns, resolved })
  }
  return { games, errors }
}
