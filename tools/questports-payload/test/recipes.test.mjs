import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import path from 'node:path'
import { test } from 'node:test'
import { DEFAULT_RECIPES_DIR, checkRecipe, loadRecipes, loadSchemaValidator } from '../src/recipes.mjs'

const sample = async (slug) =>
  JSON.parse(await readFile(path.join(DEFAULT_RECIPES_DIR, `${slug}.json`), 'utf8'))

test('every committed recipe is valid', async () => {
  const { recipes, errors } = await loadRecipes()
  assert.deepEqual(errors, {})
  assert.ok(recipes.length >= 5)
})

test('the schema rejects an unknown step', async () => {
  const validate = await loadSchemaValidator()
  const recipe = await sample('supertuxkart-vr')
  recipe.steps.push({ do: 'reboot' })
  assert.notDeepEqual(validate(recipe), [])
})

test('the schema rejects a push outside /sdcard', async () => {
  const validate = await loadSchemaValidator()
  const recipe = await sample('xelee')
  recipe.steps[1].map[0].to = '/data/local/tmp/'
  assert.notDeepEqual(validate(recipe), [])
})

test('the schema rejects a file input without names or extensions', async () => {
  const validate = await loadSchemaValidator()
  const recipe = await sample('xelee')
  delete recipe.inputs[0].extensions
  assert.notDeepEqual(validate(recipe), [])
})

test('checkRecipe finds a slug that does not match the file name', async () => {
  const recipe = await sample('xelee')
  assert.deepEqual(checkRecipe(recipe, 'other.json'), ['file name other.json does not match slug xelee'])
})

test('checkRecipe finds a push of an unknown input and an input never pushed', async () => {
  const recipe = await sample('virtua-cop-vr')
  recipe.steps[2].input = 'nothing'
  assert.deepEqual(checkRecipe(recipe), [
    '/steps/2 pushes unknown input nothing',
    'input roms is never pushed'
  ])
})

test('checkRecipe finds a from that is not one of the names', async () => {
  const recipe = await sample('starship-vr')
  recipe.steps[2].map[0].from = 'sf64.otr'
  assert.deepEqual(checkRecipe(recipe), [
    "/steps/2/map/0 from sf64.otr is not one of input o2r's names"
  ])
})

test('checkRecipe finds a launch before install-apk', async () => {
  const recipe = await sample('starship-vr')
  recipe.steps.reverse()
  assert.ok(checkRecipe(recipe).some((e) => e.includes('(launch) comes before install-apk')))
})

test('checkRecipe finds an invalid asset pattern', async () => {
  const recipe = await sample('quakequest')
  recipe.apks[0].asset = '(unclosed'
  assert.ok(checkRecipe(recipe)[0].startsWith('/apks/0/asset is not a valid regular expression'))
})
