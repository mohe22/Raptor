import { api } from "../../lib/api";
import type {
  CreateServerPayload,
  getAllServersResponse,
  ServerDetail,
  ServerInfo,
  ServerPoolStatus,
} from "../../types/server";

export const getAllServers = async () =>
  await api<getAllServersResponse>("/server/get-servers", {
    method: "GET",
  });
export const poolStatus = async () =>
  await api<ServerPoolStatus>("/server/pool-status", { method: "GET" });

export const pauseServer = async (name: string) =>
  await api(`/server/pause/${name}`, { method: "POST" });

export const resumeServer = async (name: string) =>
  await api(`/server/resume/${name}`, { method: "POST" });

export const stopServer = async (name: string) =>
  await api(`/server/stop/${name}`, { method: "POST" });

export const createServer = async (payload: CreateServerPayload) =>
  await api<{ success: boolean; message: string }>("/server/create", {
    method: "POST",
    body: JSON.stringify(payload),
  });

export const getServerById = async (id: string) =>
  await api<ServerDetail>(`/server/get-server/${id}`, { method: "GET" });
