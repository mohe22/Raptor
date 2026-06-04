export type LogLevel =
  | "Debug"
  | "Info"
  | "Warn"
  | "Error"
  | "Fatal"
  | "Unknown";

export type LogCategory = "System" | "Server" | "Agent" | "Session" | "Unknown";

export interface log {
  id: number;
  ts: number;
  level: LogLevel;
  category: LogCategory;
  event: string;
  message: string;
  meta: string;
}
