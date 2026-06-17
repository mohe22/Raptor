import type { OSKey } from "./data";

export const WsCmd = {
  NewSession: 0x1,
  Error: 0xfe,
  Unknown: 0xff,
} as const;
export type WsCmd = (typeof WsCmd)[keyof typeof WsCmd];

export const WsStatus = {
  Ok: 0x00,
  Error: 0x01,
} as const;

export type WsStatus = (typeof WsStatus)[keyof typeof WsStatus];

type WsCmdData = {
  [WsCmd.NewSession]: {
    id: string;
    hostname: string;
    os: OSKey;
    username: string;
    timezone: string;
    serverId: string;
    connectedAt: string;
  };
  [WsCmd.Error]: undefined;
  [WsCmd.Unknown]: undefined;
};

interface WsRequest {
  command: WsCmd;
  payload: unknown;
}

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

  // K is inferred from `cmd`, so `res.data` inside the handler is fully typed.
  on<K extends WsCmd>(cmd: K, handler: (res: WsJsonResponse<K>) => void): this {
    this.#handlers.set(cmd, handler as AnyHandler);
    return this;
  }

  off(cmd: WsCmd): this {
    this.#handlers.delete(cmd);
    return this;
  }

  newSession(id: string): void {
    this.#send({ command: WsCmd.NewSession, payload: { id } });
  }

  #send(request: WsRequest): void {
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
