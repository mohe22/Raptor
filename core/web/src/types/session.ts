import type { OSKey } from "../lib/data";
import type { ServerType } from "./server";

export type SessionStatus =
  | "Idle"
  | "Connected"
  | "Disconnected"
  | "Disconnecting";

export interface BriefSession {
  id: string;
  protocol: ServerType;
  status: SessionStatus;
  idleSeconds: number;
  remoteAddress: string;
  connectedTo: string;
  os: OSKey;
  username: string;
  hostname: string;
  timezone: string;
}

export interface SessionDetails extends BriefSession {
  arch: string;
  domain: string;
  homeDir: string;
  shell: string;
  isSudoer: boolean;
  isAdmin: boolean;
  osVersion: string;
  kernelVersion: string;
  cpu: string;
  cpuCores: number;
  ramBytes: number;
  diskTotalBytes: number;
  diskFreeBytes: number;
  pid: number;
  processName: string;
  processPath: string;
  uptimeSystem: number;
  uptimeSeconds: number;
  isDocker: boolean;
  isVM: boolean;
  vmType: string;
  internalIp: string;
  internalIp2: string;
  macAddress: string;
  defaultGateway: string;
  dnsServer: string;
  isProxy: boolean;
  proxyUrl: string;
  isDomainJoined: boolean;
  selinuxEnabled: boolean;
  apparmorEnabled: boolean;
  locale: string;
}

export const TaskPriority = {
  Low: 0,
  Normal: 1,
  High: 2,
  Critical: 3,
} as const;

export type TaskPriority = (typeof TaskPriority)[keyof typeof TaskPriority];
export type Shelltype = "pty" | "pip";

export interface PacketHeader {
  packetId: number;
  type: number; // 0=FileUpload, 1=FileDownload, 2=Command, 3=Register
  flags: number; // Bitmask of flags
  payloadSize: number;
}

export interface ExecuteCommand {
  sessionId: string;
  commandId: number;
  serverId: string;
  command: string;
  priority: TaskPriority;
  shellType: Shelltype;
}

export interface PacketHeader {
  packetId: number;
  type: number; // 0=FileUpload, 1=FileDownload, 2=Command, 3=Register
  flags: number; // Bitmask of flags
}
