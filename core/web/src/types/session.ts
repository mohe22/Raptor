import type { ServerType } from "./server";

export type SessionStatus =
  | "Idle"
  | "Connected"
  | "Disconnected"
  | "Disconnecting";

export interface Session {
  id: string;
  protocol: ServerType;
  status: SessionStatus;
  idleSeconds: number;
  uptimeSeconds: number;
  remoteAddress: string;
  connectedTo: string;
}
