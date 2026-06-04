import { createContext, useContext } from "react";
import type { RaptorWsClient } from "../lib/socket";

export interface SocketContextValue {
  client: RaptorWsClient | null;
  connected: boolean;
}
// The context value is the client instance, or null before it is created.
export const SocketContext = createContext<SocketContextValue | null>(null);

// Typed hook — throws if used outside <SocketProvider>
export function useSocket(): SocketContextValue {
  const ctx = useContext(SocketContext);
  if (!ctx) throw new Error("useSocket must be used inside <SocketProvider>");
  return ctx;
}
