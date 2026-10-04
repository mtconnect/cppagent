// Transport: read-only access to the agent. The agent is a one-way data source, so this module
// only issues GET requests and has no way to send anything else.

export const FORMATS = {
  xml: 'application/xml',
  json: 'application/json',
}

// Send a bare media type. The agent matches the Accept entry against its printer names, so
// parameters such as ;q=0.9 would stop it from matching.
export async function fetchDocument(url, format, { pretty = false } = {}) {
  const target = new URL(url, window.location.href)
  if (target.origin !== window.location.origin) {
    throw new Error('Refusing to read from another origin: ' + target.origin)
  }
  if (pretty) target.searchParams.set('pretty', 'true')

  const response = await fetch(target, {
    method: 'GET',
    headers: { Accept: FORMATS[format] },
    credentials: 'same-origin',
    cache: 'no-store',
  })

  // MTConnectError documents come back with a 4xx or 5xx status, so the body is still decoded.
  return { format, status: response.status, text: await response.text() }
}
