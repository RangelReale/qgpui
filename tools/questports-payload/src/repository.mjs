// Loads the per-game install data from the QuestPorts source code.
// See docs/questports-data-sources.md, "Required game files".
//
// The data is TypeScript, and part of it is computed (for example MGS_DISC_LANGUAGES.map(...)), so
// it cannot be read as text. Instead, esbuild bundles the data files into one JavaScript module,
// which is then imported. The files come from GitHub at a fixed commit, or from a local checkout.

import { execFileSync } from 'node:child_process'
import { readFile, stat } from 'node:fs/promises'
import path from 'node:path'
import * as esbuild from 'esbuild'

export const DEFAULT_REPO = 'felipevc13/questports'

/** The module esbuild bundles. Its exports are what the payload uses. */
const ENTRY = `
export {
  PORT_PACKAGE_CONFIGS,
  resolveInstallType,
  resolveWorkflowType,
  portRequiresExternalFiles,
  isSelfContainedSideload
} from '~/data/portPackageMap'
export { PORT_EXPANSIONS } from '~/data/expansions'
`

const NAMESPACE = 'questports'
const EXTENSIONS = ['', '.ts', '.js', '.mjs', '/index.ts', '/index.js']

/**
 * A source of repository files. `read(relativePath)` returns the file's text, or null when the
 * file does not exist. `describe()` returns what the payload records about the source.
 */
export function githubSource(repo, commit, fetchImpl = fetch) {
  return {
    async read(relativePath) {
      const url = `https://raw.githubusercontent.com/${repo}/${commit}/${relativePath}`
      const response = await fetchImpl(url)
      if (response.status === 404) return null
      if (!response.ok) {
        throw new Error(`GET ${url} failed: ${response.status} ${response.statusText}`)
      }
      return response.text()
    },
    describe: () => ({ kind: 'github', repo, commit })
  }
}

export function localSource(root) {
  const absolute = path.resolve(root)
  return {
    async read(relativePath) {
      const file = path.join(absolute, ...relativePath.split('/'))
      try {
        if (!(await stat(file)).isFile()) return null
        return await readFile(file, 'utf8')
      } catch (err) {
        if (err.code === 'ENOENT') return null
        throw err
      }
    },
    describe: () => ({ kind: 'local', path: absolute, commit: gitHead(absolute) })
  }
}

function gitHead(dir) {
  try {
    return execFileSync('git', ['-C', dir, 'rev-parse', 'HEAD'], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim()
  } catch {
    return null
  }
}

/** Resolves a branch, tag or commit of a GitHub repository to a full commit SHA. */
export async function resolveGithubCommit(repo, ref, fetchImpl = fetch) {
  const headers = { Accept: 'application/vnd.github+json', 'User-Agent': 'questports-payload' }
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`

  let wanted = ref
  if (!wanted) {
    const info = await getJson(`https://api.github.com/repos/${repo}`, headers, fetchImpl)
    wanted = info.default_branch
  }
  const commit = await getJson(`https://api.github.com/repos/${repo}/commits/${encodeURIComponent(wanted)}`, headers, fetchImpl)
  return { ref: wanted, commit: commit.sha }
}

async function getJson(url, headers, fetchImpl) {
  const response = await fetchImpl(url, { headers })
  if (!response.ok) {
    throw new Error(`GET ${url} failed: ${response.status} ${response.statusText}`)
  }
  return response.json()
}

/**
 * Bundles and runs the install data files from `source`. Returns the module's exports:
 * PORT_PACKAGE_CONFIGS, PORT_EXPANSIONS and the resolve helpers.
 */
export async function loadInstallData(source) {
  const result = await esbuild.build({
    stdin: { contents: ENTRY, loader: 'ts', resolveDir: '/', sourcefile: 'entry.ts' },
    bundle: true,
    write: false,
    format: 'esm',
    platform: 'neutral',
    target: 'es2022',
    logLevel: 'silent',
    plugins: [repositoryPlugin(source)]
  })
  const code = result.outputFiles[0].text
  return import(`data:text/javascript;base64,${Buffer.from(code).toString('base64')}`)
}

/** Resolves the site's imports (`~/...` and relative paths) to files in `source`. */
function repositoryPlugin(source) {
  return {
    name: 'questports-repository',
    setup(build) {
      build.onResolve({ filter: /.*/ }, async args => {
        const base = repositoryPath(args)
        if (base === null) {
          return { errors: [{ text: `Cannot load "${args.path}": only ~/ and relative imports are supported.` }] }
        }
        for (const extension of EXTENSIONS) {
          const candidate = base + extension
          const contents = await source.read(candidate)
          if (contents !== null) {
            return { path: candidate, namespace: NAMESPACE, pluginData: { contents } }
          }
        }
        return { errors: [{ text: `"${args.path}" not found in the repository (looked for ${base}).` }] }
      })
      build.onLoad({ filter: /.*/, namespace: NAMESPACE }, args => ({
        contents: args.pluginData.contents,
        loader: args.path.endsWith('.ts') ? 'ts' : 'js'
      }))
    }
  }
}

/** Returns the repository-relative path an import names, without an extension, or null. */
function repositoryPath(args) {
  if (args.path.startsWith('~/')) return `app/${args.path.slice(2)}`
  if (args.path.startsWith('./') || args.path.startsWith('../')) {
    if (args.namespace !== NAMESPACE) return null
    return path.posix.normalize(path.posix.join(path.posix.dirname(args.importer), args.path))
  }
  return null
}
