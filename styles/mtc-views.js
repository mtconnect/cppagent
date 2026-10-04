// Views: neutral tree in, DOM out. Nothing here knows whether the data arrived as XML or JSON.
import { h } from './mtc-dom.js'

const INDENT_EM = 1.1

// Pre-order walk that reports the depth of each node, like the stylesheet's `//*`
function* walk(node, depth = 0) {
  yield { node, depth }
  for (const child of node.children) yield* walk(child, depth + 1)
}

function elementCell(label, depth, hasChildren) {
  const cell = h('td', {}, h('span', { class: hasChildren ? 'mtc-branch' : 'mtc-leaf' }, label))
  cell.style.paddingLeft = depth * INDENT_EM + 0.75 + 'em'
  return cell
}

function subtable(span, headings, values) {
  return h(
    'tr',
    {},
    h(
      'td',
      { colspan: span },
      h(
        'div',
        { class: 'mtc-scroll' },
        h(
          'table',
          { class: 'subtable' },
          h('thead', {}, h('tr', {}, headings.map(t => h('th', {}, t)))),
          h('tbody', {}, h('tr', {}, values.map(v => h('td', {}, v))))
        )
      )
    )
  )
}

function attrTable(span, node, withText = false) {
  const names = Object.keys(node.attrs)
  const values = names.map(n => node.attrs[n])
  if (withText) {
    names.push('value')
    values.push(node.text)
  }
  return subtable(span, names, values)
}

function table(headings, rows) {
  return h(
    'table',
    { class: 'table table-hover' },
    h('thead', {}, h('tr', {}, headings.map(t => h('th', {}, t)))),
    h('tbody', {}, rows)
  )
}

// ---- Probe (MTConnectDevices) ----

const PROBE_BOLD = new Set(['Header', 'Agent', 'Device'])

function probeRows(root) {
  const rows = []
  for (const { node, depth } of walk(root)) {
    const a = node.attrs
    const value = node.name === 'Unavailable' ? 'UNAVAILABLE' : node.text
    rows.push(
      h(
        'tr',
        { class: PROBE_BOLD.has(node.name) ? 'mtc-bold' : false },
        elementCell(node.name, depth, node.children.length > 0),
        h('td', {}, a.id),
        h('td', {}, a.name),
        h('td', {}, a.category),
        h('td', {}, a.type),
        h('td', {}, a.subType),
        h('td', {}, value),
        h('td', {}, a.units)
      )
    )
    if (node.name === 'Header') rows.push(attrTable(7, node))
    else if (node.name === 'Agent') {
      rows.push(subtable(3, ['uuid', 'mtconnectVersion'], [a.uuid, a.mtconnectVersion]))
    } else if (node.name === 'Device') {
      rows.push(subtable(3, ['uuid', 'sampleInterval'], [a.uuid, a.sampleInterval]))
    } else if (node.name === 'Description') rows.push(attrTable(7, node, true))
  }
  return table(
    ['Element', 'Id', 'Name', 'Category', 'Type', 'SubType', 'Value', 'Units'],
    rows
  )
}

// ---- Streams (MTConnectStreams) ----

const STREAM_BOLD = new Set(['Header', 'DeviceStream', 'Samples', 'Events', 'Conditions'])
const CONDITION_LEVELS = { Normal: 'NORMAL', Warning: 'WARNING', Fault: 'FAULT', Unavailable: 'UNAVAILABLE' }

// Conditions are shown as Condition rows with a level, like the XSL stylesheet does
function streamLabel(node) {
  if (node.name === 'Condition' && node.children.length > 0) return 'Conditions'
  if (node.name in CONDITION_LEVELS) return 'Condition'
  if (node.name === 'Entry') return ''
  return node.name
}

function streamValue(node) {
  if (node.name in CONDITION_LEVELS) return CONDITION_LEVELS[node.name]
  // JSON conditions carry the level as an attribute instead of an element name
  if (node.name === 'Condition' && node.attrs.level) return node.attrs.level.toUpperCase()
  return node.text
}

function streamRows(root) {
  const rows = []
  for (const { node, depth } of walk(root)) {
    const label = streamLabel(node)
    const value = streamValue(node)
    const sequence = node.name === 'Entry' ? node.attrs.key : node.attrs.sequence
    const timestamp = node.attrs.timestamp
    rows.push(
      h(
        'tr',
        { class: STREAM_BOLD.has(label) ? 'mtc-bold' : false },
        elementCell(label, depth, node.children.length > 0),
        h('td', {}, node.attrs.dataItemId),
        h('td', {}, node.attrs.name),
        h('td', {}, timestamp && h('span', { title: timestamp }, timestamp.substring(11, 21))),
        h('td', { class: node.name === 'Entry' ? 'mtc-right' : false }, sequence),
        h('td', { class: 'mtc-value-' + value.replace(/[^A-Za-z]/g, '') }, value)
      )
    )
    if (node.name === 'Header') rows.push(attrTable(6, node))
  }
  return table(['Element', 'Id', 'Name', 'Timestamp', 'Sequence', 'Value'], rows)
}

// ---- Assets (MTConnectAssets) ----

function assetRows(root) {
  const rows = []
  for (const { node, depth } of walk(root)) {
    const a = node.attrs
    rows.push(
      h(
        'tr',
        { class: node.name === 'Header' ? 'mtc-bold' : false },
        elementCell(node.name, depth, node.children.length > 0),
        h('td', {}, a.assetId || a.id),
        h('td', {}, a.name),
        h('td', {}, a.type),
        h('td', {}, a.timestamp),
        h('td', {}, node.text)
      )
    )
    if (node.name === 'Header') rows.push(attrTable(6, node))
  }
  return table(['Element', 'Id', 'Name', 'Type', 'Timestamp', 'Value'], rows)
}

// ---- Error (MTConnectError) ----

function errorRows(root) {
  const rows = []
  for (const { node, depth } of walk(root)) {
    rows.push(
      h(
        'tr',
        { class: node.name === 'Header' ? 'mtc-bold' : false },
        elementCell(node.name, depth, node.children.length > 0),
        h('td', {}, node.attrs.errorCode),
        h('td', {}, node.text)
      )
    )
    if (node.name === 'Header') rows.push(attrTable(3, node))
  }
  return table(['Element', 'Error Code', 'Message'], rows)
}

const VIEWS = {
  MTConnectDevices: probeRows,
  MTConnectStreams: streamRows,
  MTConnectAssets: assetRows,
  MTConnectError: errorRows,
}

export function renderDocument(root) {
  const view = VIEWS[root.name]
  if (!view) throw new Error('Unknown MTConnect document: ' + root.name)
  return view(root)
}
