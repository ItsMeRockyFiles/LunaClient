import { useState, useEffect, useRef, useCallback } from 'react';

const WS_URL = `ws://${window.location.hostname}:${window.location.port}`;

export function useWebSocket() {
  const [connected, setConnected] = useState(false);
  const [pipeConnected, setPipeConnected] = useState(false);
  const [logs, setLogs] = useState([]);
  const wsRef = useRef(null);
  const reconnectRef = useRef(null);

  const addLog = useCallback((type, text) => {
    const ts = new Date().toLocaleTimeString('en-US', { hour12: false });
    setLogs(prev => {
      const next = [...prev, { type, text, ts, id: Date.now() + Math.random() }];
      return next.length > 500 ? next.slice(-500) : next;
    });
  }, []);

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return;

    try {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
        addLog('status', 'Connected to LunaClient backend.');
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          switch (msg.type) {
            case 'output':
              addLog('output', msg.text);
              break;
            case 'error':
              addLog('error', msg.text);
              break;
            case 'status':
              addLog('status', msg.text);
              break;
            case 'pipe_status':
              setPipeConnected(msg.connected);
              if (msg.connected) {
                addLog('status', '● DLL pipe connected.');
              }
              break;
            case 'pong':
              addLog('status', 'DLL is alive.');
              break;
          }
        } catch (e) { /* ignore parse errors */ }
      };

      ws.onclose = () => {
        setConnected(false);
        setPipeConnected(false);
        wsRef.current = null;
        // Auto-reconnect
        if (!reconnectRef.current) {
          reconnectRef.current = setTimeout(() => {
            reconnectRef.current = null;
            connect();
          }, 2000);
        }
      };

      ws.onerror = () => { ws.close(); };
    } catch (e) {
      setTimeout(connect, 2000);
    }
  }, [addLog]);

  useEffect(() => {
    connect();
    return () => {
      if (reconnectRef.current) clearTimeout(reconnectRef.current);
      if (wsRef.current) wsRef.current.close();
    };
  }, [connect]);

  const executeScript = useCallback((script) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ action: 'execute', script }));
    } else {
      addLog('error', 'Not connected to backend.');
    }
  }, [addLog]);

  const clearLogs = useCallback(() => setLogs([]), []);

  return {
    connected,
    pipeConnected,
    logs,
    executeScript,
    clearLogs,
    addLog,
  };
}
