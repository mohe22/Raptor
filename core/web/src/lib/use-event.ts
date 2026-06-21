import { useEffect } from "react";
import { useSocket } from "../providers/socket-context";
import type { WsCmd, WsJsonResponse } from "./socket";

export function useEvent<K extends WsCmd>(
  cmd: K,
  handler: (res: WsJsonResponse<K>) => void,
) {
  const { client } = useSocket();

  useEffect(() => {
    if (!client) return;

    client.on(cmd, handler);

    return () => {
      client.off(cmd);
    };
  }, [client, cmd, handler]);
}
