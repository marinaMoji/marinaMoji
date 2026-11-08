// Bazel HTTP remote cache backed by R2. Bazel does GET/HEAD/PUT on
// /ac/<sha256> and /cas/<sha256>. Two bearer tokens (Worker secrets):
// CACHE_TOKEN_RW may read and write, CACHE_TOKEN_RO may only read.
export default {
  async fetch(req, env) {
    const key = new URL(req.url).pathname.slice(1);
    if (!/^(ac|cas)\/[0-9a-f]{64}$/.test(key)) {
      return new Response("not found", { status: 404 });
    }

    // Fail closed: if a secret is missing, "Bearer undefined" must not match.
    if (!env.CACHE_TOKEN_RW || !env.CACHE_TOKEN_RO) {
      return new Response("not configured", { status: 500 });
    }

    const auth = req.headers.get("Authorization");
    const rw = auth === `Bearer ${env.CACHE_TOKEN_RW}`;
    const ro = rw || auth === `Bearer ${env.CACHE_TOKEN_RO}`;
    if (!ro) return new Response("unauthorized", { status: 401 });

    if (req.method === "GET" || req.method === "HEAD") {
      const obj = await env.CACHE.get(key);
      if (!obj) return new Response("not found", { status: 404 });
      return new Response(req.method === "HEAD" ? null : obj.body, {
        headers: { "Content-Length": String(obj.size) },
      });
    }
    if (req.method === "PUT") {
      if (!rw) return new Response("read-only token", { status: 403 });
      await env.CACHE.put(key, req.body);
      return new Response(null, { status: 201 });
    }
    return new Response("method not allowed", { status: 405 });
  },
};
