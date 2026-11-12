import assert from 'node:assert/strict'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { test } from 'node:test'
import { fetchPorts, findPublishableKey, totalFromContentRange } from '../src/database.mjs'
import { buildPayload, PAYLOAD_FORMAT, PAYLOAD_VERSION } from '../src/payload.mjs'
import { githubSource, loadInstallData, localSource } from '../src/repository.mjs'

const fixtureRepo = path.join(path.dirname(fileURLToPath(import.meta.url)), 'fixtures', 'repo')

test('findPublishableKey finds the key in an inline script', () => {
  const html = '<script>window.__NUXT__={config:{public:{supabase:{key:"sb_publishable_Ab-9_x"}}}}</script>'
  assert.equal(findPublishableKey(html), 'sb_publishable_Ab-9_x')
})

test('findPublishableKey accepts the same key twice', () => {
  assert.equal(findPublishableKey('"sb_publishable_k1" "sb_publishable_k1"'), 'sb_publishable_k1')
})

test('findPublishableKey rejects a page without a key or with two keys', () => {
  assert.throws(() => findPublishableKey('<html></html>'), /No Supabase publishable key/)
  assert.throws(() => findPublishableKey('sb_publishable_a sb_publishable_b'), /2 different/)
})

test('totalFromContentRange reads the total', () => {
  assert.equal(totalFromContentRange('0-144/145'), 145)
  assert.equal(totalFromContentRange('*/0'), 0)
  assert.equal(totalFromContentRange('0-1/*'), null)
  assert.equal(totalFromContentRange(null), null)
})

test('fetchPorts sends the key and fails on a short answer', async () => {
  let request
  const fakeFetch = async (url, init) => {
    request = { url, init }
    return new Response(JSON.stringify([{ slug: 'a' }]), { headers: { 'content-range': '0-0/2' } })
  }
  await assert.rejects(fetchPorts('https://db.example/', 'sb_publishable_k', fakeFetch), /2 rows but the server returned 1/)
  assert.equal(request.url, 'https://db.example/rest/v1/ports?select=*&order=title.asc,slug.asc')
  assert.equal(request.init.headers.apikey, 'sb_publishable_k')
  assert.equal(request.init.headers.Authorization, 'Bearer sb_publishable_k')
})

test('loadInstallData runs computed data and follows ~/ and relative imports', async () => {
  const install = await loadInstallData(localSource(fixtureRepo))
  const configs = install.PORT_PACKAGE_CONFIGS
  assert.deepEqual(Object.keys(configs), ['game-a', 'game-direct', 'config-only'])
  assert.deepEqual(configs['game-a'].folders, [
    { id: 'lang_english', targetPath: '/sdcard/languages/English/', required: true },
    { id: 'lang_french', targetPath: '/sdcard/languages/French/', required: false }
  ])
  assert.deepEqual(install.PORT_EXPANSIONS['game-a'].map(c => [c.id, c.clean]), [['english', true], ['french', true]])
})

test('githubSource reads raw files at a commit and maps 404 to missing', async () => {
  const requested = []
  const fakeFetch = async url => {
    requested.push(url)
    return url.endsWith('/here.ts') ? new Response('x') : new Response('', { status: 404 })
  }
  const source = githubSource('owner/name', 'abc123', fakeFetch)
  assert.equal(await source.read('app/here.ts'), 'x')
  assert.equal(await source.read('app/missing.ts'), null)
  assert.deepEqual(requested, [
    'https://raw.githubusercontent.com/owner/name/abc123/app/here.ts',
    'https://raw.githubusercontent.com/owner/name/abc123/app/missing.ts'
  ])
  assert.deepEqual(source.describe(), { kind: 'github', repo: 'owner/name', commit: 'abc123' })
})

test('buildPayload joins rows with install data by slug', async () => {
  const install = await loadInstallData(localSource(fixtureRepo))
  const ports = [
    { id: '1', slug: 'game-a', title: 'Game A', category: 'source_port' },
    { id: '2', slug: 'game-direct', title: 'Game Direct', category: 'source_port' },
    { id: '3', slug: 'no-config', title: 'No Config', category: 'emulator' }
  ]
  const payload = buildPayload({
    ports,
    install,
    sources: { database: { rows: 3 } },
    generatedAt: new Date('2026-11-11T12:00:00Z')
  })

  assert.equal(payload.format, PAYLOAD_FORMAT)
  assert.equal(payload.version, PAYLOAD_VERSION)
  assert.equal(payload.generatedAt, '2026-11-11T12:00:00.000Z')
  assert.deepEqual(payload.games.map(g => g.slug), ['game-a', 'game-direct', 'no-config'])

  const [a, direct, noConfig] = payload.games
  assert.equal(a.port, ports[0])
  assert.equal(a.install.packageName, 'com.example.a')
  assert.equal(a.campaigns.length, 2)
  assert.deepEqual(a.resolved, {
    installType: 'apk_and_assets',
    workflowType: 'pc_assets',
    requiresExternalFiles: true,
    selfContainedSideload: false
  })

  assert.equal(direct.campaigns, null)
  assert.equal(direct.resolved.selfContainedSideload, true)

  assert.equal(noConfig.install, null)
  assert.equal(noConfig.resolved.installType, 'pc_builder_required')
  assert.equal(noConfig.resolved.workflowType, 'emulator_roms')

  assert.deepEqual(payload.unmatched, {
    portsWithoutInstall: ['no-config'],
    installWithoutPort: ['config-only'],
    campaignsWithoutPort: ['campaign-only']
  })
})

test('buildPayload rejects duplicate or missing slugs', async () => {
  const install = await loadInstallData(localSource(fixtureRepo))
  const build = ports => buildPayload({ ports, install, sources: {}, generatedAt: new Date() })
  assert.throws(() => build([{ slug: 'x' }, { slug: 'x' }]), /Two ports rows have the slug "x"/)
  assert.throws(() => build([{ id: '7' }]), /no slug \(id 7\)/)
})
