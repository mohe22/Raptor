import { useEffect } from "react";
import { WsCmd, type WsFrame, type WsJsonResponse } from "./socket";
import { useSocket } from "../providers/socket-context";

export function useEvent(
  cmd: WsCmd,
  handler: (frame: WsFrame | WsJsonResponse) => void,
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
