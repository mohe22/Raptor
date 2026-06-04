import { useEffect } from "react";
import { WsCmd } from "./socket";
import { useSocket } from "../providers/socket-context";

export function useEvent(
  cmd: keyof typeof WsCmd,
  handler: (frame: unknown) => void,
) {
  const { client } = useSocket();

  useEffect(() => {
    if (!client) return;
    client.on(WsCmd[cmd], handler);
    return () => {
      client.off(WsCmd[cmd]);
    };
  }, [client, cmd, handler]);
}
