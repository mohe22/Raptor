import {
  ChevronDown,
  ChevronUp,
  Copy,
  Download,
  Trash2,
  Upload,
} from "lucide-react";
import { useEffect, useRef, useState } from "react";
import { Terminal as XTerm } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import { WebLinksAddon } from "@xterm/addon-web-links";
import "@xterm/xterm/css/xterm.css";
import { useSearchParams } from "react-router";

type ShellMode = "pip" | "pty";

interface HistoryItem {
  id: string;
  command: string;
  output: string | null;
  ok: boolean;
  isLoading: boolean;
  exitCode: number | null;
  cwd: string | null;
  /** sent time; updated to server response time once output arrives */
  timestamp: string;
  mode: ShellMode;
}

interface InteractiveProps {
  sessionId: string;
  serverId: string;
}

function abbreviatePath(path: string) {
  return path.replace(/\/home\/([^/]+)/, "~");
}

export function Interactive({ sessionId, serverId }: InteractiveProps) {
  const [cwdLabel, setCwdLabel] = useState("~");
  const [pipInput, setPipInput] = useState("");
  const [history, setHistory] = useState<HistoryItem[]>([]);
  const [copiedId, setCopiedId] = useState<string | null>(null);
  const historyRef = useRef<string[]>([]);
  const historyIndexRef = useRef(-1);
  const inputRef = useRef<HTMLInputElement>(null);
  const outputRef = useRef<HTMLDivElement>(null);
  const termRef = useRef<HTMLDivElement>(null);
  const xtermRef = useRef<XTerm | null>(null);
  const fitAddonRef = useRef<FitAddon | null>(null);

  const [searchParams, setSearchParams] = useSearchParams();
  const rawMode = searchParams.get("mode") ?? "pip";
  const mode: ShellMode = rawMode === "pty" ? "pty" : "pip";

  function setMode(m: ShellMode) {
    setSearchParams((prev) => {
      prev.set("mode", m);
      return prev;
    });
  }

  useEffect(() => {
    if (outputRef.current)
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
  }, [history]);

  useEffect(() => {
    if (mode !== "pty" || !termRef.current) return;
    const term = new XTerm({
      theme: {
        background: "#09090b",
        foreground: "#d4d4d8",
        cursor: "#22c55e",
        selectionBackground: "#22c55e33",
        black: "#18181b",
        brightBlack: "#3f3f46",
        green: "#22c55e",
        brightGreen: "#4ade80",
      },
      fontFamily: '"JetBrains Mono", "Fira Code", monospace',
      fontSize: 13,
      lineHeight: 1.5,
      cursorBlink: true,
      cursorStyle: "block",
      scrollback: 5000,
    });
    const fitAddon = new FitAddon();
    term.loadAddon(fitAddon);
    term.loadAddon(new WebLinksAddon());
    term.open(termRef.current);
    fitAddon.fit();
    xtermRef.current = term;
    fitAddonRef.current = fitAddon;
    const observer = new ResizeObserver(() => fitAddon.fit());
    observer.observe(termRef.current);
    // term.write("\x1b[32m$\x1b[0m connect your WebSocket here\r\n");
    return () => {
      observer.disconnect();
      term.dispose();
      xtermRef.current = null;
      fitAddonRef.current = null;
    };
  }, [mode]);

  function submitPip(cmd: string) {
    if (!cmd.trim()) return;
    historyRef.current.unshift(cmd);
    historyIndexRef.current = -1;
    const id = crypto.randomUUID();
    const item: HistoryItem = {
      id,
      command: cmd,
      output: null,
      ok: true,
      isLoading: true,
      exitCode: null,
      cwd: null,
      timestamp: new Date().toLocaleTimeString(),
      mode: mode,
    };
    setHistory((h) => [...h, item]);
    setPipInput("");
  }

  function navigateHistory(dir: "up" | "down") {
    const h = historyRef.current;
    if (!h.length) return;
    const next =
      dir === "up"
        ? Math.min(historyIndexRef.current + 1, h.length - 1)
        : Math.max(historyIndexRef.current - 1, -1);
    historyIndexRef.current = next;
    setPipInput(next === -1 ? "" : h[next]);
    setTimeout(() => {
      const el = inputRef.current;
      if (el) el.setSelectionRange(el.value.length, el.value.length);
    }, 0);
  }

  function copyOutput(item: HistoryItem) {
    if (!item.output) return;
    navigator.clipboard.writeText(item.output);
    setCopiedId(item.id);
    setTimeout(() => setCopiedId(null), 1200);
  }

  function clearAll() {
    setHistory([]);
    historyRef.current = [];
    historyIndexRef.current = -1;
    if (xtermRef.current) xtermRef.current.clear();
  }

  return (
    <div className="flex h-full flex-col overflow-hidden bg-zinc-950 font-mono text-sm">
      <div className="flex items-center justify-between border-b border-zinc-800/60 bg-zinc-900/50 px-3 py-1.5">
        <div className="flex items-center gap-2 text-xs text-zinc-500">
          <span className="text-zinc-400">{">"}_</span>
          <span className="text-zinc-300">shell</span>
          {cwdLabel && (
            <span className=" bg-zinc-800 px-1.5 py-0.5 text-[10px] text-zinc-500">
              {cwdLabel}
            </span>
          )}
        </div>
        <div className="flex items-center gap-1.5">
          <button
            type="button"
            title="Upload file"
            className="p-1 text-sky-500/50 transition-colors hover:bg-zinc-800 hover:text-sky-400 "
          >
            <Upload className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            title="Download file"
            className="p-1 text-violet-500/50 transition-colors hover:bg-zinc-800 hover:text-violet-400 "
          >
            <Download className="h-3.5 w-3.5" />
          </button>
          <div className="w-px h-3 bg-zinc-800" />
          <div className="flex overflow-hidden  border border-zinc-800 text-[11px] font-mono">
            {(["pip", "pty"] as ShellMode[]).map((m, i) => (
              <button
                key={m}
                type="button"
                onClick={() => setMode(m)}
                className={`px-2.5 py-0.5 transition-colors uppercase tracking-wider ${i > 0 ? "border-l border-zinc-800" : ""} ${mode === m ? "bg-emerald-500 text-black font-semibold" : "text-zinc-500 hover:bg-zinc-800 hover:text-zinc-300"}`}
              >
                {m}
              </button>
            ))}
          </div>
          <button
            type="button"
            onClick={() => navigateHistory("up")}
            className="p-1 text-zinc-600 transition-colors hover:bg-zinc-800 hover:text-zinc-400 "
          >
            <ChevronUp className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            onClick={() => navigateHistory("down")}
            className="p-1 text-zinc-600 transition-colors hover:bg-zinc-800 hover:text-zinc-400 rounded"
          >
            <ChevronDown className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            onClick={clearAll}
            className="p-1 text-zinc-600 transition-colors hover:bg-zinc-800 hover:text-red-400 "
          >
            <Trash2 className="h-3.5 w-3.5" />
          </button>
        </div>
      </div>

      {mode === "pip" ? (
        <div className="flex flex-1 flex-col overflow-hidden">
          <div
            ref={outputRef}
            className="flex-1 overflow-y-auto px-4 py-3 space-y-3"
            style={{
              scrollbarWidth: "thin",
              scrollbarColor: "#3f3f46 transparent",
            }}
          >
            {history.length === 0 && (
              <div className="text-zinc-600 text-xs pt-1">no output yet</div>
            )}
            {history.map((item) => (
              <div key={item.id}>
                <div className="flex items-center justify-between gap-2">
                  <div className="flex items-center gap-2 min-w-0">
                    <span className="shrink-0 text-emerald-500 text-xs">
                      {item.mode == "pty" ? "❯ pty" : "❯"}
                    </span>
                    <span
                      className={`truncate text-xs ${
                        item.command.startsWith("@upload")
                          ? "text-sky-400"
                          : item.command.startsWith("@download")
                            ? "text-violet-400"
                            : "text-zinc-200"
                      }`}
                    >
                      {item.command}
                    </span>
                    {item.isLoading && (
                      <span className="shrink-0 animate-pulse text-[10px] text-zinc-600">
                        running…
                      </span>
                    )}
                  </div>
                  <span className="shrink-0 text-[10px] text-zinc-700">
                    {item.timestamp}
                  </span>
                </div>

                {item.output && (
                  <div className="group relative mt-1 pl-4 border-l border-zinc-800">
                    {!item.ok && (
                      <div className="flex items-center gap-1.5 mb-1 text-[10px] text-red-400/70 uppercase tracking-widest">
                        <span className="w-1 h-1  bg-red-400/70 shrink-0" />
                        agent error
                      </div>
                    )}
                    <pre
                      className={`whitespace-pre-wrap break-all text-xs leading-relaxed ${
                        item.ok ? "text-zinc-400" : "text-red-400"
                      }`}
                    >
                      {item.output}
                    </pre>
                    {!item.isLoading && (
                      <>
                        <button
                          type="button"
                          onClick={() => copyOutput(item)}
                          className="absolute right-0 top-0  p-1 opacity-0 transition-opacity group-hover:opacity-100 hover:bg-zinc-800"
                        >
                          <Copy className="h-3 w-3 text-zinc-500" />
                        </button>
                        {copiedId === item.id && (
                          <span className="absolute right-6 top-0.5 text-[10px] text-emerald-400">
                            copied
                          </span>
                        )}
                      </>
                    )}
                  </div>
                )}

                {!item.isLoading && (
                  <div className="mt-1 flex items-center gap-2 pl-4">
                    {item.exitCode !== null && (
                      <span
                        className={`text-[10px] font-medium ${
                          item.exitCode === 0
                            ? "text-emerald-500/60"
                            : "text-red-400/80"
                        }`}
                      >
                        [{item.exitCode}]
                      </span>
                    )}
                    {item.cwd && (
                      <div className="flex items-center gap-1">
                        <span className="text-[10px] text-zinc-700 font-mono">
                          {abbreviatePath(item.cwd)}
                        </span>
                        <button
                          type="button"
                          onClick={() => {
                            navigator.clipboard.writeText(item.cwd!);
                            setCopiedId(item.id);
                            setTimeout(() => setCopiedId(null), 1200);
                          }}
                          className=" p-0.5 hover:bg-zinc-800 transition-colors"
                          title="Copy full path"
                        >
                          <Copy className="h-3 w-3 text-zinc-700 hover:text-zinc-400" />
                        </button>
                      </div>
                    )}
                  </div>
                )}
              </div>
            ))}
          </div>

          <div className="flex items-center gap-2 border-t border-zinc-800/60 bg-zinc-900/30 px-4 py-2">
            <span className="text-emerald-500 text-xs select-none">$</span>
            <input
              ref={inputRef}
              value={pipInput}
              onChange={(e) => setPipInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === "Enter") submitPip(pipInput);
                if (e.key === "ArrowUp") {
                  e.preventDefault();
                  navigateHistory("up");
                }
                if (e.key === "ArrowDown") {
                  e.preventDefault();
                  navigateHistory("down");
                }
              }}
              className="flex-1 bg-transparent text-xs text-zinc-200 outline-none placeholder:text-zinc-700 caret-emerald-500"
              placeholder="type a command…"
              autoFocus
              spellCheck={false}
              autoComplete="off"
            />
          </div>
        </div>
      ) : (
        <div ref={termRef} className="flex-1 overflow-hidden" />
      )}
    </div>
  );
}
