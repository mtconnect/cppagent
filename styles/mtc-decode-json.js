// Decode an MTConnect JSON document into the same neutral tree the XML decoder produces:
//   { name, attrs: {name: value}, text, children: [node] }
//
// The agent has two JSON serializations, chosen by the `JsonVersion` config option:
//   v1  collections are arrays of single-key objects:  "Events": [ {"Execution": {...}}, ... ]
//   v2  collections group entities by type:            "Events": { "Execution": [ {...}, ... ] }
// Scalar members are attributes, except `value`, which is the element text. A scalar or array of
// scalars under an UpperCamelCase key is a child element with text, like <Axis>0 0 1</Axis>,
// because MTConnect names elements in UpperCamelCase and attributes in lowerCamelCase.
// Namespace prefixes on element names (x:Application) are dropped to match the XML local names.
// The shape rules were derived from the agent's JSON printer (entity/json_printer.hpp) and
// checked against live agent output for JsonVersion 2. JsonVersion 1 is not yet checked.

export function decodeJson(text) {
  const doc = JSON.parse(text)
  // The root sits next to a "$schema" member
  const root = Object.keys(doc).find(k => k.startsWith('MTConnect') && isObject(doc[k]))
  if (!root) throw new Error('Response is not an MTConnect JSON document')
  return entity(root, doc[root])
}

const isScalar = v => v === null || ['string', 'number', 'boolean'].includes(typeof v)
const isObject = v => typeof v === 'object' && v !== null && !Array.isArray(v)

function node(name) {
  return { name: name.slice(name.indexOf(':') + 1), attrs: {}, text: '', children: [] }
}

const isElementName = key => /^[A-Z]/.test(key.slice(key.indexOf(':') + 1))

function textElement(name, text) {
  const el = node(name)
  el.text = text
  return el
}

// v2 collection: an object whose members are all arrays of entities, one array per entity type.
// Arrays of numbers, like Rotation and Translation, are values and not collections.
function isGrouped(value) {
  const members = Object.values(value)
  return (
    members.length > 0 && members.every(m => Array.isArray(m) && m.length > 0 && m.every(isObject))
  )
}

// v1 collection item: {"Execution": {...}} with nothing else in the object
function wrapped(item) {
  if (!isObject(item)) return null
  const keys = Object.keys(item)
  return keys.length === 1 && isObject(item[keys[0]]) ? keys[0] : null
}

function entity(name, value) {
  const el = node(name)
  if (isScalar(value)) {
    el.text = value === null ? '' : String(value)
    return el
  }

  for (const [key, member] of Object.entries(value)) {
    if (member === null || member === undefined) continue

    if (key === 'value') {
      if (isScalar(member)) el.text = String(member)
      else if (Array.isArray(member)) el.text = member.join(' ')
      else el.children.push(...dataSet(member))
    } else if (isScalar(member)) {
      if (isElementName(key)) el.children.push(textElement(key, String(member)))
      else el.attrs[key] = String(member)
    } else if (Array.isArray(member) && member.every(isScalar)) {
      const text = member.join(' ')
      if (isElementName(key)) el.children.push(textElement(key, text))
      else el.attrs[key] = text
    } else if (Array.isArray(member)) {
      el.children.push(...collection(key, member))
    } else if (isGrouped(member)) {
      el.children.push(grouped(key, member))
    } else {
      el.children.push(entity(key, member))
    }
  }
  return el
}

function grouped(name, value) {
  const container = node(name)
  for (const [type, items] of Object.entries(value)) {
    for (const item of items) container.children.push(entity(type, item))
  }
  return container
}

function collection(name, items) {
  const names = items.map(wrapped)
  if (items.length > 0 && names.every(n => n !== null)) {
    // v1: the array is a container and each item names its own entity
    const container = node(name)
    items.forEach((item, i) => container.children.push(entity(names[i], item[names[i]])))
    return [container]
  }
  // repeated entities that share the key's name
  return items.map(item => entity(name, item))
}

// Data sets and tables become Entry elements, like <Entry key="..."> in XML
function dataSet(value) {
  return Object.entries(value).map(([key, member]) => {
    const entry = node('Entry')
    entry.attrs.key = key
    if (isScalar(member)) entry.text = String(member)
    else entry.children.push(...dataSet(member))
    return entry
  })
}
