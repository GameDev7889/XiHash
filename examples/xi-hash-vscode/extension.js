const vscode = require('vscode');
const path = require('path');
const { spawn } = require('child_process');

function activate(context) {
  const diagnostics = vscode.languages.createDiagnosticCollection('xihash');
  const timers = new Map();
  const running = new Map();
  const versions = new Map();

  const check = (document) => {
    if (document.languageId !== 'xihash' || document.uri.scheme !== 'file') return;
    const key = document.uri.toString();
    versions.set(key, document.version);
    const prior = running.get(key);
    if (prior) prior.kill();

    const compiler = vscode.workspace.getConfiguration('xihash').get('compilerPath', 'XiHash');
    const cwd = vscode.workspace.getWorkspaceFolder(document.uri)?.uri.fsPath ?? path.dirname(document.uri.fsPath);
    let child;
    try {
      child = spawn(compiler, ['--check', document.uri.fsPath], { cwd, windowsHide: true });
    } catch (error) {
      diagnostics.set(document.uri, [new vscode.Diagnostic(
        new vscode.Range(0, 0, 0, 1), `Could not start XiHash compiler: ${error.message}`,
        vscode.DiagnosticSeverity.Error)]);
      return;
    }
    running.set(key, child);
    let stderr = '';
    child.stderr.setEncoding('utf8');
    child.stderr.on('data', chunk => stderr += chunk);
    child.on('error', error => {
      if (running.get(key) !== child) return;
      running.delete(key);
      diagnostics.set(document.uri, [new vscode.Diagnostic(
        new vscode.Range(0, 0, 0, 1), `Could not run XiHash compiler '${compiler}': ${error.message}`,
        vscode.DiagnosticSeverity.Error)]);
    });
    child.on('close', code => {
      if (running.get(key) !== child) return;
      running.delete(key);
      if (versions.get(key) !== document.version) return;
      const found = [];
      for (const line of stderr.split(/\r?\n/)) {
        const match = line.match(/^(.*):(\d+):(\d+): error: (.*)$/);
        if (!match) continue;
        const file = path.resolve(cwd, match[1]);
        if (file !== path.resolve(document.uri.fsPath)) continue;
        const row = Math.max(0, Number(match[2]) - 1);
        const column = Math.max(0, Number(match[3]) - 1);
        const start = new vscode.Position(Math.min(row, document.lineCount - 1), column);
        const end = start.translate(0, 1);
        const diagnostic = new vscode.Diagnostic(new vscode.Range(start, end), match[4],
          vscode.DiagnosticSeverity.Error);
        diagnostic.source = 'XiHash';
        found.push(diagnostic);
      }
      if (code !== 0 && found.length === 0 && stderr.trim()) {
        const diagnostic = new vscode.Diagnostic(new vscode.Range(0, 0, 0, 1), stderr.trim(),
          vscode.DiagnosticSeverity.Error);
        diagnostic.source = 'XiHash';
        found.push(diagnostic);
      }
      diagnostics.set(document.uri, found);
    });
  };

  const schedule = document => {
    const key = document.uri.toString();
    versions.set(key, document.version);
    clearTimeout(timers.get(key));
    const delay = vscode.workspace.getConfiguration('xihash').get('checkOnTypeDelay', 400);
    timers.set(key, setTimeout(() => check(document), delay));
  };

  context.subscriptions.push(diagnostics);
  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(check));
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(event => schedule(event.document)));
  context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(check));
  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument(document => {
    const key = document.uri.toString();
    clearTimeout(timers.get(key));
    running.get(key)?.kill();
    timers.delete(key);
    running.delete(key);
    versions.delete(key);
    diagnostics.delete(document.uri);
  }));
  for (const document of vscode.workspace.textDocuments) check(document);
}

function deactivate() {}

module.exports = { activate, deactivate };
