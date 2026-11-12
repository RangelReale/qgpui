#!/usr/bin/env node
// Builds the QuestPorts payload: one JSON file with every game from the database, joined with its
// install requirements from the source repository. See README.md.

import { mkdir, writeFile } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { parseArgs } from 'node:util'
import { DEFAULT_SITE_URL, DEFAULT_SUPABASE_URL, PORTS_TABLE, fetchPorts, fetchPublishableKey } from './database.mjs'
import { buildPayload } from './payload.mjs'
import { DEFAULT_REPO, githubSource, loadInstallData, localSource, resolveGithubCommit } from './repository.mjs'

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..')
const DEFAULT_OUT = path.join(repositoryRoot, 'local', 'questports-payload.json')

const USAGE = `Usage: node src/main.mjs [options]

Options:
  --out <file>           Output file (default: local/questports-payload.json in the repository).
  --repo <owner/name>    GitHub repository with the install data (default: ${DEFAULT_REPO}).
  --ref <ref>            Branch, tag or commit in --repo (default: the default branch).
  --source <dir>         Read the install data from a local checkout instead of GitHub.
  --key <key>            Supabase publishable key. Default: QUESTPORTS_SUPABASE_KEY, or else the
                         key found on the site's front page.
  --supabase-url <url>   Supabase project URL (default: ${DEFAULT_SUPABASE_URL}).
  --site <url>           Site to read the publishable key from (default: ${DEFAULT_SITE_URL}).
  -h, --help             Show this help.

Environment:
  QUESTPORTS_SUPABASE_KEY  Supabase publishable key.
  GITHUB_TOKEN             Optional; raises the GitHub API rate limit.`

async function main() {
  const { values } = parseArgs({
    options: {
      out: { type: 'string', default: DEFAULT_OUT },
      repo: { type: 'string', default: DEFAULT_REPO },
      ref: { type: 'string' },
      source: { type: 'string' },
      key: { type: 'string' },
      'supabase-url': { type: 'string', default: DEFAULT_SUPABASE_URL },
      site: { type: 'string', default: DEFAULT_SITE_URL },
      help: { type: 'boolean', short: 'h', default: false }
    },
    strict: true
  })
  if (values.help) {
    console.log(USAGE)
    return
  }
  if (values.source && values.ref) {
    throw new Error('--ref applies to GitHub only; check out the ref in the --source folder instead.')
  }

  let source
  let repositoryInfo
  if (values.source) {
    source = localSource(values.source)
    repositoryInfo = source.describe()
    log(`Install data: local checkout ${repositoryInfo.path} (commit ${repositoryInfo.commit ?? 'unknown'})`)
  } else {
    const { ref, commit } = await resolveGithubCommit(values.repo, values.ref)
    source = githubSource(values.repo, commit)
    repositoryInfo = { ...source.describe(), ref }
    log(`Install data: ${values.repo} ${ref} (commit ${commit})`)
  }
  const install = await loadInstallData(source)
  log(`  ${Object.keys(install.PORT_PACKAGE_CONFIGS).length} install configs, ${Object.keys(install.PORT_EXPANSIONS).length} games with campaigns`)

  let key = values.key || process.env.QUESTPORTS_SUPABASE_KEY
  if (!key) {
    log(`Reading the publishable key from ${values.site}`)
    key = await fetchPublishableKey(values.site)
  }
  log(`Game list: ${values['supabase-url']}, table ${PORTS_TABLE}`)
  const ports = await fetchPorts(values['supabase-url'], key)
  log(`  ${ports.length} games`)

  const payload = buildPayload({
    ports,
    install,
    sources: {
      database: { url: values['supabase-url'], table: PORTS_TABLE, rows: ports.length },
      repository: repositoryInfo
    },
    generatedAt: new Date()
  })

  const { portsWithoutInstall, installWithoutPort, campaignsWithoutPort } = payload.unmatched
  if (portsWithoutInstall.length) log(`Warning: games without an install config: ${portsWithoutInstall.join(', ')}`)
  if (installWithoutPort.length) log(`Warning: install configs without a game: ${installWithoutPort.join(', ')}`)
  if (campaignsWithoutPort.length) log(`Warning: campaigns without a game: ${campaignsWithoutPort.join(', ')}`)

  const out = path.resolve(values.out)
  await mkdir(path.dirname(out), { recursive: true })
  await writeFile(out, JSON.stringify(payload, null, 2) + '\n', 'utf8')
  log(`Wrote ${payload.games.length} games to ${out}`)
}

function log(message) {
  process.stderr.write(message + '\n')
}

main().catch(err => {
  log(`error: ${err.message}`)
  process.exitCode = 1
})
