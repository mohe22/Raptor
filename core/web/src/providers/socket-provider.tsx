import { type ReactNode, useEffect, useRef, useState } from "react";
import { RaptorWsClient, WsCmd, WsStatus } from "../lib/socket";
import { queryClient } from "../main";
import { SESSION_QUERY_KEYS } from "../features/session/queries";
import type { BriefSession } from "../types/session";
import { SERVER_QUERY_KEYS } from "../features/servers/queries";
import type { ServerInfo, ServerPoolStatus } from "../types/server";
import { SocketContext } from "./socket-context";

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
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout> | undefined>(
    undefined,
  );
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
      };

      ws.onClose = () => {
        if (unmountedRef.current) return;
        setClient(null);
        setConnected(false);
        reconnectTimerRef.current = setTimeout(createClient, reconnectDelay);
      };

      ws.onError = (e) => {
        console.error("[SocketProvider] WebSocket error:", e);
      };

      ws.onUnhandled = (res) => {
        console.warn("[SocketProvider] unhandled message:", res);
      };

      ws.on(WsCmd.NewSession, (res) => {
        const { data, status } = res;
        if (status !== WsStatus.Ok) {
          console.warn("[NewSession] server returned error:", res.error);
          return;
        }
        console.log(data);

        // 1. Add it to the session list for that server
        queryClient.setQueryData(
          SESSION_QUERY_KEYS.all(data.serverId),
          (old: BriefSession[] = []) => {
            if (old.some((s) => s.id === data.id)) return old;
            return [...old, data];
          },
        );

        // 2. server's sessionCounter in the "all servers" list
        queryClient.setQueryData(
          SERVER_QUERY_KEYS.all,
          (old: ServerInfo[] = []) =>
            old.map((s) =>
              s.config.instanceName === data.serverId
                ? { ...s, sessionCounter: s.sessionCounter + 1 }
                : s,
            ),
        );

        // 3. update counts in the pool status summary
        queryClient.setQueryData(
          SERVER_QUERY_KEYS.poolStatus,
          (old: ServerPoolStatus | undefined) => {
            if (!old) return old;
            return {
              ...old,
              totalSessionCount: old.totalSessionCount + 1,
              activeSessionCount: old.activeSessionCount + 1,
              servers: old.servers.map((s) =>
                s.name === data.serverId
                  ? { ...s, sessionCount: s.sessionCount + 1 }
                  : s,
              ),
            };
          },
        );
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
    };
  }, [reconnectDelay]);

  return (
    <SocketContext.Provider value={{ client, connected }}>
      {children}
    </SocketContext.Provider>
  );
}
