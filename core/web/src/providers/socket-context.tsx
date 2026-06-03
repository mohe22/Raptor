import { createContext, useContext } from "react";
import type { RaptorWsClient } from "../lib/socket";

// The context value is the client instance, or null before it is created.
export const SocketContext = createContext<RaptorWsClient | null>(null);

// Typed hook — throws if used outside <SocketProvider>
export function useSocket(): RaptorWsClient {
  const ctx = useContext(SocketContext);
  if (!ctx) throw new Error("useSocket must be used inside <SocketProvider>");
  return ctx;
}

// Hook that returns null when no connection exists (safe anywhere in the tree)
export function useSocketMaybe(): RaptorWsClient | null {
  return useContext(SocketContext);
}
