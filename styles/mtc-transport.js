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

// Transport: the agent embeds the response document in the page it returns, in a script element
// whose type is the document's media type. The page reads that document instead of asking the
// agent for it again. The agent is a one-way data source, so this module only issues GET requests
// and only to the page's own origin: the same page for autorefresh, and the raw document for the
// Raw dialog.

const DATA_ID = 'mtconnect-data'

// Read the document embedded in a page: this one, or one returned by fetchPage
export function embeddedDocument(page = document) {
  const script = page.getElementById(DATA_ID)
  if (!script) throw new Error('The page does not contain an MTConnect document')
  return { type: script.type, text: script.textContent.trim() }
}

// Request a page from the agent and read the document embedded in it. MTConnectError documents
// come back with a 4xx or 5xx status and are embedded the same way, so the status is not checked.
export async function fetchPage(url) {
  const response = await get(url, 'text/html')

  // DOMParser does not run scripts or load resources, it only builds the tree
  const page = new DOMParser().parseFromString(await response.text(), 'text/html')
  return embeddedDocument(page)
}

export const FORMATS = {
  xml: 'application/xml',
  json: 'application/json',
}

// Request the document itself, pretty printed, in the given format (xml or json). The format query
// parameter makes the agent return the raw document instead of the page. The Accept header asks
// for the same format in case a route ignores the parameter. It is a bare media type because
// parameters such as ;q=0.9 stop the agent matching it.
export async function fetchDocument(url, format) {
  const target = new URL(url, window.location.href)
  target.searchParams.set('format', format)
  target.searchParams.set('pretty', 'true')
  const response = await get(target, FORMATS[format])
  return response.text()
}

function sameOrigin(url) {
  const target = new URL(url, window.location.href)
  if (target.origin !== window.location.origin) {
    throw new Error('Refusing to read from another origin: ' + target.origin)
  }
  return target
}

function get(url, accept) {
  return fetch(sameOrigin(url), {
    method: 'GET',
    headers: { Accept: accept },
    credentials: 'same-origin',
    cache: 'no-store',
  })
}
