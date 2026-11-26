const vscode = require('vscode');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { randomBytes } = require('node:crypto');
const { spawn } = require('node:child_process');
const model = require('./model');
const schema = require('./schema.json');

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

function eventNames(root) {
  const names = new Set();
  const walk = node => {
    for (const property of model.propertiesFor(schema, node)) {
      if (property.type === 'event' && node.attrs[property.name])
        names.add(node.attrs[property.name]);
    }
    node.children.forEach(walk);
  };
  walk(root);
  return [...names];
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
            const events = eventNames(model.parse(source));
            if (events.length > 128 || events.some(name => name.length > 256 || name.includes('\0')))
              throw new Error('미리보기가 처리할 수 있는 이벤트 이름 범위를 넘었습니다.');
            const size = (value, fallback, min, max) =>
              Number.isInteger(value) ? Math.min(max, Math.max(min, value)) : fallback;
            const width = size(message.width, 720, 240, 1920);
            const height = size(message.height, 530, 180, 1400);
            const exe = await previewExecutable(document.uri);
            const directory = await fs.promises.mkdtemp(path.join(os.tmpdir(), 'lotml-preview-'));
            const file = path.join(directory, 'layout.lotml');
            const cleanup = () => fs.promises.unlink(file).catch(() => {})
              .then(() => fs.promises.rmdir(directory)).catch(() => {});
            try {
              await fs.promises.writeFile(file, source, 'utf8');
              if (disposed || preview) { await cleanup(); return; }
              const child = spawn(exe, ['--size', String(width), String(height), file, ...events], {
                cwd: path.dirname(exe), shell: false, stdio: ['ignore', 'pipe', 'pipe']
              });
              preview = child;
              child.stdout.on('data', chunk => statusOutput.append(chunk.toString()));
              child.stderr.on('data', chunk => statusOutput.append(chunk.toString()));
              child.once('error', error => {
                feedback('Vulkan 미리보기를 열지 못했습니다. ' + error.message);
              });
              child.once('close', code => {
                preview = undefined;
                cleanup();
                if (code && !disposed) {
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
