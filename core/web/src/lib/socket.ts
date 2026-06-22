import type { ExecuteCommand } from "../types/session";
import type { OSKey } from "./data";
import { formatTimestamp } from "./utils";

export const WsCmd = {
  SessionConnected: 0x1,
  SessionDisconnected: 0x2,
  ExecuteCommand: 0x03,
  CommandOutput: 0x04,
  Unknown: 0xff,
} as const;

export type WsCmd = (typeof WsCmd)[keyof typeof WsCmd];

//  status of a WebSocket/API response.
// Used to tell us whether the request successfull or failed.
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
    remoteAddress: string;
    status: WsStatus;
  };
  [WsCmd.SessionDisconnected]: {
    id: string;
    serverId: string;
    status: WsStatus;
  };
  [WsCmd.ExecuteCommand]: {
    command: 0x3;
    Data: ExecuteCommand;
    status: WsStatus;
  };
  [WsCmd.CommandOutput]: {
    output: string;
    header: {
      taskId: number;
      type: string;
      flags: string;
      status: WsStatus;
      formattedTime: string;
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
    this.#ws.binaryType = "arraybuffer";
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
    const { data } = event;

    if (data instanceof ArrayBuffer) {
      this.#handleBinaryCommandOutput(data);
      return;
    }

    if (data instanceof Blob) {
      data
        .arrayBuffer()
        .then((buffer) => this.#handleBinaryCommandOutput(buffer))
        .catch((err) =>
          console.warn("[RaptorWs] Blob conversion failed:", err),
        );
      return;
    }

    try {
      const response = JSON.parse(data as string) as WsJsonResponse;
      const handler = this.#handlers.get(response.command);
      if (handler) handler(response);
      else this.onUnhandled?.(response);
    } catch (err) {
      console.warn("[RaptorWs] failed to parse JSON message:", data, err);
    }
  }

  #handleBinaryCommandOutput(buffer: ArrayBuffer): void {
    const totalLength = buffer.byteLength;
    const view = new DataView(buffer);
    let offset = 0;

    if (totalLength < 12) {
      console.warn(`[RaptorWs] Binary packet too small: ${totalLength} bytes`);
      return;
    }

    const taskId = view.getUint32(offset, false);
    offset += 4;
    const typeNum = view.getUint8(offset);
    offset += 1;
    const flagsNum = view.getUint8(offset);
    offset += 1;
    const timestampLen = view.getUint16(offset, true);
    offset += 2;

    if (offset + timestampLen > totalLength) {
      console.warn(`[RaptorWs] Invalid timestamp length: ${timestampLen}`);
      return;
    }

    const timestampBytes = new Uint8Array(buffer, offset, timestampLen);
    const timestamp = new TextDecoder().decode(timestampBytes);
    offset += timestampLen;

    if (offset + 4 > totalLength) {
      console.warn(`[RaptorWs] Missing output length`);
      return;
    }

    const outputLength = view.getUint32(offset, true);
    offset += 4;

    if (offset + outputLength > totalLength) {
      console.warn(`[RaptorWs] Invalid output length ${outputLength}`);
      return;
    }

    const output = new Uint8Array(buffer, offset, outputLength);
    const outputText = new TextDecoder().decode(output);

    const typeStr = getPacketType(typeNum);
    const flagsStr = getFlagsString(flagsNum);
    const formattedTime = formatTimestamp(timestamp);
    const statusNum = WsStatus.Ok;

    const response: WsJsonResponse<typeof WsCmd.CommandOutput> = {
      command: WsCmd.CommandOutput,
      status: statusNum,
      data: {
        output: outputText,
        header: {
          taskId,
          type: typeStr,
          flags: flagsStr,
          status: statusNum,
          formattedTime,
        },
      },
    };

    const handler = this.#handlers.get(WsCmd.CommandOutput);
    if (handler) handler(response);
    else this.onUnhandled?.(response);
  }
}

const getPacketType = (type: number): string => {
  const types = ["FileUpload", "FileDownload", "Command", "Register"];
  return types[type] ?? "Unknown";
};

const getFlagsString = (flags: number): string => {
  const flagNames = [
    "Ack",
    "Error",
    "Binary",
    "Text",
    "KeepAlive",
    "Success",
    "LastChunk",
    "Metadata",
  ];
  const active = flagNames.filter((_, i) => (flags & (1 << i)) !== 0);
  return active.length ? active.join(", ") : "None";
};
