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
