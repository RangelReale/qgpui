import assert from 'node:assert/strict'
import { mkdtemp, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { test } from 'node:test'
import { loadCustomGames } from '../src/custom-games.mjs'
import { buildPayload } from '../src/payload.mjs'
import { loadInstallData, localSource } from '../src/repository.mjs'

const fixtureRepo = path.join(path.dirname(fileURLToPath(import.meta.url)), 'fixtures', 'repo')

const sample = (slug = 'qgpui-test') => ({
  format: 'qgpui-custom-game',
  version: 1,
  slug,
  port: { title: 'Test Game', supported_hardware: ['Quest 3'] },
  install: { packageName: 'com.example.test' },
  resolved: { originPlatform: 'pc' },
  notes: 'not copied'
})

async function withFolder(files, run) {
  const dir = await mkdtemp(path.join(tmpdir(), 'qgpui-custom-'))
  try {
    for (const [name, content] of Object.entries(files)) {
      await writeFile(path.join(dir, name), typeof content === 'string' ? content : JSON.stringify(content))
    }
    return await run(dir)
  } finally {
    await rm(dir, { recursive: true, force: true })
  }
}

test('every committed custom game is valid', async () => {
  const { errors } = await loadCustomGames()
  assert.deepEqual(errors, {})
})

test('loadCustomGames reads a game without the file-only keys', async () => {
  await withFolder({ 'qgpui-test.json': sample(), 'README.md': '# not a game' }, async dir => {
    const { games, errors } = await loadCustomGames(dir)
    assert.deepEqual(errors, {})
    assert.deepEqual(games, [{
      slug: 'qgpui-test',
      port: { title: 'Test Game', supported_hardware: ['Quest 3'] },
      install: { packageName: 'com.example.test' },
      campaigns: null,
      resolved: { originPlatform: 'pc' }
    }])
  })
})

test('loadCustomGames has no games when the folder is missing', async () => {
  assert.deepEqual(await loadCustomGames(path.join(tmpdir(), 'qgpui-no-such-folder')), { games: [], errors: {} })
})

test('loadCustomGames rejects a slug without the prefix, a wrong file name and bad JSON', async () => {
  const noPrefix = sample('my-game')
  const withSlug = sample()
  withSlug.port.slug = 'other'
  await withFolder({
    'my-game.json': noPrefix,
    'qgpui-other.json': sample(),
    'qgpui-bad.json': '{',
    'qgpui-test.json': withSlug
  }, async dir => {
    const { games, errors } = await loadCustomGames(dir)
    assert.deepEqual(games, [])
    assert.deepEqual(Object.keys(errors).sort(), ['my-game.json', 'qgpui-bad.json', 'qgpui-other.json', 'qgpui-test.json'])
    assert.match(errors['my-game.json'][0], /\/slug must match pattern/)
    assert.deepEqual(errors['qgpui-other.json'], ['file name qgpui-other.json does not match slug qgpui-test'])
    assert.match(errors['qgpui-bad.json'][0], /not valid JSON/)
    assert.match(errors['qgpui-test.json'][0], /\/port\/slug boolean schema is false/)
  })
})

test('buildPayload adds custom games after the rows', async () => {
  const install = await loadInstallData(localSource(fixtureRepo))
  const payload = buildPayload({
    ports: [{ id: '1', slug: 'no-config', title: 'No Config', category: 'emulator' }],
    install,
    sources: {},
    generatedAt: new Date(),
    customGames: [{
      slug: 'qgpui-test',
      port: { title: 'Test Game', supported_hardware: ['Quest 3'] },
      install: { packageName: 'com.example.test' },
      campaigns: null,
      resolved: { originPlatform: 'pc' }
    }]
  })

  assert.deepEqual(payload.games.map(g => g.slug), ['no-config', 'qgpui-test'])
  const custom = payload.games[1]
  assert.equal(custom.custom, true)
  assert.equal(payload.games[0].custom, undefined)
  assert.deepEqual(custom.port, { title: 'Test Game', supported_hardware: ['Quest 3'], slug: 'qgpui-test' })
  assert.deepEqual(custom.install, { packageName: 'com.example.test' })
  assert.equal(custom.resolved.originPlatform, 'pc')
  assert.deepEqual(custom.resolved.headsets, ['Quest 3'])
  assert.deepEqual(payload.unmatched.portsWithoutInstall, ['no-config'])
})

test('buildPayload rejects a custom slug that is taken or lacks the prefix', async () => {
  const install = await loadInstallData(localSource(fixtureRepo))
  const build = (ports, slug) => () => buildPayload({
    ports, install, sources: {}, generatedAt: new Date(),
    customGames: [{ slug, port: { title: 'T' }, install: null, campaigns: null, resolved: {} }]
  })
  assert.throws(build([{ id: '1', slug: 'qgpui-test', title: 'T' }], 'qgpui-test'), /has the slug of another game/)
  assert.throws(build([], 'game-a'), /must be a plain id that starts with "qgpui-"/)
  assert.throws(build([], 'qgpui-../x'), /must be a plain id/)
})
