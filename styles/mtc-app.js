// Page controller: picks the view from the URL, loads the document through the transport and
// decoders, and renders it. XML is the default format, JSON is selectable.
import { h } from './mtc-dom.js'
import { fetchDocument } from './mtc-transport.js'
import { decodeXml } from './mtc-decode-xml.js'
import { decodeJson } from './mtc-decode-json.js'
import { renderDocument } from './mtc-views.js'

const DECODERS = { xml: decodeXml, json: decodeJson }
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

// The format lives in the URL fragment as #format=json or #format=xml, written like a query
// string so more viewer settings can be added later. It keeps links and bookmarks on the same
// format. The fragment is never sent to the agent, and a ?format= query parameter would make the
// agent return the raw document instead of this page. A saved preference is the fallback.
const FORMAT_NAMES = Object.keys(DECODERS)
const fragment = () => new URLSearchParams(window.location.hash.slice(1))
const formatFromUrl = () => {
  const name = fragment().get('format')
  return FORMAT_NAMES.includes(name) ? name : null
}
let format = formatFromUrl() || (localStorage.getItem('mtc-format') === 'json' ? 'json' : 'xml')
const hash = () => {
  const params = fragment()
  params.set('format', format)
  return '#' + params
}
let refreshTimer = null

// Shown above the table only when something needs attention. The format dropdown already says
// which format is in use.
function notice(message, isError = false) {
  return h('p', { class: 'mtc-notice' + (isError ? ' mtc-error' : ''), role: 'status' }, message)
}

async function load() {
  const container = $('main-container')
  let result
  let message = null
  try {
    result = await readDocument(format)
  } catch (err) {
    // Fall back to XML, which every agent supports
    if (format === 'json') {
      try {
        result = await readDocument('xml')
        message = notice('JSON could not be read (' + err.message + '), showing XML')
      } catch (xmlErr) {
        return container.replaceChildren(notice(xmlErr.message, true))
      }
    } else {
      return container.replaceChildren(notice(err.message, true))
    }
  }
  container.replaceChildren(...[message, renderDocument(result.root)].filter(Boolean))
  linkModel(result.root)
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

async function readDocument(as) {
  const response = await fetchDocument(window.location.pathname + window.location.search, as)
  return { format: as, root: DECODERS[as](response.text) }
}

function scheduleRefresh() {
  clearTimeout(refreshTimer)
  refreshTimer = null
  if (!$('autorefresh').classList.contains('active')) return
  refreshTimer = setTimeout(async () => {
    await load()
    scheduleRefresh()
  }, REFRESH_MS)
}

function setupTabs() {
  for (const name of TABS) {
    const link = $('tab-' + name)
    link.firstElementChild.setAttribute('href', prefix + '/' + name + hash())
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
    window.location.assign(
      prefix + (sample ? '/sample' : '/current') + (qs ? '?' + qs : '') + hash()
    )
  })
}

function setupFormat() {
  const select = $('format')
  select.value = format
  select.addEventListener('change', () => useFormat(select.value))
  // Back and forward buttons, or an edited fragment
  window.addEventListener('hashchange', () => {
    const next = formatFromUrl()
    if (next && next !== format) useFormat(next, false)
  })
}

function useFormat(next, updateUrl = true) {
  format = next
  $('format').value = format
  localStorage.setItem('mtc-format', format)
  if (updateUrl) history.replaceState(null, '', window.location.pathname + window.location.search + hash())
  for (const name of TABS) {
    const link = $('tab-' + name).firstElementChild
    link.setAttribute('href', prefix + '/' + name + hash())
  }
  load()
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
      const response = await fetchDocument(
        window.location.pathname + window.location.search,
        format,
        { pretty: true }
      )
      pre.textContent = response.text
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

// Make the format in use visible in the URL, even when it came from the saved preference
if (!formatFromUrl()) history.replaceState(null, '', window.location.pathname + window.location.search + hash())

setupTabs()
setupForm()
setupFormat()
setupAutorefresh()
setupDialogs()
load().then(scheduleRefresh)
