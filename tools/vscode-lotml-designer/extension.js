const vscode = require('vscode');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { randomBytes } = require('node:crypto');
const { spawn } = require('node:child_process');
const model = require('./model');
const schema = require('./schema.json');
const eventCode = require('./event-code');

const viewType = 'lotgui.lotmlDesigner';

function html(webview, extensionUri) {
  const resource = (...parts) => webview.asWebviewUri(vscode.Uri.joinPath(extensionUri, ...parts));
  const nonce = randomBytes(16).toString('hex');
  return [
    '<!doctype html><html lang="ko"><head><meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width, initial-scale=1">',
    '<meta http-equiv="Content-Security-Policy" content="default-src \'none\'; ' +
      'style-src ' + webview.cspSource + ' \'unsafe-inline\'; script-src \'nonce-' + nonce + '\'">',
    '<link rel="stylesheet" href="' + resource('media', 'designer.css') + '">',
    '<title>LotUI Designer</title></head><body>',
    '<header><strong>LotUI Designer</strong><span id="file"></span>',
    '<button id="native">Vulkan 미리보기</button><button id="source">LotML 소스</button></header>',
    '<main><aside class="left"><section><h2>위젯</h2><div id="palette"></div>',
    '<p class="hint">컨테이너를 선택한 뒤 클릭하거나 캔버스로 끌어 놓으세요.</p></section>',
    '<section class="outline"><h2>구조</h2><div id="tree"></div></section></aside>',
    '<section class="workspace"><div class="panel-head"><h2>디자인 캔버스</h2>',
    '<div class="size"><label>가로 <input id="width" type="number" min="240" max="1920" value="720"></label>',
    '<label>세로 <input id="height" type="number" min="180" max="1400" value="530"></label></div></div>',
    '<div class="scroll"><div id="canvas" aria-label="LotML 캔버스"></div></div></section>',
    '<aside class="right"><h2>속성</h2><div id="selected"></div><div class="actions">',
    '<button id="up" title="앞으로">↑</button><button id="down" title="뒤로">↓</button>',
    '<button id="remove">삭제</button></div><div id="properties"></div></aside></main>',
    '<footer id="status" role="status"></footer>',
    '<script nonce="' + nonce + '" src="' + resource('model.js') + '"></script>',
    '<script nonce="' + nonce + '" src="' + resource('media', 'designer.js') + '"></script>',
    '</body></html>'
  ].join('');
}

async function previewExecutable(uri) {
  const configured = vscode.workspace.getConfiguration('lotgui', uri).get('previewExecutable', '');
  if (configured && !path.isAbsolute(configured))
    throw new Error('lotgui.previewExecutable에는 실행 파일의 절대 경로를 지정하세요.');
  const folder = vscode.workspace.getWorkspaceFolder(uri)?.uri.fsPath;
  const name = 'lotml_preview' + (process.platform === 'win32' ? '.exe' : '');
  const candidates = configured ? [configured] : folder ? [
    ['build', 'Release'], ['build', 'Debug'], ['build'],
    ['build-sdk-portable', 'Release'], ['build-sdk-portable'],
    ['out', 'portable'], ['out', 'macos']
  ].map(parts => path.join(folder, ...parts, name)) : [];
  for (const file of candidates) {
    try {
      if ((await fs.promises.stat(file)).isFile()) return file;
    } catch {}
  }
  throw new Error('먼저 lotml_preview 타깃을 빌드하거나 lotgui.previewExecutable을 지정하세요.');
}

function previewSnapshot(xml, root) {
  const names = new Set();
  const paths = [];
  const rows = [];
  const insertions = [];
  const usedIds = new Set();
  const collectIds = node => {
    if (node.attrs.id) usedIds.add(node.attrs.id);
    node.children.forEach(collectIds);
  };
  collectIds(root);
  let seed = 0;
  const walk = (node, widgetPath, parent) => {
    for (const property of model.propertiesFor(schema, node)) {
      if (property.type === 'event' && node.attrs[property.name])
        names.add(node.attrs[property.name]);
    }
    let id = node.attrs.id;
    if (id === undefined) {
      do { id = 'lotuiPreview' + seed++; } while (usedIds.has(id));
      usedIds.add(id);
      insertions.push({ offset: node.attributeEnd, text: ' id="' + id + '"' });
    }
    const index = paths.length;
    paths.push(widgetPath);
    const ordinal = widgetPath.at(-1) ?? 0;
    const quoted = '"' + id.replace(/[\\"]/g, char => '\\' + char) + '"';
    rows.push(parent + ' ' + ordinal + ' ' + quoted);
    node.children.forEach((child, i) => walk(child, [...widgetPath, i], index));
  };
  walk(root, [], -1);
  for (const entry of insertions.reverse())
    xml = xml.slice(0, entry.offset) + entry.text + xml.slice(entry.offset);
  return { xml, events: [...names], paths, targetList: rows.join('\n') + '\n' };
}

async function revealOffset(document, offset) {
  const position = document.positionAt(offset);
  const selection = new vscode.Range(position, position);
  const editor = await vscode.window.showTextDocument(document, {
    viewColumn: vscode.ViewColumn.Beside, selection, preview: false
  });
  editor.revealRange(selection, vscode.TextEditorRevealType.InCenterIfOutsideViewport);
}

async function existingHandler(document, companionUri, event) {
  const folder = vscode.workspace.getWorkspaceFolder(document.uri);
  if (!folder) return undefined;
  const include = new vscode.RelativePattern(folder, '**/*.{cc,cpp,cxx,h,hpp,hxx}');
  const exclude = '**/{build*,out,sdk,.git,.tools,vendor,third_party}/**';
  const uris = await vscode.workspace.findFiles(include, exclude, 1500);
  const escaped = JSON.stringify(event).slice(1, -1).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const name = '(?:u8)?"' + escaped + '"';
  const match = new RegExp('\\.events\\s*(?:\\.\\s*(?:try_emplace|emplace|insert_or_assign)' +
    '\\s*\\(\\s*' + name + '\\s*,|\\[\\s*' + name + '\\s*\\]\\s*=)\\s*');
  for (const uri of uris) {
    if (uri.path.endsWith('.lotml.events.h') || uri.toString() === companionUri.toString()) continue;
    const open = vscode.workspace.textDocuments.find(item => item.uri.toString() === uri.toString());
    let source;
    try {
      source = open ? open.getText() : Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
    } catch {
      continue;
    }
    const binding = match.exec(source);
    if (!binding) continue;
    const target = open || await vscode.workspace.openTextDocument(uri);
    const start = binding.index + binding[0].length;
    const symbol = /^&?\s*((?:[a-zA-Z_]\w*::)*[a-zA-Z_]\w*)/.exec(source.slice(start));
    if (symbol) {
      const position = target.positionAt(start + symbol[0].length - 1);
      const definitions = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, position);
      const definition = definitions?.[0];
      if (definition) {
        const implementation = await vscode.workspace.openTextDocument(definition.targetUri || definition.uri);
        const range = definition.targetSelectionRange || definition.range;
        return { document: implementation, offset: implementation.offsetAt(range.start) };
      }
    }
    return { document: target, offset: start };
  }
}

async function openEventCode(document, widgetPath, requested, version) {
  if (document.version !== version) throw new Error('화면이 변경되었습니다. 위젯을 다시 선택하세요.');
  const source = document.getText();
  const root = model.parse(source);
  const node = model.at(root, widgetPath);
  if (!node) throw new Error('위젯 위치가 변경되었습니다.');
  const propertiesFor = item => model.propertiesFor(schema, item);
  const event = eventCode.defaultEvent(node, propertiesFor(node), requested);
  if (!event) {
    await revealOffset(document, node.start);
    return false;
  }
  const boundName = node.attrs[event];
  const name = boundName || eventCode.unusedEventName(root, node, event, propertiesFor);
  const companionUri = document.uri.with({ path: document.uri.path + '.events.h' });
  let companion;
  try {
    companion = await vscode.workspace.openTextDocument(companionUri);
  } catch (error) {
    // openTextDocument may report a generic error. Verify the missing file through the filesystem.
    try {
      await vscode.workspace.fs.stat(companionUri);
      throw error;
    } catch (statError) {
      if (statError.code !== 'FileNotFound') throw statError;
    }
  }
  const headerName = path.basename(document.uri.path, '.lotml');
  const companionVersion = companion?.version;
  const header = companion ? companion.getText() : eventCode.header(headerName);
  const plan = eventCode.plan(header, name);
  if (boundName && plan.edits.length) {
    const existing = await existingHandler(document, companionUri, name);
    if (existing) {
      await revealOffset(existing.document, existing.offset);
      return false;
    }
  }
  if (document.version !== version || companion?.version !== companionVersion)
    throw new Error('화면 또는 이벤트 코드가 변경되었습니다. 위젯을 다시 선택하세요.');
  const edit = new vscode.WorkspaceEdit();
  if (!companion) {
    let contents = header;
    for (const update of [...plan.edits].reverse())
      contents = contents.slice(0, update.start) + update.text + contents.slice(update.start);
    edit.createFile(companionUri, { overwrite: false });
    edit.insert(companionUri, new vscode.Position(0, 0), contents);
  } else {
    for (const update of plan.edits)
      edit.insert(companionUri, companion.positionAt(update.start), update.text);
  }
  if (!boundName) {
    const change = model.edit(source, root, schema, { kind: 'property', path: widgetPath, name: event, value: name });
    edit.replace(document.uri, new vscode.Range(document.positionAt(change.start),
      document.positionAt(change.end)), change.text);
  }
  if (edit.size && !await vscode.workspace.applyEdit(edit))
    throw new Error('이벤트 코드와 LotML 연결을 저장하지 못했습니다.');
  const updated = await vscode.workspace.openTextDocument(companionUri);
  await revealOffset(updated, plan.handler);
  return !boundName;
}

function activate(context) {
  const statusOutput = vscode.window.createOutputChannel('LotUI Designer');
  context.subscriptions.push(statusOutput);
  context.subscriptions.push(vscode.window.registerCustomEditorProvider(viewType, {
    resolveCustomTextEditor(document, panel) {
      panel.webview.options = { enableScripts: true, localResourceRoots: [context.extensionUri] };
      panel.webview.html = html(panel.webview, context.extensionUri);
      let selected = [];
      let editing = false;
      let preview;
      let disposed = false;
      const send = () => panel.webview.postMessage({
        kind: 'document', xml: document.getText(), version: document.version,
        file: path.basename(document.fileName), schema, selection: selected
      });
      const watcher = vscode.workspace.onDidChangeTextDocument(event => {
        if (event.contentChanges.length && !editing &&
            event.document.uri.toString() === document.uri.toString()) send();
      });
      const listener = panel.webview.onDidReceiveMessage(async message => {
        const feedback = text => panel.webview.postMessage({ kind: 'feedback', text });
        try {
          if (message.kind === 'ready') {
            await send();
          } else if (message.kind === 'source') {
            await vscode.window.showTextDocument(document, vscode.ViewColumn.Beside);
          } else if (message.kind === 'openEvent') {
            if (editing) throw new Error('편집을 적용하고 있습니다. 잠시 후 다시 선택하세요.');
            editing = true;
            try {
              const changed = await openEventCode(document, message.path, message.event, message.version);
              if (changed) await send();
            } finally {
              editing = false;
            }
          } else if (message.kind === 'select' && Array.isArray(message.path)) {
            selected = message.path;
          } else if (message.kind === 'edit') {
            if (editing || message.version !== document.version) { await send(); return; }
            editing = true;
            try {
              const source = document.getText();
              const change = model.edit(source, model.parse(source), schema, message.operation);
              if (!change) return;
              const edit = new vscode.WorkspaceEdit();
              const replace = range => edit.replace(document.uri,
                new vscode.Range(document.positionAt(range.start), document.positionAt(range.end)),
                range.text);
              replace(change);
              if (change.related) replace(change.related);
              if (!await vscode.workspace.applyEdit(edit))
                throw new Error('LotML에 편집 내용을 적용하지 못했습니다.');
              selected = change.selection;
              await send();
            } finally {
              editing = false;
            }
          } else if (message.kind === 'native') {
            if (!vscode.workspace.isTrusted)
              throw new Error('Vulkan 미리보기는 신뢰할 수 있는 작업 영역에서 실행할 수 있습니다.');
            if (preview) throw new Error('열려 있는 Vulkan 미리보기 창을 먼저 닫아 주세요.');
            const source = document.getText();
            const snapshotVersion = document.version;
            const snapshot = previewSnapshot(source, model.parse(source));
            const { events } = snapshot;
            if (events.length > 128 || events.some(name => name.length > 256 || name.includes('\0')))
              throw new Error('미리보기가 처리할 수 있는 이벤트 이름 범위를 넘었습니다.');
            const size = (value, fallback, min, max) =>
              Number.isInteger(value) ? Math.min(max, Math.max(min, value)) : fallback;
            const width = size(message.width, 720, 240, 1920);
            const height = size(message.height, 530, 180, 1400);
            const exe = await previewExecutable(document.uri);
            const directory = await fs.promises.mkdtemp(path.join(os.tmpdir(), 'lotml-preview-'));
            const file = path.join(directory, 'layout.lotml');
            const targetList = path.join(directory, 'targets');
            const cleanup = () => Promise.all([file, targetList].map(item => fs.promises.unlink(item).catch(() => {})))
              .then(() => fs.promises.rmdir(directory)).catch(() => {});
            try {
              await Promise.all([fs.promises.writeFile(file, snapshot.xml, 'utf8'),
                fs.promises.writeFile(targetList, snapshot.targetList, 'utf8')]);
              if (disposed || preview) { await cleanup(); return; }
              const child = spawn(exe, ['--size', String(width), String(height),
                '--reveal', targetList, file, ...events], {
                cwd: path.dirname(exe), shell: false, stdio: ['ignore', 'pipe', 'pipe']
              });
              preview = child;
              let lineBuffer = '';
              let target;
              child.stdout.setEncoding('utf8');
              child.stdout.on('data', chunk => {
                lineBuffer += chunk;
                const lines = lineBuffer.split('\n');
                lineBuffer = lines.pop();
                for (const line of lines) {
                  const message = /^LOTML_OPEN_EVENT ([0-9]+)\r?$/.exec(line);
                  if (message) target = snapshot.paths[Number(message[1])];
                  else statusOutput.appendLine(line.trimEnd());
                }
              });
              child.stderr.on('data', chunk => statusOutput.append(chunk.toString()));
              child.once('error', error => {
                feedback('Vulkan 미리보기를 열지 못했습니다. ' + error.message);
              });
              child.once('close', async code => {
                preview = undefined;
                cleanup();
                if (disposed) return;
                if (lineBuffer) statusOutput.append(lineBuffer);
                if (!code && target) {
                  try {
                    if (editing) throw new Error('편집을 적용하고 있습니다. 디자이너에서 다시 선택하세요.');
                    editing = true;
                    try {
                      const changed = await openEventCode(document, target, undefined, snapshotVersion);
                      if (changed) await send();
                    } finally {
                      editing = false;
                    }
                  } catch (error) {
                    vscode.window.showWarningMessage(error.message);
                  }
                } else if (code) {
                  feedback('Vulkan 미리보기 오류. LotUI Designer 출력에서 자세한 내용을 확인하세요.');
                  statusOutput.show(true);
                }
              });
              await feedback('Vulkan 미리보기를 열었습니다.');
            } catch (error) {
              await cleanup();
              throw error;
            }
          }
        } catch (error) {
          await feedback(error.message || String(error));
        }
      });
      panel.onDidDispose(() => {
        disposed = true;
        if (preview) preview.kill();
        watcher.dispose();
        listener.dispose();
      });
    }
  }));
  context.subscriptions.push(vscode.commands.registerCommand('lotgui.openDesigner', uri => {
    const file = uri || vscode.window.activeTextEditor?.document.uri;
    if (!file || !file.path.toLowerCase().endsWith('.lotml')) {
      return vscode.window.showInformationMessage('.lotml 파일을 선택하세요.');
    }
    return vscode.commands.executeCommand('vscode.openWith', file, viewType);
  }));
  context.subscriptions.push(vscode.commands.registerCommand('lotgui.newLayout', async () => {
    const uri = await vscode.window.showSaveDialog({
      filters: { LotML: ['lotml'] }, saveLabel: 'LotML 파일 만들기'
    });
    if (!uri) return;
    const edit = new vscode.WorkspaceEdit();
    edit.createFile(uri, { overwrite: false });
    edit.insert(uri, new vscode.Position(0, 0),
      '<Column id="screen" padding="20" spacing="12" background="#020203">\n' +
      '  <Label text="새 화면" fontSize="24" color="#D0D8E5" />\n' +
      '  <Button text="확인" width="96" normal="#06080B" hovered="#090E18"\n' +
      '          pressed="#060A11" focusRing="#457AC8" />\n</Column>\n');
    if (!await vscode.workspace.applyEdit(edit))
      throw new Error('LotML 파일을 만들지 못했습니다.');
    const document = await vscode.workspace.openTextDocument(uri);
    await document.save();
    await vscode.commands.executeCommand('vscode.openWith', uri, viewType);
  }));
}

module.exports = { activate };
