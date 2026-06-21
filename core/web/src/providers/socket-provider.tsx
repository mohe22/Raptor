import { type ReactNode, useEffect, useRef, useState } from "react";
import {
  RaptorWsClient,
  WsCmd,
  WsStatus,
  type WsJsonResponse,
} from "../lib/socket";
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

function addSessionToCache(serverId: string, session: Partial<BriefSession>) {
  queryClient.setQueryData(
    SESSION_QUERY_KEYS.all(serverId),
    (old: BriefSession[] = []) => {
      if (old.some((s) => s.id === session.id)) return old;
      return [...old, session];
    },
  );
}
function removeSessionFromCache(serverId: string, sessionId: string) {
  queryClient.setQueryData(
    SESSION_QUERY_KEYS.all(serverId),
    (old: BriefSession[] = []) => old.filter((s) => s.id !== sessionId),
  );
}
function adjustSessionCount(serverId: string, delta: 1 | -1) {
  queryClient.setQueryData(SERVER_QUERY_KEYS.all, (old: ServerInfo[] = []) =>
    old.map((s) =>
      s.config.instanceName === serverId
        ? { ...s, sessionCounter: Math.max(0, s.sessionCounter + delta) }
        : s,
    ),
  );

  queryClient.setQueryData(
    SERVER_QUERY_KEYS.poolStatus,
    (old: ServerPoolStatus | undefined) => {
      if (!old) return old;
      return {
        ...old,
        totalSessionCount: Math.max(0, old.totalSessionCount + delta),
        activeSessionCount: Math.max(0, old.activeSessionCount + delta),
        servers: old.servers.map((s) =>
          s.name === serverId
            ? { ...s, sessionCount: Math.max(0, s.sessionCount + delta) }
            : s,
        ),
      };
    },
  );
}

function handleSessionConnected(
  res: WsJsonResponse<typeof WsCmd.SessionConnected>,
) {
  if (res.status !== WsStatus.Ok) {
    console.warn("[SessionConnected] server returned error:", res.error);
    return;
  }

  const { id, hostname, os, username, timezone, serverId, remoteAddress } =
    res.data;

  const session: Partial<BriefSession> = {
    id,
    hostname,
    os,
    username,
    timezone,
    remoteAddress,
    connectedTo: serverId,
    status: "Connected",
    idleSeconds: 0,
  };

  addSessionToCache(serverId, session);
  adjustSessionCount(serverId, 1);
}

function handleSessionDisconnected(
  res: WsJsonResponse<typeof WsCmd.SessionDisconnected>,
) {
  if (res.status !== WsStatus.Ok) {
    console.warn("[SessionDisconnected] server returned error:", res.error);
    return;
  }

  const { id, serverId } = res.data;

  removeSessionFromCache(serverId, id);
  adjustSessionCount(serverId, -1);
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

      ws.on(WsCmd.SessionConnected, handleSessionConnected);
      ws.on(WsCmd.SessionDisconnected, handleSessionDisconnected);

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
