import { type ReactNode, useEffect, useRef, useState } from "react";
import { SocketContext } from "./socket-context";
import { RaptorWsClient, WsCmd } from "../lib/socket";
import { formatConnectionTime } from "../lib/utils";

const WS_URL = import.meta.env.VITE_WS_URL ?? "ws://localhost:8000/ws";

interface SocketProviderProps {
  children: ReactNode;
  reconnectDelay?: number;
}

export function SocketProvider({
  children,
  reconnectDelay = 3000,
}: SocketProviderProps) {
  const clientRef = useRef<RaptorWsClient | null>(null);
  const [client, setClient] = useState<RaptorWsClient | null>(null);
  const [connected, setConnected] = useState(false);
  const [sessionId, setSessionId] = useState<string | null>(null);
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout>>();
  const unmountedRef = useRef(false);

  useEffect(() => {
    unmountedRef.current = false;

    function createClient() {
      const ws = new RaptorWsClient(WS_URL);
      clientRef.current = ws;

      ws.onOpen = () => {
        if (unmountedRef.current) return;
        setClient(ws);
        setConnected(true);
        // Send a NewSession as soon as the socket is open so the server can
        // associate this connection with a known client identity.
        ws.newSession(generateClientId());
      };

      ws.onClose = () => {
        if (unmountedRef.current) return;
        setClient(null);
        setConnected(false);
        setSessionId(null);
        reconnectTimerRef.current = setTimeout(createClient, reconnectDelay);
      };

      ws.onError = (e) => {
        console.error("[SocketProvider] WebSocket error:", e);
      };

      ws.onUnhandled = (res) => {
        console.warn("[SocketProvider] unhandled message:", res);
      };

      // Store the sessionId returned by the server so downstream consumers
      // can include it in subsequent requests.
      ws.on(WsCmd.NewSession, (res) => {
        console.log(res);
      });

      ws.connect();
    }

    createClient();

    return () => {
      unmountedRef.current = true;
      clearTimeout(reconnectTimerRef.current);
      clientRef.current?.disconnect();
      clientRef.current = null;
      setClient(null);
      setSessionId(null);
    };
  }, [reconnectDelay]);

  return (
    <SocketContext.Provider value={{ client, connected, sessionId }}>
      {children}
    </SocketContext.Provider>
  );
}

function generateClientId(): string {
  return `client-${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;
}
