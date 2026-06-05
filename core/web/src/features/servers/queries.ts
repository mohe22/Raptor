import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import {
  createServer,
  getAllServers,
  getServerById,
  pauseServer,
  poolStatus,
  resumeServer,
  stopServer,
} from "./api";
import type {
  CreateServerPayload,
  ServerInfo,
  ServerPoolStatus,
  UpdateServerPayload,
} from "../../types/server";
import { updateServer } from "../session/api";

export const SERVER_QUERY_KEYS = {
  all: ["servers"] as const,
  byId: (id: string) => ["servers", id] as const,
  poolStatus: ["servers", "pool-status"] as const,
} as const;

const REFRESH_INTERVAL_MS = 2 * 60 * 1000; // 2 min
const REFRESH_INTERVAL_MAX_MS = 5 * 60 * 1000; // 5 min

export const useGetAllServers = () =>
  useQuery({
    queryKey: SERVER_QUERY_KEYS.all,
    queryFn: getAllServers,
    refetchInterval: REFRESH_INTERVAL_MAX_MS,
    staleTime: REFRESH_INTERVAL_MAX_MS,
  });

export const usePoolStatus = () =>
  useQuery({
    queryKey: SERVER_QUERY_KEYS.poolStatus,
    queryFn: poolStatus,
    refetchInterval: REFRESH_INTERVAL_MS,
    staleTime: REFRESH_INTERVAL_MS,
  });
export const usePauseServer = () => {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: pauseServer,
    onSuccess: (_, name) => {
      qc.setQueryData(SERVER_QUERY_KEYS.all, (old: ServerInfo[] = []) =>
        old.map((s) =>
          s.config.instanceName === name ? { ...s, status: "paused" } : s,
        ),
      );
      qc.setQueryData(
        SERVER_QUERY_KEYS.poolStatus,
        (old: ServerPoolStatus | undefined) => {
          if (!old) return old;
          return {
            ...old,
            runningServerCount: old.runningServerCount - 1,
            servers: old.servers.map((s) =>
              s.name === name ? { ...s, status: "paused" } : s,
            ),
          };
        },
      );
    },
  });
};

export const useResumeServer = () => {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: resumeServer,
    onSuccess: (_, name) => {
      qc.setQueryData(SERVER_QUERY_KEYS.all, (old: ServerInfo[] = []) =>
        old.map((s) =>
          s.config.instanceName === name ? { ...s, status: "running" } : s,
        ),
      );
      qc.setQueryData(
        SERVER_QUERY_KEYS.poolStatus,
        (old: ServerPoolStatus | undefined) => {
          if (!old) return old;
          return {
            ...old,
            runningServerCount: old.runningServerCount + 1,
            servers: old.servers.map((s) =>
              s.name === name ? { ...s, status: "running" } : s,
            ),
          };
        },
      );
    },
  });
};

export const useStopServer = () => {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: stopServer,
    onSuccess: (_, name) => {
      qc.setQueryData(SERVER_QUERY_KEYS.all, (old: ServerInfo[] = []) =>
        old.filter((s) => s.config.instanceName !== name),
      );
      qc.setQueryData(
        SERVER_QUERY_KEYS.poolStatus,
        (old: ServerPoolStatus | undefined) => {
          if (!old) return old;
          const wasRunning =
            old.servers.find((s) => s.name === name)?.status === "running";
          return {
            ...old,
            totalServerCount: old.totalServerCount - 1,
            runningServerCount: wasRunning
              ? old.runningServerCount - 1
              : old.runningServerCount,
            servers: old.servers.filter((s) => s.name !== name),
          };
        },
      );
    },
  });
};

export const useCreateServer = () => {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: (payload: CreateServerPayload) => createServer(payload),
    onSuccess: (_, payload) => {
      // Invalidate rather than optimistically patch  we don't have the full
      // ServerInfo shape (uptimeSeconds, rxBytes, etc.) client-side yet.
      qc.invalidateQueries({ queryKey: SERVER_QUERY_KEYS.all });
      qc.setQueryData(
        SERVER_QUERY_KEYS.poolStatus,
        (old: ServerPoolStatus | undefined) => {
          if (!old) return old;
          const newEntry = {
            name: payload.name,
            ipAddress: payload.ip,
            port: payload.port,
            type: payload.type,
            status: "running" as const,
            sessionCount: 0,
            bytesSent: 0,
            bytesReceived: 0,
            startTime: 0,
          };
          return {
            ...old,
            totalServerCount: old.totalServerCount + 1,
            runningServerCount: old.runningServerCount + 1,
            servers: [...old.servers, newEntry],
          };
        },
      );
    },
  });
};

export const useUpdateServer = () => {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: (payload: UpdateServerPayload) => updateServer(payload),
    onSuccess: (_, payload) => {
      // Patch all-servers list: update the matching entry by originalName
      qc.setQueryData(SERVER_QUERY_KEYS.all, (old: ServerInfo[] = []) =>
        old.map((s) =>
          s.config.instanceName === payload.name
            ? {
                ...s,
                config: {
                  ...s.config,
                  ip: payload.ip,
                  port: payload.port,
                },
              }
            : s,
        ),
      );

      // Patch pool status: update the matching server entry
      qc.setQueryData(
        SERVER_QUERY_KEYS.poolStatus,
        (old: ServerPoolStatus | undefined) => {
          if (!old) return old;
          return {
            ...old,
            servers: old.servers.map((s) =>
              s.name === payload.name
                ? {
                    ...s,
                    name: payload.name,
                    ipAddress: payload.ip,
                    port: payload.port,
                    status: "running" as const, // server restarts on update
                    sessionCount: 0, // sessions are dropped on restart
                  }
                : s,
            ),
          };
        },
      );
    },
  });
};
export const useGetServerById = (id: string) =>
  useQuery({
    queryKey: SERVER_QUERY_KEYS.byId(id),
    queryFn: () => getServerById(id),
    enabled: !!id,
  });
