import type { log } from "./logs";

export type IpType = "IPv4" | "IPv6" | "domain";
export type Status = "running" | "paused" | "error" | "stopped";
export type ServerType = "TCP" | "UDP" | "HTTP" | "HTTPS" | "DNS";
export interface CreateServerPayload {
  name: string;
  ip: string;
  port: number;
  type: ServerType;
}
export interface UpdateServerPayload {
  originalName: string;
  newName: string;
  ip: string;
  port: number;
}
export interface Config {
  ip: string;
  port: number;
  ipType: IpType;
  timeout: number;
  instanceName: string;
}

export interface ServerInfo {
  config: Config;
  status: Status;
  sessionCounter: number;
  type: ServerType;
  error?: string; // if there is error
  rxBytes: number;
  txBytes: number;
}

export type getAllServersResponse = ServerInfo[];

export interface ServerEntry {
  name: string;
  ipAddress: string;
  port: number;
  type: ServerType;
  status: Status;
  sessionCount: number;
  bytesSent: number;
  bytesReceived: number;
  startTime: number; // seconds
}

export interface ServerPoolStatus {
  runningServerCount: number;
  totalServerCount: number;
  totalSessionCount: number;
  activeSessionCount: number;
  totalBytesReceived: number;
  totalBytesSent: number;
  servers: ServerEntry[];

  serversLogs: log[];
  sessionLogs: log[];
}
