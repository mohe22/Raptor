export const WsCmd = {
  DashboardStatus: 0x00,
  Unknown: 0xff,
} as const;

export type WsCmd = (typeof WsCmd)[keyof typeof WsCmd];

export const WsStatus = {
  Ok: 0x00,
  Error: 0x01,
} as const;

export type WsStatus = (typeof WsStatus)[keyof typeof WsStatus];

const HEADER_SIZE = 6;
const PAYLOAD_LEN_OFF = 2;

interface WsFrame {
  header: {
    cmd: WsCmd;
    status: WsStatus;
    payloadLen: number;
  };
  payload: Uint8Array;
}

interface WsRequest {
  command: WsCmd;
  payload?: unknown;
}

interface WsJsonResponse {
  command: WsCmd;
  status: WsStatus;
  data?: unknown;
  error?: string;
}

export class RaptorWsClient {
  url: string;
  #ws: WebSocket | null = null;
  #handlers: Map<WsCmd, (frame: WsFrame | WsJsonResponse) => void> = new Map();

  onOpen: (() => void) | null = null;
  onClose: ((e: CloseEvent) => void) | null = null;
  onError: ((e: Event) => void) | null = null;
  onUnhandled: ((data: WsFrame | WsJsonResponse) => void) | null = null;

  constructor(url: string) {
    this.url = url;
  }

  connect(): void {
    if (this.#ws) this.disconnect();
    this.#ws = new WebSocket(this.url);
    this.#ws.binaryType = "arraybuffer";
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

  #sendJson(request: WsRequest): void {
    if (!this.connected) {
      throw new Error("WebSocket is not connected");
    }
    const jsonStr = JSON.stringify(request);

    this.#ws!.send(jsonStr); // This should be Text
  }

  requestDashboardStatus(): void {
    this.#sendJson({
      command: WsCmd.DashboardStatus,
    });
  }

  on(cmd: WsCmd, handler: (data: WsFrame | WsJsonResponse) => void): this {
    this.#handlers.set(cmd, handler);
    return this;
  }

  off(cmd: WsCmd): this {
    this.#handlers.delete(cmd);
    return this;
  }

  #handleMessage(event: MessageEvent): void {
    // Try to parse as binary frame first
    if (event.data instanceof ArrayBuffer) {
      const frame = this.#parseFrame(event.data);
      if (frame) {
        const handler = this.#handlers.get(frame.header.cmd);

        if (handler) {
          handler(frame);
        } else {
          this.onUnhandled?.(frame);
        }
      }
      return;
    }

    // Try to parse as JSON
    if (typeof event.data === "string") {
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
      return;
    }

    console.warn("[RaptorWs] unexpected message type:", typeof event.data);
  }

  #parseFrame(buffer: ArrayBuffer): WsFrame | null {
    if (buffer.byteLength < HEADER_SIZE) {
      console.error(
        `[RaptorWs] frame too short: ${buffer.byteLength} bytes (need ${HEADER_SIZE})`,
      );
      return null;
    }

    const view = new DataView(buffer);
    const header = {
      cmd: view.getUint8(0) as WsCmd,
      status: view.getUint8(1) as WsStatus,
      payloadLen: view.getUint32(PAYLOAD_LEN_OFF, false),
    };

    const expectedTotal = HEADER_SIZE + header.payloadLen;
    if (buffer.byteLength < expectedTotal) {
      console.error(
        `[RaptorWs] truncated frame: have ${buffer.byteLength} bytes, ` +
          `header says payload is ${header.payloadLen} bytes`,
      );
      return null;
    }

    const payload = new Uint8Array(buffer, HEADER_SIZE, header.payloadLen);
    return { header, payload };
  }
}
