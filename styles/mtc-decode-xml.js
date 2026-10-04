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
