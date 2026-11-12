#!/usr/bin/env node
// Builds the QGPUI catalog: the QuestPorts payload joined with QGPUI's install recipes. See README.md.

import { mkdir, readFile, writeFile } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { parseArgs } from 'node:util'
import { SERIOUS_DRIFT, buildCatalog } from './catalog.mjs'
import { DEFAULT_RECIPES_DIR, DEFAULT_SCHEMA_FILE, loadRecipes } from './recipes.mjs'

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..')
const DEFAULT_PAYLOAD = path.join(repositoryRoot, 'local', 'questports-payload.json')
const DEFAULT_OUT = path.join(repositoryRoot, 'local', 'qgpui-catalog.json')

const USAGE = `Usage: node src/build-catalog.mjs [options]

Options:
  --payload <file>   Payload to read (default: local/questports-payload.json in the repository).
  --recipes <dir>    Recipe folder (default: data/recipes in the repository).
  --schema <file>    Recipe schema (default: data/recipe.schema.json in the repository).
  --out <file>       Output file (default: local/qgpui-catalog.json in the repository).
  --strict           Fail, and write nothing, on recipes without a game or on a changed
                     package name or target path.
  --verbose          Also list the games without a recipe.
  -h, --help         Show this help.`

async function main() {
  const { values } = parseArgs({
    options: {
      payload: { type: 'string', default: DEFAULT_PAYLOAD },
      recipes: { type: 'string', default: DEFAULT_RECIPES_DIR },
      schema: { type: 'string', default: DEFAULT_SCHEMA_FILE },
      out: { type: 'string', default: DEFAULT_OUT },
      strict: { type: 'boolean', default: false },
      verbose: { type: 'boolean', default: false },
      help: { type: 'boolean', short: 'h', default: false }
    },
    strict: true
  })
  if (values.help) {
    console.log(USAGE)
    return
  }

  const payload = JSON.parse(await readFile(values.payload, 'utf8'))
  log(`Payload: ${values.payload} (${payload.games?.length ?? 0} games, generated ${payload.generatedAt})`)

  const { recipes, errors } = await loadRecipes(values.recipes, values.schema)
  const bad = Object.entries(errors)
  if (bad.length) {
    for (const [file, list] of bad) for (const e of list) log(`error: ${file}: ${e}`)
    throw new Error(`${bad.length} recipe file(s) have errors; nothing written.`)
  }
  log(`Recipes: ${recipes.length} in ${values.recipes}`)

  const catalog = buildCatalog({ payload, recipes, generatedAt: new Date() })

  const { gamesWithoutRecipe, recipesWithoutGame } = catalog.unmatched
  let serious = recipesWithoutGame.length
  for (const slug of recipesWithoutGame) log(`Warning: recipe without a game: ${slug}`)
  for (const game of catalog.games) {
    for (const d of game.drift) {
      const isSerious = SERIOUS_DRIFT.has(d.field)
      if (isSerious) serious++
      log(`${isSerious ? 'Warning' : 'Note'}: ${game.slug}: ${d.field} was ${show(d.recipe)}, is now ${show(d.payload)}`)
    }
  }
  log(`${catalog.games.length - gamesWithoutRecipe.length} of ${catalog.games.length} games have a recipe.`)
  if (values.verbose && gamesWithoutRecipe.length) log(`Without a recipe: ${gamesWithoutRecipe.join(', ')}`)

  if (values.strict && serious) throw new Error(`${serious} warning(s) with --strict; nothing written.`)

  const out = path.resolve(values.out)
  await mkdir(path.dirname(out), { recursive: true })
  await writeFile(out, JSON.stringify(catalog, null, 2) + '\n', 'utf8')
  log(`Wrote ${out}`)
}

function show(value) {
  return value === null ? 'unset' : JSON.stringify(value)
}

function log(message) {
  process.stderr.write(message + '\n')
}

main().catch(err => {
  log(`error: ${err.message}`)
  process.exitCode = 1
})
