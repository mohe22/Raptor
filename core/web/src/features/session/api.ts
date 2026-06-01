import { api } from "../../lib/api";
import type { BriefSession } from "../../types/session";

export const getSesssionForServer = async (server: string) =>
  await api<BriefSession[]>(`/sessions/get-sessions/${server}`, {
    method: "GET",
  });
