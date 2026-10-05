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

// DOM helpers. Everything that reaches the page goes through textContent, never innerHTML,
// because device names, data item values and adapter text are untrusted.

export function h(tag, attrs, ...children) {
  const el = document.createElement(tag)
  for (const [key, value] of Object.entries(attrs || {})) {
    if (value === undefined || value === null || value === false) continue
    if (key === 'class') el.className = value
    else if (key === 'onclick') el.addEventListener('click', value)
    else el.setAttribute(key, value === true ? '' : String(value))
  }
  for (const child of children.flat()) {
    if (child === undefined || child === null || child === false) continue
    el.append(child instanceof Node ? child : document.createTextNode(String(child)))
  }
  return el
}

export function clear(el) {
  el.replaceChildren()
  return el
}
