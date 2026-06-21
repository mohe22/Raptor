import {
  Box,
  Clock,
  Cpu,
  HardDrive,
  Network,
  Server,
  ShieldCheck,
  Terminal,
  User,
} from "lucide-react";
import { Link } from "react-router";
import type { OSConfig } from "../../lib/data";
import type { SessionDetails } from "../../types/session";
import { formatBytes, formatUptime } from "../../lib/utils";

function Seg({
  icon: Icon,
  label,
  value,
  accent,
  children,
}: {
  icon: React.ElementType;
  label?: string;
  value?: React.ReactNode;
  accent?: boolean;
  children?: React.ReactNode;
}) {
  return (
    <div className="flex items-center gap-1.5 px-3 h-full border-r border-zinc-800 last:border-r-0 shrink-0">
      <Icon className="w-3 h-3 text-zinc-500 shrink-0" />
      {label && (
        <span className="text-[10px] text-zinc-600 uppercase tracking-wider font-mono">
          {label}
        </span>
      )}
      <span
        className={`text-xs font-mono ${accent ? "text-emerald-400" : "text-zinc-300"}`}
      >
        {value ?? children}
      </span>
    </div>
  );
}

export function StatusBar({
  data,
  config,
}: {
  data: SessionDetails;
  config: OSConfig;
}) {
  return (
    <div
      style={{ scrollbarWidth: "none", msOverflowStyle: "none" }}
      className="flex items-stretch min-h-9 border-b border-zinc-800 bg-zinc-950 overflow-x-auto"
    >
      <div className="flex items-center gap-2 px-3 border-r border-zinc-800 shrink-0">
        <span className="text-sm leading-none">{config.emoji}</span>
        <span className="text-xs font-mono text-emerald-400">
          {data.hostname}
        </span>
      </div>

      <Seg icon={Cpu} value={`${data.os} ${data.osVersion}`}>
        <span className="text-zinc-500 ml-1 text-[10px]">{data.arch}</span>
      </Seg>

      <Seg icon={Server}>
        <Link
          className="text-xs font-mono text-zinc-300 hover:text-emerald-400 transition-colors"
          to={`/servers/${data.connectedTo}`}
        >
          {data.connectedTo}
        </Link>
      </Seg>

      <Seg icon={User}>
        <span className="text-zinc-300 font-mono text-xs">{data.username}</span>
        {data.isAdmin && (
          <span className="ml-1.5 text-[9px] font-mono px-1 py-px  bg-red-950 text-red-400 border border-red-800">
            admin
          </span>
        )}
      </Seg>

      <Seg icon={Network} value={data.internalIp} />
      <Seg icon={Network} value={data.remoteAddress} />
      <Seg icon={Cpu} value={`${data.cpuCores} cores`} />
      <Seg icon={HardDrive} value={formatBytes(data.ramBytes)} />
      <Seg icon={Terminal} label="PID" value={String(data.pid)} accent />
      <Seg icon={Clock} value={formatUptime(data.uptimeSystem)} accent />

      {data.isAdmin && (
        <Seg icon={ShieldCheck}>
          <span className="text-[9px] font-mono text-emerald-400">
            elevated
          </span>
        </Seg>
      )}

      {data.isVM && (
        <Seg icon={Box}>
          <span className="text-[9px] font-mono px-1 py-px rounded bg-blue-950 text-blue-400 border border-blue-800">
            VM · {data.vmType}
          </span>
        </Seg>
      )}

      {data.isDocker && (
        <Seg icon={Box}>
          <span className="text-[9px] font-mono px-1 py-px rounded bg-sky-950 text-sky-400 border border-sky-800">
            docker
          </span>
        </Seg>
      )}
    </div>
  );
}
