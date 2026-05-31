export type IpType = "IPv4" | "IPv6" | "domain";
export type Status = "running" | "paused" | "error" | "stopped";
export type ServerType = "TCP" | "UDP" | "HTTP";

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
