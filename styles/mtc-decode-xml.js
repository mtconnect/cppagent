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

// Decode an MTConnect XML document into the neutral tree used by the views:
//   { name, attrs: {name: value}, text, children: [node] }
// Names are local names, so the schema version in the namespace does not matter.

export function decodeXml(text) {
  const doc = new DOMParser().parseFromString(text, 'application/xml')
  if (doc.getElementsByTagName('parsererror').length > 0 || !doc.documentElement) {
    throw new Error('Response is not well formed XML')
  }
  const root = convert(doc.documentElement)
  // JSON documents carry schemaVersion, XML documents carry it in the namespace:
  // urn:mtconnect.org:MTConnectDevices:2.7
  const version = /:(\d+\.\d+)$/.exec(doc.documentElement.namespaceURI || '')
  if (version && !root.attrs.schemaVersion) root.attrs.schemaVersion = version[1]
  return root
}

function convert(element) {
  const attrs = {}
  for (const attr of element.attributes) {
    if (attr.name === 'xmlns' || attr.name.startsWith('xmlns:')) continue
    attrs[attr.name] = attr.value
  }

  let text = ''
  const children = []
  for (const node of element.childNodes) {
    if (node.nodeType === Node.ELEMENT_NODE) children.push(convert(node))
    else if (node.nodeType === Node.TEXT_NODE || node.nodeType === Node.CDATA_SECTION_NODE) {
      text += node.nodeValue
    }
  }

  return { name: element.localName, attrs, text: text.trim(), children }
}
