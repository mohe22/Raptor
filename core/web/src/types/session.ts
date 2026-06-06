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
