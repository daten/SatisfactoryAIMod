import { GameError, ConnectionError } from "./errors.js";

export interface GameClientOptions {
  /** Base RPC URL, default http://127.0.0.1:51902/rpc */
  url?: string;
  /** Default per-call timeout (ms). */
  defaultTimeoutMs?: number;
}

export interface CallOptions {
  timeoutMs?: number;
}

let requestCounter = 0;

/**
 * Thin client for the mod's loopback JSON endpoint.
 * - Serializes every call (the real build gun can't run two builds at once).
 * - Per-call timeout via AbortController.
 * - Maps transport failures to ConnectionError and mod errors to GameError.
 */
export class GameClient {
  readonly url: string;
  readonly defaultTimeoutMs: number;
  private chain: Promise<unknown> = Promise.resolve();

  constructor(opts: GameClientOptions = {}) {
    this.url = opts.url ?? process.env.SATISFACTORY_RPC_URL ?? "http://127.0.0.1:51902/rpc";
    this.defaultTimeoutMs = opts.defaultTimeoutMs ?? 30_000;
  }

  /** Serialized RPC call. Resolves to the `result` object, throws GameError on failure. */
  call(method: string, params?: Record<string, unknown>, opts: CallOptions = {}): Promise<any> {
    const run = () => this.callNow(method, params, opts);
    // Queue so calls never overlap; a failure in one call must not break the chain.
    const result = this.chain.then(run, run);
    this.chain = result.then(
      () => undefined,
      () => undefined,
    );
    return result;
  }

  private async callNow(
    method: string,
    params: Record<string, unknown> | undefined,
    opts: CallOptions,
  ): Promise<any> {
    const timeoutMs = opts.timeoutMs ?? this.defaultTimeoutMs;
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs);
    const body = JSON.stringify({
      protocolVersion: 1,
      requestId: String(++requestCounter),
      method,
      ...(params ? { params } : {}),
    });
    let res: Response;
    try {
      res = await fetch(this.url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body,
        signal: controller.signal,
      });
    } catch (e: any) {
      if (e?.name === "AbortError") {
        throw new ConnectionError(
          `the game didn't respond within ${Math.round(timeoutMs / 1000)}s`,
          method,
        );
      }
      throw new ConnectionError(describeNetworkError(e), method);
    } finally {
      clearTimeout(timer);
    }

    if (!res.ok) {
      throw new ConnectionError(`HTTP ${res.status} from the game endpoint`, method);
    }

    let json: any;
    try {
      json = await res.json();
    } catch {
      throw new ConnectionError("the game returned a response that wasn't valid JSON", method);
    }

    if (json?.success === true) {
      return json.result ?? {};
    }
    const code = json?.error?.code ?? "INTERNAL_ERROR";
    const message = json?.error?.message ?? "unknown error";
    throw new GameError(code, message, method);
  }

  /** Read mod identity; throws ConnectionError if the game is unreachable. */
  async version(): Promise<{ modVersion?: string; buildStamp?: string; buildConfig?: string }> {
    return this.call("world.version", undefined, { timeoutMs: 8_000 });
  }
}

function describeNetworkError(e: any): string {
  const code = e?.cause?.code ?? e?.code;
  if (code === "ECONNREFUSED")
    return "connection refused - the mod's local endpoint isn't listening (game not running, mod not installed, or no save loaded)";
  if (code === "ETIMEDOUT" || code === "UND_ERR_CONNECT_TIMEOUT")
    return "connection timed out reaching the game";
  if (code === "ENOTFOUND" || code === "EAI_AGAIN")
    return "couldn't resolve the game endpoint host";
  return e?.message ? `network error: ${e.message}` : "network error reaching the game";
}
