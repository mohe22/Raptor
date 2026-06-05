import { TrendingUp } from "lucide-react";
import { formatBytes } from "../../lib/utils";

interface TrafficPoint {
  time: number;
  rx: number;
  tx: number;
}

interface TrafficGraphProps {
  trafficHistory: TrafficPoint[];
  currentRx: number;
  currentTx: number;
}

export function TrafficGraph({
  trafficHistory,
  currentRx,
  currentTx,
}: TrafficGraphProps) {
  if (trafficHistory.length === 0) {
    return (
      <div className="h-80 flex flex-col items-center justify-center bg-zinc-950 border border-zinc-800 rounded-xl">
        <TrendingUp className="h-12 w-12 text-zinc-600 mb-4" />
        <p className="text-zinc-400">Traffic graph will appear here</p>
        <p className="text-[10px] text-zinc-500 mt-1">
          Waiting for data updates...
        </p>
      </div>
    );
  }

  const maxRx = Math.max(...trafficHistory.map((p) => p.rx), 10);
  const maxTx = Math.max(...trafficHistory.map((p) => p.tx), 10);

  const width = 620;
  const height = 260;
  const padding = 32;

  const pointsRx: string[] = [];
  const pointsTx: string[] = [];

  trafficHistory.forEach((point, i) => {
    const x =
      padding + (i / (trafficHistory.length - 1)) * (width - padding * 2);
    const yRx =
      padding +
      (height - padding * 2) -
      (point.rx / maxRx) * (height - padding * 2);
    const yTx =
      padding +
      (height - padding * 2) -
      (point.tx / maxTx) * (height - padding * 2);

    pointsRx.push(`${x.toFixed(2)},${yRx.toFixed(2)}`);
    pointsTx.push(`${x.toFixed(2)},${yTx.toFixed(2)}`);
  });

  return (
    <div className="relative h-80 bg-zinc-950 border border-zinc-800  overflow-hidden">
      <svg className="w-full h-full" viewBox={`0 0 ${width} ${height}`}>
        {/* Subtle grid */}
        {[0.2, 0.4, 0.6, 0.8].map((ratio, i) => (
          <line
            key={i}
            x1={padding}
            y1={padding + (height - padding * 2) * ratio}
            x2={width - padding}
            y2={padding + (height - padding * 2) * ratio}
            stroke="#1a1f24"
            strokeWidth="1"
          />
        ))}

        {/* RX Area Fill */}
        <polyline
          points={`${pointsRx[0]} ${pointsRx.join(" ")} ${width - padding},${height - padding} ${padding},${height - padding}`}
          fill="rgba(34, 197, 94, 0.08)"
          stroke="none"
        />

        {/* TX Area Fill */}
        <polyline
          points={`${pointsTx[0]} ${pointsTx.join(" ")} ${width - padding},${height - padding} ${padding},${height - padding}`}
          fill="rgba(59, 130, 246, 0.08)"
          stroke="none"
        />

        {/* RX Line */}
        <polyline
          points={pointsRx.join(" ")}
          fill="none"
          stroke="#22c55e"
          strokeWidth="3.5"
          strokeLinejoin="round"
          strokeLinecap="round"
        />

        {/* TX Line */}
        <polyline
          points={pointsTx.join(" ")}
          fill="none"
          stroke="#3b82f6"
          strokeWidth="3.5"
          strokeLinejoin="round"
          strokeLinecap="round"
        />
      </svg>

      {/* Legend */}
      <div className="absolute top-5 left-5 flex flex-col gap-2 text-xs font-mono z-10">
        <div className="flex items-center gap-2 bg-zinc-900/90 backdrop-blur-sm px-3.5 py-1.5  border border-green-500/20">
          <div className="w-3 h-px bg-green-500 rounded" />
          <span className="text-green-400">RX ↓ {formatBytes(currentRx)}</span>
        </div>
        <div className="flex items-center gap-2 bg-zinc-900/90 backdrop-blur-sm px-3.5 py-1.5  border border-blue-500/20">
          <div className="w-3 h-px bg-blue-500 rounded" />
          <span className="text-blue-400">TX ↑ {formatBytes(currentTx)}</span>
        </div>
      </div>

      {/* Live indicator */}
      <div className="absolute bottom-5 right-5 px-3 py-1 text-[10px] font-mono bg-zinc-900/80 text-emerald-500 rounded-full border border-emerald-500/30">
        LIVE • {trafficHistory.length} pts
      </div>
    </div>
  );
}
