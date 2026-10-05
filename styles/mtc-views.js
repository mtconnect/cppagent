//
// Copyright 2009-2026, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

// Views: neutral tree in, DOM out. Nothing here knows whether the data arrived as XML or JSON.
import { h } from './mtc-dom.js'

const INDENT_EM = 1.1

// Rows that were collapsed by the user. The keys identify a node by its path, so the choice
// survives the re-render that autorefresh does.
const collapsed = new Set()

// Pre-order walk that reports the depth and a stable key for each node, like the stylesheet's `//*`
function* walk(node, depth = 0, key = node.name) {
  yield { node, depth, key }
  const seen = new Map()
  for (const child of node.children) {
    const a = child.attrs
    const id = child.name + ':' + (a.id || a.dataItemId || a.assetId || a.uuid || a.key || a.name || '')
    const n = seen.get(id) || 0
    seen.set(id, n + 1)
    yield* walk(child, depth + 1, key + '/' + id + (n ? '#' + n : ''))
  }
}

// Rows carry their depth, so hiding a branch hides the rows below it until one is as shallow
function rowAttrs(depth, key, cls) {
  return { class: cls || false, 'data-depth': depth, 'data-key': key }
}

function elementCell(label, depth, hasChildren) {
  const marker = hasChildren
    ? h(
        'button',
        {
          type: 'button',
          class: 'mtc-branch',
          'aria-expanded': 'true',
          'aria-label': label ? false : 'Entry',
          onclick: toggle,
        },
        label
      )
    : h('span', { class: 'mtc-leaf' }, label)
  const cell = h('td', {}, marker)
  cell.style.paddingLeft = depth * INDENT_EM + 0.75 + 'em'
  return cell
}

function toggle(event) {
  const row = event.currentTarget.closest('tr')
  const key = row.dataset.key
  if (!collapsed.delete(key)) collapsed.add(key)
  applyCollapsed(row.parentElement)
}

function applyCollapsed(body) {
  let hideBelow = Infinity
  for (const row of body.rows) {
    const depth = Number(row.dataset.depth)
    if (depth > hideBelow) {
      row.hidden = true
      continue
    }
    hideBelow = Infinity
    row.hidden = false
    const button = row.querySelector('.mtc-branch')
    if (button) {
      const closed = collapsed.has(row.dataset.key)
      button.setAttribute('aria-expanded', String(!closed))
      if (closed) hideBelow = depth
    }
  }
}

// The attribute subtables belong to the row above them and sit one level deeper
function detail(depth, row) {
  row.dataset.depth = depth + 1
  return row
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
  const body = h('tbody', {}, rows)
  applyCollapsed(body)
  return h(
    'table',
    { class: 'table table-hover' },
    h('thead', {}, h('tr', {}, headings.map(t => h('th', {}, t)))),
    body
  )
}

// ---- Probe (MTConnectDevices) ----

const PROBE_BOLD = new Set(['Header', 'Agent', 'Device'])

function probeRows(root) {
  const rows = []
  for (const { node, depth, key } of walk(root)) {
    const a = node.attrs
    const value = node.name === 'Unavailable' ? 'UNAVAILABLE' : node.text
    rows.push(
      h(
        'tr',
        rowAttrs(depth, key, PROBE_BOLD.has(node.name) && 'mtc-bold'),
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
    if (node.name === 'Header') rows.push(detail(depth, attrTable(7, node)))
    else if (node.name === 'Agent') {
      rows.push(detail(depth, subtable(3, ['uuid', 'mtconnectVersion'], [a.uuid, a.mtconnectVersion])))
    } else if (node.name === 'Device') {
      rows.push(detail(depth, subtable(3, ['uuid', 'sampleInterval'], [a.uuid, a.sampleInterval])))
    } else if (node.name === 'Description') rows.push(detail(depth, attrTable(7, node, true)))
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

// A condition's text is its message, shown after the level: "FAULT: Feed stalled"
function streamDisplay(node, value) {
  const isCondition = node.name in CONDITION_LEVELS || (node.name === 'Condition' && node.attrs.level)
  return isCondition && node.text ? value + ': ' + node.text : value
}

function streamRows(root) {
  const rows = []
  for (const { node, depth, key } of walk(root)) {
    const label = streamLabel(node)
    const value = streamValue(node)
    const sequence = node.name === 'Entry' ? node.attrs.key : node.attrs.sequence
    const timestamp = node.attrs.timestamp
    rows.push(
      h(
        'tr',
        rowAttrs(depth, key, STREAM_BOLD.has(label) && 'mtc-bold'),
        elementCell(label, depth, node.children.length > 0),
        h('td', {}, node.attrs.dataItemId),
        h('td', {}, node.attrs.name),
        h('td', {}, timestamp && h('span', { title: timestamp }, timestamp.substring(11, 21))),
        h('td', { class: node.name === 'Entry' ? 'mtc-right' : false }, sequence),
        h('td', { class: 'mtc-value-' + value.replace(/[^A-Za-z]/g, '') }, streamDisplay(node, value))
      )
    )
    if (node.name === 'Header') rows.push(detail(depth, attrTable(6, node)))
  }
  return table(['Element', 'Id', 'Name', 'Timestamp', 'Sequence', 'Value'], rows)
}

// ---- Assets (MTConnectAssets) ----

function assetRows(root) {
  const rows = []
  for (const { node, depth, key } of walk(root)) {
    const a = node.attrs
    rows.push(
      h(
        'tr',
        rowAttrs(depth, key, node.name === 'Header' && 'mtc-bold'),
        elementCell(node.name, depth, node.children.length > 0),
        h('td', {}, a.assetId || a.id),
        h('td', {}, a.name),
        h('td', {}, a.type),
        h('td', {}, a.timestamp),
        h('td', {}, node.text)
      )
    )
    if (node.name === 'Header') rows.push(detail(depth, attrTable(6, node)))
  }
  return table(['Element', 'Id', 'Name', 'Type', 'Timestamp', 'Value'], rows)
}

// ---- Error (MTConnectError) ----

function errorRows(root) {
  const rows = []
  for (const { node, depth, key } of walk(root)) {
    rows.push(
      h(
        'tr',
        rowAttrs(depth, key, node.name === 'Header' && 'mtc-bold'),
        elementCell(node.name, depth, node.children.length > 0),
        h('td', {}, node.attrs.errorCode),
        h('td', {}, node.text)
      )
    )
    if (node.name === 'Header') rows.push(detail(depth, attrTable(3, node)))
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
