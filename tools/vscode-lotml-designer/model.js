const sharedProperties = [
  { name: 'id', type: 'string', default: '' },
  ...['flex', 'minWidth', 'minHeight', 'maxWidth', 'maxHeight'].map(name =>
    ({ name, type: 'number', default: '' }))
];

const enums = {
  mainAlign: ['start', 'center', 'end', 'spaceBetween'],
  crossAlign: ['start', 'center', 'end', 'stretch'],
  horizontalAlign: ['start', 'center', 'end'],
  verticalAlign: ['top', 'center', 'bottom']
};

function decode(value) {
  return value.replace(/&(amp|lt|gt|quot|apos|#[0-9]+|#x[0-9a-fA-F]+);|&/g, (match, entity) => {
    if (!entity) throw new Error('잘못된 XML 엔티티입니다.');
    const entities = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'" };
    if (entity in entities) return entities[entity];
    const code = entity.startsWith('#x') ?
      parseInt(entity.slice(2), 16) : Number(entity.slice(1));
    if (code !== 9 && code !== 10 && code !== 13 &&
        !(code >= 32 && code <= 0x10ffff && (code < 0xd800 || code > 0xdfff) &&
          code !== 0xfffe && code !== 0xffff))
      throw new Error('잘못된 XML 문자 번호입니다.');
    return String.fromCodePoint(code);
  });
}

function encode(value) {
  return value.replace(/[&<>"']/g, c =>
    ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&apos;' })[c]);
}

function parse(source) {
  const root = { children: [] };
  const stack = [root];
  const tag = /<(\/?)([A-Za-z_][\w.-]*)/y;
  const attribute = /([A-Za-z_][\w.-]*)\s*=\s*("([^"<]*)"|'([^'<]*)')/y;
  let cursor = 0;
  const fail = message => {
    const line = source.slice(0, cursor).split('\n').length;
    throw new Error('줄 ' + line + ' · ' + message);
  };
  while (cursor < source.length) {
    const parent = stack[stack.length - 1];
    if (source[cursor] !== '<') {
      const next = source.indexOf('<', cursor);
      const text = source.slice(cursor, next < 0 ? source.length : next);
      if (parent === root && text.trim()) fail('루트 바깥에 텍스트가 있습니다.');
      if (parent !== root) parent.text += decode(text);
      cursor += text.length;
      continue;
    }
    const marker = [['<!--', '-->'], ['<?', '?>'], ['<![CDATA[', ']]>']]
      .find(([open]) => source.startsWith(open, cursor));
    if (marker) {
      const start = cursor + marker[0].length;
      const end = source.indexOf(marker[1], start);
      if (end < 0) fail('닫히지 않은 XML 구문입니다.');
      if (marker[0] === '<![CDATA[') {
        if (parent === root) fail('루트 바깥에 CDATA가 있습니다.');
        parent.text += source.slice(start, end);
      }
      cursor = end + marker[1].length;
      continue;
    }
    tag.lastIndex = cursor;
    const match = tag.exec(source);
    if (!match) fail('지원하지 않거나 잘못된 XML 태그입니다.');
    const start = cursor;
    const [, closing, type] = match;
    let pos = tag.lastIndex;
    if (closing) {
      while (/\s/.test(source[pos] || '')) pos++;
      if (parent === root || parent.type !== type || source[pos] !== '>')
        fail('닫는 태그가 여는 태그와 일치하지 않습니다.');
      parent.closeStart = cursor;
      parent.end = pos + 1;
      stack.pop();
      cursor = parent.end;
      continue;
    }
    const node = { type, start, text: '', attrs: Object.create(null), properties: [], children: [] };
    for (;;) {
      const whitespaceStart = pos;
      while (/\s/.test(source[pos] || '')) pos++;
      const selfClosing = source.startsWith('/>', pos);
      if (selfClosing || source[pos] === '>') {
        node.attributeEnd = whitespaceStart;
        node.openEnd = pos + (selfClosing ? 2 : 1);
        node.selfClosing = selfClosing;
        if (selfClosing) node.end = node.openEnd;
        break;
      }
      if (pos === whitespaceStart) fail('속성 앞에 공백이 필요합니다.');
      attribute.lastIndex = pos;
      const attr = attribute.exec(source);
      if (!attr || Object.hasOwn(node.attrs, attr[1])) fail('속성이 잘못되었거나 중복되었습니다.');
      const raw = attr[3] ?? attr[4];
      const end = attribute.lastIndex;
      node.attrs[attr[1]] = decode(raw);
      node.properties.push({ name: attr[1], start: whitespaceStart, end,
        valueStart: end - raw.length - 1, valueEnd: end - 1 });
      pos = end;
    }
    parent.children.push(node);
    cursor = node.openEnd;
    if (!node.selfClosing) stack.push(node);
  }
  if (stack.length !== 1) fail('닫히지 않은 위젯이 있습니다.');
  if (root.children.length !== 1) fail('LotML에는 루트 위젯이 하나 필요합니다.');
  return root.children[0];
}

function at(root, path) {
  if (!Array.isArray(path) || path.length > 64 ||
      path.some(i => !Number.isSafeInteger(i) || i < 0)) return undefined;
  return path.reduce((node, index) => node?.children[index], root);
}

function propertiesFor(schema, node) {
  return [...(schema.find(item => item.type === node.type)?.properties || []),
    ...sharedProperties];
}

function canInsert(schema, node, type) {
  if (!schema.some(item => item.type === type)) return false;
  if (node.type === 'Ribbon') return type === 'RibbonTab';
  if (type === 'RibbonTab') return false;
  if (['Row', 'Column'].includes(node.type)) return true;
  if (!['RibbonTab', 'RibbonGroup', 'Button', 'Checkbox'].includes(node.type))
    return false;
  return node.children.length === 0 && !(node.attrs.text || node.text.trim());
}

function edit(source, root, schema, operation) {
  const { path, kind } = operation;
  const node = at(root, path);
  const make = (start, end, text, selection = path) => ({ start, end, text, selection });
  if (!node || node.end === undefined) throw new Error('위젯 위치가 변경되었습니다.');
  const parentPath = path.slice(0, -1);
  const parent = path.length ? at(root, parentPath) : undefined;
  const ribbon = parent?.type === 'Ribbon' ? parent : undefined;
  const selectedTab = ribbon && ribbon.attrs.selectedTab === node.attrs.tabId
    ? ribbon.properties.find(item => item.name === 'selectedTab') : undefined;
  if (kind === 'property') {
    const property = propertiesFor(schema, node).find(item => item.name === operation.name);
    if (!property || typeof operation.value !== 'string' || operation.value.length > 4096)
      throw new Error('지원하지 않는 속성입니다.');
    const value = operation.value;
    const existing = node.properties.find(item => item.name === property.name);
    if (!value) {
      if (property.required) throw new Error('필수 속성입니다.');
      return existing && make(existing.start, existing.end, '');
    }
    const number = /^[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?$/;
    const isNumber = part => number.test(part) && Number.isFinite(Number(part));
    const parts = value.split(',').map(part => part.trim());
    if ((property.type === 'number' && !isNumber(value)) ||
        (property.type === 'boolean' && !['true', 'false'].includes(value)) ||
        (property.type === 'color' && !/^#(?:[a-f\d]{6}|[a-f\d]{8})$/i.test(value)) ||
        (enums[property.name] && !enums[property.name].includes(value)) ||
        (property.type === 'insets' && (![1, 2, 4].includes(parts.length) || !parts.every(isNumber))))
      throw new Error('속성 형식을 확인하세요.');
    const escaped = encode(value);
    const change = existing ? make(existing.valueStart, existing.valueEnd, escaped) :
      make(node.attributeEnd, node.attributeEnd, ' ' + property.name + '="' + escaped + '"');
    if (node.type === 'Ribbon' && property.name === 'selectedTab' &&
        !node.children.some(tab => tab.attrs.tabId === value))
      throw new Error('등록된 리본 탭 ID를 입력하세요.');
    if (ribbon && property.name === 'tabId') {
      if (ribbon.children.some(tab => tab !== node && tab.attrs.tabId === value))
        throw new Error('같은 리본에 이미 이 탭 ID가 있습니다.');
      if (selectedTab)
        change.related = make(selectedTab.valueStart, selectedTab.valueEnd, escaped);
    }
    return change;
  }
  if (kind === 'add') {
    if (!canInsert(schema, node, operation.type))
      throw new Error('선택한 위젯에는 이 컨트롤을 추가할 수 없습니다.');
    const type = operation.type;
    const defaults = {
      Label: ' text="레이블"', Button: ' text="버튼"',
      Checkbox: ' text="확인"', TextField: ' text="입력"',
      Box: ' width="64" height="28" color="#42556D"',
      Row: ' spacing="8"', Column: ' spacing="8"',
      RibbonGroup: ' title="그룹"'
    };
    const ids = new Set(node.children.map(child => child.attrs.tabId));
    let number = 1;
    while (ids.has('tab' + number)) number++;
    const attrs = type === 'RibbonTab' ? ' tabId="tab' + number + '" title="탭 ' + number + '"' :
      defaults[type] || '';
    const xml = '<' + type + attrs + ' />';
    const eol = source.includes('\r\n') ? '\r\n' : '\n';
    const lineStart = source.lastIndexOf('\n', node.start - 1) + 1;
    const before = source.slice(lineStart, node.start);
    const indent = /^[\t ]*$/.test(before) ? before : '';
    const first = node.children[0];
    const childIndent = first && source.slice(source.lastIndexOf('\n', first.start - 1) + 1, first.start);
    const nested = childIndent && /^[\t ]+$/.test(childIndent) ? childIndent :
      indent + (indent.includes('\t') ? '\t' : '  ');
    const selection = [...path, node.children.length];
    if (node.selfClosing)
      return make(node.attributeEnd, node.openEnd,
        '>' + eol + nested + xml + eol + indent + '</' + node.type + '>', selection);
    const closeLineStart = source.lastIndexOf('\n', node.closeStart - 1) + 1;
    if (/^[\t ]*$/.test(source.slice(closeLineStart, node.closeStart)))
      return make(closeLineStart, closeLineStart, nested + xml + eol, selection);
    return make(node.closeStart, node.closeStart, eol + nested + xml + eol + indent, selection);
  }
  if (!path.length) throw new Error('루트 위젯은 삭제하거나 이동할 수 없습니다.');
  if (kind === 'remove') {
    const change = make(node.start, node.end, '', parentPath);
    if (selectedTab) change.related = make(selectedTab.start, selectedTab.end, '');
    return change;
  }
  if (kind === 'up' || kind === 'down') {
    const siblingIndex = path[path.length - 1] + (kind === 'up' ? -1 : 1);
    const sibling = at(root, [...parentPath, siblingIndex]);
    if (!sibling) return undefined;
    const [first, second] = [node, sibling].sort((a, b) => a.start - b.start);
    return make(first.start, second.end, source.slice(second.start, second.end) +
      source.slice(first.end, second.start) + source.slice(first.start, first.end),
    [...parentPath, siblingIndex]);
  }
  throw new Error('지원하지 않는 편집입니다.');
}

if (typeof module !== 'undefined')
  module.exports = { parse, at, propertiesFor, canInsert, edit, enums };
else
  window.lotml = { parse, at, propertiesFor, canInsert, edit, enums };
