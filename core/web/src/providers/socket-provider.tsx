import { type ReactNode, useEffect, useRef, useState } from "react";
import { SocketContext } from "./socket-context";
import { RaptorWsClient } from "../lib/socket";

const WS_URL = import.meta.env.VITE_WS_URL ?? "ws://localhost:8000/ws";

interface SocketProviderProps {
  children: ReactNode;
  /** ms between reconnect attempts — default 3000 */
  reconnectDelay?: number;
}

export function SocketProvider({
  children,
  reconnectDelay = 3000,
}: SocketProviderProps) {
  const clientRef = useRef<RaptorWsClient | null>(null);

  const [client, setClient] = useState<RaptorWsClient | null>(null);
  const [connected, setConnected] = useState(false);
  useEffect(() => {
    let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
    let unmounted = false;

    function createClient() {
      const ws = new RaptorWsClient(WS_URL);
      clientRef.current = ws;

      ws.onOpen = () => {
        if (!unmounted) {
          setClient(ws);
          setConnected(true);
        }
      };
      ws.onClose = () => {
        if (!unmounted) {
          setClient(null);
          setConnected(false);
          reconnectTimer = setTimeout(createClient, reconnectDelay);
        }
      };
      ws.onError = (e) => {
        console.error("[SocketProvider] WebSocket error:", e);
      };

      ws.onUnhandled = (frame) => {
        console.warn("[SocketProvider] unhandled frame:", frame);
      };

      ws.connect();
    }

    createClient();

    return () => {
      unmounted = true;
      if (reconnectTimer) clearTimeout(reconnectTimer);
      clientRef.current?.disconnect();
      clientRef.current = null;
      setClient(null);
    };
  }, [reconnectDelay]);

  return (
    <SocketContext.Provider value={{ client, connected }}>
      {children}
    </SocketContext.Provider>
  );
}
