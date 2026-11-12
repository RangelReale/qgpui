#!/usr/bin/env node
// Checks install recipes against the schema and the extra checks in recipes.mjs.
// Usage: node src/check-recipes.mjs [recipe.json ...]   (default: every recipe in data/recipes)

import { readFile, readdir } from 'node:fs/promises'
import path from 'node:path'
import { DEFAULT_RECIPES_DIR, checkRecipe, loadSchemaValidator } from './recipes.mjs'

const validate = await loadSchemaValidator()
let files = process.argv.slice(2)
if (files.length === 0) {
  files = (await readdir(DEFAULT_RECIPES_DIR)).filter(f => f.endsWith('.json')).map(f => path.join(DEFAULT_RECIPES_DIR, f))
}

let bad = 0
for (const file of files.sort()) {
  const name = path.basename(file)
  let errors
  try {
    const recipe = JSON.parse(await readFile(file, 'utf8'))
    errors = [...validate(recipe), ...checkRecipe(recipe, name)]
  } catch (e) {
    errors = [e.message]
  }
  if (errors.length) {
    bad++
    for (const e of errors) console.log(`${name}: ${e}`)
  }
}
console.log(`${files.length - bad} of ${files.length} recipe(s) are valid.`)
process.exitCode = bad ? 1 : 0
