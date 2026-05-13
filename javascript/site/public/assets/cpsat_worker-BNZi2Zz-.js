(async ()=>{
    let u = null, i = null;
    function m() {
        const r = WebAssembly;
        return typeof r < "u" && typeof r.promising == "function";
    }
    function y() {
        return i || (i = m() ? "jspi" : "asyncify", console.log(i === "jspi" ? "JSPI is supported. Using ASYNCIFY=2." : "JSPI not found. Falling back to ASYNCIFY=1."), i);
    }
    async function g() {
        if (y() === "jspi") {
            const { default: n } = await import("./cp_sat_runtime-BSzCkFJB.js");
            return n;
        }
        const { default: e } = await import("./cp_sat_runtime_asyncify-Bt3WG6KD.js");
        return e;
    }
    async function _() {
        return u || (u = (async ()=>(await g())({}))()), u;
    }
    const a = self;
    let t = null;
    const w = _().then((r)=>(t = r, a.postMessage({
            type: "ready"
        }), r)).catch((r)=>{
        throw console.error("[cpsat_worker] cpSatModule init failed:", r), a.postMessage({
            type: "error",
            id: 0,
            error: String(r)
        }), r;
    }), d = (r, e)=>new DataView(r, e, 4).getUint32(0, !0), f = (r)=>{
        if (!t || !r?.length) return 0;
        const e = t._malloc(r.length);
        return t.HEAPU8.set(r, e), e;
    };
    async function b(r, e) {
        if (!t) throw new Error("Module not initialized.");
        const n = t._malloc(4), s = f(r), l = f(e ?? null);
        let o = 0;
        try {
            o = await t.ccall("solve_model", "number", [
                "number",
                "number",
                "number",
                "number",
                "number"
            ], [
                s,
                r.length,
                l,
                e ? e.length : 0,
                n
            ], {
                async: !0
            });
        } finally{
            s && t._free(s), l && t._free(l);
        }
        const c = d(t.HEAPU8.buffer, n);
        if (t._free(n), !o || c === 0) return o && t._free_buffer(o), new Uint8Array();
        const p = t.HEAPU8.slice(o, o + c);
        return t._free_buffer(o), p;
    }
    async function h(r) {
        if (!t) throw new Error("Module not initialized.");
        const e = t._malloc(4), n = f(r);
        let s = 0;
        try {
            s = t.ccall("validate_model", "number", [
                "number",
                "number",
                "number"
            ], [
                n,
                r.length,
                e
            ]);
        } finally{
            n && t._free(n);
        }
        const l = d(t.HEAPU8.buffer, e);
        if (t._free(e), !s || l === 0) return s && t._free_buffer(s), {
            ok: !0,
            message: ""
        };
        const o = t.HEAPU8.slice(s, s + l);
        return t._free_buffer(s), {
            ok: !1,
            message: new TextDecoder().decode(o)
        };
    }
    a.onmessage = async (r)=>{
        const e = r.data;
        try {
            if (await w, !t) throw new Error("Module not initialized.");
            if (e.type === "validate") {
                const n = await h(e.modelBytes);
                a.postMessage({
                    type: "validateResult",
                    id: e.id,
                    ok: n.ok,
                    message: n.message
                });
                return;
            } else if (e.type === "solve") {
                const n = await b(e.modelBytes, e.paramsBytes);
                a.postMessage({
                    type: "solveResult",
                    id: e.id,
                    bytes: n
                });
                return;
            } else if (e.type === "getSchemas") {
                const n = {
                    cp_model: t.ccall("get_cp_model_schema", "string", [], []),
                    sat_parameters: t.ccall("get_sat_parameters_schema", "string", [], [])
                };
                self.postMessage({
                    type: "schemaResult",
                    id: e.id,
                    schemas: n
                });
                return;
            } else if (e.type === "cancel_solve") {
                t.ccall("interrupt_solve", "void", [], []), self.postMessage({
                    type: "solved_cancelled",
                    id: e.id
                });
                return;
            }
        } catch (n) {
            console.error("[cpsat_worker] request failed", e?.type, n), a.postMessage({
                type: "error",
                id: e?.id ?? 0,
                error: String(n)
            });
        }
    };
})();
