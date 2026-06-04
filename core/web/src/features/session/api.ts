import { api } from "../../lib/api";
import type { UpdateServerPayload } from "../../types/server";
import type { BriefSession } from "../../types/session";

export const getSesssionForServer = async (server: string) =>
  await api<BriefSession[]>(`/sessions/get-sessions/${server}`, {
    method: "GET",
  });
export const updateServer = async (payload: UpdateServerPayload) =>
  await api<{ success: boolean; message: string }>("/server/update", {
    method: "POST",
    body: JSON.stringify(payload),
  });
