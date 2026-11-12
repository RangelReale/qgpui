// Reads and checks QGPUI install recipes (data/recipes/<slug>.json). See docs/architecture/install-recipes.md.

import { readFile, readdir } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import Ajv2020 from 'ajv/dist/2020.js'

const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..')
export const DEFAULT_RECIPES_DIR = path.join(repositoryRoot, 'data', 'recipes')
export const DEFAULT_SCHEMA_FILE = path.join(repositoryRoot, 'data', 'recipe.schema.json')

/** Returns a function that checks one recipe against the JSON Schema and returns a list of errors. */
export async function loadSchemaValidator(schemaFile = DEFAULT_SCHEMA_FILE) {
  const schema = JSON.parse(await readFile(schemaFile, 'utf8'))
  // strictRequired would reject `anyOf: [{ required: [...] }]`, which the schema uses for "one of these keys".
  const ajv = new Ajv2020({ allErrors: true, strict: true, strictRequired: false, discriminator: true })
  const validate = ajv.compile(schema)
  return (recipe) =>
    validate(recipe) ? [] : validate.errors.map(e => `${e.instancePath || '/'} ${e.message}`)
}

/** Checks what the schema cannot express. Returns a list of errors. */
export function checkRecipe(recipe, fileName) {
  const errors = []
  if (fileName !== undefined && fileName !== `${recipe.slug}.json`) {
    errors.push(`file name ${fileName} does not match slug ${recipe.slug}`)
  }

  for (const [i, apk] of (recipe.apks ?? []).entries()) {
    for (const key of ['tag', 'asset', 'inZip']) {
      if (apk[key] !== undefined) checkRegex(apk[key], `/apks/${i}/${key}`, errors)
    }
  }

  const inputs = new Map()
  for (const input of recipe.inputs ?? []) {
    if (inputs.has(input.id)) errors.push(`input id ${input.id} is used twice`)
    inputs.set(input.id, input)
    if (input.multiple && !input.extensions) errors.push(`input ${input.id} has multiple but no extensions`)
    if (input.group && input.required !== false) errors.push(`input ${input.id} is in group ${input.group} but required`)
  }
  const groups = new Map()
  for (const input of inputs.values()) {
    if (input.group) groups.set(input.group, (groups.get(input.group) ?? 0) + 1)
  }
  for (const [group, count] of groups) {
    if (count < 2) errors.push(`group ${group} has only one input`)
  }

  const pushed = new Set()
  for (const [i, step] of (recipe.steps ?? []).entries()) {
    if (step.do !== 'push') continue
    const input = inputs.get(step.input)
    if (!input) {
      errors.push(`/steps/${i} pushes unknown input ${step.input}`)
      continue
    }
    pushed.add(input.id)
    if (input.kind === 'file' && input.names) {
      const names = input.names.map(n => n.toLowerCase())
      for (const [j, m] of step.map.entries()) {
        if (m.from !== undefined && !names.includes(m.from.toLowerCase())) {
          errors.push(`/steps/${i}/map/${j} from ${m.from} is not one of input ${input.id}'s names`)
        }
      }
    }
  }
  for (const id of inputs.keys()) {
    if (!pushed.has(id)) errors.push(`input ${id} is never pushed`)
  }

  const steps = recipe.steps ?? []
  const installAt = steps.findIndex(s => s.do === 'install-apk')
  if (installAt === -1) errors.push('no install-apk step')
  const needsApp = new Set(['grant', 'launch'])
  for (const [i, step] of steps.entries()) {
    if (needsApp.has(step.do) && (installAt === -1 || i < installAt)) {
      errors.push(`/steps/${i} (${step.do}) comes before install-apk`)
    }
  }
  return errors
}

/** Reads every recipe in a folder. Returns { recipes, errors }, where errors maps a file name to its errors. */
export async function loadRecipes(dir = DEFAULT_RECIPES_DIR, schemaFile = DEFAULT_SCHEMA_FILE) {
  const validate = await loadSchemaValidator(schemaFile)
  const files = (await readdir(dir)).filter(f => f.endsWith('.json')).sort()
  const recipes = []
  const errors = {}
  for (const file of files) {
    let recipe
    try {
      recipe = JSON.parse(await readFile(path.join(dir, file), 'utf8'))
    } catch (e) {
      errors[file] = [`not valid JSON: ${e.message}`]
      continue
    }
    const found = [...validate(recipe), ...checkRecipe(recipe, file)]
    if (found.length) errors[file] = found
    else recipes.push(recipe)
  }
  return { recipes, errors }
}

function checkRegex(source, where, errors) {
  try {
    new RegExp(source)
  } catch (e) {
    errors.push(`${where} is not a valid regular expression: ${e.message}`)
  }
}
