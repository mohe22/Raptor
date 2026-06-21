import type { ExecuteCommand } from "../types/session";
import type { OSKey } from "./data";

export const WsCmd = {
  SessionConnected: 0x1,
  SessionDisconnected: 0x2,
  ExecuteCommand: 0x03, // Server to Client: request to run a shell command
  CommandOutput: 0x04, // Client to Server: response with command output
  Unknown: 0xff,
} as const;

export type WsCmd = (typeof WsCmd)[keyof typeof WsCmd];

export const WsStatus = {
  Ok: 0x00,
  Error: 0x01,
} as const;

export type WsStatus = (typeof WsStatus)[keyof typeof WsStatus];

type WsCmdData = {
  [WsCmd.SessionConnected]: {
    id: string;
    hostname: string;
    os: OSKey;
    username: string;
    timezone: string;
    serverId: string;
    connectedAt: string;
    remoteAddress: string; // ip:port
  };
  [WsCmd.SessionDisconnected]: {
    id: string; // session ID that is disconnected
    serverId: string; // to which server belong
  };

  // send to server
  [WsCmd.ExecuteCommand]: {
    command: 0x3;
    Data: ExecuteCommand;
  };
  [WsCmd.CommandOutput]: {
    output: string;
    header: {
      packetId: number;
      type: number; // 0=FileUpload, 1=FileDownload, 2=Command, 3=Register
      flags: number; // Bitmask of flags
      timestamp: string; // ISO 8601
    };
  };

  [WsCmd.Unknown]: undefined;
};

export interface WsJsonResponse<K extends WsCmd = WsCmd> {
  command: K;
  status: WsStatus;
  data: WsCmdData[K];
  error?: string;
}

type AnyHandler = (res: WsJsonResponse<WsCmd>) => void;

export class RaptorWsClient {
  url: string;
  #ws: WebSocket | null = null;
  #handlers: Map<WsCmd, AnyHandler> = new Map();

  onOpen: (() => void) | null = null;
  onClose: ((e: CloseEvent) => void) | null = null;
  onError: ((e: Event) => void) | null = null;
  onUnhandled: ((res: WsJsonResponse) => void) | null = null;

  constructor(url: string) {
    this.url = url;
  }

  connect(): void {
    if (this.#ws) this.disconnect();
    this.#ws = new WebSocket(this.url);
    this.#ws.onopen = () => this.onOpen?.();
    this.#ws.onclose = (e) => this.onClose?.(e);
    this.#ws.onerror = (e) => this.onError?.(e);
    this.#ws.onmessage = (e) => this.#handleMessage(e);
  }

  disconnect(): void {
    this.#ws?.close();
    this.#ws = null;
  }

  get connected(): boolean {
    return this.#ws?.readyState === WebSocket.OPEN;
  }

  on<K extends WsCmd>(cmd: K, handler: (res: WsJsonResponse<K>) => void): this {
    this.#handlers.set(cmd, handler as AnyHandler);
    return this;
  }

  off(cmd: WsCmd): this {
    this.#handlers.delete(cmd);
    return this;
  }

  sendExecuteCommandEvent(Data: ExecuteCommand): void {
    this.#send({ command: WsCmd.ExecuteCommand, Data });
  }

  #send(request: { command: WsCmd; Data: unknown }): void {
    if (!this.connected) throw new Error("WebSocket is not connected");
    this.#ws!.send(JSON.stringify(request));
  }

  #handleMessage(event: MessageEvent): void {
    if (typeof event.data !== "string") {
      console.warn("[RaptorWs] unexpected message type:", typeof event.data);
      return;
    }

    try {
      const response = JSON.parse(event.data) as WsJsonResponse;
      const handler = this.#handlers.get(response.command);

      if (handler) {
        handler(response);
      } else {
        this.onUnhandled?.(response);
      }
    } catch (err) {
      console.warn("[RaptorWs] failed to parse message:", event.data, err);
    }
  }
}
