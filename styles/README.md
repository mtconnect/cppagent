# MTConnect Agent Stylesheet

Stylesheet for the MTConnect Agent - transforms XML data to a grid UI - with tabs for Probe, Current, and Sample endpoints. Uses [Bootstrap](http://getbootstrap.com/) CSS for some styling.

![](screenshot.jpg)

## Note

Windows had trouble handling the xsl:include statements, like

  <xsl:include href="styles-probe.xsl" />

it appends extra text at the end of the xml, confusing the browser. So copied/pasted that code into the main styles.xsl file.

Windows also had trouble rendering the small png, gif, and jpg files, so changed those to icos. 


## Installation

1. Copy the contents of this repository into the "Styles" folder for the MTConnect Agent.

2. Edit the Agent's configuration file (e.g. agent.cfg) to look for the stylesheets as shown below:

```
Files {
    styles {
        Path = ../styles
        Location = /styles/
    }
    Favicon {
        Path = ../styles/favicon.ico
        Location = /favicon.ico
    }
}

DevicesStyle { Location = /styles/styles.xsl }
StreamsStyle { Location = /styles/styles.xsl }

```

3. Restart Agent
4. Navigate to Agent's url to view


## Browser view without XSLT

Chrome is removing XSLT support (see [Chrome's deprecation notice](https://developer.chrome.com/docs/web-platform/deprecating-xslt)), which will stop `styles.xsl` from rendering in that browser. `viewer.html` is a replacement that does the same job with JavaScript. It reads the Agent's XML or JSON and shows the same Probe, Current, Sample, Assets and Error grids. The XSL files above are unchanged and still work for browsers that support XSLT.

### Configuration

Add a `BrowserView` entry next to the `Files` entry that serves this folder:

```
Files {
    styles {
        Path = ../styles
        Location = /styles/
    }
}

BrowserView { Location = /styles/viewer.html }
```

`Location` must be a file the Agent serves. `Path` is optional and is only needed if no `Files` entry serves the file.

Without a `BrowserView` entry the Agent behaves exactly as before.

An optional top level `StyleType` sets the `type` of the `xml-stylesheet` processing instruction. It defaults to `text/xsl`.

### How the Agent chooses a response

The Agent returns the viewer for a GET of `/probe`, `/current`, `/sample`, `/asset` or `/assets` (with or without a device) when `text/html` is the first of `text/html`, `application/xml`, `text/xml`, `application/json` and `*/*` in the `Accept` header. Browsers send that when you open a URL.

| Request | Response |
|---|---|
| Browser (`Accept: text/html,...`) | `viewer.html` |
| No `Accept`, `*/*`, `application/xml` or `text/xml` | XML |
| `application/json` or `application/mtconnect+json` | JSON |
| Any of the above plus `?format=xml` or `?format=json` | The raw document |

The viewer then requests the same URL again with an explicit `Accept` header, so API clients are not affected. The Agent remains a read only source of data: the viewer only sends GET requests and only to its own origin.

### Using the viewer

- **Format**: the dropdown chooses whether the page reads XML or JSON. XML is the default. If JSON cannot be read the page falls back to XML and says so. The choice is kept in the URL fragment, for example `/current?path=//DataItem[@type="AVAILABILITY"]#format=json`. The fragment is never sent to the Agent. Do not use `?format=` for this, because the Agent treats it as a request for the raw document.
- **Path, From, Count**: as in the XSL view. Path is an XPath filter and applies to Current and Sample.
- **Raw**: shows the document exactly as the Agent sent it, in the chosen format.
- **JSON**: both `JsonVersion` 1 and 2 are handled. Version 1 has not been checked against a running Agent.

### Files

| File | Purpose |
|---|---|
| `viewer.html`, `viewer.css` | The page and its styles |
| `mtc-app.js` | Picks the view from the URL, handles the controls and refresh |
| `mtc-transport.js` | GET only, same origin, explicit `Accept` |
| `mtc-decode-xml.js`, `mtc-decode-json.js` | Turn XML or JSON into one neutral tree |
| `mtc-views.js` | Turns that tree into the grids |
| `mtc-dom.js` | DOM helper that only uses `textContent` |

The views only read the neutral tree, so supporting another output format means adding a decoder. The folder is kept flat.

### Security

Device names, data item values and adapter text are untrusted. The viewer builds every element with `createElement` and `textContent`, never `innerHTML` or `document.write`, and it uses no JavaScript library. `viewer.html` sets `Content-Security-Policy: default-src 'self'` in a `<meta>` tag, so it loads nothing from other sites. The links to [model.mtconnect.org](https://model.mtconnect.org) open in a new tab and the version in the link is checked before it is used.

## Todo

- click row to highlight and switch between current and probe views
- handle all non-standard dataitem elements with generic subtables (currently each subtable type is hardcoded)
- try nbsp instead of white 'x' for indentation
- set max-width for id column and truncate with ellipsis? what set at though? what if someone has wide monitor?
- handle collapsible sections


## Contributing

Note: The MTConnect cppagent doesn't allow using subfolders in the styles folder, so keep it flat.

## License

MIT
