export const WsCmd = Object.freeze({
  DashboardStatus: 0,
  Unknown: 0xff,
});

export const WsStatus = Object.freeze({
  Ok: 0,
  Error: 1,
});

const WsCmdName = Object.fromEntries(
  Object.entries(WsCmd).map(([k, v]) => [v, k]),
);
const WsStatusName = Object.fromEntries(
  Object.entries(WsStatus).map(([k, v]) => [v, k]),
);

const HEADER_SIZE = 10;
const PAYLOAD_LEN_OFF = 2;

export class RaptorWsClient {
  url = "";

  #ws = null;
  #handlers = new Map();
  #decoder = new TextDecoder("utf-8");

  onOpen = null;
  onClose = null;
  onError = null;
  onUnhandled = null;

  constructor(url) {
    this.url = url;
  }

  connect() {
    if (this.#ws) this.disconnect();
    this.#ws = new WebSocket(this.url);
    this.#ws.binaryType = "arraybuffer";
    this.#ws.onopen = () => this.onOpen?.();
    this.#ws.onclose = (e) => this.onClose?.(e);
    this.#ws.onerror = (e) => this.onError?.(e);
    this.#ws.onmessage = (e) => this.#handleMessage(e);
  }

  disconnect() {
    this.#ws?.close();
    this.#ws = null;
  }

  get connected() {
    return this.#ws?.readyState === WebSocket.OPEN;
  }

  send(request) {
    if (!this.connected) throw new Error("WebSocket is not connected");
    this.#ws.send(JSON.stringify(request));
  }

  requestDashboardStatus() {
    this.send({ command: "DashboardStatus", payload: {} });
  }

  on(cmd, handler) {
    this.#handlers.set(cmd, handler);
    return this;
  }

  off(cmd) {
    this.#handlers.delete(cmd);
    return this;
  }

  #handleMessage(event) {
    if (!(event.data instanceof ArrayBuffer)) {
      console.warn("[RaptorWs] unexpected text frame:", event.data);
      return;
    }

    const frame = this.#parseFrame(event.data);
    if (!frame) return;

    console.group("[RaptorWs] frame received");
    console.log(
      "cmd       :",
      frame.header.cmd,
      `(${WsCmdName[frame.header.cmd] ?? "unknown"})`,
    );
    console.log(
      "status    :",
      frame.header.status,
      `(${WsStatusName[frame.header.status] ?? "unknown"})`,
    );
    console.log("payloadLen:", frame.header.payloadLen, "bytes");
    console.log("payload   :", frame.payload);

    if (
      frame.header.cmd === WsCmd.DashboardStatus &&
      frame.header.status === WsStatus.Ok
    ) {
      const token = this.#decoder.decode(frame.payload);
      console.log("token     :", token);
    }

    console.groupEnd();

    const handler = this.#handlers.get(frame.header.cmd);
    if (handler) {
      handler(frame);
    } else {
      this.onUnhandled?.(frame);
    }
  }

  #parseFrame(buffer) {
    if (buffer.byteLength < HEADER_SIZE) {
      console.error(
        `[RaptorWs] frame too short: ${buffer.byteLength} bytes (need ${HEADER_SIZE})`,
      );
      return null;
    }

    const view = new DataView(buffer);

    const header = {
      cmd: view.getUint8(0),
      status: view.getUint8(1),
      payloadLen: Number(view.getBigUint64(PAYLOAD_LEN_OFF, false)),
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
