import {
  ChevronDown,
  ChevronUp,
  Copy,
  Download,
  Trash2,
  Upload,
} from "lucide-react";
import { useCallback, useEffect, useRef, useState } from "react";
import { Terminal as XTerm } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import { WebLinksAddon } from "@xterm/addon-web-links";
import "@xterm/xterm/css/xterm.css";
import { useSearchParams } from "react-router";
import { useSocket } from "../../providers/socket-context";
import {
  TaskPriority,
  type ExecuteCommand,
  type Shelltype,
} from "../../types/session";
import { WsCmd } from "../../lib/socket";

interface HistoryItem {
  id: number;
  command: string;
  output: string | null;
  ok: boolean;
  isLoading: boolean;
  timestamp: string;
  mode: Shelltype;
  exitCode?: number;
  cwd?: string;
  header?: {
    taskId: number;
    type: string;
    flags: string;
  };
}

const parseEndMarker = (
  output: string,
): { clean: string; exitCode?: number; cwd?: string } => {
  const nullChar = String.fromCharCode(0);
  const startIdx = output.lastIndexOf(`${nullChar}END:`);

  if (startIdx === -1) {
    return { clean: output.trim() };
  }

  const endIdx = output.indexOf(nullChar, startIdx + 1);
  if (endIdx === -1) {
    return { clean: output.trim() };
  }

  const markerText = output.substring(startIdx + 5, endIdx);
  const parts = markerText.split(":");

  const exitCode = parseInt(parts[1], 10);
  const cwd = parts.slice(2).join(":");
  const clean = output.substring(0, startIdx).trim();

  return { clean, exitCode, cwd };
};

const abbreviatePath = (path: string): string => {
  return path.replace(/\/home\/([^/]+)/, "~");
};

interface InteractiveProps {
  sessionId: string;
  serverId: string;
}

export function Interactive({ sessionId, serverId }: InteractiveProps) {
  const [cwdLabel, setCwdLabel] = useState("~");
  const [pipInput, setPipInput] = useState("");
  const [history, setHistory] = useState<HistoryItem[]>([]);
  const [copiedId, setCopiedId] = useState<number | null>(null);

  const historyRef = useRef<string[]>([]);
  const historyIndexRef = useRef(-1);
  const inputRef = useRef<HTMLInputElement>(null);
  const outputRef = useRef<HTMLDivElement>(null);
  const termRef = useRef<HTMLDivElement>(null);
  const xtermRef = useRef<XTerm | null>(null);
  const fitAddonRef = useRef<FitAddon | null>(null);

  const NextId = useRef<number>(2);

  const [searchParams, setSearchParams] = useSearchParams();
  const rawMode = searchParams.get("mode") ?? "pip";
  const mode: Shelltype = rawMode === "pty" ? "pty" : "pip";

  const { client } = useSocket();

  const setMode = useCallback(
    (m: Shelltype) => {
      setSearchParams((prev) => {
        prev.set("mode", m);
        return prev;
      });
    },
    [setSearchParams],
  );

  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
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
      allowTransparency: true,
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

    return () => {
      observer.disconnect();
      term.dispose();
      xtermRef.current = null;
      fitAddonRef.current = null;
    };
  }, [mode]);

  const submitPip = useCallback(
    (cmd: string) => {
      if (!cmd.trim()) return;

      historyRef.current.unshift(cmd);
      historyIndexRef.current = -1;

      const commandId = NextId.current++;

      const item: HistoryItem = {
        id: commandId,
        command: cmd,
        output: "",
        ok: true,
        isLoading: true,
        timestamp: new Date().toLocaleTimeString(),
        mode,
      };

      const executeCmd: ExecuteCommand = {
        sessionId,
        commandId,
        serverId,
        command: cmd,
        priority: TaskPriority.High,
        shellType: mode,
      };
      client.sendExecuteCommandEvent(executeCmd);
      setHistory((h) => [...h, item]);
      setPipInput("");
    },
    [client, mode, sessionId, serverId],
  );

  useEffect(() => {
    const handleCommandOutput = (res) => {
      const header = res.data?.header || {};
      const taskId = header.taskId;
      const flagsStr = header.flags || "";
      const outputText = res.error || res.data?.output || "";

      const { clean, exitCode, cwd } = parseEndMarker(outputText);

      if (cwd) {
        setCwdLabel(abbreviatePath(cwd));
      }

      // Check for error using string
      const isError = !!res.error || flagsStr.includes("Error");

      setHistory((prev) => {
        const idx = prev.findIndex(
          (x) => x.id === taskId || x.header?.taskId === taskId,
        );

        if (idx === -1) return prev;

        const updated = [...prev];
        const current = updated[idx];

        updated[idx] = {
          ...current,
          output: [current.output, clean.trim()].filter(Boolean).join("\n"),
          ok: current.ok && !isError,
          isLoading: false,
          timestamp: header.formattedTime,
          exitCode: exitCode ?? current.exitCode,
          cwd: cwd ?? current.cwd,
          header: {
            taskId,
            type: header.type || "Command",
            flags: flagsStr,
          },
        };

        return updated;
      });
    };

    client.on(WsCmd.CommandOutput, handleCommandOutput);

    return () => {
      client.off(WsCmd.CommandOutput);
    };
  }, [client]);

  const navigateHistory = useCallback((dir: "up" | "down") => {
    const h = historyRef.current;
    if (!h.length) return;

    const next =
      dir === "up"
        ? Math.min(historyIndexRef.current + 1, h.length - 1)
        : Math.max(historyIndexRef.current - 1, -1);

    historyIndexRef.current = next;
    setPipInput(next === -1 ? "" : h[next]);

    setTimeout(() => {
      if (inputRef.current) {
        inputRef.current.setSelectionRange(
          inputRef.current.value.length,
          inputRef.current.value.length,
        );
      }
    }, 0);
  }, []);

  const copyOutput = useCallback((item: HistoryItem) => {
    if (!item.output) return;
    navigator.clipboard.writeText(item.output);
    setCopiedId(item.id);
    setTimeout(() => setCopiedId(null), 1200);
  }, []);

  const copyPath = useCallback((path: string) => {
    navigator.clipboard.writeText(path);
  }, []);

  const clearAll = useCallback(() => {
    setHistory([]);
    historyRef.current = [];
    historyIndexRef.current = -1;
    if (xtermRef.current) xtermRef.current.clear();
  }, []);

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLInputElement>) => {
      if (e.key === "Enter") {
        submitPip(pipInput);
      }
      if (e.key === "ArrowUp") {
        e.preventDefault();
        navigateHistory("up");
      }
      if (e.key === "ArrowDown") {
        e.preventDefault();
        navigateHistory("down");
      }
    },
    [pipInput, submitPip, navigateHistory],
  );

  return (
    <div className="flex h-full flex-col overflow-hidden bg-zinc-950 font-mono text-sm">
      <div className="flex items-center justify-between border-b border-zinc-800/60 bg-zinc-900/50 px-3 py-1.5">
        <div className="flex items-center gap-2 text-xs text-zinc-500">
          <span className="text-zinc-400">{">"}_</span>
          <span className="text-zinc-300">shell</span>
          {cwdLabel && (
            <span className="bg-zinc-800 px-1.5 py-0.5 text-[10px] text-zinc-500">
              {cwdLabel}
            </span>
          )}
        </div>

        <div className="flex items-center gap-1.5">
          <button
            type="button"
            title="Upload file"
            className="p-1 text-sky-500/50 transition-colors hover:bg-zinc-800 hover:text-sky-400"
          >
            <Upload className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            title="Download file"
            className="p-1 text-violet-500/50 transition-colors hover:bg-zinc-800 hover:text-violet-400"
          >
            <Download className="h-3.5 w-3.5" />
          </button>

          <div className="w-px h-3 bg-zinc-800" />

          <div className="flex overflow-hidden border border-zinc-800 text-[11px] font-mono">
            {(["pip", "pty"] as Shelltype[]).map((m, i) => (
              <button
                key={m}
                type="button"
                onClick={() => setMode(m)}
                className={`px-2.5 py-0.5 transition-colors uppercase tracking-wider ${
                  i > 0 ? "border-l border-zinc-800" : ""
                } ${
                  mode === m
                    ? "bg-emerald-500 text-black font-semibold"
                    : "text-zinc-500 hover:bg-zinc-800 hover:text-zinc-300"
                }`}
              >
                {m}
              </button>
            ))}
          </div>

          <button
            type="button"
            onClick={() => navigateHistory("up")}
            className="p-1 text-zinc-600 transition-colors hover:bg-zinc-800 hover:text-zinc-400"
          >
            <ChevronUp className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            onClick={() => navigateHistory("down")}
            className="p-1 text-zinc-600 transition-colors hover:bg-zinc-800 hover:text-zinc-400"
          >
            <ChevronDown className="h-3.5 w-3.5" />
          </button>

          <button
            type="button"
            onClick={clearAll}
            className="p-1 text-zinc-600 transition-colors hover:bg-zinc-800 hover:text-red-400"
          >
            <Trash2 className="h-3.5 w-3.5" />
          </button>
        </div>
      </div>

      {mode === "pip" ? (
        <div className="flex flex-1 flex-col overflow-hidden">
          <div
            ref={outputRef}
            className="flex-1 overflow-y-auto px-4 py-4 space-y-4"
            style={{
              scrollbarWidth: "thin",
              scrollbarColor: "#3f3f46 transparent",
            }}
          >
            {history.length === 0 && (
              <div className="text-zinc-600 text-xs pt-2 pl-4">
                Type a command and press Enter...
              </div>
            )}

            {history.map((item) => (
              <div key={item.id} className="space-y-1">
                <div className="flex items-start gap-3">
                  <span className="shrink-0 text-emerald-500 select-none mt-0.5">
                    ❯
                  </span>
                  <span className="text-zinc-200 break-all">
                    {item.command}
                  </span>
                  {item.isLoading && (
                    <span className="shrink-0 animate-pulse text-emerald-500/70 text-xs mt-0.5">
                      ...
                    </span>
                  )}
                  <span className="ml-auto shrink-0 text-[10px] text-zinc-700 tabular-nums">
                    {item.timestamp}
                  </span>
                </div>

                {item.output && (
                  <div className="pl-6 border-l border-zinc-800/70">
                    <pre
                      className={`whitespace-pre-wrap break-words text-xs leading-relaxed ${
                        item.ok ? "text-zinc-400" : "text-red-400"
                      }`}
                    >
                      {item.output}
                    </pre>

                    {!item.isLoading && (
                      <button
                        type="button"
                        onClick={() => copyOutput(item)}
                        className="mt-2 opacity-40 hover:opacity-100 transition-opacity flex items-center gap-1.5 text-[10px] text-zinc-500 hover:text-zinc-400"
                      >
                        <Copy className="h-3 w-3" />
                        copy
                      </button>
                    )}
                  </div>
                )}

                {!item.isLoading && (
                  <div className="pl-6 space-y-1">
                    {item.exitCode !== undefined && (
                      <div
                        className={`text-[10px] font-semibold ${
                          item.exitCode === 0
                            ? "text-emerald-600"
                            : "text-red-400"
                        }`}
                      >
                        exit {item.exitCode}
                      </div>
                    )}
                    {item.cwd && (
                      <div
                        className="text-[10px] text-zinc-600 cursor-pointer hover:text-zinc-400 transition-colors"
                        onClick={() => copyPath(item.cwd!)}
                      >
                        {abbreviatePath(item.cwd)}
                      </div>
                    )}
                  </div>
                )}
              </div>
            ))}
          </div>

          <div className="border-t border-zinc-800 bg-zinc-900 px-4 py-3 flex items-center gap-3">
            <span className="text-emerald-500 select-none">❯</span>
            <input
              ref={inputRef}
              value={pipInput}
              onChange={(e) => setPipInput(e.target.value)}
              onKeyDown={handleKeyDown}
              className="flex-1 bg-transparent outline-none text-zinc-200 placeholder:text-zinc-600 caret-emerald-500"
              placeholder="Enter command..."
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
