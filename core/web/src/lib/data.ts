import {
  Activity,
  Clock,
  Cpu,
  FileText,
  Folder,
  Globe,
  HardDrive,
  Radio,
  Server,
  Shield,
  Wifi,
} from "lucide-react";
export const STATUS_LABEL: Record<string, string> = {
  running: "text-primary",
  paused: "text-yellow-400",
  error: "text-destructive",
  stopped: "text-muted-foreground",
};
export const STATUS_DOT: Record<string, string> = {
  running: "status-online animate-pulse",
  paused: "status-warning animate-pulse",
  error: "status-error animate-pulse",
  stopped: "status-offline",
};
export const iconMap: Record<
  string,
  React.ComponentType<{ className?: string }>
> = {
  globe: Globe,
  shield: Shield,
  radio: Radio,
  server: Server,
  folder: Folder,
  "hard-drive": HardDrive,
  // plugin icons
  activity: Activity,
  "file-text": FileText,
  wifi: Wifi,
  cpu: Cpu,
  clock: Clock,
};

export interface OSConfig {
  icon: string;
  emoji: string;
  color: string;
  label: string;
  bgColor: string;
}

const OS_CONFIGS = {
  Windows: {
    icon: "windows",
    emoji: "🪟",
    color: "text-blue-400",
    label: "Windows",
    bgColor: "bg-blue-950/50",
  },
  Ubuntu: {
    icon: "linux",
    emoji: "🐧",
    color: "text-orange-400",
    label: "Ubuntu",
    bgColor: "bg-orange-950/50",
  },
  Debian: {
    icon: "linux",
    emoji: "🐧",
    color: "text-red-400",
    label: "Debian",
    bgColor: "bg-red-950/50",
  },
  "Red Hat": {
    icon: "red-hat",
    emoji: "🎩",
    color: "text-red-500",
    label: "Red Hat",
    bgColor: "bg-red-950/50",
  },
  CentOS: {
    icon: "centos",
    emoji: "🐧",
    color: "text-yellow-400",
    label: "CentOS",
    bgColor: "bg-yellow-950/50",
  },
  Fedora: {
    icon: "linux",
    emoji: "🐧",
    color: "text-blue-300",
    label: "Fedora",
    bgColor: "bg-blue-950/50",
  },
  Arch: {
    icon: "linux",
    emoji: "🐧",
    color: "text-cyan-400",
    label: "Arch",
    bgColor: "bg-cyan-950/50",
  },
  Kali: {
    icon: "linux",
    emoji: "🐧",
    color: "text-blue-500",
    label: "Kali",
    bgColor: "bg-blue-950/50",
  },
  macOS: {
    icon: "macos",
    emoji: "🍎",
    color: "text-gray-300",
    label: "macOS",
    bgColor: "bg-gray-800/70",
  },
  Linux: {
    icon: "linux",
    emoji: "🐧",
    color: "text-emerald-400",
    label: "Linux",
    bgColor: "bg-emerald-950/50",
  },
  Unknown: {
    icon: "unknown",
    emoji: "❓",
    color: "text-gray-400",
    label: "Unknown",
    bgColor: "bg-gray-800",
  },
} as const satisfies Record<string, OSConfig>;

export type OSKey = keyof typeof OS_CONFIGS;

// Ordered match rules: checked top-to-bottom against lowercased os string.
// More specific strings (e.g. "red hat") must come before broader ones ("linux").
const OS_MATCH_RULES: Array<[pattern: string, key: OSKey]> = [
  ["windows", "Windows"],
  ["ubuntu", "Ubuntu"],
  ["debian", "Debian"],
  ["red hat", "Red Hat"],
  ["redhat", "Red Hat"],
  ["rhel", "Red Hat"],
  ["centos", "CentOS"],
  ["fedora", "Fedora"],
  ["arch", "Arch"],
  ["kali", "Kali"],
  ["macos", "macOS"],
  ["darwin", "macOS"],
  ["osx", "macOS"],
  ["linux", "Linux"],
];

/**
 * Returns the OSConfig for any OS string coming from the agent/server.
 * Performs case-insensitive substring matching so strings like
 * "Windows 11 Pro", "Ubuntu 22.04 LTS", "Windows Server 2022" all resolve
 * correctly without requiring an exact key match.
 */
export function getOSConfig(os: string): OSConfig {
  if (!os) return OS_CONFIGS.Unknown;

  const lower = os.toLowerCase();

  for (const [pattern, key] of OS_MATCH_RULES) {
    if (lower.includes(pattern)) return OS_CONFIGS[key];
  }

  return OS_CONFIGS.Unknown;
}
