import { useState, useRef, useEffect, useCallback } from 'react';
import Editor from '@monaco-editor/react';
import { useWebSocket } from './hooks/useWebSocket';
import './App.css';

// ── Default script content ───────────────────────────────────────────────────
const DEFAULT_SCRIPT = `-- LunaClient Script Editor
-- Write your Lua script here and press Execute

print("Hello from LunaClient!")
`;

// ── Monaco editor theme definition ───────────────────────────────────────────
const LUNA_THEME = {
  base: 'vs-dark',
  inherit: true,
  rules: [
    { token: '', foreground: 'e0e0f0', background: '0c0c1a' },
    { token: 'comment', foreground: '5a5a80', fontStyle: 'italic' },
    { token: 'keyword', foreground: 'c792ea' },
    { token: 'string', foreground: 'c3e88d' },
    { token: 'number', foreground: 'f78c6c' },
    { token: 'delimiter', foreground: '7878a0' },
    { token: 'identifier', foreground: 'e0e0f0' },
    { token: 'type', foreground: 'ffcb6b' },
    { token: 'variable', foreground: '82aaff' },
    { token: 'predefined', foreground: '82aaff' },
    { token: 'global', foreground: 'ff5370' },
  ],
  colors: {
    'editor.background': '#0c0c1a',
    'editor.foreground': '#e0e0f0',
    'editor.lineHighlightBackground': '#12122a',
    'editor.selectionBackground': '#7c4dff30',
    'editor.inactiveSelectionBackground': '#7c4dff15',
    'editorCursor.foreground': '#a07cff',
    'editorLineNumber.foreground': '#3a3a60',
    'editorLineNumber.activeForeground': '#7c4dff',
    'editorIndentGuide.background': '#1a1a35',
    'editorIndentGuide.activeBackground': '#2a2a55',
    'editorWidget.background': '#0f0f22',
    'editorWidget.border': '#1e1e3e',
    'editorSuggestWidget.background': '#0f0f22',
    'editorSuggestWidget.border': '#1e1e3e',
    'editorSuggestWidget.selectedBackground': '#7c4dff30',
    'scrollbarSlider.background': '#5a35c050',
    'scrollbarSlider.hoverBackground': '#7c4dff80',
    'scrollbarSlider.activeBackground': '#a07cff90',
  },
};

export default function App() {
  const { connected, pipeConnected, logs, executeScript, clearLogs, addLog } = useWebSocket();

  // ── Script tabs ──────────────────────────────────────────────────────────
  const [tabs, setTabs] = useState([
    { id: 1, name: 'Script 1', content: DEFAULT_SCRIPT, modified: false },
  ]);
  const [activeTab, setActiveTab] = useState(1);
  const [injecting, setInjecting] = useState(false);
  const [injectMsg, setInjectMsg] = useState('');
  const [injectOk, setInjectOk] = useState(false);
  const consoleEndRef = useRef(null);
  const tabCounter = useRef(1);

  // ── Auto-scroll console ──────────────────────────────────────────────────
  useEffect(() => {
    consoleEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logs]);

  // ── Get current tab ──────────────────────────────────────────────────────
  const currentTab = tabs.find(t => t.id === activeTab) || tabs[0];

  // ── Tab operations ───────────────────────────────────────────────────────
  const addTab = useCallback(() => {
    tabCounter.current += 1;
    const newTab = {
      id: Date.now(),
      name: `Script ${tabCounter.current}`,
      content: '-- New Script\nprint("hello")\n',
      modified: false,
    };
    setTabs(prev => [...prev, newTab]);
    setActiveTab(newTab.id);
  }, []);

  const closeTab = useCallback((id) => {
    setTabs(prev => {
      if (prev.length <= 1) return prev;
      const next = prev.filter(t => t.id !== id);
      if (activeTab === id) setActiveTab(next[next.length - 1].id);
      return next;
    });
  }, [activeTab]);

  const updateContent = useCallback((value) => {
    setTabs(prev => prev.map(t =>
      t.id === activeTab ? { ...t, content: value || '', modified: true } : t
    ));
  }, [activeTab]);

  // ── Inject ───────────────────────────────────────────────────────────────
  const handleInject = useCallback(async () => {
    setInjecting(true);
    setInjectMsg('');
    try {
      const res = await fetch('/api/inject', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({}),
      });
      const data = await res.json();
      setInjectOk(data.success);
      setInjectMsg(data.message || data.error || 'Unknown result');
      addLog(data.success ? 'status' : 'error', `[Injector] ${data.message || data.error}`);
    } catch (e) {
      setInjectOk(false);
      setInjectMsg('Failed to reach backend: ' + e.message);
      addLog('error', '[Injector] ' + e.message);
    }
    setInjecting(false);
  }, [addLog]);

  // ── Execute ──────────────────────────────────────────────────────────────
  const handleExecute = useCallback(() => {
    if (currentTab) {
      executeScript(currentTab.content);
    }
  }, [currentTab, executeScript]);

  // ── Save ─────────────────────────────────────────────────────────────────
  const handleSave = useCallback(() => {
    if (!currentTab) return;
    const blob = new Blob([currentTab.content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${currentTab.name}.lua`;
    a.click();
    URL.revokeObjectURL(url);
    addLog('status', `[Save] Downloaded ${currentTab.name}.lua`);
  }, [currentTab, addLog]);

  // ── Load ─────────────────────────────────────────────────────────────────
  const handleLoad = useCallback(() => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.lua,.txt';
    input.onchange = (e) => {
      const file = e.target.files[0];
      if (!file) return;
      const reader = new FileReader();
      reader.onload = (ev) => {
        const content = ev.target.result;
        updateContent(content);
        addLog('status', `[Load] Loaded ${file.name}`);
      };
      reader.readAsText(file);
    };
    input.click();
  }, [updateContent, addLog]);

  // ── Monaco setup ─────────────────────────────────────────────────────────
  const handleEditorMount = useCallback((editor, monaco) => {
    monaco.editor.defineTheme('luna', LUNA_THEME);
    monaco.editor.setTheme('luna');

    // Ctrl+Enter to execute
    editor.addAction({
      id: 'luna-execute',
      label: 'Execute Script',
      keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
      run: () => handleExecute(),
    });

    // Ctrl+S to save
    editor.addAction({
      id: 'luna-save',
      label: 'Save Script',
      keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS],
      run: () => handleSave(),
    });
  }, [handleExecute, handleSave]);

  // ── Render ───────────────────────────────────────────────────────────────
  return (
    <div className="app">
      {/* ── Title Bar ──────────────────────────────────────────────────── */}
      <div className="titlebar">
        <div className="titlebar-brand">
          <span className="luna">LUNA</span>
          <span className="client">CLIENT</span>
          <span className="version">v1.0.0</span>
        </div>
        <div className="titlebar-right">
          <div className="connection-dot">
            <span className={`dot ${pipeConnected ? 'connected' : ''}`} />
            <span style={{ color: pipeConnected ? 'var(--ok)' : 'var(--text-dim)' }}>
              {pipeConnected ? 'Attached' : 'Not Attached'}
            </span>
          </div>
        </div>
      </div>

      <div className="app-body">
        {/* ── Sidebar ────────────────────────────────────────────────── */}
        <div className="sidebar">
          {/* Inject Section */}
          <div className="sidebar-section">
            <div className="sidebar-section-title">ATTACH</div>
            <div className="sidebar-separator" />

            <button
              className={`btn-inject ${pipeConnected ? 'attached' : ''}`}
              onClick={handleInject}
              disabled={injecting || pipeConnected}
            >
              {injecting ? '⏳ Injecting...' : pipeConnected ? '✓ Attached' : '⚡ Inject DLL'}
            </button>

            {injectMsg && (
              <div className="inject-status" style={{ color: injectOk ? 'var(--ok)' : 'var(--err)' }}>
                {injectMsg}
              </div>
            )}
          </div>

          {/* Script Tabs */}
          <div className="sidebar-section">
            <div className="sidebar-section-title">SCRIPTS</div>
            <div className="sidebar-separator" />

            <button className="btn-new-tab" onClick={addTab}>+ New Tab</button>

            <div className="script-list">
              {tabs.map(tab => (
                <button
                  key={tab.id}
                  className={`script-tab-btn ${tab.id === activeTab ? 'active' : ''}`}
                  onClick={() => setActiveTab(tab.id)}
                >
                  <span>{tab.name}{tab.modified ? ' •' : ''}</span>
                  {tabs.length > 1 && (
                    <span
                      className="close-x"
                      onClick={(e) => { e.stopPropagation(); closeTab(tab.id); }}
                    >×</span>
                  )}
                </button>
              ))}
            </div>
          </div>

          {/* Info */}
          <div className="sidebar-section" style={{ marginTop: 'auto' }}>
            <div className="sidebar-section-title">SHORTCUTS</div>
            <div className="sidebar-separator" />
            <div style={{ fontSize: 11, color: 'var(--text-faint)', lineHeight: 1.8 }}>
              <div><span className="mono" style={{ color: 'var(--text-dim)' }}>Ctrl+Enter</span> Execute</div>
              <div><span className="mono" style={{ color: 'var(--text-dim)' }}>Ctrl+S</span> Save</div>
              <div><span className="mono" style={{ color: 'var(--text-dim)' }}>END</span> Eject DLL</div>
            </div>
          </div>
        </div>

        {/* ── Main Content ───────────────────────────────────────────── */}
        <div className="main-content">
          {/* Editor */}
          <div className="editor-area">
            <div className="editor-toolbar">
              <button
                className="btn-execute"
                onClick={handleExecute}
                disabled={!pipeConnected}
                title="Ctrl+Enter"
              >
                ▶ Execute
              </button>
              <button className="btn-tool" onClick={() => {
                if (currentTab) updateContent('');
              }}>Clear</button>
              <button className="btn-tool" onClick={handleSave}>Save</button>
              <button className="btn-tool" onClick={handleLoad}>Load</button>
            </div>

            <div className="editor-container">
              <Editor
                height="100%"
                defaultLanguage="lua"
                theme="luna"
                value={currentTab?.content || ''}
                onChange={updateContent}
                onMount={handleEditorMount}
                options={{
                  fontSize: 14,
                  fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
                  fontLigatures: true,
                  minimap: { enabled: false },
                  lineNumbers: 'on',
                  roundedSelection: true,
                  scrollBeyondLastLine: false,
                  padding: { top: 10, bottom: 10 },
                  cursorBlinking: 'smooth',
                  cursorSmoothCaretAnimation: 'on',
                  smoothScrolling: true,
                  tabSize: 4,
                  insertSpaces: true,
                  wordWrap: 'on',
                  bracketPairColorization: { enabled: true },
                  renderLineHighlight: 'line',
                  suggestOnTriggerCharacters: true,
                  acceptSuggestionOnEnter: 'on',
                  contextmenu: true,
                }}
                loading={
                  <div style={{
                    display: 'flex', alignItems: 'center', justifyContent: 'center',
                    height: '100%', color: 'var(--text-dim)', fontSize: 14,
                  }}>
                    Loading editor...
                  </div>
                }
              />
            </div>
          </div>

          {/* Console */}
          <div className="console">
            <div className="console-header">
              <span className="console-header-title">CONSOLE</span>
              <button className="btn-clear-console" onClick={clearLogs}>Clear</button>
            </div>
            <div className="console-log">
              {logs.map(entry => (
                <div key={entry.id} className={`log-entry ${entry.type}`}>
                  <span className="log-ts">[{entry.ts}]</span>
                  <span className="log-text">
                    {entry.type === 'error' ? '✗ ' : entry.type === 'status' ? '• ' : '  '}
                    {entry.text}
                  </span>
                </div>
              ))}
              <div ref={consoleEndRef} />
            </div>
          </div>
        </div>
      </div>

      {/* ── Status Bar ──────────────────────────────────────────────── */}
      <div className="statusbar">
        <div className="status-item">
          <span className="dot-sm" style={{ background: connected ? 'var(--ok)' : 'var(--err)' }} />
          <span>{connected ? 'Backend Connected' : 'Backend Offline'}</span>
        </div>
        <div className="status-item">
          <span className="dot-sm" style={{ background: pipeConnected ? 'var(--ok)' : 'var(--text-faint)' }} />
          <span>{pipeConnected ? 'DLL Attached' : 'DLL Not Attached'}</span>
        </div>
        <div style={{ marginLeft: 'auto', color: 'var(--text-faint)' }}>
          LunaClient v1.0.0 — UNC Standard
        </div>
      </div>
    </div>
  );
}
