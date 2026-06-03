import {
  Activity,
  AlertTriangle,
  Bug,
  Clock,
  Cpu,
  FileText,
  Folder,
  Globe,
  HardDrive,
  Info,
  Radio,
  Server,
  Shield,
  Skull,
  Wifi,
  XCircle,
} from "lucide-react";
import type { ServerType } from "../types/server";

export const SESSION_STATUS_DOT: Record<string, string> = {
  Connected: "status-online animate-pulse",
  Idle: "status-warning animate-pulse",
  Disconnecting: "status-error animate-pulse",
  Disconnected: "status-offline",
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

export const sessionOSConfig: Record<OSKey, OSConfig> = OS_CONFIGS;

/**
 * Get country flag emoji from timezone
 * @param {string} timezone - e.g. "Asia/Kuala_Lumpur"
 * @returns {string} Flag emoji or fallback "🌍"
 */
export function getFlagByTimezone(timezone) {
  if (!timezone || typeof timezone !== "string") {
    return "🌍";
  }

  const tz = timezone.trim().toLowerCase();

  const timezoneMap = {
    // Malaysia
    "asia/kuala_lumpur": "🇲🇾",
    "asia/kuching": "🇲🇾",

    // Singapore
    "asia/singapore": "🇸🇬",

    // Indonesia
    "asia/jakarta": "🇮🇩",
    "asia/makassar": "🇮🇩",
    "asia/jayapura": "🇮🇩",

    // Thailand
    "asia/bangkok": "🇹🇭",

    // Vietnam
    "asia/ho_chi_minh": "🇻🇳",

    // Philippines
    "asia/manila": "🇵🇭",

    // Japan
    "asia/tokyo": "🇯🇵",

    // South Korea
    "asia/seoul": "🇰🇷",

    // China
    "asia/shanghai": "🇨🇳",
    "asia/hong_kong": "🇭🇰",
    "asia/taipei": "🇹🇼",

    // India
    "asia/kolkata": "🇮🇳",
    "asia/new_delhi": "🇮🇳",

    // Saudi Arabia / GCC
    "asia/riyadh": "🇸🇦",
    "asia/dubai": "🇦🇪",

    // Turkey
    "europe/istanbul": "🇹🇷",

    // UK
    "europe/london": "🇬🇧",

    // USA
    "america/new_york": "🇺🇸",
    "america/chicago": "🇺🇸",
    "america/los_angeles": "🇺🇸",
    "america/denver": "🇺🇸",

    // Brazil
    "america/sao_paulo": "🇧🇷",

    // Germany
    "europe/berlin": "🇩🇪",
    "europe/paris": "🇫🇷",

    // Australia
    "australia/sydney": "🇦🇺",
    "australia/melbourne": "🇦🇺",
  };

  // Direct match
  if (timezoneMap[tz]) {
    return timezoneMap[tz];
  }

  // Partial matching (more flexible)
  if (tz.includes("kuala_lumpur") || tz.includes("malaysia")) return "🇲🇾";
  if (tz.includes("singapore")) return "🇸🇬";
  if (tz.includes("jakarta") || tz.includes("indonesia")) return "🇮🇩";
  if (tz.includes("bangkok") || tz.includes("thailand")) return "🇹🇭";
  if (tz.includes("tokyo") || tz.includes("japan")) return "🇯🇵";
  if (tz.includes("seoul") || tz.includes("korea")) return "🇰🇷";
  if (tz.includes("shanghai") || tz.includes("china")) return "🇨🇳";
  if (tz.includes("dubai") || tz.includes("uae")) return "🇦🇪";
  if (tz.includes("riyadh") || tz.includes("saudi")) return "🇸🇦";
  if (tz.includes("london") || tz.includes("gmt")) return "🇬🇧";
  if (tz.includes("new_york") || tz.includes("america")) return "🇺🇸";
  if (tz.includes("sydney") || tz.includes("australia")) return "🇦🇺";

  // Default
  return "🌍";
}

export const serverTypeConfig: Record<
  ServerType,
  { label: string; color: string; icon: string }
> = {
  HTTP: { label: "HTTP", color: "text-chart-1", icon: "globe" },
  TCP: { label: "TCP", color: "text-chart-3", icon: "radio" },
  DNS: { label: "DNS", color: "text-chart-5", icon: "server" },
  UDP: { label: "UDP", color: "text-chart-2", icon: "shield" },
};
export const logLevelColor = {
  Debug: "text-slate-400",
  Info: "text-blue-400",
  Warn: "text-yellow-400",
  Error: "text-red-400",
  Fatal: "text-red-600",
  Unknown: "text-muted-foreground",
} as const;

export const logLevelIcon = {
  Debug: Bug,
  Info: Info,
  Warn: AlertTriangle,
  Error: XCircle,
  Fatal: Skull,
  Unknown: Info,
} as const;
