import { api } from "../../lib/api";
import type { BriefSession, SessionDetails } from "../../types/session";

export const getSesssionForServer = async (server: string) =>
  await api<BriefSession[]>(`/sessions/get-sessions/${server}`, {
    method: "GET",
  });
export const getSessionById = async (server: string, sessionId: string) =>
  await api<SessionDetails>(`/sessions/get-session/${server}/${sessionId}`, {
    method: "GET",
  });
