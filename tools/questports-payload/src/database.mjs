// Reads the game list from the QuestPorts Supabase database.
// See docs/questports-data-sources.md, "Option 1: Supabase REST API".

export const DEFAULT_SITE_URL = 'https://questports.com'
export const DEFAULT_SUPABASE_URL = 'https://ccjteoxolasldhfgnoyx.supabase.co'
export const PORTS_TABLE = 'ports'

const PUBLISHABLE_KEY = /sb_publishable_[A-Za-z0-9_-]+/g

/**
 * Returns the publishable key found in a page's HTML. The site puts it in an inline script.
 * Throws when the page holds no key, or more than one different key.
 */
export function findPublishableKey(html) {
  const keys = [...new Set(html.match(PUBLISHABLE_KEY) || [])]
  if (keys.length === 0) {
    throw new Error('No Supabase publishable key (sb_publishable_...) found in the page.')
  }
  if (keys.length > 1) {
    throw new Error(`The page holds ${keys.length} different publishable keys; pass one with --key.`)
  }
  return keys[0]
}

/** Downloads the site's front page and returns the publishable key in it. */
export async function fetchPublishableKey(siteUrl, fetchImpl = fetch) {
  const response = await fetchImpl(siteUrl)
  if (!response.ok) {
    throw new Error(`GET ${siteUrl} failed: ${response.status} ${response.statusText}`)
  }
  return findPublishableKey(await response.text())
}

/**
 * Returns every row of the ports table, all columns, ordered by title and then slug.
 * Throws when the server reports more rows than it returned.
 */
export async function fetchPorts(supabaseUrl, key, fetchImpl = fetch) {
  const url = `${supabaseUrl.replace(/\/+$/, '')}/rest/v1/${PORTS_TABLE}?select=*&order=title.asc,slug.asc`
  const response = await fetchImpl(url, {
    headers: {
      apikey: key,
      Authorization: `Bearer ${key}`,
      Accept: 'application/json',
      Prefer: 'count=exact'
    }
  })
  if (!response.ok) {
    const body = await response.text()
    throw new Error(`GET ${url} failed: ${response.status} ${response.statusText}: ${body}`)
  }
  const rows = await response.json()
  if (!Array.isArray(rows)) {
    throw new Error(`GET ${url} did not return a JSON array.`)
  }
  const total = totalFromContentRange(response.headers.get('content-range'))
  if (total !== null && total !== rows.length) {
    throw new Error(`The ${PORTS_TABLE} table has ${total} rows but the server returned ${rows.length}.`)
  }
  return rows
}

/** Reads the total from a PostgREST Content-Range header such as "0-144/145". */
export function totalFromContentRange(header) {
  const match = /\/(\d+)$/.exec(header || '')
  return match ? Number(match[1]) : null
}
