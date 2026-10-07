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

// Page controller: picks the view from the URL, reads the document the agent embedded in the
// page, and renders it. Autorefresh requests the page again and renders the document in it. The
// format dropdown only applies to the Raw dialog, which requests the document in that format.
import { h } from './mtc-dom.js'
import { embeddedDocument, fetchDocument, fetchPage, FORMATS } from './mtc-transport.js'
import { decodeXml } from './mtc-decode-xml.js'
import { decodeJson } from './mtc-decode-json.js'
import { renderDocument } from './mtc-views.js'

const TABS = ['probe', 'current', 'sample']
const DOCUMENT_SEGMENTS = [...TABS, 'asset', 'assets']
const REFRESH_MS = 2000

const $ = id => document.getElementById(id)
const params = new URLSearchParams(window.location.search)

// /current, /sample, /{device}/probe, /asset/{ids}, ...  The device prefix is kept in tab links.
const segments = window.location.pathname.split('/').filter(Boolean)
const docIndex = segments.findIndex(s => DOCUMENT_SEGMENTS.includes(s))
const base = '/' + (docIndex >= 0 ? segments.slice(0, docIndex) : segments).join('/')
const prefix = base === '/' ? '' : base
const tab = docIndex >= 0 ? segments[docIndex] : 'probe'

let refreshTimer = null

// The script element's type is the media type of the embedded document
function decode({ type, text }) {
  if (type.endsWith('xml')) return decodeXml(text)
  if (type.endsWith('json')) return decodeJson(text)
  throw new Error('Cannot read an embedded document of type ' + (type || 'unknown'))
}

// Shown in place of the table when the document cannot be read
function notice(message) {
  return h('p', { class: 'mtc-notice mtc-error', role: 'status' }, message)
}

function render(embedded) {
  const container = $('main-container')
  try {
    const root = decode(embedded)
    container.replaceChildren(renderDocument(root))
    linkModel(root)
  } catch (err) {
    container.replaceChildren(notice(err.message))
  }
}

// model.mtconnect.org has one site per MTConnect version, from 2.0. The version comes from the
// document, so it is checked before it goes into a URL.
function linkModel(root) {
  const link = $('model-link')
  const version = root.attrs.schemaVersion || ''
  const match = /^(\d+)\.(\d+)$/.exec(version)
  if (match && Number(match[1]) >= 2) {
    link.setAttribute('href', 'https://model.mtconnect.org/Version' + version + '/')
    link.textContent = 'model.mtconnect.org (version ' + version + ')'
  } else {
    link.setAttribute('href', 'https://model.mtconnect.org/')
    link.textContent = 'model.mtconnect.org'
  }
}

async function refresh() {
  try {
    render(await fetchPage(window.location.pathname + window.location.search))
  } catch (err) {
    $('main-container').replaceChildren(notice(err.message))
  }
}

function scheduleRefresh() {
  clearTimeout(refreshTimer)
  refreshTimer = null
  if (!$('autorefresh').classList.contains('active')) return
  refreshTimer = setTimeout(async () => {
    await refresh()
    scheduleRefresh()
  }, REFRESH_MS)
}

function setupTabs() {
  for (const name of TABS) {
    const link = $('tab-' + name)
    link.firstElementChild.setAttribute('href', prefix + '/' + name)
    if (name === tab) link.classList.add('selected')
  }
}

function setupForm() {
  for (const id of ['path', 'from', 'count']) $(id).value = params.get(id) || ''

  $('query').addEventListener('submit', event => {
    event.preventDefault()
    const query = new URLSearchParams()
    for (const id of ['path', 'from', 'count']) if ($(id).value) query.set(id, $(id).value)
    const sample = query.has('from') || query.has('count')
    const qs = query.toString()
    window.location.assign(prefix + (sample ? '/sample' : '/current') + (qs ? '?' + qs : ''))
  })
}

// The format for the Raw dialog, remembered between pages
function setupFormat() {
  const select = $('format')
  let saved = null
  try {
    saved = localStorage.getItem('mtc-format')
  } catch {}
  select.value = saved in FORMATS ? saved : 'xml'
  select.addEventListener('change', () => {
    try {
      localStorage.setItem('mtc-format', select.value)
    } catch {}
  })
}

function setupAutorefresh() {
  const button = $('autorefresh')
  if (localStorage.getItem('mtc-autorefresh') === 'on') button.classList.add('active')
  button.addEventListener('click', () => {
    button.classList.toggle('active')
    localStorage.setItem('mtc-autorefresh', button.classList.contains('active') ? 'on' : '')
    scheduleRefresh()
  })
}

function setupDialogs() {
  $('help').addEventListener('click', () => $('help-dialog').showModal())
  $('raw').addEventListener('click', async () => {
    const pre = $('raw-text')
    pre.textContent = 'Loading...'
    $('raw-dialog').showModal()
    try {
      pre.textContent = await fetchDocument(
        window.location.pathname + window.location.search,
        $('format').value
      )
    } catch (err) {
      pre.textContent = err.message
    }
  })
  for (const close of document.querySelectorAll('[data-close]')) {
    close.addEventListener('click', () => close.closest('dialog').close())
  }
  $('gotoTop').addEventListener('click', () =>
    $('main-container').scrollTo({ top: 0, behavior: 'smooth' })
  )
}

// The navbar wraps on narrow windows, so reserve exactly the height it ends up with
function fitHeader() {
  const nav = document.querySelector('nav')
  document.documentElement.style.setProperty('--header-height', nav.offsetHeight + 'px')
}
window.addEventListener('resize', fitHeader)
fitHeader()

setupTabs()
setupForm()
setupFormat()
setupAutorefresh()
setupDialogs()
try {
  render(embeddedDocument())
} catch (err) {
  $('main-container').replaceChildren(notice(err.message))
}
scheduleRefresh()
