import assert from 'node:assert/strict'
import { test } from 'node:test'
import { CATALOG_FORMAT, buildCatalog, findDrift } from '../src/catalog.mjs'

const generatedAt = new Date('2026-11-12T10:00:00Z')

function game(slug, { packageName = `com.example.${slug.replaceAll('-', '')}`, version = 'v1', targetPath } = {}) {
  return {
    slug,
    port: { slug, title: slug, latest_version: version },
    install: { slug, packageName, ...(targetPath ? { targetPath } : {}) },
    campaigns: null,
    resolved: { installType: 'direct_apk' }
  }
}

function payload(games) {
  return {
    format: 'qgp-questports-payload',
    version: 1,
    generatedAt: '2026-11-11T12:00:00.000Z',
    sources: { database: { rows: games.length } },
    games,
    unmatched: { portsWithoutInstall: [], installWithoutPort: [], campaignsWithoutPort: [] }
  }
}

function recipe(slug, checkedAgainst) {
  return {
    $schema: '../recipe.schema.json',
    format: 'qgp-install-recipe',
    version: 1,
    slug,
    checkedAgainst,
    apks: [{ source: 'url', url: 'https://example.com/a.apk' }],
    inputs: [],
    steps: [{ do: 'install-apk' }],
    notes: 'maintainers only'
  }
}

test('buildCatalog keeps every game and attaches its recipe', () => {
  const catalog = buildCatalog({
    payload: payload([game('alpha'), game('beta')]),
    recipes: [recipe('beta', { packageName: 'com.example.beta', version: 'v1' })],
    generatedAt
  })
  assert.equal(catalog.format, CATALOG_FORMAT)
  assert.equal(catalog.generatedAt, '2026-11-12T10:00:00.000Z')
  assert.deepEqual(catalog.sources.recipes, { count: 1 })
  assert.deepEqual(catalog.games.map(g => g.slug), ['alpha', 'beta'])
  assert.equal(catalog.games[0].recipe, null)
  assert.deepEqual(catalog.games[1].recipe, {
    apks: [{ source: 'url', url: 'https://example.com/a.apk' }],
    inputs: [],
    steps: [{ do: 'install-apk' }]
  })
  assert.deepEqual(catalog.games[1].drift, [])
  assert.equal(catalog.games[1].port.title, 'beta')
  assert.deepEqual(catalog.unmatched.gamesWithoutRecipe, ['alpha'])
  assert.deepEqual(catalog.unmatched.recipesWithoutGame, [])
})

test('buildCatalog lists recipes without a game and leaves them out', () => {
  const catalog = buildCatalog({
    payload: payload([game('alpha')]),
    recipes: [recipe('gone', { packageName: 'com.example.gone', version: 'v1' })],
    generatedAt
  })
  assert.deepEqual(catalog.unmatched.recipesWithoutGame, ['gone'])
  assert.equal(catalog.games.length, 1)
})

test('buildCatalog rejects two recipes with one slug, and a payload of another format', () => {
  const r = recipe('alpha', { packageName: 'com.example.alpha', version: 'v1' })
  assert.throws(
    () => buildCatalog({ payload: payload([game('alpha')]), recipes: [r, r], generatedAt }),
    /Two recipes have the slug "alpha"/
  )
  assert.throws(
    () => buildCatalog({ payload: { ...payload([]), version: 2 }, recipes: [], generatedAt }),
    /not qgp-questports-payload version 1/
  )
})

test('findDrift reports changed and newly set fields', () => {
  const g = game('alpha', { packageName: 'com.example.new', version: 'v2', targetPath: '/sdcard/Alpha/' })
  assert.deepEqual(findDrift(recipe('alpha', { packageName: 'com.example.old', version: 'v1' }), g), [
    { field: 'packageName', recipe: 'com.example.old', payload: 'com.example.new' },
    { field: 'version', recipe: 'v1', payload: 'v2' },
    { field: 'targetPath', recipe: null, payload: '/sdcard/Alpha/' }
  ])
  assert.deepEqual(
    findDrift(recipe('alpha', { packageName: 'com.example.new', version: 'v2', targetPath: '/sdcard/Alpha/' }), g),
    []
  )
})
