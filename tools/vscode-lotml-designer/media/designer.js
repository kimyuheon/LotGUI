(() => {
  const vscode = acquireVsCodeApi();
  const model = window.lotml;
  const byId = id => document.getElementById(id);
  const create = (tag, cls, text) => {
    const node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text !== undefined) node.textContent = text;
    return node;
  };
  const key = path => path.join('.');
  const dragType = 'application/x-lotml-widget';
  let root, schema = [], selection = [], version = 0;
  const captions = { Row: '가로 배치', Column: '세로 배치', Box: '박스',
    Label: '레이블', Button: '버튼', Checkbox: '체크박스',
    TextField: '입력창', NumericInput: '숫자 입력', Ribbon: '리본',
    RibbonTab: '리본 탭', RibbonGroup: '리본 그룹' };
  const mainProps = new Set(['id', 'text', 'title', 'tabId', 'width', 'height',
    'padding', 'spacing', 'fontSize', 'value', 'checked', 'enabled', 'background',
    'minWidth', 'minHeight', 'maxWidth', 'maxHeight', 'flex', 'mainAlign', 'crossAlign']);
  const sequences = new Map();

  const say = text => { byId('status').textContent = text; };
  const postEdit = (kind, options = {}) =>
    vscode.postMessage({ kind: 'edit', version, operation: { kind, path: selection, ...options } });
  const openEvent = (path, event) =>
    vscode.postMessage({ kind: 'openEvent', path, event, version });
  function selectRibbonAncestors(path) {
    let changedTab = false;
    for (let depth = 0; depth < path.length; depth++) {
      const parentPath = path.slice(0, depth);
      const parent = model.at(root, parentPath);
      if (parent?.type !== 'Ribbon') continue;
      const tabId = parent.children[path[depth]].attrs.tabId;
      if (sequences.get(key(parentPath)) !== tabId) {
        sequences.set(key(parentPath), tabId);
        changedTab = true;
      }
    }
    return changedTab;
  }
  const pick = path => {
    selection = path;
    vscode.postMessage({ kind: 'select', path });
    if (selectRibbonAncestors(path)) { paint(); return; }
    markSelection();
    showProperties();
    updatePalette();
  };
  const selectedNode = () => root && model.at(root, selection);
  const canAdd = (node, type) => !!node && model.canInsert(schema, node, type);

  function markSelection() {
    for (const widget of document.querySelectorAll('[data-path]')) {
      widget.classList.toggle('selected', widget.dataset.path === key(selection));
      if (widget.classList.contains('tree-node'))
        widget.setAttribute('aria-pressed', String(widget.classList.contains('selected')));
    }
  }

  function updatePalette() {
    const selected = selectedNode();
    for (const button of byId('palette').querySelectorAll('button')) {
      button.disabled = !canAdd(selected, button.dataset.type);
      button.draggable = !!root;
    }
  }

  function fillPalette() {
    const palette = byId('palette');
    palette.replaceChildren();
    for (const { type } of schema) {
      const item = create('div', 'palette-item');
      item.draggable = true;
      item.addEventListener('dragstart', event => {
        event.dataTransfer.effectAllowed = 'copy';
        event.dataTransfer.setData(dragType, type);
      });
      const button = create('button', '', captions[type] || type);
      button.type = 'button';
      button.dataset.type = type;
      button.title = type;
      button.addEventListener('click', () => postEdit('add', { type }));
      item.append(button);
      palette.append(item);
    }
    updatePalette();
  }

  function treeItem(node, path, depth) {
    const entry = create('button', 'tree-node');
    entry.type = 'button';
    entry.style.paddingInlineStart = (9 + depth * 13) + 'px';
    entry.dataset.path = key(path);
    const caption = node.attrs.id || node.attrs.title || node.attrs.text;
    entry.textContent = node.type + (caption ? ' · ' + caption : '');
    entry.title = entry.textContent;
    entry.addEventListener('click', () => pick(path));
    entry.addEventListener('dblclick', () => openEvent(path));
    byId('tree').append(entry);
    node.children.forEach((child, index) => treeItem(child, [...path, index], depth + 1));
  }

  function px(value) {
    if (value === undefined || value === '') return '';
    const n = Number(value);
    return Number.isFinite(n) ? Math.max(0, Math.min(10000, n)) + 'px' : '';
  }

  function inset(value) {
    if (!value) return '';
    const [left, top = left, right = left, bottom = top] = value.split(',').map(px);
    return [top, right, bottom, left].join(' ');
  }

  function applyColors(widget, attrs, bindings) {
    for (const [property, css] of Object.entries(bindings)) {
      const hex = attrs[property];
      if (!/^#[a-f\d]{6}([a-f\d]{2})?$/i.test(hex ?? '')) continue;
      const [red, green, blue, alpha = 1] =
        hex.slice(1).match(/../g).map(byte => parseInt(byte, 16) / 255);
      // LotML uses linear RGB, as does the native renderer before display conversion.
      widget.style.setProperty(css, `color(srgb-linear ${red} ${green} ${blue} / ${alpha})`);
    }
  }

  function styles(node, outer) {
    const a = node.attrs;
    const style = outer.style;
    if (a.flex !== undefined) style.flexGrow = String(Math.max(0, Number(a.flex)));
    for (const prop of ['minWidth', 'minHeight', 'maxWidth', 'maxHeight',
      'width', 'height', 'fontSize', 'lineHeight']) {
      if (a[prop] !== undefined) style[prop] = px(a[prop]);
    }
    if (a.fontWeight) style.fontWeight = a.fontWeight;
    if (a.italic === 'true') style.fontStyle = 'italic';
    if (a.cornerRadius) style.borderRadius = px(a.cornerRadius);
    outer.classList.toggle('disabled-widget', a.enabled === 'false');
  }

  function designWidget(node, path) {
    const a = node.attrs;
    const type = node.type;
    const nodeKey = key(path);
    const outer = create('div', 'widget widget-' + type);
    outer.dataset.path = nodeKey;
    outer.title = type + (a.id ? ' · ' + a.id : '');
    styles(node, outer);
    outer.addEventListener('click', event => { event.stopPropagation(); pick(path); });
    outer.addEventListener('dblclick', event => { event.stopPropagation(); openEvent(path); });
    if (schema.find(item => item.type === type)?.allowsChildren) {
      outer.addEventListener('dragover', event => {
        if (event.dataTransfer.types.includes(dragType)) {
          event.preventDefault();
          event.stopPropagation();
          event.dataTransfer.dropEffect = 'copy';
        }
      });
      outer.addEventListener('drop', event => {
        const dragged = event.dataTransfer.getData(dragType);
        if (!dragged) return;
        event.preventDefault();
        event.stopPropagation();
        if (canAdd(node, dragged)) postEdit('add', { type: dragged, path });
        else say('이 위치에는 ' + dragged + '을(를) 추가할 수 없습니다.');
      });
    }
    const appendChildren = (host = outer) =>
      node.children.forEach((child, index) => host.append(designWidget(child, [...path, index])));
    if (type === 'Column' || type === 'Row') {
      outer.style.display = 'flex';
      outer.style.flexDirection = type === 'Row' ? 'row' : 'column';
      outer.style.gap = px(a.spacing) || '0';
      outer.style.padding = inset(a.padding) || (node.children.length ? '0' : '7px');
      applyColors(outer, a, { background: 'background-color' });
      outer.style.justifyContent = a.mainAlign === 'spaceBetween' ? 'space-between' : a.mainAlign || 'start';
      outer.style.alignItems = a.crossAlign || 'start';
      if (path.length === 0) {
        outer.style.minHeight = '100%';
        outer.style.width = '100%';
      }
      appendChildren();
      if (!node.children.length && path.length)
        outer.append(create('span', 'hint', type));
    } else if (type === 'Label' || type === 'Button' || type === 'Checkbox' ||
        type === 'TextField' || type === 'NumericInput') {
      if (type === 'Checkbox') {
        const checked = a.checked === 'true';
        const check = create('span', 'check', checked ? '✓' : '');
        applyColors(check, a, { [checked ? 'checkedColor' : 'unchecked']: '--check-fill',
          checkmark: '--checkmark' });
        outer.append(check);
      }
      if (node.children.length) appendChildren();
      else outer.append(create('span', '', type === 'NumericInput' ?
        a.value ?? '0' : a.text ?? (node.text.trim() || captions[type] || type)));
      if (type === 'NumericInput') {
        const step = create('span', 'step', '−   +');
        applyColors(step, a, { stepButton: '--step-fill', indicator: '--step-text' });
        outer.append(step);
      }
      applyColors(outer, a, { normal: '--widget-background',
        hovered: '--widget-hover', color: 'color', textColor: 'color' });
      if (a.contentPadding) outer.style.padding = inset(a.contentPadding);
      if (type === 'Label' && a.horizontalAlign) outer.style.textAlign = a.horizontalAlign;
    } else if (type === 'Box') {
      applyColors(outer, a, { color: 'background-color' });
    } else if (type === 'Ribbon') {
      const tabs = create('div', 'tabs');
      applyColors(outer, a, { background: 'background-color', tabBar: '--tab-bar',
        tabNormal: '--tab-normal', tabSelected: '--tab-selected', tabHovered: '--tab-hover',
        textColor: '--tab-text', selectedTextColor: '--tab-selected-text' });
      const saved = node.children.findIndex(tab => tab.attrs.tabId === sequences.get(nodeKey));
      const active = saved >= 0 ? saved :
        Math.max(0, node.children.findIndex(tab => tab.attrs.tabId === a.selectedTab));
      node.children.forEach((tab, index) => {
        const button = create('button', index === active ? 'active' : '', tab.attrs.title || '탭');
        button.addEventListener('click', event => {
          event.stopPropagation();
          pick([...path, index]);
        });
        tabs.append(button);
      });
      outer.append(tabs);
      if (node.children[active]) outer.append(designWidget(node.children[active], [...path, active]));
    } else if (type === 'RibbonTab' || type === 'RibbonGroup') {
      appendChildren();
      if (type === 'RibbonGroup') {
        applyColors(outer, a, { background: 'background-color',
          titleColor: '--group-title', separator: '--group-separator' });
        outer.append(create('span', 'group-label', a.title || '그룹'));
      } else if (!node.children.length) outer.append(create('span', 'hint', '빈 리본 탭'));
    } else {
      outer.append(create('span', 'hint', type));
      appendChildren();
    }
    return outer;
  }

  function showProperties() {
    const host = byId('properties');
    host.replaceChildren();
    const node = selectedNode();
    byId('selected').textContent = node ? node.type + (node.attrs.id ? ' · ' + node.attrs.id : '') : '선택 없음';
    const parent = selection.length && root ? model.at(root, selection.slice(0, -1)) : undefined;
    const index = selection[selection.length - 1];
    byId('remove').disabled = !node || !selection.length;
    byId('up').disabled = !parent || index === 0;
    byId('down').disabled = !parent || index >= parent.children.length - 1;
    if (!node) return;
    const additional = create('details');
    additional.append(create('summary', '', '스타일 및 이벤트'));
    for (const property of model.propertiesFor(schema, node)) {
      const row = create('label', 'property');
      row.append(create('span', '', property.name + (property.required ? ' *' : '')));
      const choices = model.enums[property.name] ||
        (property.type === 'boolean' ? ['true', 'false'] : undefined);
      const field = create(choices ? 'select' : 'input');
      const current = node.attrs[property.name] ?? '';
      if (choices) {
        const values = current && !choices.includes(current) ? [...choices, current] : choices;
        for (const value of ['', ...values]) {
          const opt = create('option', '', value || '기본값');
          opt.value = value;
          field.append(opt);
        }
      } else {
        field.type = 'text';
        field.placeholder = property.default || '';
      }
      field.value = current;
      const propertyPath = [...selection];
      field.addEventListener('change', () => postEdit('property',
        { path: propertyPath, name: property.name, value: field.value }));
      field.addEventListener('keydown', event => {
        if (event.key === 'Enter' && !event.isComposing && field.tagName === 'INPUT') field.blur();
      });
      row.append(field);
      if (property.type === 'event') {
        const open = create('button', 'event-code', '코드');
        open.type = 'button';
        open.title = property.name + ' C++ 코드 열기';
        open.addEventListener('click', () => openEvent(propertyPath, property.name));
        row.append(open);
      }
      (mainProps.has(property.name) || current ? host : additional).append(row);
    }
    if (additional.childElementCount > 1) host.append(additional);
  }

  function sizeCanvas() {
    const limit = (id, fallback, min, max) =>
      Math.max(min, Math.min(max, Number(byId(id).value) || fallback));
    byId('canvas').style.width = limit('width', 720, 240, 1920) + 'px';
    byId('canvas').style.height = limit('height', 530, 180, 1400) + 'px';
  }

  function paint() {
    byId('tree').replaceChildren();
    byId('canvas').replaceChildren();
    if (root) {
      treeItem(root, [], 0);
      byId('canvas').append(designWidget(root, []));
    }
    markSelection();
    showProperties();
    updatePalette();
  }

  for (const name of ['remove', 'up', 'down'])
    byId(name).addEventListener('click', () => postEdit(name));
  byId('source').addEventListener('click', () => vscode.postMessage({ kind: 'source' }));
  byId('native').addEventListener('click', () => vscode.postMessage({
    kind: 'native', width: byId('width').valueAsNumber, height: byId('height').valueAsNumber
  }));
  for (const id of ['width', 'height']) byId(id).addEventListener('change', sizeCanvas);
  sizeCanvas();
  window.addEventListener('message', ({ data }) => {
    if (data.kind === 'feedback') { say(data.text); return; }
    if (data.kind !== 'document') return;
    const first = schema.length === 0;
    schema = data.schema;
    version = data.version;
    selection = data.selection || [];
    byId('file').textContent = data.file;
    try {
      root = model.parse(data.xml);
      if (!model.at(root, selection)) selection = [];
      selectRibbonAncestors(selection);
      say('편집은 LotML에 반영됩니다. 실제 배치는 Vulkan 미리보기에서 확인할 수 있습니다.');
    } catch (error) {
      root = undefined;
      say(error.message + ' LotML 소스에서 수정하세요.');
    }
    if (first) fillPalette();
    paint();
  });
  vscode.postMessage({ kind: 'ready' });
})();
