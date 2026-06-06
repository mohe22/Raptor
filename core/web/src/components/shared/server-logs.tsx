import { useState, useMemo, useRef, useEffect } from "react";
import type { log, LogLevel } from "../../types/logs";

interface ServerLogsProps {
  serverLogs: log[];
  sessionLogs: log[];
}

const EVENT_STYLES: Record<string, { color: string; bg: string }> = {
  SERVER_CREATED: { color: "#10b981", bg: "rgba(16,185,129,0.08)" },
  SERVER_UPDATED: { color: "#f59e0b", bg: "rgba(245,158,11,0.08)" },
  SESSION_CREATED: { color: "#3b82f6", bg: "rgba(59,130,246,0.08)" },
  SESSION_REMOVED: { color: "#ef4444", bg: "rgba(239,68,68,0.08)" },
  SESSION_IDLE: { color: "#8b5cf6", bg: "rgba(139,92,246,0.08)" },
};

const LEVEL_STYLES: Record<LogLevel, { color: string }> = {
  Info: { color: "#10b981" },
  Debug: { color: "#8b5cf6" },
  Warn: { color: "#f59e0b" },
  Error: { color: "#ef4444" },
  Fatal: { color: "#f43f5e" },
  Unknown: { color: "#52525b" },
};

function formatTs(ts: number) {
  return new Date(ts * 1000).toLocaleTimeString("en-US", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  });
}

function formatDate(ts: number) {
  return new Date(ts * 1000).toLocaleDateString("en-US", {
    month: "short",
    day: "numeric",
  });
}

function highlight(text: string, query: string) {
  if (!query) return <>{text}</>;
  const idx = text.toLowerCase().indexOf(query.toLowerCase());
  if (idx === -1) return <>{text}</>;
  return (
    <>
      {text.slice(0, idx)}
      <mark
        style={{
          background: "rgba(16,185,129,0.35)",
          color: "#d1fae5",
          borderRadius: 2,
          padding: "0 1px",
        }}
      >
        {text.slice(idx, idx + query.length)}
      </mark>
      {text.slice(idx + query.length)}
    </>
  );
}

function MetaExpand({ meta }: { meta: string }) {
  const [open, setOpen] = useState(false);
  if (!meta) return null;
  let parsed: Record<string, unknown> | null = null;
  try {
    parsed = JSON.parse(meta);
  } catch {}
  if (!parsed) return null;
  return (
    <div style={{ marginTop: 4 }}>
      <button
        onClick={() => setOpen((o) => !o)}
        style={{
          fontSize: 10,
          color: "#52525b",
          background: "none",
          border: "none",
          cursor: "pointer",
          fontFamily: "inherit",
          padding: 0,
          letterSpacing: "0.05em",
        }}
      >
        {open ? "▾ hide meta" : "▸ show meta"}
      </button>
      {open && (
        <div
          style={{ marginTop: 4, display: "flex", gap: 6, flexWrap: "wrap" }}
        >
          {Object.entries(parsed).map(([k, v]) => (
            <span
              key={k}
              style={{
                fontSize: 10,
                fontFamily: "inherit",
                background: "rgba(255,255,255,0.04)",
                border: "1px solid rgba(255,255,255,0.07)",
                padding: "1px 6px",
                color: "#a1a1aa",
              }}
            >
              <span style={{ color: "#52525b" }}>{k}=</span>
              {String(v)}
            </span>
          ))}
        </div>
      )}
    </div>
  );
}

function FilterGroup({
  label,
  options,
  value,
  onChange,
}: {
  label: string;
  options: string[];
  value: string;
  onChange: (v: string) => void;
}) {
  return (
    <div
      style={{
        display: "flex",
        alignItems: "center",
        gap: 5,
        flexWrap: "wrap",
      }}
    >
      <span
        style={{
          fontSize: 9,
          color: "#3f3f46",
          letterSpacing: "0.08em",
          textTransform: "uppercase",
          marginRight: 2,
        }}
      >
        {label}:
      </span>
      {options.map((opt) => (
        <button
          key={opt}
          onClick={() => onChange(opt)}
          style={{
            fontSize: 9,
            fontFamily: "inherit",
            fontWeight: 600,
            letterSpacing: "0.07em",
            textTransform: "uppercase",
            padding: "3px 8px",
            border:
              value === opt
                ? "1px solid rgba(16,185,129,0.4)"
                : "1px solid #27272a",
            background: value === opt ? "rgba(16,185,129,0.1)" : "transparent",
            color: value === opt ? "#10b981" : "#52525b",
            cursor: "pointer",
            whiteSpace: "nowrap",
            transition: "all 0.15s",
          }}
        >
          {opt}
        </button>
      ))}
    </div>
  );
}

export function ServerLogs({ serverLogs, sessionLogs }: ServerLogsProps) {
  const allLogs = useMemo(
    () =>
      [...(serverLogs ?? []), ...(sessionLogs ?? [])].sort(
        (a, b) => b.ts - a.ts,
      ),
    [serverLogs, sessionLogs],
  );

  const [search, setSearch] = useState("");
  const [categoryFilter, setCategoryFilter] = useState<string>("All");
  const [eventFilter, setEventFilter] = useState<string>("All");
  const [levelFilter, setLevelFilter] = useState<string>("All");
  const inputRef = useRef<HTMLInputElement>(null);

  const categories = useMemo(
    () => ["All", ...Array.from(new Set(allLogs.map((l) => l.category)))],
    [allLogs],
  );
  const events = useMemo(
    () => ["All", ...Array.from(new Set(allLogs.map((l) => l.event)))],
    [allLogs],
  );
  const levels = useMemo(
    () => ["All", ...Array.from(new Set(allLogs.map((l) => l.level)))],
    [allLogs],
  );

  const filtered = useMemo(
    () =>
      allLogs.filter((log) => {
        if (categoryFilter !== "All" && log.category !== categoryFilter)
          return false;
        if (eventFilter !== "All" && log.event !== eventFilter) return false;
        if (levelFilter !== "All" && log.level !== levelFilter) return false;
        if (search) {
          const q = search.toLowerCase();
          return (
            (log.message ?? "").toLowerCase().includes(q) ||
            (log.event ?? "").toLowerCase().includes(q) ||
            (log.category ?? "").toLowerCase().includes(q) ||
            (log.serverId ?? "").toLowerCase().includes(q) ||
            String(log.id).includes(q)
          );
        }
        return true;
      }),
    [allLogs, search, categoryFilter, eventFilter, levelFilter],
  );
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === "k") {
        e.preventDefault();
        inputRef.current?.focus();
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, []);

  const stats = useMemo(
    () => ({
      total: allLogs.length,
      server: allLogs.filter((l) => l.category === "Server").length,
      session: allLogs.filter((l) => l.category === "Session").length,
      debug: allLogs.filter((l) => l.level === "Debug").length,
    }),
    [allLogs],
  );

  return (
    <div
      style={{
        fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
        color: "#e4e4e7",
      }}
    >
      <style>{`
        .log-row { transition: background 0.1s; }
        .log-row:hover { background: rgba(255,255,255,0.025) !important; }
        .logs-search:focus { outline: none; border-color: rgba(16,185,129,0.4) !important; box-shadow: 0 0 0 2px rgba(16,185,129,0.08); }
        @keyframes logFadeIn { from { opacity: 0; transform: translateY(2px); } to { opacity: 1; transform: none; } }
        .log-row { animation: logFadeIn 0.12s ease both; }
        ::-webkit-scrollbar { width: 4px; } ::-webkit-scrollbar-thumb { background: #27272a; }
      `}</style>

      <div style={{ marginBottom: 16 }}>
        <div
          style={{
            display: "flex",
            alignItems: "center",
            justifyContent: "space-between",
            marginBottom: 14,
          }}
        >
          <span
            style={{ fontSize: 10, color: "#52525b", letterSpacing: "0.06em" }}
          >
            {filtered.length} of {stats.total} entries
          </span>
          <div style={{ display: "flex", gap: 20 }}>
            {[
              { label: "Total", val: stats.total, color: "#e4e4e7" },
              { label: "Server", val: stats.server, color: "#10b981" },
              { label: "Session", val: stats.session, color: "#3b82f6" },
              { label: "Debug", val: stats.debug, color: "#8b5cf6" },
            ].map((s) => (
              <div key={s.label} style={{ textAlign: "right" }}>
                <div
                  style={{
                    fontSize: 16,
                    fontWeight: 600,
                    color: s.color,
                    lineHeight: 1,
                  }}
                >
                  {s.val}
                </div>
                <div
                  style={{
                    fontSize: 9,
                    color: "#52525b",
                    letterSpacing: "0.08em",
                    marginTop: 2,
                    textTransform: "uppercase",
                  }}
                >
                  {s.label}
                </div>
              </div>
            ))}
          </div>
        </div>

        <div
          style={{
            display: "flex",
            gap: 8,
            alignItems: "center",
            flexWrap: "wrap",
          }}
        >
          <div style={{ position: "relative", flex: "1 1 220px" }}>
            <span
              style={{
                position: "absolute",
                left: 10,
                top: "50%",
                transform: "translateY(-50%)",
                color: "#52525b",
                fontSize: 13,
                pointerEvents: "none",
              }}
            >
              ⌕
            </span>
            <input
              ref={inputRef}
              className="logs-search"
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              placeholder="Search… (⌘K)"
              style={{
                width: "100%",
                background: "#0f0f11",
                border: "1px solid #27272a",
                padding: "7px 10px 7px 28px",
                color: "#e4e4e7",
                fontSize: 12,
                fontFamily: "inherit",
              }}
            />
            {search && (
              <button
                onClick={() => setSearch("")}
                style={{
                  position: "absolute",
                  right: 8,
                  top: "50%",
                  transform: "translateY(-50%)",
                  background: "none",
                  border: "none",
                  color: "#52525b",
                  cursor: "pointer",
                  fontSize: 16,
                  lineHeight: 1,
                  padding: 0,
                }}
              >
                ×
              </button>
            )}
          </div>
          <FilterGroup
            label="Cat"
            options={categories}
            value={categoryFilter}
            onChange={setCategoryFilter}
          />
          <FilterGroup
            label="Event"
            options={events}
            value={eventFilter}
            onChange={setEventFilter}
          />
          <FilterGroup
            label="Level"
            options={levels}
            value={levelFilter}
            onChange={setLevelFilter}
          />
        </div>
      </div>

      <div style={{ border: "1px solid #18181b", overflow: "hidden" }}>
        <div
          style={{
            display: "grid",
            gridTemplateColumns: "44px 72px 88px 100px 90px 1fr",
            padding: "6px 16px",
            borderBottom: "1px solid #18181b",
            background: "#0a0a0b",
          }}
        >
          {["ID", "LEVEL", "CAT", "EVENT", "TIME", "MESSAGE"].map((h) => (
            <span
              key={h}
              style={{
                fontSize: 9,
                color: "#3f3f46",
                letterSpacing: "0.1em",
                fontWeight: 600,
              }}
            >
              {h}
            </span>
          ))}
        </div>

        <div style={{ maxHeight: 520, overflowY: "auto" }}>
          {filtered.length === 0 && (
            <div
              style={{
                padding: "40px 16px",
                textAlign: "center",
                color: "#3f3f46",
                fontSize: 11,
                letterSpacing: "0.06em",
              }}
            >
              NO LOGS MATCH YOUR FILTERS
            </div>
          )}
          {filtered.map((log, i) => {
            const es = EVENT_STYLES[log.event] ?? {
              color: "#a1a1aa",
              bg: "rgba(255,255,255,0.04)",
            };
            const ls = LEVEL_STYLES[log.level] ?? LEVEL_STYLES.Unknown;
            return (
              <div
                key={`${log.id}-${i}`}
                className="log-row"
                style={{
                  display: "grid",
                  gridTemplateColumns: "44px 72px 88px 100px 90px 1fr",
                  padding: "8px 16px",
                  borderBottom: "1px solid rgba(255,255,255,0.03)",
                  alignItems: "start",
                  animationDelay: `${Math.min(i * 6, 160)}ms`,
                }}
              >
                <span style={{ fontSize: 10, color: "#3f3f46", paddingTop: 1 }}>
                  #{log.id}
                </span>

                <span
                  style={{
                    display: "flex",
                    alignItems: "center",
                    gap: 4,
                    paddingTop: 2,
                  }}
                >
                  <span
                    style={{
                      width: 5,
                      height: 5,
                      borderRadius: "50%",
                      background: ls.color,
                      flexShrink: 0,
                    }}
                  />
                  <span style={{ fontSize: 10, color: ls.color }}>
                    {log.level}
                  </span>
                </span>

                <span style={{ fontSize: 10, color: "#71717a", paddingTop: 1 }}>
                  {highlight(log.category, search)}
                </span>

                <span
                  style={{
                    display: "inline-block",
                    fontSize: 9,
                    fontWeight: 600,
                    letterSpacing: "0.05em",
                    color: es.color,
                    background: es.bg,
                    border: `1px solid ${es.color}22`,
                    padding: "2px 5px",
                    width: "fit-content",
                    textTransform: "uppercase",
                  }}
                >
                  {log.event.replace(/^(SERVER_|SESSION_)/, "")}
                </span>

                <div style={{ paddingTop: 1 }}>
                  <div style={{ fontSize: 10, color: "#52525b" }}>
                    {formatTs(log.ts)}
                  </div>
                  <div style={{ fontSize: 9, color: "#3f3f46", marginTop: 1 }}>
                    {formatDate(log.ts)}
                  </div>
                </div>

                <div>
                  <span
                    style={{
                      fontSize: 11,
                      color: "#d4d4d8",
                      lineHeight: 1.5,
                      wordBreak: "break-all",
                    }}
                  >
                    {highlight(log.message, search)}
                  </span>
                  {log.meta && <MetaExpand meta={log.meta} />}
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
