import { useState } from "react";
import { logLevelColor } from "../../lib/data";
import type { log } from "../../types/logs";
import { cn } from "../../lib/utils";
import { ChevronDown } from "lucide-react";

export function LogRow({ log }: { log: log }) {
  const color = logLevelColor[log.level];
  const [expanded, setExpanded] = useState(false);

  let meta: unknown = null;
  try {
    meta = JSON.parse(log.meta);
  } catch {
    meta = null;
  }

  const time = new Date(log.ts * 1000);
  return (
    <div
      onClick={() => meta && setExpanded(!expanded)}
      className={cn(
        "px-4 py-1.5 font-mono text-[11px] transition-colors border-l-2 border-border hover:bg-secondary/20",
        meta && "cursor-pointer",
      )}
    >
      <span className="text-muted-foreground mr-2">
        {time.toLocaleDateString([], { month: "2-digit", day: "2-digit" })}{" "}
        {time.toLocaleTimeString([], {
          hour: "2-digit",
          minute: "2-digit",
          second: "2-digit",
        })}
      </span>

      <span className={cn("uppercase font-bold mr-2", color)}>
        [{log.level}]
      </span>

      <span className="text-muted-foreground mr-2">[{log.category}]</span>

      <span className="text-chart-2 mr-2">{log.event}</span>

      <span className="text-foreground/80">{log.message}</span>

      {meta && (
        <ChevronDown
          className={cn(
            "inline h-3 w-3 ml-1 text-muted-foreground transition-transform align-middle",
            expanded && "rotate-180",
          )}
        />
      )}

      {expanded && meta && (
        <pre
          className={cn(
            "mt-1 text-[10px] overflow-x-auto whitespace-pre-wrap",
            color,
          )}
        >
          {JSON.stringify(meta, null, 2)}
        </pre>
      )}
    </div>
  );
}
