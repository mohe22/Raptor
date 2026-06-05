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

export interface SessionDetails {
  arch: string;
  connectedAtNs: number;
  dns: string;
  domain: string;
  homeDir: string;
  hostname: string;
  id: number;
  idleSeconds: number;
  internalIp: string;
  isAdmin: boolean;
  isDocker: boolean;
  isDomainJoined: boolean;
  isSudoer: boolean;
  isVm: boolean;
  locale: boolean;
  macAddress: string;
  os: OSKey;
  pid: number;
  processName: string;
  processPath: string;
  protocol: ServerType;
  remoteAddress: string;
  shell: string;
  status: SessionStatus;
  timezone: string;
  username: string;
}
