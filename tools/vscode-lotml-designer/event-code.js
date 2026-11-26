const { createHash } = require('node:crypto');

const handlersEnd = '// LotUI handler insertion point';
const bindingsEnd = '    // LotUI binding insertion point';

function identifier(value) {
  return value.replace(/[^a-zA-Z0-9]+/g, '_').replace(/^_+|_+$/g, '') || 'layout';
}

function eventSymbol(name) {
  const hash = createHash('sha256').update(name).digest('hex').slice(0, 10);
  return 'handle_' + identifier(name).slice(0, 40) + '_' + hash;
}

function stringLiteral(text) {
  const escaped = Array.from(Buffer.from(text, 'utf8'), byte => {
    if (byte === 0x5c || byte === 0x22) return '\\' + String.fromCharCode(byte);
    if (byte < 0x20 || byte >= 0x7f) return '\\' + byte.toString(8).padStart(3, '0');
    return String.fromCharCode(byte);
  }).join('');
  return '"' + escaped + '"';
}

function header(layoutName) {
  const scope = 'layout_' + identifier(layoutName);
  return [
    '#pragma once', '', '#include "declarative/lotml.h"', '',
    'namespace lotui::designer::' + scope + ' {', '', handlersEnd, '',
    'inline void bindEvents(lotui::declarative::LoadOptions& options) {',
    bindingsEnd, '}', '', '} // namespace lotui::designer::' + scope, ''
  ].join('\n');
}

function plan(source, event) {
  const symbol = eventSymbol(event);
  const declaration = 'inline void ' + symbol + '() {';
  const oldHandler = source.search(new RegExp('\\binline\\s+void\\s+' + symbol + '\\s*\\('));
  if (oldHandler >= 0) return { handler: oldHandler, edits: [] };
  const handlers = source.indexOf(handlersEnd);
  const bindings = source.indexOf(bindingsEnd);
  if (handlers < 0 || bindings < handlers)
    throw new Error('이벤트 헤더의 LotUI 삽입 표시가 변경되었습니다.');
  const eol = source.includes('\r\n') ? '\r\n' : '\n';
  return {
    handler: handlers,
    edits: [
      { start: handlers, text: [declaration, '    // TODO: Implement this event.', '}', '', ''].join(eol) },
      { start: bindings, text: '    options.events.try_emplace(' +
        stringLiteral(event) + ', &' + symbol + ');' + eol }
    ]
  };
}

function defaultEvent(node, properties, requested) {
  const names = properties.filter(property => property.type === 'event').map(property => property.name);
  if (requested) return names.includes(requested) ? requested : undefined;
  return names.find(name => node.attrs[name]) || names[0];
}

function unusedEventName(root, node, event, propertiesFor) {
  const used = new Set();
  const collect = item => {
    for (const property of propertiesFor(item))
      if (property.type === 'event' && item.attrs[property.name]) used.add(item.attrs[property.name]);
    item.children.forEach(collect);
  };
  collect(root);
  const base = identifier(node.attrs.id || node.type) + '_' + event;
  let name = base;
  for (let suffix = 2; used.has(name); suffix++) name = base + '_' + suffix;
  return name;
}

module.exports = { header, plan, defaultEvent, unusedEventName };
