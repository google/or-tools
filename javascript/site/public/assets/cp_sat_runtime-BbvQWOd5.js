async function $e(El = {}) {
  var Ml;
  (function() {
    function l(W) {
      W = W.split("-")[0];
      for (var X = W.split(".").slice(0, 3); X.length < 3; ) X.push("00");
      return X = X.map((G, R, r) => G.padStart(2, "0")), X.join("");
    }
    var Z = (W) => [W / 1e4 | 0, (W / 100 | 0) % 100, W % 100].join("."), c = 2147483647, d = typeof process < "u" && process.versions?.node ? l(process.versions.node) : c;
    if (d < c)
      throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    if (d < 2147483647)
      throw new Error(`This emscripten-generated code requires node v${Z(2147483647)} (detected v${Z(d)})`);
    var b = typeof navigator < "u" && navigator.userAgent;
    if (b) {
      var t = b.includes("Safari/") && b.match(/Version\/(\d+\.?\d*\.?\d*)/) ? l(b.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : c;
      if (t < 15e4)
        throw new Error(`This emscripten-generated code requires Safari v${Z(15e4)} (detected v${t})`);
      var m = b.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(b.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : c;
      if (m < 114)
        throw new Error(`This emscripten-generated code requires Firefox v114 (detected v${m})`);
      var n = b.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(b.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : c;
      if (n < 85)
        throw new Error(`This emscripten-generated code requires Chrome v85 (detected v${n})`);
    }
  })();
  var a = El, Nl = !!globalThis.window, D = !!globalThis.WorkerGlobalScope, Ll = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", RZ = !Nl && !Ll && !D, s = D && self.name?.startsWith("em-pthread");
  s && (V(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0);
  var sZ = "./this.program", hZ = (l, Z) => {
    throw Z;
  }, Oc = import.meta.url, Ql = "";
  function _c(l) {
    return a.locateFile ? a.locateFile(l, Ql) : Ql + l;
  }
  var jl, vl;
  if (!RZ) if (Nl || D) {
    try {
      Ql = new URL(".", Oc).href;
    } catch {
    }
    if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    D && (vl = (l) => {
      var Z = new XMLHttpRequest();
      return Z.open("GET", l, !1), Z.responseType = "arraybuffer", Z.send(null), new Uint8Array(Z.response);
    }), jl = async (l) => {
      V(!uZ(l), "readAsync does not work with file:// URLs");
      var Z = await fetch(l, { credentials: "same-origin" });
      if (Z.ok)
        return Z.arrayBuffer();
      throw new Error(Z.status + " : " + Z.url);
    };
  } else
    throw new Error("environment detection error");
  var A = console.log.bind(console), u = console.error.bind(console);
  V(Nl || D || Ll, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), V(!Ll, "node environment detected but not enabled at build time.  Add `node` to `-sENVIRONMENT` to enable."), V(!RZ, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
  var hl;
  globalThis.WebAssembly || u("no native wasm support detected");
  var ll, ml = !1, nl;
  function V(l, Z) {
    l || N("Assertion failed" + (Z ? ": " + Z : ""));
  }
  var uZ = (l) => l.startsWith("file://");
  function FZ() {
    var l = GZ();
    V((l & 3) == 0), l == 0 && (l += 4), (i(), y)[l >> 2] = 34821223, (i(), y)[l + 4 >> 2] = 2310721022, (i(), y)[0] = 1668509029;
  }
  function ul() {
    if (!ml) {
      var l = GZ();
      l == 0 && (l += 4);
      var Z = (i(), y)[l >> 2], c = (i(), y)[l + 4 >> 2];
      (Z != 34821223 || c != 2310721022) && N(`Stack overflow! Stack cookie has been overwritten at ${el(l)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${el(c)} ${el(Z)}`), (i(), y)[0] != 1668509029 && N("Runtime error: The application has corrupted its heap memory area (address zero)!");
    }
  }
  function Pl(...l) {
    console.warn(...l);
  }
  (() => {
    var l = new Int16Array(1), Z = new Int8Array(l.buffer);
    l[0] = 25459, (Z[0] !== 115 || Z[1] !== 99) && N("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
  })();
  function Ul(l) {
    Object.getOwnPropertyDescriptor(a, l) || Object.defineProperty(a, l, { configurable: !0, set() {
      N(`Attempt to set \`Module.${l}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
    } });
  }
  function I(l) {
    return () => V(!1, `call to '${l}' via reference taken before Wasm module initialization`);
  }
  function Dc(l) {
    Object.getOwnPropertyDescriptor(a, l) && N(`\`Module.${l}\` was supplied but \`${l}\` not included in INCOMING_MODULE_JS_API`);
  }
  function Ac(l) {
    return l === "FS_createPath" || l === "FS_createDataFile" || l === "FS_createPreloadedFile" || l === "FS_preloadFile" || l === "FS_unlink" || l === "addRunDependency" || l === "FS_createLazyFile" || l === "FS_createDevice" || l === "removeRunDependency";
  }
  function $c(l) {
    pZ(l);
  }
  function pZ(l) {
    s || Object.getOwnPropertyDescriptor(a, l) || Object.defineProperty(a, l, { configurable: !0, get() {
      var Z = `'${l}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
      Ac(l) && (Z += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), N(Z);
    } });
  }
  function qc() {
    function l() {
      var c = 0;
      return Wl && typeof ol < "u" && (c = ol()), `w:${YZ},t:${el(c)}:`;
    }
    var Z = Pl;
    Pl = (...c) => Z(l(), ...c);
  }
  qc();
  function i() {
    C.buffer != g.buffer && Tl();
  }
  var Ol, _l, YZ = 0, JZ;
  if (s) {
    var Dl = !1;
    self.onunhandledrejection = (Z) => {
      throw Z.reason || Z;
    };
    async function l(Z) {
      try {
        var c = Z.data, d = c.cmd;
        if (d === "load") {
          YZ = c.workerID;
          let b = [];
          self.onmessage = (t) => b.push(t), JZ = () => {
            postMessage({ cmd: "loaded" });
            for (let t of b)
              l(t);
            self.onmessage = l;
          };
          for (const t of c.handlers)
            (!a[t] || a[t].proxy) && (a[t] = (...m) => {
              postMessage({ cmd: "callHandler", handler: t, args: m });
            }, t == "print" && (A = a[t]), t == "printErr" && (u = a[t]));
          C = c.wasmMemory, Tl(), ll = c.wasmModule, TZ(), Cl();
        } else if (d === "run") {
          V(c.pthread_ptr), Vd(c.pthread_ptr), rZ(c.pthread_ptr, 0, 0, 1, 0, 0), p.threadInitTLS(), WZ(c.pthread_ptr), Dl || (Bc(), Dl = !0);
          try {
            await LZ(c.start_routine, c.arg);
          } catch (b) {
            if (b != "unwind")
              throw b;
          }
        } else c.target === "setimmediate" || (d === "checkMailbox" ? Dl && Bl() : d && (u(`worker: received unknown command ${d}`), u(c)));
      } catch (b) {
        throw u(`worker: onmessage() captured an uncaught exception: ${b}`), b?.stack && u(b.stack), Kc(), b;
      }
    }
    self.onmessage = l;
  }
  var g, K, Zl, Fl, U, y, IZ, pl, k, NZ, Wl = !1;
  function Tl() {
    var l = C.buffer;
    g = new Int8Array(l), Zl = new Int16Array(l), a.HEAPU8 = K = new Uint8Array(l), Fl = new Uint16Array(l), U = new Int32Array(l), y = new Uint32Array(l), IZ = new Float32Array(l), pl = new Float64Array(l), k = new BigInt64Array(l), NZ = new BigUint64Array(l);
  }
  function ld() {
    if (!s) {
      if (a.wasmMemory)
        C = a.wasmMemory;
      else {
        var l = a.INITIAL_MEMORY || 16777216;
        V(l >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + l + "! (STACK_SIZE=65536)"), C = new WebAssembly.Memory({ initial: l / 65536, maximum: 32768, shared: !0 });
      }
      Tl();
    }
  }
  V(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
  function Zd() {
    if (V(!s), a.preRun)
      for (typeof a.preRun == "function" && (a.preRun = [a.preRun]); a.preRun.length; )
        zZ(a.preRun.shift());
    Ul("preRun"), gZ(BZ);
  }
  function vZ() {
    if (V(!Wl), Wl = !0, s) return JZ();
    ul(), !a.noFSInit && !e.initialized && e.init(), _.__wasm_call_ctors(), e.ignorePermissions = !1;
  }
  function cd() {
    if (ul(), !s) {
      if (a.postRun)
        for (typeof a.postRun == "function" && (a.postRun = [a.postRun]); a.postRun.length; )
          Wd(a.postRun.shift());
      Ul("postRun"), gZ(EZ);
    }
  }
  function N(l) {
    a.onAbort?.(l), l = "Aborted(" + l + ")", u(l), ml = !0;
    var Z = new WebAssembly.RuntimeError(l);
    throw _l?.(Z), Z;
  }
  function H(l, Z) {
    return (...c) => {
      V(Wl, `native function \`${l}\` called before runtime initialization`);
      var d = _[l];
      return V(d, `exported native function \`${l}\` not found`), V(c.length <= Z, `native function \`${l}\` called with ${c.length} args but expects ${Z}`), d(...c);
    };
  }
  var Al;
  function dd() {
    return a.locateFile ? _c("cp_sat_runtime.wasm") : new URL("/assets/cp_sat_runtime-CGxDzkeQ.wasm", import.meta.url).href;
  }
  function ed(l) {
    if (l == Al && hl)
      return new Uint8Array(hl);
    if (vl)
      return vl(l);
    throw "both async and sync fetching of the wasm failed";
  }
  async function bd(l) {
    if (!hl)
      try {
        var Z = await jl(l);
        return new Uint8Array(Z);
      } catch {
      }
    return ed(l);
  }
  async function td(l, Z) {
    try {
      var c = await bd(l), d = await WebAssembly.instantiate(c, Z);
      return d;
    } catch (b) {
      u(`failed to asynchronously prepare wasm: ${b}`), uZ(l) && u(`warning: Loading from a file URI (${l}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), N(b);
    }
  }
  async function md(l, Z, c) {
    if (!l)
      try {
        var d = fetch(Z, { credentials: "same-origin" }), b = await WebAssembly.instantiateStreaming(d, c);
        return b;
      } catch (t) {
        u(`wasm streaming compile failed: ${t}`), u("falling back to ArrayBuffer instantiation");
      }
    return td(Z, c);
  }
  function UZ() {
    _e(), Rl.__instrumented || (Rl.__instrumented = !0, M.instrumentWasmImports(Rl));
    var l = { env: Rl, wasi_snapshot_preview1: Rl };
    return l;
  }
  async function TZ() {
    function l(n, W) {
      return _ = n.exports, _ = M.instrumentWasmExports(_), id(_._emscripten_tls_init), Oe(_), ll = W, _;
    }
    var Z = a;
    function c(n) {
      return V(a === Z, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), Z = null, l(n.instance, n.module);
    }
    var d = UZ();
    if (a.instantiateWasm)
      return new Promise((n, W) => {
        try {
          a.instantiateWasm(d, (X, G) => {
            n(l(X, G));
          });
        } catch (X) {
          u(`Module.instantiateWasm callback failed with error: ${X}`), W(X);
        }
      });
    if (s) {
      V(ll, "wasmModule should have been received via postMessage");
      var b = new WebAssembly.Instance(ll, UZ());
      return l(b, ll);
    }
    Al ??= dd();
    var t = await md(hl, Al, d), m = c(t);
    return m;
  }
  class kZ {
    name = "ExitStatus";
    constructor(Z) {
      this.message = `Program terminated with exit(${Z})`, this.status = Z;
    }
  }
  var HZ = (l) => {
    l.terminate(), l.onmessage = (Z) => {
      var c = Z.data.cmd;
      u(`received "${c}" command from terminated worker: ${l.workerID}`);
    };
  }, fZ = (l) => {
    V(!s, "Internal Error! cleanupThread() can only ever be called from main application thread!"), V(l, "Internal Error! Null pthread_ptr in cleanupThread!");
    var Z = p.pthreads[l];
    V(Z), p.returnWorkerToPool(Z);
  }, gZ = (l) => {
    for (; l.length > 0; )
      l.shift()(a);
  }, BZ = [], zZ = (l) => BZ.push(l), cl = 0, Yl = null, Vl = {}, dl = null, SZ = (l) => {
    if (cl--, a.monitorRunDependencies?.(cl), V(l, "removeRunDependency requires an ID"), V(Vl[l]), delete Vl[l], cl == 0 && (dl !== null && (clearInterval(dl), dl = null), Yl)) {
      var Z = Yl;
      Yl = null, Z();
    }
  }, KZ = (l) => {
    cl++, a.monitorRunDependencies?.(cl), V(l, "addRunDependency requires an ID"), V(!Vl[l]), Vl[l] = 1, dl === null && globalThis.setInterval && (dl = setInterval(() => {
      if (ml) {
        clearInterval(dl), dl = null;
        return;
      }
      var Z = !1;
      for (var c in Vl)
        Z || (Z = !0, u("still waiting on run dependencies:")), u(`dependency: ${c}`);
      Z && u("(end of list)");
    }, 1e4));
  }, wZ = (l) => {
    V(!s, "Internal Error! spawnThread() can only ever be called from main application thread!"), V(l.pthread_ptr, "Internal error, no pthread ptr!");
    var Z = p.getNewWorker();
    if (!Z)
      return 6;
    V(!Z.pthread_ptr, "Internal error!"), p.runningWorkers.push(Z), p.pthreads[l.pthread_ptr] = Z, Z.pthread_ptr = l.pthread_ptr;
    var c = { cmd: "run", start_routine: l.startRoutine, arg: l.arg, pthread_ptr: l.pthread_ptr };
    return Z.postMessage(c, l.transferList), 0;
  }, il = 0, kl = () => dZ || il > 0, xZ = () => oZ(), $l = (l) => Lc(l), ql = (l) => Qc(l), f = (l, Z, c, ...d) => {
    for (var b = d.length * 2, t = xZ(), m = ql(b * 8), n = m >> 3, W = 0; W < d.length; W++) {
      var X = d[W];
      typeof X == "bigint" ? ((i(), k)[n + 2 * W] = 1n, (i(), k)[n + 2 * W + 1] = X) : ((i(), k)[n + 2 * W] = 0n, (i(), pl)[n + 2 * W + 1] = X);
    }
    var G = wc(l, Z, b, m, c);
    return $l(t), G;
  };
  function lZ(l) {
    if (s) return f(0, 0, 1, l);
    nl = l, kl() || (p.terminateAllThreads(), a.onExit?.(l), ml = !0), hZ(l, new kZ(l));
  }
  function CZ(l) {
    if (s) return f(1, 0, 0, l);
    ZZ(l);
  }
  var nd = (l, Z) => {
    if (nl = l, Ae(), s)
      throw V(!Z), CZ(l), "unwind";
    if (kl() && !Z) {
      var c = `program exited (with status: ${l}), but keepRuntimeAlive() is set (counter=${il}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
      _l?.(c), u(c);
    }
    lZ(l);
  }, ZZ = nd, el = (l) => (V(typeof l == "number", `ptrToString expects a number, got ${typeof l}`), l >>>= 0, "0x" + l.toString(16).padStart(8, "0")), p = { unusedWorkers: [], runningWorkers: [], tlsInitFunctions: [], pthreads: {}, nextWorkerID: 1, init() {
    s || p.initMainThread();
  }, initMainThread() {
    for (var l = navigator.hardwareConcurrency; l--; )
      p.allocateUnusedWorker();
    zZ(async () => {
      var Z = p.loadWasmModuleToAllWorkers();
      KZ("loading-workers"), await Z, SZ("loading-workers");
    });
  }, terminateAllThreads: () => {
    V(!s, "Internal Error! terminateAllThreads() can only ever be called from main application thread!");
    for (var l of p.runningWorkers)
      HZ(l);
    for (var l of p.unusedWorkers)
      HZ(l);
    p.unusedWorkers = [], p.runningWorkers = [], p.pthreads = {};
  }, returnWorkerToPool: (l) => {
    var Z = l.pthread_ptr;
    delete p.pthreads[Z], p.unusedWorkers.push(l), p.runningWorkers.splice(p.runningWorkers.indexOf(l), 1), l.pthread_ptr = 0, xc(Z);
  }, threadInitTLS() {
    p.tlsInitFunctions.forEach((l) => l());
  }, loadWasmModuleToWorker: (l) => new Promise((Z) => {
    l.onmessage = (t) => {
      var m = t.data, n = m.cmd;
      if (m.targetThread && m.targetThread != ol()) {
        var W = p.pthreads[m.targetThread];
        W ? W.postMessage(m, m.transferList) : u(`Internal error! Worker sent a message "${n}" to target pthread ${m.targetThread}, but that thread no longer exists!`);
        return;
      }
      n === "checkMailbox" ? Bl() : n === "spawnThread" ? wZ(m) : n === "cleanupThread" ? oc(() => fZ(m.thread)) : n === "loaded" ? (l.loaded = !0, Z(l)) : m.target === "setimmediate" ? l.postMessage(m) : n === "callHandler" ? a[m.handler](...m.args) : n && u(`worker sent an unknown command ${n}`);
    }, l.onerror = (t) => {
      var m = "worker sent an error!";
      throw l.pthread_ptr && (m = `Pthread ${el(l.pthread_ptr)} sent an error!`), u(`${m} ${t.filename}:${t.lineno}: ${t.message}`), t;
    }, V(C instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), V(ll instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
    var c = [], d = ["onExit", "onAbort", "print", "printErr"];
    for (var b of d)
      a.propertyIsEnumerable(b) && c.push(b);
    l.postMessage({ cmd: "load", handlers: c, wasmMemory: C, wasmModule: ll, workerID: l.workerID });
  }), async loadWasmModuleToAllWorkers() {
    return s ? void 0 : Promise.all(p.unusedWorkers.map(p.loadWasmModuleToWorker));
  }, allocateUnusedWorker() {
    var l;
    if (a.mainScriptUrlOrBlob) {
      var Z = a.mainScriptUrlOrBlob;
      typeof Z != "string" && (Z = URL.createObjectURL(Z)), l = new Worker(
        Z,
        /* @vite-ignore */
        { type: "module", name: "em-pthread-" + p.nextWorkerID }
      );
    } else l = new Worker(
      /* @vite-ignore */
      new URL("data:text/javascript;base64,YXN5bmMgZnVuY3Rpb24gY3BTYXRNb2R1bGUobW9kdWxlQXJnPXt9KXt2YXIgbW9kdWxlUnRuOyhmdW5jdGlvbigpe2Z1bmN0aW9uIGh1bWFuUmVhZGFibGVWZXJzaW9uVG9QYWNrZWQoc3RyKXtzdHI9c3RyLnNwbGl0KCItIilbMF07dmFyIHZlcnM9c3RyLnNwbGl0KCIuIikuc2xpY2UoMCwzKTt3aGlsZSh2ZXJzLmxlbmd0aDwzKXZlcnMucHVzaCgiMDAiKTt2ZXJzPXZlcnMubWFwKChuLGksYXJyKT0+bi5wYWRTdGFydCgyLCIwIikpO3JldHVybiB2ZXJzLmpvaW4oIiIpfXZhciBwYWNrZWRWZXJzaW9uVG9IdW1hblJlYWRhYmxlPW49PltuLzFlNHwwLChuLzEwMHwwKSUxMDAsbiUxMDBdLmpvaW4oIi4iKTt2YXIgVEFSR0VUX05PVF9TVVBQT1JURUQ9MjE0NzQ4MzY0Nzt2YXIgY3VycmVudE5vZGVWZXJzaW9uPXR5cGVvZiBwcm9jZXNzIT09InVuZGVmaW5lZCImJnByb2Nlc3MudmVyc2lvbnM/Lm5vZGU/aHVtYW5SZWFkYWJsZVZlcnNpb25Ub1BhY2tlZChwcm9jZXNzLnZlcnNpb25zLm5vZGUpOlRBUkdFVF9OT1RfU1VQUE9SVEVEO2lmKGN1cnJlbnROb2RlVmVyc2lvbjxUQVJHRVRfTk9UX1NVUFBPUlRFRCl7dGhyb3cgbmV3IEVycm9yKCJub3QgY29tcGlsZWQgZm9yIHRoaXMgZW52aXJvbm1lbnQgKGRpZCB5b3UgYnVpbGQgdG8gSFRNTCBhbmQgdHJ5IHRvIHJ1biBpdCBub3Qgb24gdGhlIHdlYiwgb3Igc2V0IEVOVklST05NRU5UIHRvIHNvbWV0aGluZyAtIGxpa2Ugbm9kZSAtIGFuZCBydW4gaXQgc29tZXBsYWNlIGVsc2UgLSBsaWtlIG9uIHRoZSB3ZWI/KSIpfWlmKGN1cnJlbnROb2RlVmVyc2lvbjwyMTQ3NDgzNjQ3KXt0aHJvdyBuZXcgRXJyb3IoYFRoaXMgZW1zY3JpcHRlbi1nZW5lcmF0ZWQgY29kZSByZXF1aXJlcyBub2RlIHYke3BhY2tlZFZlcnNpb25Ub0h1bWFuUmVhZGFibGUoMjE0NzQ4MzY0Nyl9IChkZXRlY3RlZCB2JHtwYWNrZWRWZXJzaW9uVG9IdW1hblJlYWRhYmxlKGN1cnJlbnROb2RlVmVyc2lvbil9KWApfXZhciB1c2VyQWdlbnQ9dHlwZW9mIG5hdmlnYXRvciE9PSJ1bmRlZmluZWQiJiZuYXZpZ2F0b3IudXNlckFnZW50O2lmKCF1c2VyQWdlbnQpe3JldHVybn12YXIgY3VycmVudFNhZmFyaVZlcnNpb249dXNlckFnZW50LmluY2x1ZGVzKCJTYWZhcmkvIikmJnVzZXJBZ2VudC5tYXRjaCgvVmVyc2lvblwvKFxkK1wuP1xkKlwuP1xkKikvKT9odW1hblJlYWRhYmxlVmVyc2lvblRvUGFja2VkKHVzZXJBZ2VudC5tYXRjaCgvVmVyc2lvblwvKFxkK1wuP1xkKlwuP1xkKikvKVsxXSk6VEFSR0VUX05PVF9TVVBQT1JURUQ7aWYoY3VycmVudFNhZmFyaVZlcnNpb248MTVlNCl7dGhyb3cgbmV3IEVycm9yKGBUaGlzIGVtc2NyaXB0ZW4tZ2VuZXJhdGVkIGNvZGUgcmVxdWlyZXMgU2FmYXJpIHYke3BhY2tlZFZlcnNpb25Ub0h1bWFuUmVhZGFibGUoMTVlNCl9IChkZXRlY3RlZCB2JHtjdXJyZW50U2FmYXJpVmVyc2lvbn0pYCl9dmFyIGN1cnJlbnRGaXJlZm94VmVyc2lvbj11c2VyQWdlbnQubWF0Y2goL0ZpcmVmb3hcLyhcZCsoPzpcLlxkKyk/KS8pP3BhcnNlRmxvYXQodXNlckFnZW50Lm1hdGNoKC9GaXJlZm94XC8oXGQrKD86XC5cZCspPykvKVsxXSk6VEFSR0VUX05PVF9TVVBQT1JURUQ7aWYoY3VycmVudEZpcmVmb3hWZXJzaW9uPDExNCl7dGhyb3cgbmV3IEVycm9yKGBUaGlzIGVtc2NyaXB0ZW4tZ2VuZXJhdGVkIGNvZGUgcmVxdWlyZXMgRmlyZWZveCB2MTE0IChkZXRlY3RlZCB2JHtjdXJyZW50RmlyZWZveFZlcnNpb259KWApfXZhciBjdXJyZW50Q2hyb21lVmVyc2lvbj11c2VyQWdlbnQubWF0Y2goL0Nocm9tZVwvKFxkKyg/OlwuXGQrKT8pLyk/cGFyc2VGbG9hdCh1c2VyQWdlbnQubWF0Y2goL0Nocm9tZVwvKFxkKyg/OlwuXGQrKT8pLylbMV0pOlRBUkdFVF9OT1RfU1VQUE9SVEVEO2lmKGN1cnJlbnRDaHJvbWVWZXJzaW9uPDg1KXt0aHJvdyBuZXcgRXJyb3IoYFRoaXMgZW1zY3JpcHRlbi1nZW5lcmF0ZWQgY29kZSByZXF1aXJlcyBDaHJvbWUgdjg1IChkZXRlY3RlZCB2JHtjdXJyZW50Q2hyb21lVmVyc2lvbn0pYCl9fSkoKTt2YXIgTW9kdWxlPW1vZHVsZUFyZzt2YXIgRU5WSVJPTk1FTlRfSVNfV0VCPSEhZ2xvYmFsVGhpcy53aW5kb3c7dmFyIEVOVklST05NRU5UX0lTX1dPUktFUj0hIWdsb2JhbFRoaXMuV29ya2VyR2xvYmFsU2NvcGU7dmFyIEVOVklST05NRU5UX0lTX05PREU9Z2xvYmFsVGhpcy5wcm9jZXNzPy52ZXJzaW9ucz8ubm9kZSYmZ2xvYmFsVGhpcy5wcm9jZXNzPy50eXBlIT0icmVuZGVyZXIiO3ZhciBFTlZJUk9OTUVOVF9JU19TSEVMTD0hRU5WSVJPTk1FTlRfSVNfV0VCJiYhRU5WSVJPTk1FTlRfSVNfTk9ERSYmIUVOVklST05NRU5UX0lTX1dPUktFUjt2YXIgRU5WSVJPTk1FTlRfSVNfUFRIUkVBRD1FTlZJUk9OTUVOVF9JU19XT1JLRVImJnNlbGYubmFtZT8uc3RhcnRzV2l0aCgiZW0tcHRocmVhZCIpO2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe2Fzc2VydCghZ2xvYmFsVGhpcy5tb2R1bGVMb2FkZWQsIm1vZHVsZSBzaG91bGQgb25seSBiZSBsb2FkZWQgb25jZSBvbiBlYWNoIHB0aHJlYWQgd29ya2VyIik7Z2xvYmFsVGhpcy5tb2R1bGVMb2FkZWQ9dHJ1ZX12YXIgYXJndW1lbnRzXz1bXTt2YXIgdGhpc1Byb2dyYW09Ii4vdGhpcy5wcm9ncmFtIjt2YXIgcXVpdF89KHN0YXR1cyx0b1Rocm93KT0+e3Rocm93IHRvVGhyb3d9O3ZhciBfc2NyaXB0TmFtZT1pbXBvcnQubWV0YS51cmw7dmFyIHNjcmlwdERpcmVjdG9yeT0iIjtmdW5jdGlvbiBsb2NhdGVGaWxlKHBhdGgpe2lmKE1vZHVsZVsibG9jYXRlRmlsZSJdKXtyZXR1cm4gTW9kdWxlWyJsb2NhdGVGaWxlIl0ocGF0aCxzY3JpcHREaXJlY3RvcnkpfXJldHVybiBzY3JpcHREaXJlY3RvcnkrcGF0aH12YXIgcmVhZEFzeW5jLHJlYWRCaW5hcnk7aWYoRU5WSVJPTk1FTlRfSVNfU0hFTEwpe31lbHNlIGlmKEVOVklST05NRU5UX0lTX1dFQnx8RU5WSVJPTk1FTlRfSVNfV09SS0VSKXt0cnl7c2NyaXB0RGlyZWN0b3J5PW5ldyBVUkwoIi4iLF9zY3JpcHROYW1lKS5ocmVmfWNhdGNoe31pZighKGdsb2JhbFRoaXMud2luZG93fHxnbG9iYWxUaGlzLldvcmtlckdsb2JhbFNjb3BlKSl0aHJvdyBuZXcgRXJyb3IoIm5vdCBjb21waWxlZCBmb3IgdGhpcyBlbnZpcm9ubWVudCAoZGlkIHlvdSBidWlsZCB0byBIVE1MIGFuZCB0cnkgdG8gcnVuIGl0IG5vdCBvbiB0aGUgd2ViLCBvciBzZXQgRU5WSVJPTk1FTlQgdG8gc29tZXRoaW5nIC0gbGlrZSBub2RlIC0gYW5kIHJ1biBpdCBzb21lcGxhY2UgZWxzZSAtIGxpa2Ugb24gdGhlIHdlYj8pIik7e2lmKEVOVklST05NRU5UX0lTX1dPUktFUil7cmVhZEJpbmFyeT11cmw9Pnt2YXIgeGhyPW5ldyBYTUxIdHRwUmVxdWVzdDt4aHIub3BlbigiR0VUIix1cmwsZmFsc2UpO3hoci5yZXNwb25zZVR5cGU9ImFycmF5YnVmZmVyIjt4aHIuc2VuZChudWxsKTtyZXR1cm4gbmV3IFVpbnQ4QXJyYXkoeGhyLnJlc3BvbnNlKX19cmVhZEFzeW5jPWFzeW5jIHVybD0+e2Fzc2VydCghaXNGaWxlVVJJKHVybCksInJlYWRBc3luYyBkb2VzIG5vdCB3b3JrIHdpdGggZmlsZTovLyBVUkxzIik7dmFyIHJlc3BvbnNlPWF3YWl0IGZldGNoKHVybCx7Y3JlZGVudGlhbHM6InNhbWUtb3JpZ2luIn0pO2lmKHJlc3BvbnNlLm9rKXtyZXR1cm4gcmVzcG9uc2UuYXJyYXlCdWZmZXIoKX10aHJvdyBuZXcgRXJyb3IocmVzcG9uc2Uuc3RhdHVzKyIgOiAiK3Jlc3BvbnNlLnVybCl9fX1lbHNle3Rocm93IG5ldyBFcnJvcigiZW52aXJvbm1lbnQgZGV0ZWN0aW9uIGVycm9yIil9dmFyIG91dD1jb25zb2xlLmxvZy5iaW5kKGNvbnNvbGUpO3ZhciBlcnI9Y29uc29sZS5lcnJvci5iaW5kKGNvbnNvbGUpO2Fzc2VydChFTlZJUk9OTUVOVF9JU19XRUJ8fEVOVklST05NRU5UX0lTX1dPUktFUnx8RU5WSVJPTk1FTlRfSVNfTk9ERSwiUHRocmVhZHMgZG8gbm90IHdvcmsgaW4gdGhpcyBlbnZpcm9ubWVudCB5ZXQgKG5lZWQgV2ViIFdvcmtlcnMsIG9yIGFuIGFsdGVybmF0aXZlIHRvIHRoZW0pIik7YXNzZXJ0KCFFTlZJUk9OTUVOVF9JU19OT0RFLCJub2RlIGVudmlyb25tZW50IGRldGVjdGVkIGJ1dCBub3QgZW5hYmxlZCBhdCBidWlsZCB0aW1lLiAgQWRkIGBub2RlYCB0byBgLXNFTlZJUk9OTUVOVGAgdG8gZW5hYmxlLiIpO2Fzc2VydCghRU5WSVJPTk1FTlRfSVNfU0hFTEwsInNoZWxsIGVudmlyb25tZW50IGRldGVjdGVkIGJ1dCBub3QgZW5hYmxlZCBhdCBidWlsZCB0aW1lLiAgQWRkIGBzaGVsbGAgdG8gYC1zRU5WSVJPTk1FTlRgIHRvIGVuYWJsZS4iKTt2YXIgd2FzbUJpbmFyeTtpZighZ2xvYmFsVGhpcy5XZWJBc3NlbWJseSl7ZXJyKCJubyBuYXRpdmUgd2FzbSBzdXBwb3J0IGRldGVjdGVkIil9dmFyIHdhc21Nb2R1bGU7dmFyIEFCT1JUPWZhbHNlO3ZhciBFWElUU1RBVFVTO2Z1bmN0aW9uIGFzc2VydChjb25kaXRpb24sdGV4dCl7aWYoIWNvbmRpdGlvbil7YWJvcnQoIkFzc2VydGlvbiBmYWlsZWQiKyh0ZXh0PyI6ICIrdGV4dDoiIikpfX12YXIgaXNGaWxlVVJJPWZpbGVuYW1lPT5maWxlbmFtZS5zdGFydHNXaXRoKCJmaWxlOi8vIik7ZnVuY3Rpb24gd3JpdGVTdGFja0Nvb2tpZSgpe3ZhciBtYXg9X2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2VuZCgpO2Fzc2VydCgobWF4JjMpPT0wKTtpZihtYXg9PTApe21heCs9NH0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4Pj4yXT0zNDgyMTIyMzsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4KzQ+PjJdPTIzMTA3MjEwMjI7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpWzA+PjJdPTE2Njg1MDkwMjl9ZnVuY3Rpb24gY2hlY2tTdGFja0Nvb2tpZSgpe2lmKEFCT1JUKXJldHVybjt2YXIgbWF4PV9lbXNjcmlwdGVuX3N0YWNrX2dldF9lbmQoKTtpZihtYXg9PTApe21heCs9NH12YXIgY29va2llMT0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4Pj4yXTt2YXIgY29va2llMj0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4KzQ+PjJdO2lmKGNvb2tpZTEhPTM0ODIxMjIzfHxjb29raWUyIT0yMzEwNzIxMDIyKXthYm9ydChgU3RhY2sgb3ZlcmZsb3chIFN0YWNrIGNvb2tpZSBoYXMgYmVlbiBvdmVyd3JpdHRlbiBhdCAke3B0clRvU3RyaW5nKG1heCl9LCBleHBlY3RlZCBoZXggZHdvcmRzIDB4ODlCQUNERkUgYW5kIDB4MjEzNTQ2NywgYnV0IHJlY2VpdmVkICR7cHRyVG9TdHJpbmcoY29va2llMil9ICR7cHRyVG9TdHJpbmcoY29va2llMSl9YCl9aWYoKGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpWzA+PjJdIT0xNjY4NTA5MDI5KXthYm9ydCgiUnVudGltZSBlcnJvcjogVGhlIGFwcGxpY2F0aW9uIGhhcyBjb3JydXB0ZWQgaXRzIGhlYXAgbWVtb3J5IGFyZWEgKGFkZHJlc3MgemVybykhIil9fXZhciBydW50aW1lRGVidWc9dHJ1ZTtmdW5jdGlvbiBkYmcoLi4uYXJncyl7aWYoIXJ1bnRpbWVEZWJ1ZyYmdHlwZW9mIHJ1bnRpbWVEZWJ1ZyE9InVuZGVmaW5lZCIpcmV0dXJuO2NvbnNvbGUud2FybiguLi5hcmdzKX0oKCk9Pnt2YXIgaDE2PW5ldyBJbnQxNkFycmF5KDEpO3ZhciBoOD1uZXcgSW50OEFycmF5KGgxNi5idWZmZXIpO2gxNlswXT0yNTQ1OTtpZihoOFswXSE9PTExNXx8aDhbMV0hPT05OSlhYm9ydCgiUnVudGltZSBlcnJvcjogZXhwZWN0ZWQgdGhlIHN5c3RlbSB0byBiZSBsaXR0bGUtZW5kaWFuISAoUnVuIHdpdGggLXNTVVBQT1JUX0JJR19FTkRJQU4gdG8gYnlwYXNzKSIpfSkoKTtmdW5jdGlvbiBjb25zdW1lZE1vZHVsZVByb3AocHJvcCl7aWYoIU9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3IoTW9kdWxlLHByb3ApKXtPYmplY3QuZGVmaW5lUHJvcGVydHkoTW9kdWxlLHByb3Ase2NvbmZpZ3VyYWJsZTp0cnVlLHNldCgpe2Fib3J0KGBBdHRlbXB0IHRvIHNldCBcYE1vZHVsZS4ke3Byb3B9XGAgYWZ0ZXIgaXQgaGFzIGFscmVhZHkgYmVlbiBwcm9jZXNzZWQuICBUaGlzIGNhbiBoYXBwZW4sIGZvciBleGFtcGxlLCB3aGVuIGNvZGUgaXMgaW5qZWN0ZWQgdmlhICctLXBvc3QtanMnIHJhdGhlciB0aGFuICctLXByZS1qcydgKX19KX19ZnVuY3Rpb24gbWFrZUludmFsaWRFYXJseUFjY2VzcyhuYW1lKXtyZXR1cm4oKT0+YXNzZXJ0KGZhbHNlLGBjYWxsIHRvICcke25hbWV9JyB2aWEgcmVmZXJlbmNlIHRha2VuIGJlZm9yZSBXYXNtIG1vZHVsZSBpbml0aWFsaXphdGlvbmApfWZ1bmN0aW9uIGlnbm9yZWRNb2R1bGVQcm9wKHByb3Ape2lmKE9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3IoTW9kdWxlLHByb3ApKXthYm9ydChgXGBNb2R1bGUuJHtwcm9wfVxgIHdhcyBzdXBwbGllZCBidXQgXGAke3Byb3B9XGAgbm90IGluY2x1ZGVkIGluIElOQ09NSU5HX01PRFVMRV9KU19BUElgKX19ZnVuY3Rpb24gaXNFeHBvcnRlZEJ5Rm9yY2VGaWxlc3lzdGVtKG5hbWUpe3JldHVybiBuYW1lPT09IkZTX2NyZWF0ZVBhdGgifHxuYW1lPT09IkZTX2NyZWF0ZURhdGFGaWxlInx8bmFtZT09PSJGU19jcmVhdGVQcmVsb2FkZWRGaWxlInx8bmFtZT09PSJGU19wcmVsb2FkRmlsZSJ8fG5hbWU9PT0iRlNfdW5saW5rInx8bmFtZT09PSJhZGRSdW5EZXBlbmRlbmN5Inx8bmFtZT09PSJGU19jcmVhdGVMYXp5RmlsZSJ8fG5hbWU9PT0iRlNfY3JlYXRlRGV2aWNlInx8bmFtZT09PSJyZW1vdmVSdW5EZXBlbmRlbmN5In1mdW5jdGlvbiBtaXNzaW5nTGlicmFyeVN5bWJvbChzeW0pe3VuZXhwb3J0ZWRSdW50aW1lU3ltYm9sKHN5bSl9ZnVuY3Rpb24gdW5leHBvcnRlZFJ1bnRpbWVTeW1ib2woc3ltKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtyZXR1cm59aWYoIU9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3IoTW9kdWxlLHN5bSkpe09iamVjdC5kZWZpbmVQcm9wZXJ0eShNb2R1bGUsc3ltLHtjb25maWd1cmFibGU6dHJ1ZSxnZXQoKXt2YXIgbXNnPWAnJHtzeW19JyB3YXMgbm90IGV4cG9ydGVkLiBhZGQgaXQgdG8gRVhQT1JURURfUlVOVElNRV9NRVRIT0RTIChzZWUgdGhlIEVtc2NyaXB0ZW4gRkFRKWA7aWYoaXNFeHBvcnRlZEJ5Rm9yY2VGaWxlc3lzdGVtKHN5bSkpe21zZys9Ii4gQWx0ZXJuYXRpdmVseSwgZm9yY2luZyBmaWxlc3lzdGVtIHN1cHBvcnQgKC1zRk9SQ0VfRklMRVNZU1RFTSkgY2FuIGV4cG9ydCB0aGlzIGZvciB5b3UifWFib3J0KG1zZyl9fSl9fWZ1bmN0aW9uIGluaXRXb3JrZXJMb2dnaW5nKCl7ZnVuY3Rpb24gZ2V0TG9nUHJlZml4KCl7dmFyIHQ9MDtpZihydW50aW1lSW5pdGlhbGl6ZWQmJnR5cGVvZiBfcHRocmVhZF9zZWxmIT0idW5kZWZpbmVkIil7dD1fcHRocmVhZF9zZWxmKCl9cmV0dXJuYHc6JHt3b3JrZXJJRH0sdDoke3B0clRvU3RyaW5nKHQpfTpgfXZhciBvcmlnRGJnPWRiZztkYmc9KC4uLmFyZ3MpPT5vcmlnRGJnKGdldExvZ1ByZWZpeCgpLC4uLmFyZ3MpfWluaXRXb3JrZXJMb2dnaW5nKCk7ZnVuY3Rpb24gZ3Jvd01lbVZpZXdzKCl7aWYod2FzbU1lbW9yeS5idWZmZXIhPUhFQVA4LmJ1ZmZlcil7dXBkYXRlTWVtb3J5Vmlld3MoKX19dmFyIHJlYWR5UHJvbWlzZVJlc29sdmUscmVhZHlQcm9taXNlUmVqZWN0O3ZhciB3b3JrZXJJRD0wO3ZhciBzdGFydFdvcmtlcjtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXt2YXIgaW5pdGlhbGl6ZWRKUz1mYWxzZTtzZWxmLm9udW5oYW5kbGVkcmVqZWN0aW9uPWU9Pnt0aHJvdyBlLnJlYXNvbnx8ZX07YXN5bmMgZnVuY3Rpb24gaGFuZGxlTWVzc2FnZShlKXt0cnl7dmFyIG1zZ0RhdGE9ZVsiZGF0YSJdO3ZhciBjbWQ9bXNnRGF0YS5jbWQ7aWYoY21kPT09ImxvYWQiKXt3b3JrZXJJRD1tc2dEYXRhLndvcmtlcklEO2xldCBtZXNzYWdlUXVldWU9W107c2VsZi5vbm1lc3NhZ2U9ZT0+bWVzc2FnZVF1ZXVlLnB1c2goZSk7c3RhcnRXb3JrZXI9KCk9Pntwb3N0TWVzc2FnZSh7Y21kOiJsb2FkZWQifSk7Zm9yKGxldCBtc2cgb2YgbWVzc2FnZVF1ZXVlKXtoYW5kbGVNZXNzYWdlKG1zZyl9c2VsZi5vbm1lc3NhZ2U9aGFuZGxlTWVzc2FnZX07Zm9yKGNvbnN0IGhhbmRsZXIgb2YgbXNnRGF0YS5oYW5kbGVycyl7aWYoIU1vZHVsZVtoYW5kbGVyXXx8TW9kdWxlW2hhbmRsZXJdLnByb3h5KXtNb2R1bGVbaGFuZGxlcl09KC4uLmFyZ3MpPT57cG9zdE1lc3NhZ2Uoe2NtZDoiY2FsbEhhbmRsZXIiLGhhbmRsZXIsYXJnc30pfTtpZihoYW5kbGVyPT0icHJpbnQiKW91dD1Nb2R1bGVbaGFuZGxlcl07aWYoaGFuZGxlcj09InByaW50RXJyIillcnI9TW9kdWxlW2hhbmRsZXJdfX13YXNtTWVtb3J5PW1zZ0RhdGEud2FzbU1lbW9yeTt1cGRhdGVNZW1vcnlWaWV3cygpO3dhc21Nb2R1bGU9bXNnRGF0YS53YXNtTW9kdWxlO2NyZWF0ZVdhc20oKTtydW4oKX1lbHNlIGlmKGNtZD09PSJydW4iKXthc3NlcnQobXNnRGF0YS5wdGhyZWFkX3B0cik7ZXN0YWJsaXNoU3RhY2tTcGFjZShtc2dEYXRhLnB0aHJlYWRfcHRyKTtfX2Vtc2NyaXB0ZW5fdGhyZWFkX2luaXQobXNnRGF0YS5wdGhyZWFkX3B0ciwwLDAsMSwwLDApO1BUaHJlYWQudGhyZWFkSW5pdFRMUygpO19fZW1zY3JpcHRlbl90aHJlYWRfbWFpbGJveF9hd2FpdChtc2dEYXRhLnB0aHJlYWRfcHRyKTtpZighaW5pdGlhbGl6ZWRKUyl7X19lbWJpbmRfaW5pdGlhbGl6ZV9iaW5kaW5ncygpO2luaXRpYWxpemVkSlM9dHJ1ZX10cnl7YXdhaXQgaW52b2tlRW50cnlQb2ludChtc2dEYXRhLnN0YXJ0X3JvdXRpbmUsbXNnRGF0YS5hcmcpfWNhdGNoKGV4KXtpZihleCE9InVud2luZCIpe3Rocm93IGV4fX19ZWxzZSBpZihtc2dEYXRhLnRhcmdldD09PSJzZXRpbW1lZGlhdGUiKXt9ZWxzZSBpZihjbWQ9PT0iY2hlY2tNYWlsYm94Iil7aWYoaW5pdGlhbGl6ZWRKUyl7Y2hlY2tNYWlsYm94KCl9fWVsc2UgaWYoY21kKXtlcnIoYHdvcmtlcjogcmVjZWl2ZWQgdW5rbm93biBjb21tYW5kICR7Y21kfWApO2Vycihtc2dEYXRhKX19Y2F0Y2goZXgpe2Vycihgd29ya2VyOiBvbm1lc3NhZ2UoKSBjYXB0dXJlZCBhbiB1bmNhdWdodCBleGNlcHRpb246ICR7ZXh9YCk7aWYoZXg/LnN0YWNrKWVycihleC5zdGFjayk7X19lbXNjcmlwdGVuX3RocmVhZF9jcmFzaGVkKCk7dGhyb3cgZXh9fXNlbGYub25tZXNzYWdlPWhhbmRsZU1lc3NhZ2V9dmFyIEhFQVA4LEhFQVBVOCxIRUFQMTYsSEVBUFUxNixIRUFQMzIsSEVBUFUzMixIRUFQRjMyLEhFQVBGNjQ7dmFyIEhFQVA2NCxIRUFQVTY0O3ZhciBydW50aW1lSW5pdGlhbGl6ZWQ9ZmFsc2U7ZnVuY3Rpb24gdXBkYXRlTWVtb3J5Vmlld3MoKXt2YXIgYj13YXNtTWVtb3J5LmJ1ZmZlcjtIRUFQOD1uZXcgSW50OEFycmF5KGIpO0hFQVAxNj1uZXcgSW50MTZBcnJheShiKTtNb2R1bGVbIkhFQVBVOCJdPUhFQVBVOD1uZXcgVWludDhBcnJheShiKTtIRUFQVTE2PW5ldyBVaW50MTZBcnJheShiKTtIRUFQMzI9bmV3IEludDMyQXJyYXkoYik7SEVBUFUzMj1uZXcgVWludDMyQXJyYXkoYik7SEVBUEYzMj1uZXcgRmxvYXQzMkFycmF5KGIpO0hFQVBGNjQ9bmV3IEZsb2F0NjRBcnJheShiKTtIRUFQNjQ9bmV3IEJpZ0ludDY0QXJyYXkoYik7SEVBUFU2ND1uZXcgQmlnVWludDY0QXJyYXkoYil9ZnVuY3Rpb24gaW5pdE1lbW9yeSgpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe3JldHVybn1pZihNb2R1bGVbIndhc21NZW1vcnkiXSl7d2FzbU1lbW9yeT1Nb2R1bGVbIndhc21NZW1vcnkiXX1lbHNle3ZhciBJTklUSUFMX01FTU9SWT1Nb2R1bGVbIklOSVRJQUxfTUVNT1JZIl18fDE2Nzc3MjE2O2Fzc2VydChJTklUSUFMX01FTU9SWT49NjU1MzYsIklOSVRJQUxfTUVNT1JZIHNob3VsZCBiZSBsYXJnZXIgdGhhbiBTVEFDS19TSVpFLCB3YXMgIitJTklUSUFMX01FTU9SWSsiISAoU1RBQ0tfU0laRT0iKzY1NTM2KyIpIik7d2FzbU1lbW9yeT1uZXcgV2ViQXNzZW1ibHkuTWVtb3J5KHtpbml0aWFsOklOSVRJQUxfTUVNT1JZLzY1NTM2LG1heGltdW06MzI3Njgsc2hhcmVkOnRydWV9KX11cGRhdGVNZW1vcnlWaWV3cygpfWFzc2VydChnbG9iYWxUaGlzLkludDMyQXJyYXkmJmdsb2JhbFRoaXMuRmxvYXQ2NEFycmF5JiZJbnQzMkFycmF5LnByb3RvdHlwZS5zdWJhcnJheSYmSW50MzJBcnJheS5wcm90b3R5cGUuc2V0LCJKUyBlbmdpbmUgZG9lcyBub3QgcHJvdmlkZSBmdWxsIHR5cGVkIGFycmF5IHN1cHBvcnQiKTtmdW5jdGlvbiBwcmVSdW4oKXthc3NlcnQoIUVOVklST05NRU5UX0lTX1BUSFJFQUQpO2lmKE1vZHVsZVsicHJlUnVuIl0pe2lmKHR5cGVvZiBNb2R1bGVbInByZVJ1biJdPT0iZnVuY3Rpb24iKU1vZHVsZVsicHJlUnVuIl09W01vZHVsZVsicHJlUnVuIl1dO3doaWxlKE1vZHVsZVsicHJlUnVuIl0ubGVuZ3RoKXthZGRPblByZVJ1bihNb2R1bGVbInByZVJ1biJdLnNoaWZ0KCkpfX1jb25zdW1lZE1vZHVsZVByb3AoInByZVJ1biIpO2NhbGxSdW50aW1lQ2FsbGJhY2tzKG9uUHJlUnVucyl9ZnVuY3Rpb24gaW5pdFJ1bnRpbWUoKXthc3NlcnQoIXJ1bnRpbWVJbml0aWFsaXplZCk7cnVudGltZUluaXRpYWxpemVkPXRydWU7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gc3RhcnRXb3JrZXIoKTtjaGVja1N0YWNrQ29va2llKCk7aWYoIU1vZHVsZVsibm9GU0luaXQiXSYmIUZTLmluaXRpYWxpemVkKUZTLmluaXQoKTtUVFkuaW5pdCgpO3dhc21FeHBvcnRzWyJfX3dhc21fY2FsbF9jdG9ycyJdKCk7RlMuaWdub3JlUGVybWlzc2lvbnM9ZmFsc2V9ZnVuY3Rpb24gcG9zdFJ1bigpe2NoZWNrU3RhY2tDb29raWUoKTtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtyZXR1cm59aWYoTW9kdWxlWyJwb3N0UnVuIl0pe2lmKHR5cGVvZiBNb2R1bGVbInBvc3RSdW4iXT09ImZ1bmN0aW9uIilNb2R1bGVbInBvc3RSdW4iXT1bTW9kdWxlWyJwb3N0UnVuIl1dO3doaWxlKE1vZHVsZVsicG9zdFJ1biJdLmxlbmd0aCl7YWRkT25Qb3N0UnVuKE1vZHVsZVsicG9zdFJ1biJdLnNoaWZ0KCkpfX1jb25zdW1lZE1vZHVsZVByb3AoInBvc3RSdW4iKTtjYWxsUnVudGltZUNhbGxiYWNrcyhvblBvc3RSdW5zKX1mdW5jdGlvbiBhYm9ydCh3aGF0KXtNb2R1bGVbIm9uQWJvcnQiXT8uKHdoYXQpO3doYXQ9IkFib3J0ZWQoIit3aGF0KyIpIjtlcnIod2hhdCk7QUJPUlQ9dHJ1ZTt2YXIgZT1uZXcgV2ViQXNzZW1ibHkuUnVudGltZUVycm9yKHdoYXQpO3JlYWR5UHJvbWlzZVJlamVjdD8uKGUpO3Rocm93IGV9ZnVuY3Rpb24gY3JlYXRlRXhwb3J0V3JhcHBlcihuYW1lLG5hcmdzKXtyZXR1cm4oLi4uYXJncyk9Pnthc3NlcnQocnVudGltZUluaXRpYWxpemVkLGBuYXRpdmUgZnVuY3Rpb24gXGAke25hbWV9XGAgY2FsbGVkIGJlZm9yZSBydW50aW1lIGluaXRpYWxpemF0aW9uYCk7dmFyIGY9d2FzbUV4cG9ydHNbbmFtZV07YXNzZXJ0KGYsYGV4cG9ydGVkIG5hdGl2ZSBmdW5jdGlvbiBcYCR7bmFtZX1cYCBub3QgZm91bmRgKTthc3NlcnQoYXJncy5sZW5ndGg8PW5hcmdzLGBuYXRpdmUgZnVuY3Rpb24gXGAke25hbWV9XGAgY2FsbGVkIHdpdGggJHthcmdzLmxlbmd0aH0gYXJncyBidXQgZXhwZWN0cyAke25hcmdzfWApO3JldHVybiBmKC4uLmFyZ3MpfX12YXIgd2FzbUJpbmFyeUZpbGU7ZnVuY3Rpb24gZmluZFdhc21CaW5hcnkoKXtpZihNb2R1bGVbImxvY2F0ZUZpbGUiXSl7cmV0dXJuIGxvY2F0ZUZpbGUoImNwX3NhdF9ydW50aW1lLndhc20iKX1yZXR1cm4gbmV3IFVSTCgiY3Bfc2F0X3J1bnRpbWUud2FzbSIsaW1wb3J0Lm1ldGEudXJsKS5ocmVmfWZ1bmN0aW9uIGdldEJpbmFyeVN5bmMoZmlsZSl7aWYoZmlsZT09d2FzbUJpbmFyeUZpbGUmJndhc21CaW5hcnkpe3JldHVybiBuZXcgVWludDhBcnJheSh3YXNtQmluYXJ5KX1pZihyZWFkQmluYXJ5KXtyZXR1cm4gcmVhZEJpbmFyeShmaWxlKX10aHJvdyJib3RoIGFzeW5jIGFuZCBzeW5jIGZldGNoaW5nIG9mIHRoZSB3YXNtIGZhaWxlZCJ9YXN5bmMgZnVuY3Rpb24gZ2V0V2FzbUJpbmFyeShiaW5hcnlGaWxlKXtpZighd2FzbUJpbmFyeSl7dHJ5e3ZhciByZXNwb25zZT1hd2FpdCByZWFkQXN5bmMoYmluYXJ5RmlsZSk7cmV0dXJuIG5ldyBVaW50OEFycmF5KHJlc3BvbnNlKX1jYXRjaHt9fXJldHVybiBnZXRCaW5hcnlTeW5jKGJpbmFyeUZpbGUpfWFzeW5jIGZ1bmN0aW9uIGluc3RhbnRpYXRlQXJyYXlCdWZmZXIoYmluYXJ5RmlsZSxpbXBvcnRzKXt0cnl7dmFyIGJpbmFyeT1hd2FpdCBnZXRXYXNtQmluYXJ5KGJpbmFyeUZpbGUpO3ZhciBpbnN0YW5jZT1hd2FpdCBXZWJBc3NlbWJseS5pbnN0YW50aWF0ZShiaW5hcnksaW1wb3J0cyk7cmV0dXJuIGluc3RhbmNlfWNhdGNoKHJlYXNvbil7ZXJyKGBmYWlsZWQgdG8gYXN5bmNocm9ub3VzbHkgcHJlcGFyZSB3YXNtOiAke3JlYXNvbn1gKTtpZihpc0ZpbGVVUkkoYmluYXJ5RmlsZSkpe2Vycihgd2FybmluZzogTG9hZGluZyBmcm9tIGEgZmlsZSBVUkkgKCR7YmluYXJ5RmlsZX0pIGlzIG5vdCBzdXBwb3J0ZWQgaW4gbW9zdCBicm93c2Vycy4gU2VlIGh0dHBzOi8vZW1zY3JpcHRlbi5vcmcvZG9jcy9nZXR0aW5nX3N0YXJ0ZWQvRkFRLmh0bWwjaG93LWRvLWktcnVuLWEtbG9jYWwtd2Vic2VydmVyLWZvci10ZXN0aW5nLXdoeS1kb2VzLW15LXByb2dyYW0tc3RhbGwtaW4tZG93bmxvYWRpbmctb3ItcHJlcGFyaW5nYCl9YWJvcnQocmVhc29uKX19YXN5bmMgZnVuY3Rpb24gaW5zdGFudGlhdGVBc3luYyhiaW5hcnksYmluYXJ5RmlsZSxpbXBvcnRzKXtpZighYmluYXJ5KXt0cnl7dmFyIHJlc3BvbnNlPWZldGNoKGJpbmFyeUZpbGUse2NyZWRlbnRpYWxzOiJzYW1lLW9yaWdpbiJ9KTt2YXIgaW5zdGFudGlhdGlvblJlc3VsdD1hd2FpdCBXZWJBc3NlbWJseS5pbnN0YW50aWF0ZVN0cmVhbWluZyhyZXNwb25zZSxpbXBvcnRzKTtyZXR1cm4gaW5zdGFudGlhdGlvblJlc3VsdH1jYXRjaChyZWFzb24pe2Vycihgd2FzbSBzdHJlYW1pbmcgY29tcGlsZSBmYWlsZWQ6ICR7cmVhc29ufWApO2VycigiZmFsbGluZyBiYWNrIHRvIEFycmF5QnVmZmVyIGluc3RhbnRpYXRpb24iKX19cmV0dXJuIGluc3RhbnRpYXRlQXJyYXlCdWZmZXIoYmluYXJ5RmlsZSxpbXBvcnRzKX1mdW5jdGlvbiBnZXRXYXNtSW1wb3J0cygpe2Fzc2lnbldhc21JbXBvcnRzKCk7aWYoIXdhc21JbXBvcnRzLl9faW5zdHJ1bWVudGVkKXt3YXNtSW1wb3J0cy5fX2luc3RydW1lbnRlZD10cnVlO0FzeW5jaWZ5Lmluc3RydW1lbnRXYXNtSW1wb3J0cyh3YXNtSW1wb3J0cyl9dmFyIGltcG9ydHM9e2Vudjp3YXNtSW1wb3J0cyx3YXNpX3NuYXBzaG90X3ByZXZpZXcxOndhc21JbXBvcnRzfTtyZXR1cm4gaW1wb3J0c31hc3luYyBmdW5jdGlvbiBjcmVhdGVXYXNtKCl7ZnVuY3Rpb24gcmVjZWl2ZUluc3RhbmNlKGluc3RhbmNlLG1vZHVsZSl7d2FzbUV4cG9ydHM9aW5zdGFuY2UuZXhwb3J0czt3YXNtRXhwb3J0cz1Bc3luY2lmeS5pbnN0cnVtZW50V2FzbUV4cG9ydHMod2FzbUV4cG9ydHMpO3JlZ2lzdGVyVExTSW5pdCh3YXNtRXhwb3J0c1siX2Vtc2NyaXB0ZW5fdGxzX2luaXQiXSk7YXNzaWduV2FzbUV4cG9ydHMod2FzbUV4cG9ydHMpO3dhc21Nb2R1bGU9bW9kdWxlO3JldHVybiB3YXNtRXhwb3J0c312YXIgdHJ1ZU1vZHVsZT1Nb2R1bGU7ZnVuY3Rpb24gcmVjZWl2ZUluc3RhbnRpYXRpb25SZXN1bHQocmVzdWx0KXthc3NlcnQoTW9kdWxlPT09dHJ1ZU1vZHVsZSwidGhlIE1vZHVsZSBvYmplY3Qgc2hvdWxkIG5vdCBiZSByZXBsYWNlZCBkdXJpbmcgYXN5bmMgY29tcGlsYXRpb24gLSBwZXJoYXBzIHRoZSBvcmRlciBvZiBIVE1MIGVsZW1lbnRzIGlzIHdyb25nPyIpO3RydWVNb2R1bGU9bnVsbDtyZXR1cm4gcmVjZWl2ZUluc3RhbmNlKHJlc3VsdFsiaW5zdGFuY2UiXSxyZXN1bHRbIm1vZHVsZSJdKX12YXIgaW5mbz1nZXRXYXNtSW1wb3J0cygpO2lmKE1vZHVsZVsiaW5zdGFudGlhdGVXYXNtIl0pe3JldHVybiBuZXcgUHJvbWlzZSgocmVzb2x2ZSxyZWplY3QpPT57dHJ5e01vZHVsZVsiaW5zdGFudGlhdGVXYXNtIl0oaW5mbywoaW5zdCxtb2QpPT57cmVzb2x2ZShyZWNlaXZlSW5zdGFuY2UoaW5zdCxtb2QpKX0pfWNhdGNoKGUpe2VycihgTW9kdWxlLmluc3RhbnRpYXRlV2FzbSBjYWxsYmFjayBmYWlsZWQgd2l0aCBlcnJvcjogJHtlfWApO3JlamVjdChlKX19KX1pZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXthc3NlcnQod2FzbU1vZHVsZSwid2FzbU1vZHVsZSBzaG91bGQgaGF2ZSBiZWVuIHJlY2VpdmVkIHZpYSBwb3N0TWVzc2FnZSIpO3ZhciBpbnN0YW5jZT1uZXcgV2ViQXNzZW1ibHkuSW5zdGFuY2Uod2FzbU1vZHVsZSxnZXRXYXNtSW1wb3J0cygpKTtyZXR1cm4gcmVjZWl2ZUluc3RhbmNlKGluc3RhbmNlLHdhc21Nb2R1bGUpfXdhc21CaW5hcnlGaWxlPz89ZmluZFdhc21CaW5hcnkoKTt2YXIgcmVzdWx0PWF3YWl0IGluc3RhbnRpYXRlQXN5bmMod2FzbUJpbmFyeSx3YXNtQmluYXJ5RmlsZSxpbmZvKTt2YXIgZXhwb3J0cz1yZWNlaXZlSW5zdGFudGlhdGlvblJlc3VsdChyZXN1bHQpO3JldHVybiBleHBvcnRzfWNsYXNzIEV4aXRTdGF0dXN7bmFtZT0iRXhpdFN0YXR1cyI7Y29uc3RydWN0b3Ioc3RhdHVzKXt0aGlzLm1lc3NhZ2U9YFByb2dyYW0gdGVybWluYXRlZCB3aXRoIGV4aXQoJHtzdGF0dXN9KWA7dGhpcy5zdGF0dXM9c3RhdHVzfX12YXIgdGVybWluYXRlV29ya2VyPXdvcmtlcj0+e3dvcmtlci50ZXJtaW5hdGUoKTt3b3JrZXIub25tZXNzYWdlPWU9Pnt2YXIgY21kPWVbImRhdGEiXS5jbWQ7ZXJyKGByZWNlaXZlZCAiJHtjbWR9IiBjb21tYW5kIGZyb20gdGVybWluYXRlZCB3b3JrZXI6ICR7d29ya2VyLndvcmtlcklEfWApfX07dmFyIGNsZWFudXBUaHJlYWQ9cHRocmVhZF9wdHI9Pnthc3NlcnQoIUVOVklST05NRU5UX0lTX1BUSFJFQUQsIkludGVybmFsIEVycm9yISBjbGVhbnVwVGhyZWFkKCkgY2FuIG9ubHkgZXZlciBiZSBjYWxsZWQgZnJvbSBtYWluIGFwcGxpY2F0aW9uIHRocmVhZCEiKTthc3NlcnQocHRocmVhZF9wdHIsIkludGVybmFsIEVycm9yISBOdWxsIHB0aHJlYWRfcHRyIGluIGNsZWFudXBUaHJlYWQhIik7dmFyIHdvcmtlcj1QVGhyZWFkLnB0aHJlYWRzW3B0aHJlYWRfcHRyXTthc3NlcnQod29ya2VyKTtQVGhyZWFkLnJldHVybldvcmtlclRvUG9vbCh3b3JrZXIpfTt2YXIgY2FsbFJ1bnRpbWVDYWxsYmFja3M9Y2FsbGJhY2tzPT57d2hpbGUoY2FsbGJhY2tzLmxlbmd0aD4wKXtjYWxsYmFja3Muc2hpZnQoKShNb2R1bGUpfX07dmFyIG9uUHJlUnVucz1bXTt2YXIgYWRkT25QcmVSdW49Y2I9Pm9uUHJlUnVucy5wdXNoKGNiKTt2YXIgcnVuRGVwZW5kZW5jaWVzPTA7dmFyIGRlcGVuZGVuY2llc0Z1bGZpbGxlZD1udWxsO3ZhciBydW5EZXBlbmRlbmN5VHJhY2tpbmc9e307dmFyIHJ1bkRlcGVuZGVuY3lXYXRjaGVyPW51bGw7dmFyIHJlbW92ZVJ1bkRlcGVuZGVuY3k9aWQ9PntydW5EZXBlbmRlbmNpZXMtLTtNb2R1bGVbIm1vbml0b3JSdW5EZXBlbmRlbmNpZXMiXT8uKHJ1bkRlcGVuZGVuY2llcyk7YXNzZXJ0KGlkLCJyZW1vdmVSdW5EZXBlbmRlbmN5IHJlcXVpcmVzIGFuIElEIik7YXNzZXJ0KHJ1bkRlcGVuZGVuY3lUcmFja2luZ1tpZF0pO2RlbGV0ZSBydW5EZXBlbmRlbmN5VHJhY2tpbmdbaWRdO2lmKHJ1bkRlcGVuZGVuY2llcz09MCl7aWYocnVuRGVwZW5kZW5jeVdhdGNoZXIhPT1udWxsKXtjbGVhckludGVydmFsKHJ1bkRlcGVuZGVuY3lXYXRjaGVyKTtydW5EZXBlbmRlbmN5V2F0Y2hlcj1udWxsfWlmKGRlcGVuZGVuY2llc0Z1bGZpbGxlZCl7dmFyIGNhbGxiYWNrPWRlcGVuZGVuY2llc0Z1bGZpbGxlZDtkZXBlbmRlbmNpZXNGdWxmaWxsZWQ9bnVsbDtjYWxsYmFjaygpfX19O3ZhciBhZGRSdW5EZXBlbmRlbmN5PWlkPT57cnVuRGVwZW5kZW5jaWVzKys7TW9kdWxlWyJtb25pdG9yUnVuRGVwZW5kZW5jaWVzIl0/LihydW5EZXBlbmRlbmNpZXMpO2Fzc2VydChpZCwiYWRkUnVuRGVwZW5kZW5jeSByZXF1aXJlcyBhbiBJRCIpO2Fzc2VydCghcnVuRGVwZW5kZW5jeVRyYWNraW5nW2lkXSk7cnVuRGVwZW5kZW5jeVRyYWNraW5nW2lkXT0xO2lmKHJ1bkRlcGVuZGVuY3lXYXRjaGVyPT09bnVsbCYmZ2xvYmFsVGhpcy5zZXRJbnRlcnZhbCl7cnVuRGVwZW5kZW5jeVdhdGNoZXI9c2V0SW50ZXJ2YWwoKCk9PntpZihBQk9SVCl7Y2xlYXJJbnRlcnZhbChydW5EZXBlbmRlbmN5V2F0Y2hlcik7cnVuRGVwZW5kZW5jeVdhdGNoZXI9bnVsbDtyZXR1cm59dmFyIHNob3duPWZhbHNlO2Zvcih2YXIgZGVwIGluIHJ1bkRlcGVuZGVuY3lUcmFja2luZyl7aWYoIXNob3duKXtzaG93bj10cnVlO2Vycigic3RpbGwgd2FpdGluZyBvbiBydW4gZGVwZW5kZW5jaWVzOiIpfWVycihgZGVwZW5kZW5jeTogJHtkZXB9YCl9aWYoc2hvd24pe2VycigiKGVuZCBvZiBsaXN0KSIpfX0sMWU0KX19O3ZhciBzcGF3blRocmVhZD10aHJlYWRQYXJhbXM9Pnthc3NlcnQoIUVOVklST05NRU5UX0lTX1BUSFJFQUQsIkludGVybmFsIEVycm9yISBzcGF3blRocmVhZCgpIGNhbiBvbmx5IGV2ZXIgYmUgY2FsbGVkIGZyb20gbWFpbiBhcHBsaWNhdGlvbiB0aHJlYWQhIik7YXNzZXJ0KHRocmVhZFBhcmFtcy5wdGhyZWFkX3B0ciwiSW50ZXJuYWwgZXJyb3IsIG5vIHB0aHJlYWQgcHRyISIpO3ZhciB3b3JrZXI9UFRocmVhZC5nZXROZXdXb3JrZXIoKTtpZighd29ya2VyKXtyZXR1cm4gNn1hc3NlcnQoIXdvcmtlci5wdGhyZWFkX3B0ciwiSW50ZXJuYWwgZXJyb3IhIik7UFRocmVhZC5ydW5uaW5nV29ya2Vycy5wdXNoKHdvcmtlcik7UFRocmVhZC5wdGhyZWFkc1t0aHJlYWRQYXJhbXMucHRocmVhZF9wdHJdPXdvcmtlcjt3b3JrZXIucHRocmVhZF9wdHI9dGhyZWFkUGFyYW1zLnB0aHJlYWRfcHRyO3ZhciBtc2c9e2NtZDoicnVuIixzdGFydF9yb3V0aW5lOnRocmVhZFBhcmFtcy5zdGFydFJvdXRpbmUsYXJnOnRocmVhZFBhcmFtcy5hcmcscHRocmVhZF9wdHI6dGhyZWFkUGFyYW1zLnB0aHJlYWRfcHRyfTt3b3JrZXIucG9zdE1lc3NhZ2UobXNnLHRocmVhZFBhcmFtcy50cmFuc2Zlckxpc3QpO3JldHVybiAwfTt2YXIgcnVudGltZUtlZXBhbGl2ZUNvdW50ZXI9MDt2YXIga2VlcFJ1bnRpbWVBbGl2ZT0oKT0+bm9FeGl0UnVudGltZXx8cnVudGltZUtlZXBhbGl2ZUNvdW50ZXI+MDt2YXIgc3RhY2tTYXZlPSgpPT5fZW1zY3JpcHRlbl9zdGFja19nZXRfY3VycmVudCgpO3ZhciBzdGFja1Jlc3RvcmU9dmFsPT5fX2Vtc2NyaXB0ZW5fc3RhY2tfcmVzdG9yZSh2YWwpO3ZhciBzdGFja0FsbG9jPXN6PT5fX2Vtc2NyaXB0ZW5fc3RhY2tfYWxsb2Moc3opO3ZhciBwcm94eVRvTWFpblRocmVhZD0oZnVuY0luZGV4LGVtQXNtQWRkcixzeW5jLC4uLmNhbGxBcmdzKT0+e3ZhciBzZXJpYWxpemVkTnVtQ2FsbEFyZ3M9Y2FsbEFyZ3MubGVuZ3RoKjI7dmFyIHNwPXN0YWNrU2F2ZSgpO3ZhciBhcmdzPXN0YWNrQWxsb2Moc2VyaWFsaXplZE51bUNhbGxBcmdzKjgpO3ZhciBiPWFyZ3M+PjM7Zm9yKHZhciBpPTA7aTxjYWxsQXJncy5sZW5ndGg7aSsrKXt2YXIgYXJnPWNhbGxBcmdzW2ldO2lmKHR5cGVvZiBhcmc9PSJiaWdpbnQiKXsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtiKzIqaV09MW47KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYisyKmkrMV09YXJnfWVsc2V7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYisyKmldPTBuOyhncm93TWVtVmlld3MoKSxIRUFQRjY0KVtiKzIqaSsxXT1hcmd9fXZhciBydG49X19lbXNjcmlwdGVuX3J1bl9qc19vbl9tYWluX3RocmVhZChmdW5jSW5kZXgsZW1Bc21BZGRyLHNlcmlhbGl6ZWROdW1DYWxsQXJncyxhcmdzLHN5bmMpO3N0YWNrUmVzdG9yZShzcCk7cmV0dXJuIHJ0bn07ZnVuY3Rpb24gX3Byb2NfZXhpdChjb2RlKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgwLDAsMSxjb2RlKTtFWElUU1RBVFVTPWNvZGU7aWYoIWtlZXBSdW50aW1lQWxpdmUoKSl7UFRocmVhZC50ZXJtaW5hdGVBbGxUaHJlYWRzKCk7TW9kdWxlWyJvbkV4aXQiXT8uKGNvZGUpO0FCT1JUPXRydWV9cXVpdF8oY29kZSxuZXcgRXhpdFN0YXR1cyhjb2RlKSl9ZnVuY3Rpb24gZXhpdE9uTWFpblRocmVhZChyZXR1cm5Db2RlKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxLDAsMCxyZXR1cm5Db2RlKTtfZXhpdChyZXR1cm5Db2RlKX12YXIgZXhpdEpTPShzdGF0dXMsaW1wbGljaXQpPT57RVhJVFNUQVRVUz1zdGF0dXM7Y2hlY2tVbmZsdXNoZWRDb250ZW50KCk7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCl7YXNzZXJ0KCFpbXBsaWNpdCk7ZXhpdE9uTWFpblRocmVhZChzdGF0dXMpO3Rocm93InVud2luZCJ9aWYoa2VlcFJ1bnRpbWVBbGl2ZSgpJiYhaW1wbGljaXQpe3ZhciBtc2c9YHByb2dyYW0gZXhpdGVkICh3aXRoIHN0YXR1czogJHtzdGF0dXN9KSwgYnV0IGtlZXBSdW50aW1lQWxpdmUoKSBpcyBzZXQgKGNvdW50ZXI9JHtydW50aW1lS2VlcGFsaXZlQ291bnRlcn0pIGR1ZSB0byBhbiBhc3luYyBvcGVyYXRpb24sIHNvIGhhbHRpbmcgZXhlY3V0aW9uIGJ1dCBub3QgZXhpdGluZyB0aGUgcnVudGltZSBvciBwcmV2ZW50aW5nIGZ1cnRoZXIgYXN5bmMgZXhlY3V0aW9uICh5b3UgY2FuIHVzZSBlbXNjcmlwdGVuX2ZvcmNlX2V4aXQsIGlmIHlvdSB3YW50IHRvIGZvcmNlIGEgdHJ1ZSBzaHV0ZG93bilgO3JlYWR5UHJvbWlzZVJlamVjdD8uKG1zZyk7ZXJyKG1zZyl9X3Byb2NfZXhpdChzdGF0dXMpfTt2YXIgX2V4aXQ9ZXhpdEpTO3ZhciBwdHJUb1N0cmluZz1wdHI9Pnthc3NlcnQodHlwZW9mIHB0cj09PSJudW1iZXIiLGBwdHJUb1N0cmluZyBleHBlY3RzIGEgbnVtYmVyLCBnb3QgJHt0eXBlb2YgcHRyfWApO3B0cj4+Pj0wO3JldHVybiIweCIrcHRyLnRvU3RyaW5nKDE2KS5wYWRTdGFydCg4LCIwIil9O3ZhciBQVGhyZWFkPXt1bnVzZWRXb3JrZXJzOltdLHJ1bm5pbmdXb3JrZXJzOltdLHRsc0luaXRGdW5jdGlvbnM6W10scHRocmVhZHM6e30sbmV4dFdvcmtlcklEOjEsaW5pdCgpe2lmKCFFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtQVGhyZWFkLmluaXRNYWluVGhyZWFkKCl9fSxpbml0TWFpblRocmVhZCgpe3ZhciBwdGhyZWFkUG9vbFNpemU9bmF2aWdhdG9yLmhhcmR3YXJlQ29uY3VycmVuY3k7d2hpbGUocHRocmVhZFBvb2xTaXplLS0pe1BUaHJlYWQuYWxsb2NhdGVVbnVzZWRXb3JrZXIoKX1hZGRPblByZVJ1bihhc3luYygpPT57dmFyIHB0aHJlYWRQb29sUmVhZHk9UFRocmVhZC5sb2FkV2FzbU1vZHVsZVRvQWxsV29ya2VycygpO2FkZFJ1bkRlcGVuZGVuY3koImxvYWRpbmctd29ya2VycyIpO2F3YWl0IHB0aHJlYWRQb29sUmVhZHk7cmVtb3ZlUnVuRGVwZW5kZW5jeSgibG9hZGluZy13b3JrZXJzIil9KX0sdGVybWluYXRlQWxsVGhyZWFkczooKT0+e2Fzc2VydCghRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCwiSW50ZXJuYWwgRXJyb3IhIHRlcm1pbmF0ZUFsbFRocmVhZHMoKSBjYW4gb25seSBldmVyIGJlIGNhbGxlZCBmcm9tIG1haW4gYXBwbGljYXRpb24gdGhyZWFkISIpO2Zvcih2YXIgd29ya2VyIG9mIFBUaHJlYWQucnVubmluZ1dvcmtlcnMpe3Rlcm1pbmF0ZVdvcmtlcih3b3JrZXIpfWZvcih2YXIgd29ya2VyIG9mIFBUaHJlYWQudW51c2VkV29ya2Vycyl7dGVybWluYXRlV29ya2VyKHdvcmtlcil9UFRocmVhZC51bnVzZWRXb3JrZXJzPVtdO1BUaHJlYWQucnVubmluZ1dvcmtlcnM9W107UFRocmVhZC5wdGhyZWFkcz17fX0scmV0dXJuV29ya2VyVG9Qb29sOndvcmtlcj0+e3ZhciBwdGhyZWFkX3B0cj13b3JrZXIucHRocmVhZF9wdHI7ZGVsZXRlIFBUaHJlYWQucHRocmVhZHNbcHRocmVhZF9wdHJdO1BUaHJlYWQudW51c2VkV29ya2Vycy5wdXNoKHdvcmtlcik7UFRocmVhZC5ydW5uaW5nV29ya2Vycy5zcGxpY2UoUFRocmVhZC5ydW5uaW5nV29ya2Vycy5pbmRleE9mKHdvcmtlciksMSk7d29ya2VyLnB0aHJlYWRfcHRyPTA7X19lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGEocHRocmVhZF9wdHIpfSx0aHJlYWRJbml0VExTKCl7UFRocmVhZC50bHNJbml0RnVuY3Rpb25zLmZvckVhY2goZj0+ZigpKX0sbG9hZFdhc21Nb2R1bGVUb1dvcmtlcjp3b3JrZXI9Pm5ldyBQcm9taXNlKG9uRmluaXNoZWRMb2FkaW5nPT57d29ya2VyLm9ubWVzc2FnZT1lPT57dmFyIGQ9ZVsiZGF0YSJdO3ZhciBjbWQ9ZC5jbWQ7aWYoZC50YXJnZXRUaHJlYWQmJmQudGFyZ2V0VGhyZWFkIT1fcHRocmVhZF9zZWxmKCkpe3ZhciB0YXJnZXRXb3JrZXI9UFRocmVhZC5wdGhyZWFkc1tkLnRhcmdldFRocmVhZF07aWYodGFyZ2V0V29ya2VyKXt0YXJnZXRXb3JrZXIucG9zdE1lc3NhZ2UoZCxkLnRyYW5zZmVyTGlzdCl9ZWxzZXtlcnIoYEludGVybmFsIGVycm9yISBXb3JrZXIgc2VudCBhIG1lc3NhZ2UgIiR7Y21kfSIgdG8gdGFyZ2V0IHB0aHJlYWQgJHtkLnRhcmdldFRocmVhZH0sIGJ1dCB0aGF0IHRocmVhZCBubyBsb25nZXIgZXhpc3RzIWApfXJldHVybn1pZihjbWQ9PT0iY2hlY2tNYWlsYm94Iil7Y2hlY2tNYWlsYm94KCl9ZWxzZSBpZihjbWQ9PT0ic3Bhd25UaHJlYWQiKXtzcGF3blRocmVhZChkKX1lbHNlIGlmKGNtZD09PSJjbGVhbnVwVGhyZWFkIil7Y2FsbFVzZXJDYWxsYmFjaygoKT0+Y2xlYW51cFRocmVhZChkLnRocmVhZCkpfWVsc2UgaWYoY21kPT09ImxvYWRlZCIpe3dvcmtlci5sb2FkZWQ9dHJ1ZTtvbkZpbmlzaGVkTG9hZGluZyh3b3JrZXIpfWVsc2UgaWYoZC50YXJnZXQ9PT0ic2V0aW1tZWRpYXRlIil7d29ya2VyLnBvc3RNZXNzYWdlKGQpfWVsc2UgaWYoY21kPT09ImNhbGxIYW5kbGVyIil7TW9kdWxlW2QuaGFuZGxlcl0oLi4uZC5hcmdzKX1lbHNlIGlmKGNtZCl7ZXJyKGB3b3JrZXIgc2VudCBhbiB1bmtub3duIGNvbW1hbmQgJHtjbWR9YCl9fTt3b3JrZXIub25lcnJvcj1lPT57dmFyIG1lc3NhZ2U9IndvcmtlciBzZW50IGFuIGVycm9yISI7aWYod29ya2VyLnB0aHJlYWRfcHRyKXttZXNzYWdlPWBQdGhyZWFkICR7cHRyVG9TdHJpbmcod29ya2VyLnB0aHJlYWRfcHRyKX0gc2VudCBhbiBlcnJvciFgfWVycihgJHttZXNzYWdlfSAke2UuZmlsZW5hbWV9OiR7ZS5saW5lbm99OiAke2UubWVzc2FnZX1gKTt0aHJvdyBlfTthc3NlcnQod2FzbU1lbW9yeSBpbnN0YW5jZW9mIFdlYkFzc2VtYmx5Lk1lbW9yeSwiV2ViQXNzZW1ibHkgbWVtb3J5IHNob3VsZCBoYXZlIGJlZW4gbG9hZGVkIGJ5IG5vdyEiKTthc3NlcnQod2FzbU1vZHVsZSBpbnN0YW5jZW9mIFdlYkFzc2VtYmx5Lk1vZHVsZSwiV2ViQXNzZW1ibHkgTW9kdWxlIHNob3VsZCBoYXZlIGJlZW4gbG9hZGVkIGJ5IG5vdyEiKTt2YXIgaGFuZGxlcnM9W107dmFyIGtub3duSGFuZGxlcnM9WyJvbkV4aXQiLCJvbkFib3J0IiwicHJpbnQiLCJwcmludEVyciJdO2Zvcih2YXIgaGFuZGxlciBvZiBrbm93bkhhbmRsZXJzKXtpZihNb2R1bGUucHJvcGVydHlJc0VudW1lcmFibGUoaGFuZGxlcikpe2hhbmRsZXJzLnB1c2goaGFuZGxlcil9fXdvcmtlci5wb3N0TWVzc2FnZSh7Y21kOiJsb2FkIixoYW5kbGVycyx3YXNtTWVtb3J5LHdhc21Nb2R1bGUsd29ya2VySUQ6d29ya2VyLndvcmtlcklEfSl9KSxhc3luYyBsb2FkV2FzbU1vZHVsZVRvQWxsV29ya2Vycygpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe3JldHVybn1sZXQgcHRocmVhZFBvb2xSZWFkeT1Qcm9taXNlLmFsbChQVGhyZWFkLnVudXNlZFdvcmtlcnMubWFwKFBUaHJlYWQubG9hZFdhc21Nb2R1bGVUb1dvcmtlcikpO3JldHVybiBwdGhyZWFkUG9vbFJlYWR5fSxhbGxvY2F0ZVVudXNlZFdvcmtlcigpe3ZhciB3b3JrZXI7aWYoTW9kdWxlWyJtYWluU2NyaXB0VXJsT3JCbG9iIl0pe3ZhciBwdGhyZWFkTWFpbkpzPU1vZHVsZVsibWFpblNjcmlwdFVybE9yQmxvYiJdO2lmKHR5cGVvZiBwdGhyZWFkTWFpbkpzIT0ic3RyaW5nIil7cHRocmVhZE1haW5Kcz1VUkwuY3JlYXRlT2JqZWN0VVJMKHB0aHJlYWRNYWluSnMpfXdvcmtlcj1uZXcgV29ya2VyKHB0aHJlYWRNYWluSnMse3R5cGU6Im1vZHVsZSIsbmFtZToiZW0tcHRocmVhZC0iK1BUaHJlYWQubmV4dFdvcmtlcklEfSl9ZWxzZSB3b3JrZXI9bmV3IFdvcmtlcihuZXcgVVJMKCJjcF9zYXRfcnVudGltZS5qcyIsaW1wb3J0Lm1ldGEudXJsKSx7dHlwZToibW9kdWxlIixuYW1lOiJlbS1wdGhyZWFkLSIrUFRocmVhZC5uZXh0V29ya2VySUR9KTt3b3JrZXIud29ya2VySUQ9UFRocmVhZC5uZXh0V29ya2VySUQrKztQVGhyZWFkLnVudXNlZFdvcmtlcnMucHVzaCh3b3JrZXIpfSxnZXROZXdXb3JrZXIoKXtpZihQVGhyZWFkLnVudXNlZFdvcmtlcnMubGVuZ3RoPT0wKXtlcnIoIlRyaWVkIHRvIHNwYXduIGEgbmV3IHRocmVhZCwgYnV0IHRoZSB0aHJlYWQgcG9vbCBpcyBleGhhdXN0ZWQuXG4iKyJUaGlzIG1pZ2h0IHJlc3VsdCBpbiBhIGRlYWRsb2NrIHVubGVzcyBzb21lIHRocmVhZHMgZXZlbnR1YWxseSBleGl0IG9yIHRoZSBjb2RlIGV4cGxpY2l0bHkgYnJlYWtzIG91dCB0byB0aGUgZXZlbnQgbG9vcC5cbiIrIklmIHlvdSB3YW50IHRvIGluY3JlYXNlIHRoZSBwb29sIHNpemUsIHVzZSBzZXR0aW5nIGAtc1BUSFJFQURfUE9PTF9TSVpFPS4uLmAuIik7cmV0dXJufXJldHVybiBQVGhyZWFkLnVudXNlZFdvcmtlcnMucG9wKCl9fTt2YXIgb25Qb3N0UnVucz1bXTt2YXIgYWRkT25Qb3N0UnVuPWNiPT5vblBvc3RSdW5zLnB1c2goY2IpO2Z1bmN0aW9uIGVzdGFibGlzaFN0YWNrU3BhY2UocHRocmVhZF9wdHIpe3ZhciBzdGFja0hpZ2g9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3B0aHJlYWRfcHRyKzUyPj4yXTt2YXIgc3RhY2tTaXplPShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwdGhyZWFkX3B0cis1Nj4+Ml07dmFyIHN0YWNrTG93PXN0YWNrSGlnaC1zdGFja1NpemU7YXNzZXJ0KHN0YWNrSGlnaCE9MCk7YXNzZXJ0KHN0YWNrTG93IT0wKTthc3NlcnQoc3RhY2tIaWdoPnN0YWNrTG93LCJzdGFja0hpZ2ggbXVzdCBiZSBoaWdoZXIgdGhlbiBzdGFja0xvdyIpO19lbXNjcmlwdGVuX3N0YWNrX3NldF9saW1pdHMoc3RhY2tIaWdoLHN0YWNrTG93KTtzdGFja1Jlc3RvcmUoc3RhY2tIaWdoKTt3cml0ZVN0YWNrQ29va2llKCl9dmFyIHdhc21UYWJsZU1pcnJvcj1bXTt2YXIgZ2V0V2FzbVRhYmxlRW50cnk9ZnVuY1B0cj0+e3ZhciBmdW5jPXdhc21UYWJsZU1pcnJvcltmdW5jUHRyXTtpZighZnVuYyl7d2FzbVRhYmxlTWlycm9yW2Z1bmNQdHJdPWZ1bmM9d2FzbVRhYmxlLmdldChmdW5jUHRyKTtpZihBc3luY2lmeS5pc0FzeW5jRXhwb3J0KGZ1bmMpKXt3YXNtVGFibGVNaXJyb3JbZnVuY1B0cl09ZnVuYz1Bc3luY2lmeS5tYWtlQXN5bmNGdW5jdGlvbihmdW5jKX19cmV0dXJuIGZ1bmN9O3ZhciBpbnZva2VFbnRyeVBvaW50PWFzeW5jKHB0cixhcmcpPT57cnVudGltZUtlZXBhbGl2ZUNvdW50ZXI9MDtub0V4aXRSdW50aW1lPTA7dmFyIHJlc3VsdD1XZWJBc3NlbWJseS5wcm9taXNpbmcoZ2V0V2FzbVRhYmxlRW50cnkocHRyKSkoYXJnKTtjaGVja1N0YWNrQ29va2llKCk7ZnVuY3Rpb24gZmluaXNoKHJlc3VsdCl7aWYoa2VlcFJ1bnRpbWVBbGl2ZSgpKXtFWElUU1RBVFVTPXJlc3VsdDtyZXR1cm59X19lbXNjcmlwdGVuX3RocmVhZF9leGl0KHJlc3VsdCl9cmVzdWx0PWF3YWl0IHJlc3VsdDtmaW5pc2gocmVzdWx0KX07aW52b2tlRW50cnlQb2ludC5pc0FzeW5jPXRydWU7dmFyIG5vRXhpdFJ1bnRpbWU9dHJ1ZTt2YXIgcmVnaXN0ZXJUTFNJbml0PXRsc0luaXRGdW5jPT5QVGhyZWFkLnRsc0luaXRGdW5jdGlvbnMucHVzaCh0bHNJbml0RnVuYyk7dmFyIHdhcm5PbmNlPXRleHQ9Pnt3YXJuT25jZS5zaG93bnx8PXt9O2lmKCF3YXJuT25jZS5zaG93blt0ZXh0XSl7d2Fybk9uY2Uuc2hvd25bdGV4dF09MTtlcnIodGV4dCl9fTt2YXIgd2FzbU1lbW9yeTt2YXIgVVRGOERlY29kZXI9Z2xvYmFsVGhpcy5UZXh0RGVjb2RlciYmbmV3IFRleHREZWNvZGVyO3ZhciBmaW5kU3RyaW5nRW5kPShoZWFwT3JBcnJheSxpZHgsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKT0+e3ZhciBtYXhJZHg9aWR4K21heEJ5dGVzVG9SZWFkO2lmKGlnbm9yZU51bClyZXR1cm4gbWF4SWR4O3doaWxlKGhlYXBPckFycmF5W2lkeF0mJiEoaWR4Pj1tYXhJZHgpKSsraWR4O3JldHVybiBpZHh9O3ZhciBVVEY4QXJyYXlUb1N0cmluZz0oaGVhcE9yQXJyYXksaWR4PTAsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKT0+e3ZhciBlbmRQdHI9ZmluZFN0cmluZ0VuZChoZWFwT3JBcnJheSxpZHgsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKTtpZihlbmRQdHItaWR4PjE2JiZoZWFwT3JBcnJheS5idWZmZXImJlVURjhEZWNvZGVyKXtyZXR1cm4gVVRGOERlY29kZXIuZGVjb2RlKGhlYXBPckFycmF5LmJ1ZmZlciBpbnN0YW5jZW9mIEFycmF5QnVmZmVyP2hlYXBPckFycmF5LnN1YmFycmF5KGlkeCxlbmRQdHIpOmhlYXBPckFycmF5LnNsaWNlKGlkeCxlbmRQdHIpKX12YXIgc3RyPSIiO3doaWxlKGlkeDxlbmRQdHIpe3ZhciB1MD1oZWFwT3JBcnJheVtpZHgrK107aWYoISh1MCYxMjgpKXtzdHIrPVN0cmluZy5mcm9tQ2hhckNvZGUodTApO2NvbnRpbnVlfXZhciB1MT1oZWFwT3JBcnJheVtpZHgrK10mNjM7aWYoKHUwJjIyNCk9PTE5Mil7c3RyKz1TdHJpbmcuZnJvbUNoYXJDb2RlKCh1MCYzMSk8PDZ8dTEpO2NvbnRpbnVlfXZhciB1Mj1oZWFwT3JBcnJheVtpZHgrK10mNjM7aWYoKHUwJjI0MCk9PTIyNCl7dTA9KHUwJjE1KTw8MTJ8dTE8PDZ8dTJ9ZWxzZXtpZigodTAmMjQ4KSE9MjQwKXdhcm5PbmNlKCJJbnZhbGlkIFVURi04IGxlYWRpbmcgYnl0ZSAiK3B0clRvU3RyaW5nKHUwKSsiIGVuY291bnRlcmVkIHdoZW4gZGVzZXJpYWxpemluZyBhIFVURi04IHN0cmluZyBpbiB3YXNtIG1lbW9yeSB0byBhIEpTIHN0cmluZyEiKTt1MD0odTAmNyk8PDE4fHUxPDwxMnx1Mjw8NnxoZWFwT3JBcnJheVtpZHgrK10mNjN9aWYodTA8NjU1MzYpe3N0cis9U3RyaW5nLmZyb21DaGFyQ29kZSh1MCl9ZWxzZXt2YXIgY2g9dTAtNjU1MzY7c3RyKz1TdHJpbmcuZnJvbUNoYXJDb2RlKDU1Mjk2fGNoPj4xMCw1NjMyMHxjaCYxMDIzKX19cmV0dXJuIHN0cn07dmFyIFVURjhUb1N0cmluZz0ocHRyLG1heEJ5dGVzVG9SZWFkLGlnbm9yZU51bCk9Pnthc3NlcnQodHlwZW9mIHB0cj09Im51bWJlciIsYFVURjhUb1N0cmluZyBleHBlY3RzIGEgbnVtYmVyIChnb3QgJHt0eXBlb2YgcHRyfSlgKTtyZXR1cm4gcHRyP1VURjhBcnJheVRvU3RyaW5nKChncm93TWVtVmlld3MoKSxIRUFQVTgpLHB0cixtYXhCeXRlc1RvUmVhZCxpZ25vcmVOdWwpOiIifTt2YXIgX19fYXNzZXJ0X2ZhaWw9KGNvbmRpdGlvbixmaWxlbmFtZSxsaW5lLGZ1bmMpPT5hYm9ydChgQXNzZXJ0aW9uIGZhaWxlZDogJHtVVEY4VG9TdHJpbmcoY29uZGl0aW9uKX0sIGF0OiBgK1tmaWxlbmFtZT9VVEY4VG9TdHJpbmcoZmlsZW5hbWUpOiJ1bmtub3duIGZpbGVuYW1lIixsaW5lLGZ1bmM/VVRGOFRvU3RyaW5nKGZ1bmMpOiJ1bmtub3duIGZ1bmN0aW9uIl0pO2NsYXNzIEV4Y2VwdGlvbkluZm97Y29uc3RydWN0b3IoZXhjUHRyKXt0aGlzLmV4Y1B0cj1leGNQdHI7dGhpcy5wdHI9ZXhjUHRyLTI0fXNldF90eXBlKHR5cGUpeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cis0Pj4yXT10eXBlfWdldF90eXBlKCl7cmV0dXJuKGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3RoaXMucHRyKzQ+PjJdfXNldF9kZXN0cnVjdG9yKGRlc3RydWN0b3Ipeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cis4Pj4yXT1kZXN0cnVjdG9yfWdldF9kZXN0cnVjdG9yKCl7cmV0dXJuKGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3RoaXMucHRyKzg+PjJdfXNldF9jYXVnaHQoY2F1Z2h0KXtjYXVnaHQ9Y2F1Z2h0PzE6MDsoZ3Jvd01lbVZpZXdzKCksSEVBUDgpW3RoaXMucHRyKzEyXT1jYXVnaHR9Z2V0X2NhdWdodCgpe3JldHVybihncm93TWVtVmlld3MoKSxIRUFQOClbdGhpcy5wdHIrMTJdIT0wfXNldF9yZXRocm93bihyZXRocm93bil7cmV0aHJvd249cmV0aHJvd24/MTowOyhncm93TWVtVmlld3MoKSxIRUFQOClbdGhpcy5wdHIrMTNdPXJldGhyb3dufWdldF9yZXRocm93bigpe3JldHVybihncm93TWVtVmlld3MoKSxIRUFQOClbdGhpcy5wdHIrMTNdIT0wfWluaXQodHlwZSxkZXN0cnVjdG9yKXt0aGlzLnNldF9hZGp1c3RlZF9wdHIoMCk7dGhpcy5zZXRfdHlwZSh0eXBlKTt0aGlzLnNldF9kZXN0cnVjdG9yKGRlc3RydWN0b3IpfXNldF9hZGp1c3RlZF9wdHIoYWRqdXN0ZWRQdHIpeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cisxNj4+Ml09YWRqdXN0ZWRQdHJ9Z2V0X2FkanVzdGVkX3B0cigpe3JldHVybihncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cisxNj4+Ml19fXZhciBleGNlcHRpb25MYXN0PTA7dmFyIHVuY2F1Z2h0RXhjZXB0aW9uQ291bnQ9MDt2YXIgX19fY3hhX3Rocm93PShwdHIsdHlwZSxkZXN0cnVjdG9yKT0+e3ZhciBpbmZvPW5ldyBFeGNlcHRpb25JbmZvKHB0cik7aW5mby5pbml0KHR5cGUsZGVzdHJ1Y3Rvcik7ZXhjZXB0aW9uTGFzdD1wdHI7dW5jYXVnaHRFeGNlcHRpb25Db3VudCsrO2Fzc2VydChmYWxzZSwiRXhjZXB0aW9uIHRocm93biwgYnV0IGV4Y2VwdGlvbiBjYXRjaGluZyBpcyBub3QgZW5hYmxlZC4gQ29tcGlsZSB3aXRoIC1zTk9fRElTQUJMRV9FWENFUFRJT05fQ0FUQ0hJTkcgb3IgLXNFWENFUFRJT05fQ0FUQ0hJTkdfQUxMT1dFRD1bLi5dIHRvIGNhdGNoLiIpfTtmdW5jdGlvbiBwdGhyZWFkQ3JlYXRlUHJveGllZChwdGhyZWFkX3B0cixhdHRyLHN0YXJ0Um91dGluZSxhcmcpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDIsMCwxLHB0aHJlYWRfcHRyLGF0dHIsc3RhcnRSb3V0aW5lLGFyZyk7cmV0dXJuIF9fX3B0aHJlYWRfY3JlYXRlX2pzKHB0aHJlYWRfcHRyLGF0dHIsc3RhcnRSb3V0aW5lLGFyZyl9dmFyIF9lbXNjcmlwdGVuX2hhc190aHJlYWRpbmdfc3VwcG9ydD0oKT0+ISFnbG9iYWxUaGlzLlNoYXJlZEFycmF5QnVmZmVyO3ZhciBfX19wdGhyZWFkX2NyZWF0ZV9qcz0ocHRocmVhZF9wdHIsYXR0cixzdGFydFJvdXRpbmUsYXJnKT0+e2lmKCFfZW1zY3JpcHRlbl9oYXNfdGhyZWFkaW5nX3N1cHBvcnQoKSl7ZGJnKCJwdGhyZWFkX2NyZWF0ZTogZW52aXJvbm1lbnQgZG9lcyBub3Qgc3VwcG9ydCBTaGFyZWRBcnJheUJ1ZmZlciwgcHRocmVhZHMgYXJlIG5vdCBhdmFpbGFibGUiKTtyZXR1cm4gNn12YXIgdHJhbnNmZXJMaXN0PVtdO3ZhciBlcnJvcj0wO2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQmJih0cmFuc2Zlckxpc3QubGVuZ3RoPT09MHx8ZXJyb3IpKXtyZXR1cm4gcHRocmVhZENyZWF0ZVByb3hpZWQocHRocmVhZF9wdHIsYXR0cixzdGFydFJvdXRpbmUsYXJnKX1pZihlcnJvcilyZXR1cm4gZXJyb3I7dmFyIHRocmVhZFBhcmFtcz17c3RhcnRSb3V0aW5lLHB0aHJlYWRfcHRyLGFyZyx0cmFuc2Zlckxpc3R9O2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe3RocmVhZFBhcmFtcy5jbWQ9InNwYXduVGhyZWFkIjtwb3N0TWVzc2FnZSh0aHJlYWRQYXJhbXMsdHJhbnNmZXJMaXN0KTtyZXR1cm4gMH1yZXR1cm4gc3Bhd25UaHJlYWQodGhyZWFkUGFyYW1zKX07dmFyIHN5c2NhbGxHZXRWYXJhcmdJPSgpPT57YXNzZXJ0KFNZU0NBTExTLnZhcmFyZ3MhPXVuZGVmaW5lZCk7dmFyIHJldD0oZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVsrU1lTQ0FMTFMudmFyYXJncz4+Ml07U1lTQ0FMTFMudmFyYXJncys9NDtyZXR1cm4gcmV0fTt2YXIgc3lzY2FsbEdldFZhcmFyZ1A9c3lzY2FsbEdldFZhcmFyZ0k7dmFyIFBBVEg9e2lzQWJzOnBhdGg9PnBhdGguY2hhckF0KDApPT09Ii8iLHNwbGl0UGF0aDpmaWxlbmFtZT0+e3ZhciBzcGxpdFBhdGhSZT0vXihcLz98KShbXHNcU10qPykoKD86XC57MSwyfXxbXlwvXSs/fCkoXC5bXi5cL10qfCkpKD86W1wvXSopJC87cmV0dXJuIHNwbGl0UGF0aFJlLmV4ZWMoZmlsZW5hbWUpLnNsaWNlKDEpfSxub3JtYWxpemVBcnJheToocGFydHMsYWxsb3dBYm92ZVJvb3QpPT57dmFyIHVwPTA7Zm9yKHZhciBpPXBhcnRzLmxlbmd0aC0xO2k+PTA7aS0tKXt2YXIgbGFzdD1wYXJ0c1tpXTtpZihsYXN0PT09Ii4iKXtwYXJ0cy5zcGxpY2UoaSwxKX1lbHNlIGlmKGxhc3Q9PT0iLi4iKXtwYXJ0cy5zcGxpY2UoaSwxKTt1cCsrfWVsc2UgaWYodXApe3BhcnRzLnNwbGljZShpLDEpO3VwLS19fWlmKGFsbG93QWJvdmVSb290KXtmb3IoO3VwO3VwLS0pe3BhcnRzLnVuc2hpZnQoIi4uIil9fXJldHVybiBwYXJ0c30sbm9ybWFsaXplOnBhdGg9Pnt2YXIgaXNBYnNvbHV0ZT1QQVRILmlzQWJzKHBhdGgpLHRyYWlsaW5nU2xhc2g9cGF0aC5zbGljZSgtMSk9PT0iLyI7cGF0aD1QQVRILm5vcm1hbGl6ZUFycmF5KHBhdGguc3BsaXQoIi8iKS5maWx0ZXIocD0+ISFwKSwhaXNBYnNvbHV0ZSkuam9pbigiLyIpO2lmKCFwYXRoJiYhaXNBYnNvbHV0ZSl7cGF0aD0iLiJ9aWYocGF0aCYmdHJhaWxpbmdTbGFzaCl7cGF0aCs9Ii8ifXJldHVybihpc0Fic29sdXRlPyIvIjoiIikrcGF0aH0sZGlybmFtZTpwYXRoPT57dmFyIHJlc3VsdD1QQVRILnNwbGl0UGF0aChwYXRoKSxyb290PXJlc3VsdFswXSxkaXI9cmVzdWx0WzFdO2lmKCFyb290JiYhZGlyKXtyZXR1cm4iLiJ9aWYoZGlyKXtkaXI9ZGlyLnNsaWNlKDAsLTEpfXJldHVybiByb290K2Rpcn0sYmFzZW5hbWU6cGF0aD0+cGF0aCYmcGF0aC5tYXRjaCgvKFteXC9dK3xcLylcLyokLylbMV0sam9pbjooLi4ucGF0aHMpPT5QQVRILm5vcm1hbGl6ZShwYXRocy5qb2luKCIvIikpLGpvaW4yOihsLHIpPT5QQVRILm5vcm1hbGl6ZShsKyIvIityKX07dmFyIGluaXRSYW5kb21GaWxsPSgpPT52aWV3PT52aWV3LnNldChjcnlwdG8uZ2V0UmFuZG9tVmFsdWVzKG5ldyBVaW50OEFycmF5KHZpZXcuYnl0ZUxlbmd0aCkpKTt2YXIgcmFuZG9tRmlsbD12aWV3PT57KHJhbmRvbUZpbGw9aW5pdFJhbmRvbUZpbGwoKSkodmlldyl9O3ZhciBQQVRIX0ZTPXtyZXNvbHZlOiguLi5hcmdzKT0+e3ZhciByZXNvbHZlZFBhdGg9IiIscmVzb2x2ZWRBYnNvbHV0ZT1mYWxzZTtmb3IodmFyIGk9YXJncy5sZW5ndGgtMTtpPj0tMSYmIXJlc29sdmVkQWJzb2x1dGU7aS0tKXt2YXIgcGF0aD1pPj0wP2FyZ3NbaV06RlMuY3dkKCk7aWYodHlwZW9mIHBhdGghPSJzdHJpbmciKXt0aHJvdyBuZXcgVHlwZUVycm9yKCJBcmd1bWVudHMgdG8gcGF0aC5yZXNvbHZlIG11c3QgYmUgc3RyaW5ncyIpfWVsc2UgaWYoIXBhdGgpe3JldHVybiIifXJlc29sdmVkUGF0aD1wYXRoKyIvIityZXNvbHZlZFBhdGg7cmVzb2x2ZWRBYnNvbHV0ZT1QQVRILmlzQWJzKHBhdGgpfXJlc29sdmVkUGF0aD1QQVRILm5vcm1hbGl6ZUFycmF5KHJlc29sdmVkUGF0aC5zcGxpdCgiLyIpLmZpbHRlcihwPT4hIXApLCFyZXNvbHZlZEFic29sdXRlKS5qb2luKCIvIik7cmV0dXJuKHJlc29sdmVkQWJzb2x1dGU/Ii8iOiIiKStyZXNvbHZlZFBhdGh8fCIuIn0scmVsYXRpdmU6KGZyb20sdG8pPT57ZnJvbT1QQVRIX0ZTLnJlc29sdmUoZnJvbSkuc2xpY2UoMSk7dG89UEFUSF9GUy5yZXNvbHZlKHRvKS5zbGljZSgxKTtmdW5jdGlvbiB0cmltKGFycil7dmFyIHN0YXJ0PTA7Zm9yKDtzdGFydDxhcnIubGVuZ3RoO3N0YXJ0Kyspe2lmKGFycltzdGFydF0hPT0iIilicmVha312YXIgZW5kPWFyci5sZW5ndGgtMTtmb3IoO2VuZD49MDtlbmQtLSl7aWYoYXJyW2VuZF0hPT0iIilicmVha31pZihzdGFydD5lbmQpcmV0dXJuW107cmV0dXJuIGFyci5zbGljZShzdGFydCxlbmQtc3RhcnQrMSl9dmFyIGZyb21QYXJ0cz10cmltKGZyb20uc3BsaXQoIi8iKSk7dmFyIHRvUGFydHM9dHJpbSh0by5zcGxpdCgiLyIpKTt2YXIgbGVuZ3RoPU1hdGgubWluKGZyb21QYXJ0cy5sZW5ndGgsdG9QYXJ0cy5sZW5ndGgpO3ZhciBzYW1lUGFydHNMZW5ndGg9bGVuZ3RoO2Zvcih2YXIgaT0wO2k8bGVuZ3RoO2krKyl7aWYoZnJvbVBhcnRzW2ldIT09dG9QYXJ0c1tpXSl7c2FtZVBhcnRzTGVuZ3RoPWk7YnJlYWt9fXZhciBvdXRwdXRQYXJ0cz1bXTtmb3IodmFyIGk9c2FtZVBhcnRzTGVuZ3RoO2k8ZnJvbVBhcnRzLmxlbmd0aDtpKyspe291dHB1dFBhcnRzLnB1c2goIi4uIil9b3V0cHV0UGFydHM9b3V0cHV0UGFydHMuY29uY2F0KHRvUGFydHMuc2xpY2Uoc2FtZVBhcnRzTGVuZ3RoKSk7cmV0dXJuIG91dHB1dFBhcnRzLmpvaW4oIi8iKX19O3ZhciBGU19zdGRpbl9nZXRDaGFyX2J1ZmZlcj1bXTt2YXIgbGVuZ3RoQnl0ZXNVVEY4PXN0cj0+e3ZhciBsZW49MDtmb3IodmFyIGk9MDtpPHN0ci5sZW5ndGg7KytpKXt2YXIgYz1zdHIuY2hhckNvZGVBdChpKTtpZihjPD0xMjcpe2xlbisrfWVsc2UgaWYoYzw9MjA0Nyl7bGVuKz0yfWVsc2UgaWYoYz49NTUyOTYmJmM8PTU3MzQzKXtsZW4rPTQ7KytpfWVsc2V7bGVuKz0zfX1yZXR1cm4gbGVufTt2YXIgc3RyaW5nVG9VVEY4QXJyYXk9KHN0cixoZWFwLG91dElkeCxtYXhCeXRlc1RvV3JpdGUpPT57YXNzZXJ0KHR5cGVvZiBzdHI9PT0ic3RyaW5nIixgc3RyaW5nVG9VVEY4QXJyYXkgZXhwZWN0cyBhIHN0cmluZyAoZ290ICR7dHlwZW9mIHN0cn0pYCk7aWYoIShtYXhCeXRlc1RvV3JpdGU+MCkpcmV0dXJuIDA7dmFyIHN0YXJ0SWR4PW91dElkeDt2YXIgZW5kSWR4PW91dElkeCttYXhCeXRlc1RvV3JpdGUtMTtmb3IodmFyIGk9MDtpPHN0ci5sZW5ndGg7KytpKXt2YXIgdT1zdHIuY29kZVBvaW50QXQoaSk7aWYodTw9MTI3KXtpZihvdXRJZHg+PWVuZElkeClicmVhaztoZWFwW291dElkeCsrXT11fWVsc2UgaWYodTw9MjA0Nyl7aWYob3V0SWR4KzE+PWVuZElkeClicmVhaztoZWFwW291dElkeCsrXT0xOTJ8dT4+NjtoZWFwW291dElkeCsrXT0xMjh8dSY2M31lbHNlIGlmKHU8PTY1NTM1KXtpZihvdXRJZHgrMj49ZW5kSWR4KWJyZWFrO2hlYXBbb3V0SWR4KytdPTIyNHx1Pj4xMjtoZWFwW291dElkeCsrXT0xMjh8dT4+NiY2MztoZWFwW291dElkeCsrXT0xMjh8dSY2M31lbHNle2lmKG91dElkeCszPj1lbmRJZHgpYnJlYWs7aWYodT4xMTE0MTExKXdhcm5PbmNlKCJJbnZhbGlkIFVuaWNvZGUgY29kZSBwb2ludCAiK3B0clRvU3RyaW5nKHUpKyIgZW5jb3VudGVyZWQgd2hlbiBzZXJpYWxpemluZyBhIEpTIHN0cmluZyB0byBhIFVURi04IHN0cmluZyBpbiB3YXNtIG1lbW9yeSEgKFZhbGlkIHVuaWNvZGUgY29kZSBwb2ludHMgc2hvdWxkIGJlIGluIHJhbmdlIDAtMHgxMEZGRkYpLiIpO2hlYXBbb3V0SWR4KytdPTI0MHx1Pj4xODtoZWFwW291dElkeCsrXT0xMjh8dT4+MTImNjM7aGVhcFtvdXRJZHgrK109MTI4fHU+PjYmNjM7aGVhcFtvdXRJZHgrK109MTI4fHUmNjM7aSsrfX1oZWFwW291dElkeF09MDtyZXR1cm4gb3V0SWR4LXN0YXJ0SWR4fTt2YXIgaW50QXJyYXlGcm9tU3RyaW5nPShzdHJpbmd5LGRvbnRBZGROdWxsLGxlbmd0aCk9Pnt2YXIgbGVuPWxlbmd0aD4wP2xlbmd0aDpsZW5ndGhCeXRlc1VURjgoc3RyaW5neSkrMTt2YXIgdThhcnJheT1uZXcgQXJyYXkobGVuKTt2YXIgbnVtQnl0ZXNXcml0dGVuPXN0cmluZ1RvVVRGOEFycmF5KHN0cmluZ3ksdThhcnJheSwwLHU4YXJyYXkubGVuZ3RoKTtpZihkb250QWRkTnVsbCl1OGFycmF5Lmxlbmd0aD1udW1CeXRlc1dyaXR0ZW47cmV0dXJuIHU4YXJyYXl9O3ZhciBGU19zdGRpbl9nZXRDaGFyPSgpPT57aWYoIUZTX3N0ZGluX2dldENoYXJfYnVmZmVyLmxlbmd0aCl7dmFyIHJlc3VsdD1udWxsO2lmKGdsb2JhbFRoaXMud2luZG93Py5wcm9tcHQpe3Jlc3VsdD13aW5kb3cucHJvbXB0KCJJbnB1dDogIik7aWYocmVzdWx0IT09bnVsbCl7cmVzdWx0Kz0iXG4ifX1lbHNle31pZighcmVzdWx0KXtyZXR1cm4gbnVsbH1GU19zdGRpbl9nZXRDaGFyX2J1ZmZlcj1pbnRBcnJheUZyb21TdHJpbmcocmVzdWx0LHRydWUpfXJldHVybiBGU19zdGRpbl9nZXRDaGFyX2J1ZmZlci5zaGlmdCgpfTt2YXIgVFRZPXt0dHlzOltdLGluaXQoKXt9LHNodXRkb3duKCl7fSxyZWdpc3RlcihkZXYsb3BzKXtUVFkudHR5c1tkZXZdPXtpbnB1dDpbXSxvdXRwdXQ6W10sb3BzfTtGUy5yZWdpc3RlckRldmljZShkZXYsVFRZLnN0cmVhbV9vcHMpfSxzdHJlYW1fb3BzOntvcGVuKHN0cmVhbSl7dmFyIHR0eT1UVFkudHR5c1tzdHJlYW0ubm9kZS5yZGV2XTtpZighdHR5KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0Myl9c3RyZWFtLnR0eT10dHk7c3RyZWFtLnNlZWthYmxlPWZhbHNlfSxjbG9zZShzdHJlYW0pe3N0cmVhbS50dHkub3BzLmZzeW5jKHN0cmVhbS50dHkpfSxmc3luYyhzdHJlYW0pe3N0cmVhbS50dHkub3BzLmZzeW5jKHN0cmVhbS50dHkpfSxyZWFkKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3Mpe2lmKCFzdHJlYW0udHR5fHwhc3RyZWFtLnR0eS5vcHMuZ2V0X2NoYXIpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYwKX12YXIgYnl0ZXNSZWFkPTA7Zm9yKHZhciBpPTA7aTxsZW5ndGg7aSsrKXt2YXIgcmVzdWx0O3RyeXtyZXN1bHQ9c3RyZWFtLnR0eS5vcHMuZ2V0X2NoYXIoc3RyZWFtLnR0eSl9Y2F0Y2goZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjkpfWlmKHJlc3VsdD09PXVuZGVmaW5lZCYmYnl0ZXNSZWFkPT09MCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNil9aWYocmVzdWx0PT09bnVsbHx8cmVzdWx0PT09dW5kZWZpbmVkKWJyZWFrO2J5dGVzUmVhZCsrO2J1ZmZlcltvZmZzZXQraV09cmVzdWx0fWlmKGJ5dGVzUmVhZCl7c3RyZWFtLm5vZGUuYXRpbWU9RGF0ZS5ub3coKX1yZXR1cm4gYnl0ZXNSZWFkfSx3cml0ZShzdHJlYW0sYnVmZmVyLG9mZnNldCxsZW5ndGgscG9zKXtpZighc3RyZWFtLnR0eXx8IXN0cmVhbS50dHkub3BzLnB1dF9jaGFyKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig2MCl9dHJ5e2Zvcih2YXIgaT0wO2k8bGVuZ3RoO2krKyl7c3RyZWFtLnR0eS5vcHMucHV0X2NoYXIoc3RyZWFtLnR0eSxidWZmZXJbb2Zmc2V0K2ldKX19Y2F0Y2goZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjkpfWlmKGxlbmd0aCl7c3RyZWFtLm5vZGUubXRpbWU9c3RyZWFtLm5vZGUuY3RpbWU9RGF0ZS5ub3coKX1yZXR1cm4gaX19LGRlZmF1bHRfdHR5X29wczp7Z2V0X2NoYXIodHR5KXtyZXR1cm4gRlNfc3RkaW5fZ2V0Q2hhcigpfSxwdXRfY2hhcih0dHksdmFsKXtpZih2YWw9PT1udWxsfHx2YWw9PT0xMCl7b3V0KFVURjhBcnJheVRvU3RyaW5nKHR0eS5vdXRwdXQpKTt0dHkub3V0cHV0PVtdfWVsc2V7aWYodmFsIT0wKXR0eS5vdXRwdXQucHVzaCh2YWwpfX0sZnN5bmModHR5KXtpZih0dHkub3V0cHV0Py5sZW5ndGg+MCl7b3V0KFVURjhBcnJheVRvU3RyaW5nKHR0eS5vdXRwdXQpKTt0dHkub3V0cHV0PVtdfX0saW9jdGxfdGNnZXRzKHR0eSl7cmV0dXJue2NfaWZsYWc6MjU4NTYsY19vZmxhZzo1LGNfY2ZsYWc6MTkxLGNfbGZsYWc6MzUzODcsY19jYzpbMywyOCwxMjcsMjEsNCwwLDEsMCwxNywxOSwyNiwwLDE4LDE1LDIzLDIyLDAsMCwwLDAsMCwwLDAsMCwwLDAsMCwwLDAsMCwwLDBdfX0saW9jdGxfdGNzZXRzKHR0eSxvcHRpb25hbF9hY3Rpb25zLGRhdGEpe3JldHVybiAwfSxpb2N0bF90aW9jZ3dpbnN6KHR0eSl7cmV0dXJuWzI0LDgwXX19LGRlZmF1bHRfdHR5MV9vcHM6e3B1dF9jaGFyKHR0eSx2YWwpe2lmKHZhbD09PW51bGx8fHZhbD09PTEwKXtlcnIoVVRGOEFycmF5VG9TdHJpbmcodHR5Lm91dHB1dCkpO3R0eS5vdXRwdXQ9W119ZWxzZXtpZih2YWwhPTApdHR5Lm91dHB1dC5wdXNoKHZhbCl9fSxmc3luYyh0dHkpe2lmKHR0eS5vdXRwdXQ/Lmxlbmd0aD4wKXtlcnIoVVRGOEFycmF5VG9TdHJpbmcodHR5Lm91dHB1dCkpO3R0eS5vdXRwdXQ9W119fX19O3ZhciB6ZXJvTWVtb3J5PShwdHIsc2l6ZSk9Pihncm93TWVtVmlld3MoKSxIRUFQVTgpLmZpbGwoMCxwdHIscHRyK3NpemUpO3ZhciBhbGlnbk1lbW9yeT0oc2l6ZSxhbGlnbm1lbnQpPT57YXNzZXJ0KGFsaWdubWVudCwiYWxpZ25tZW50IGFyZ3VtZW50IGlzIHJlcXVpcmVkIik7cmV0dXJuIE1hdGguY2VpbChzaXplL2FsaWdubWVudCkqYWxpZ25tZW50fTt2YXIgbW1hcEFsbG9jPXNpemU9PntzaXplPWFsaWduTWVtb3J5KHNpemUsNjU1MzYpO3ZhciBwdHI9X2Vtc2NyaXB0ZW5fYnVpbHRpbl9tZW1hbGlnbig2NTUzNixzaXplKTtpZihwdHIpemVyb01lbW9yeShwdHIsc2l6ZSk7cmV0dXJuIHB0cn07dmFyIE1FTUZTPXtvcHNfdGFibGU6bnVsbCxtb3VudChtb3VudCl7cmV0dXJuIE1FTUZTLmNyZWF0ZU5vZGUobnVsbCwiLyIsMTY4OTUsMCl9LGNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxkZXYpe2lmKEZTLmlzQmxrZGV2KG1vZGUpfHxGUy5pc0ZJRk8obW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYzKX1NRU1GUy5vcHNfdGFibGV8fD17ZGlyOntub2RlOntnZXRhdHRyOk1FTUZTLm5vZGVfb3BzLmdldGF0dHIsc2V0YXR0cjpNRU1GUy5ub2RlX29wcy5zZXRhdHRyLGxvb2t1cDpNRU1GUy5ub2RlX29wcy5sb29rdXAsbWtub2Q6TUVNRlMubm9kZV9vcHMubWtub2QscmVuYW1lOk1FTUZTLm5vZGVfb3BzLnJlbmFtZSx1bmxpbms6TUVNRlMubm9kZV9vcHMudW5saW5rLHJtZGlyOk1FTUZTLm5vZGVfb3BzLnJtZGlyLHJlYWRkaXI6TUVNRlMubm9kZV9vcHMucmVhZGRpcixzeW1saW5rOk1FTUZTLm5vZGVfb3BzLnN5bWxpbmt9LHN0cmVhbTp7bGxzZWVrOk1FTUZTLnN0cmVhbV9vcHMubGxzZWVrfX0sZmlsZTp7bm9kZTp7Z2V0YXR0cjpNRU1GUy5ub2RlX29wcy5nZXRhdHRyLHNldGF0dHI6TUVNRlMubm9kZV9vcHMuc2V0YXR0cn0sc3RyZWFtOntsbHNlZWs6TUVNRlMuc3RyZWFtX29wcy5sbHNlZWsscmVhZDpNRU1GUy5zdHJlYW1fb3BzLnJlYWQsd3JpdGU6TUVNRlMuc3RyZWFtX29wcy53cml0ZSxtbWFwOk1FTUZTLnN0cmVhbV9vcHMubW1hcCxtc3luYzpNRU1GUy5zdHJlYW1fb3BzLm1zeW5jfX0sbGluazp7bm9kZTp7Z2V0YXR0cjpNRU1GUy5ub2RlX29wcy5nZXRhdHRyLHNldGF0dHI6TUVNRlMubm9kZV9vcHMuc2V0YXR0cixyZWFkbGluazpNRU1GUy5ub2RlX29wcy5yZWFkbGlua30sc3RyZWFtOnt9fSxjaHJkZXY6e25vZGU6e2dldGF0dHI6TUVNRlMubm9kZV9vcHMuZ2V0YXR0cixzZXRhdHRyOk1FTUZTLm5vZGVfb3BzLnNldGF0dHJ9LHN0cmVhbTpGUy5jaHJkZXZfc3RyZWFtX29wc319O3ZhciBub2RlPUZTLmNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxkZXYpO2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe25vZGUubm9kZV9vcHM9TUVNRlMub3BzX3RhYmxlLmRpci5ub2RlO25vZGUuc3RyZWFtX29wcz1NRU1GUy5vcHNfdGFibGUuZGlyLnN0cmVhbTtub2RlLmNvbnRlbnRzPXt9fWVsc2UgaWYoRlMuaXNGaWxlKG5vZGUubW9kZSkpe25vZGUubm9kZV9vcHM9TUVNRlMub3BzX3RhYmxlLmZpbGUubm9kZTtub2RlLnN0cmVhbV9vcHM9TUVNRlMub3BzX3RhYmxlLmZpbGUuc3RyZWFtO25vZGUudXNlZEJ5dGVzPTA7bm9kZS5jb250ZW50cz1udWxsfWVsc2UgaWYoRlMuaXNMaW5rKG5vZGUubW9kZSkpe25vZGUubm9kZV9vcHM9TUVNRlMub3BzX3RhYmxlLmxpbmsubm9kZTtub2RlLnN0cmVhbV9vcHM9TUVNRlMub3BzX3RhYmxlLmxpbmsuc3RyZWFtfWVsc2UgaWYoRlMuaXNDaHJkZXYobm9kZS5tb2RlKSl7bm9kZS5ub2RlX29wcz1NRU1GUy5vcHNfdGFibGUuY2hyZGV2Lm5vZGU7bm9kZS5zdHJlYW1fb3BzPU1FTUZTLm9wc190YWJsZS5jaHJkZXYuc3RyZWFtfW5vZGUuYXRpbWU9bm9kZS5tdGltZT1ub2RlLmN0aW1lPURhdGUubm93KCk7aWYocGFyZW50KXtwYXJlbnQuY29udGVudHNbbmFtZV09bm9kZTtwYXJlbnQuYXRpbWU9cGFyZW50Lm10aW1lPXBhcmVudC5jdGltZT1ub2RlLmF0aW1lfXJldHVybiBub2RlfSxnZXRGaWxlRGF0YUFzVHlwZWRBcnJheShub2RlKXtpZighbm9kZS5jb250ZW50cylyZXR1cm4gbmV3IFVpbnQ4QXJyYXkoMCk7aWYobm9kZS5jb250ZW50cy5zdWJhcnJheSlyZXR1cm4gbm9kZS5jb250ZW50cy5zdWJhcnJheSgwLG5vZGUudXNlZEJ5dGVzKTtyZXR1cm4gbmV3IFVpbnQ4QXJyYXkobm9kZS5jb250ZW50cyl9LGV4cGFuZEZpbGVTdG9yYWdlKG5vZGUsbmV3Q2FwYWNpdHkpe3ZhciBwcmV2Q2FwYWNpdHk9bm9kZS5jb250ZW50cz9ub2RlLmNvbnRlbnRzLmxlbmd0aDowO2lmKHByZXZDYXBhY2l0eT49bmV3Q2FwYWNpdHkpcmV0dXJuO3ZhciBDQVBBQ0lUWV9ET1VCTElOR19NQVg9MTAyNCoxMDI0O25ld0NhcGFjaXR5PU1hdGgubWF4KG5ld0NhcGFjaXR5LHByZXZDYXBhY2l0eSoocHJldkNhcGFjaXR5PENBUEFDSVRZX0RPVUJMSU5HX01BWD8yOjEuMTI1KT4+PjApO2lmKHByZXZDYXBhY2l0eSE9MCluZXdDYXBhY2l0eT1NYXRoLm1heChuZXdDYXBhY2l0eSwyNTYpO3ZhciBvbGRDb250ZW50cz1ub2RlLmNvbnRlbnRzO25vZGUuY29udGVudHM9bmV3IFVpbnQ4QXJyYXkobmV3Q2FwYWNpdHkpO2lmKG5vZGUudXNlZEJ5dGVzPjApbm9kZS5jb250ZW50cy5zZXQob2xkQ29udGVudHMuc3ViYXJyYXkoMCxub2RlLnVzZWRCeXRlcyksMCl9LHJlc2l6ZUZpbGVTdG9yYWdlKG5vZGUsbmV3U2l6ZSl7aWYobm9kZS51c2VkQnl0ZXM9PW5ld1NpemUpcmV0dXJuO2lmKG5ld1NpemU9PTApe25vZGUuY29udGVudHM9bnVsbDtub2RlLnVzZWRCeXRlcz0wfWVsc2V7dmFyIG9sZENvbnRlbnRzPW5vZGUuY29udGVudHM7bm9kZS5jb250ZW50cz1uZXcgVWludDhBcnJheShuZXdTaXplKTtpZihvbGRDb250ZW50cyl7bm9kZS5jb250ZW50cy5zZXQob2xkQ29udGVudHMuc3ViYXJyYXkoMCxNYXRoLm1pbihuZXdTaXplLG5vZGUudXNlZEJ5dGVzKSkpfW5vZGUudXNlZEJ5dGVzPW5ld1NpemV9fSxub2RlX29wczp7Z2V0YXR0cihub2RlKXt2YXIgYXR0cj17fTthdHRyLmRldj1GUy5pc0NocmRldihub2RlLm1vZGUpP25vZGUuaWQ6MTthdHRyLmlubz1ub2RlLmlkO2F0dHIubW9kZT1ub2RlLm1vZGU7YXR0ci5ubGluaz0xO2F0dHIudWlkPTA7YXR0ci5naWQ9MDthdHRyLnJkZXY9bm9kZS5yZGV2O2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe2F0dHIuc2l6ZT00MDk2fWVsc2UgaWYoRlMuaXNGaWxlKG5vZGUubW9kZSkpe2F0dHIuc2l6ZT1ub2RlLnVzZWRCeXRlc31lbHNlIGlmKEZTLmlzTGluayhub2RlLm1vZGUpKXthdHRyLnNpemU9bm9kZS5saW5rLmxlbmd0aH1lbHNle2F0dHIuc2l6ZT0wfWF0dHIuYXRpbWU9bmV3IERhdGUobm9kZS5hdGltZSk7YXR0ci5tdGltZT1uZXcgRGF0ZShub2RlLm10aW1lKTthdHRyLmN0aW1lPW5ldyBEYXRlKG5vZGUuY3RpbWUpO2F0dHIuYmxrc2l6ZT00MDk2O2F0dHIuYmxvY2tzPU1hdGguY2VpbChhdHRyLnNpemUvYXR0ci5ibGtzaXplKTtyZXR1cm4gYXR0cn0sc2V0YXR0cihub2RlLGF0dHIpe2Zvcihjb25zdCBrZXkgb2ZbIm1vZGUiLCJhdGltZSIsIm10aW1lIiwiY3RpbWUiXSl7aWYoYXR0cltrZXldIT1udWxsKXtub2RlW2tleV09YXR0cltrZXldfX1pZihhdHRyLnNpemUhPT11bmRlZmluZWQpe01FTUZTLnJlc2l6ZUZpbGVTdG9yYWdlKG5vZGUsYXR0ci5zaXplKX19LGxvb2t1cChwYXJlbnQsbmFtZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDQpfSxta25vZChwYXJlbnQsbmFtZSxtb2RlLGRldil7cmV0dXJuIE1FTUZTLmNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxkZXYpfSxyZW5hbWUob2xkX25vZGUsbmV3X2RpcixuZXdfbmFtZSl7dmFyIG5ld19ub2RlO3RyeXtuZXdfbm9kZT1GUy5sb29rdXBOb2RlKG5ld19kaXIsbmV3X25hbWUpfWNhdGNoKGUpe31pZihuZXdfbm9kZSl7aWYoRlMuaXNEaXIob2xkX25vZGUubW9kZSkpe2Zvcih2YXIgaSBpbiBuZXdfbm9kZS5jb250ZW50cyl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTUpfX1GUy5oYXNoUmVtb3ZlTm9kZShuZXdfbm9kZSl9ZGVsZXRlIG9sZF9ub2RlLnBhcmVudC5jb250ZW50c1tvbGRfbm9kZS5uYW1lXTtuZXdfZGlyLmNvbnRlbnRzW25ld19uYW1lXT1vbGRfbm9kZTtvbGRfbm9kZS5uYW1lPW5ld19uYW1lO25ld19kaXIuY3RpbWU9bmV3X2Rpci5tdGltZT1vbGRfbm9kZS5wYXJlbnQuY3RpbWU9b2xkX25vZGUucGFyZW50Lm10aW1lPURhdGUubm93KCl9LHVubGluayhwYXJlbnQsbmFtZSl7ZGVsZXRlIHBhcmVudC5jb250ZW50c1tuYW1lXTtwYXJlbnQuY3RpbWU9cGFyZW50Lm10aW1lPURhdGUubm93KCl9LHJtZGlyKHBhcmVudCxuYW1lKXt2YXIgbm9kZT1GUy5sb29rdXBOb2RlKHBhcmVudCxuYW1lKTtmb3IodmFyIGkgaW4gbm9kZS5jb250ZW50cyl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTUpfWRlbGV0ZSBwYXJlbnQuY29udGVudHNbbmFtZV07cGFyZW50LmN0aW1lPXBhcmVudC5tdGltZT1EYXRlLm5vdygpfSxyZWFkZGlyKG5vZGUpe3JldHVyblsiLiIsIi4uIiwuLi5PYmplY3Qua2V5cyhub2RlLmNvbnRlbnRzKV19LHN5bWxpbmsocGFyZW50LG5ld25hbWUsb2xkcGF0aCl7dmFyIG5vZGU9TUVNRlMuY3JlYXRlTm9kZShwYXJlbnQsbmV3bmFtZSw1MTF8NDA5NjAsMCk7bm9kZS5saW5rPW9sZHBhdGg7cmV0dXJuIG5vZGV9LHJlYWRsaW5rKG5vZGUpe2lmKCFGUy5pc0xpbmsobm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXJldHVybiBub2RlLmxpbmt9fSxzdHJlYW1fb3BzOntyZWFkKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbil7dmFyIGNvbnRlbnRzPXN0cmVhbS5ub2RlLmNvbnRlbnRzO2lmKHBvc2l0aW9uPj1zdHJlYW0ubm9kZS51c2VkQnl0ZXMpcmV0dXJuIDA7dmFyIHNpemU9TWF0aC5taW4oc3RyZWFtLm5vZGUudXNlZEJ5dGVzLXBvc2l0aW9uLGxlbmd0aCk7YXNzZXJ0KHNpemU+PTApO2lmKHNpemU+OCYmY29udGVudHMuc3ViYXJyYXkpe2J1ZmZlci5zZXQoY29udGVudHMuc3ViYXJyYXkocG9zaXRpb24scG9zaXRpb24rc2l6ZSksb2Zmc2V0KX1lbHNle2Zvcih2YXIgaT0wO2k8c2l6ZTtpKyspYnVmZmVyW29mZnNldCtpXT1jb250ZW50c1twb3NpdGlvbitpXX1yZXR1cm4gc2l6ZX0sd3JpdGUoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uLGNhbk93bil7YXNzZXJ0KCEoYnVmZmVyIGluc3RhbmNlb2YgQXJyYXlCdWZmZXIpKTtpZihidWZmZXIuYnVmZmVyPT09KGdyb3dNZW1WaWV3cygpLEhFQVA4KS5idWZmZXIpe2Nhbk93bj1mYWxzZX1pZighbGVuZ3RoKXJldHVybiAwO3ZhciBub2RlPXN0cmVhbS5ub2RlO25vZGUubXRpbWU9bm9kZS5jdGltZT1EYXRlLm5vdygpO2lmKGJ1ZmZlci5zdWJhcnJheSYmKCFub2RlLmNvbnRlbnRzfHxub2RlLmNvbnRlbnRzLnN1YmFycmF5KSl7aWYoY2FuT3duKXthc3NlcnQocG9zaXRpb249PT0wLCJjYW5Pd24gbXVzdCBpbXBseSBubyB3ZWlyZCBwb3NpdGlvbiBpbnNpZGUgdGhlIGZpbGUiKTtub2RlLmNvbnRlbnRzPWJ1ZmZlci5zdWJhcnJheShvZmZzZXQsb2Zmc2V0K2xlbmd0aCk7bm9kZS51c2VkQnl0ZXM9bGVuZ3RoO3JldHVybiBsZW5ndGh9ZWxzZSBpZihub2RlLnVzZWRCeXRlcz09PTAmJnBvc2l0aW9uPT09MCl7bm9kZS5jb250ZW50cz1idWZmZXIuc2xpY2Uob2Zmc2V0LG9mZnNldCtsZW5ndGgpO25vZGUudXNlZEJ5dGVzPWxlbmd0aDtyZXR1cm4gbGVuZ3RofWVsc2UgaWYocG9zaXRpb24rbGVuZ3RoPD1ub2RlLnVzZWRCeXRlcyl7bm9kZS5jb250ZW50cy5zZXQoYnVmZmVyLnN1YmFycmF5KG9mZnNldCxvZmZzZXQrbGVuZ3RoKSxwb3NpdGlvbik7cmV0dXJuIGxlbmd0aH19TUVNRlMuZXhwYW5kRmlsZVN0b3JhZ2Uobm9kZSxwb3NpdGlvbitsZW5ndGgpO2lmKG5vZGUuY29udGVudHMuc3ViYXJyYXkmJmJ1ZmZlci5zdWJhcnJheSl7bm9kZS5jb250ZW50cy5zZXQoYnVmZmVyLnN1YmFycmF5KG9mZnNldCxvZmZzZXQrbGVuZ3RoKSxwb3NpdGlvbil9ZWxzZXtmb3IodmFyIGk9MDtpPGxlbmd0aDtpKyspe25vZGUuY29udGVudHNbcG9zaXRpb24raV09YnVmZmVyW29mZnNldCtpXX19bm9kZS51c2VkQnl0ZXM9TWF0aC5tYXgobm9kZS51c2VkQnl0ZXMscG9zaXRpb24rbGVuZ3RoKTtyZXR1cm4gbGVuZ3RofSxsbHNlZWsoc3RyZWFtLG9mZnNldCx3aGVuY2Upe3ZhciBwb3NpdGlvbj1vZmZzZXQ7aWYod2hlbmNlPT09MSl7cG9zaXRpb24rPXN0cmVhbS5wb3NpdGlvbn1lbHNlIGlmKHdoZW5jZT09PTIpe2lmKEZTLmlzRmlsZShzdHJlYW0ubm9kZS5tb2RlKSl7cG9zaXRpb24rPXN0cmVhbS5ub2RlLnVzZWRCeXRlc319aWYocG9zaXRpb248MCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXJldHVybiBwb3NpdGlvbn0sbW1hcChzdHJlYW0sbGVuZ3RoLHBvc2l0aW9uLHByb3QsZmxhZ3Mpe2lmKCFGUy5pc0ZpbGUoc3RyZWFtLm5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQzKX12YXIgcHRyO3ZhciBhbGxvY2F0ZWQ7dmFyIGNvbnRlbnRzPXN0cmVhbS5ub2RlLmNvbnRlbnRzO2lmKCEoZmxhZ3MmMikmJmNvbnRlbnRzJiZjb250ZW50cy5idWZmZXI9PT0oZ3Jvd01lbVZpZXdzKCksSEVBUDgpLmJ1ZmZlcil7YWxsb2NhdGVkPWZhbHNlO3B0cj1jb250ZW50cy5ieXRlT2Zmc2V0fWVsc2V7YWxsb2NhdGVkPXRydWU7cHRyPW1tYXBBbGxvYyhsZW5ndGgpO2lmKCFwdHIpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ4KX1pZihjb250ZW50cyl7aWYocG9zaXRpb24+MHx8cG9zaXRpb24rbGVuZ3RoPGNvbnRlbnRzLmxlbmd0aCl7aWYoY29udGVudHMuc3ViYXJyYXkpe2NvbnRlbnRzPWNvbnRlbnRzLnN1YmFycmF5KHBvc2l0aW9uLHBvc2l0aW9uK2xlbmd0aCl9ZWxzZXtjb250ZW50cz1BcnJheS5wcm90b3R5cGUuc2xpY2UuY2FsbChjb250ZW50cyxwb3NpdGlvbixwb3NpdGlvbitsZW5ndGgpfX0oZ3Jvd01lbVZpZXdzKCksSEVBUDgpLnNldChjb250ZW50cyxwdHIpfX1yZXR1cm57cHRyLGFsbG9jYXRlZH19LG1zeW5jKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxtbWFwRmxhZ3Mpe01FTUZTLnN0cmVhbV9vcHMud3JpdGUoc3RyZWFtLGJ1ZmZlciwwLGxlbmd0aCxvZmZzZXQsZmFsc2UpO3JldHVybiAwfX19O3ZhciBGU19tb2RlU3RyaW5nVG9GbGFncz1zdHI9Pnt2YXIgZmxhZ01vZGVzPXtyOjAsInIrIjoyLHc6NTEyfDY0fDEsIncrIjo1MTJ8NjR8MixhOjEwMjR8NjR8MSwiYSsiOjEwMjR8NjR8Mn07dmFyIGZsYWdzPWZsYWdNb2Rlc1tzdHJdO2lmKHR5cGVvZiBmbGFncz09InVuZGVmaW5lZCIpe3Rocm93IG5ldyBFcnJvcihgVW5rbm93biBmaWxlIG9wZW4gbW9kZTogJHtzdHJ9YCl9cmV0dXJuIGZsYWdzfTt2YXIgRlNfZ2V0TW9kZT0oY2FuUmVhZCxjYW5Xcml0ZSk9Pnt2YXIgbW9kZT0wO2lmKGNhblJlYWQpbW9kZXw9MjkyfDczO2lmKGNhbldyaXRlKW1vZGV8PTE0NjtyZXR1cm4gbW9kZX07dmFyIHN0ckVycm9yPWVycm5vPT5VVEY4VG9TdHJpbmcoX3N0cmVycm9yKGVycm5vKSk7dmFyIEVSUk5PX0NPREVTPXtFUEVSTTo2MyxFTk9FTlQ6NDQsRVNSQ0g6NzEsRUlOVFI6MjcsRUlPOjI5LEVOWElPOjYwLEUyQklHOjEsRU5PRVhFQzo0NSxFQkFERjo4LEVDSElMRDoxMixFQUdBSU46NixFV09VTERCTE9DSzo2LEVOT01FTTo0OCxFQUNDRVM6MixFRkFVTFQ6MjEsRU5PVEJMSzoxMDUsRUJVU1k6MTAsRUVYSVNUOjIwLEVYREVWOjc1LEVOT0RFVjo0MyxFTk9URElSOjU0LEVJU0RJUjozMSxFSU5WQUw6MjgsRU5GSUxFOjQxLEVNRklMRTozMyxFTk9UVFk6NTksRVRYVEJTWTo3NCxFRkJJRzoyMixFTk9TUEM6NTEsRVNQSVBFOjcwLEVST0ZTOjY5LEVNTElOSzozNCxFUElQRTo2NCxFRE9NOjE4LEVSQU5HRTo2OCxFTk9NU0c6NDksRUlEUk06MjQsRUNIUk5HOjEwNixFTDJOU1lOQzoxNTYsRUwzSExUOjEwNyxFTDNSU1Q6MTA4LEVMTlJORzoxMDksRVVOQVRDSDoxMTAsRU5PQ1NJOjExMSxFTDJITFQ6MTEyLEVERUFETEs6MTYsRU5PTENLOjQ2LEVCQURFOjExMyxFQkFEUjoxMTQsRVhGVUxMOjExNSxFTk9BTk86MTA0LEVCQURSUUM6MTAzLEVCQURTTFQ6MTAyLEVERUFETE9DSzoxNixFQkZPTlQ6MTAxLEVOT1NUUjoxMDAsRU5PREFUQToxMTYsRVRJTUU6MTE3LEVOT1NSOjExOCxFTk9ORVQ6MTE5LEVOT1BLRzoxMjAsRVJFTU9URToxMjEsRU5PTElOSzo0NyxFQURWOjEyMixFU1JNTlQ6MTIzLEVDT01NOjEyNCxFUFJPVE86NjUsRU1VTFRJSE9QOjM2LEVET1RET1Q6MTI1LEVCQURNU0c6OSxFTk9UVU5JUToxMjYsRUJBREZEOjEyNyxFUkVNQ0hHOjEyOCxFTElCQUNDOjEyOSxFTElCQkFEOjEzMCxFTElCU0NOOjEzMSxFTElCTUFYOjEzMixFTElCRVhFQzoxMzMsRU5PU1lTOjUyLEVOT1RFTVBUWTo1NSxFTkFNRVRPT0xPTkc6MzcsRUxPT1A6MzIsRU9QTk9UU1VQUDoxMzgsRVBGTk9TVVBQT1JUOjEzOSxFQ09OTlJFU0VUOjE1LEVOT0JVRlM6NDIsRUFGTk9TVVBQT1JUOjUsRVBST1RPVFlQRTo2NyxFTk9UU09DSzo1NyxFTk9QUk9UT09QVDo1MCxFU0hVVERPV046MTQwLEVDT05OUkVGVVNFRDoxNCxFQUREUklOVVNFOjMsRUNPTk5BQk9SVEVEOjEzLEVORVRVTlJFQUNIOjQwLEVORVRET1dOOjM4LEVUSU1FRE9VVDo3MyxFSE9TVERPV046MTQyLEVIT1NUVU5SRUFDSDoyMyxFSU5QUk9HUkVTUzoyNixFQUxSRUFEWTo3LEVERVNUQUREUlJFUToxNyxFTVNHU0laRTozNSxFUFJPVE9OT1NVUFBPUlQ6NjYsRVNPQ0tUTk9TVVBQT1JUOjEzNyxFQUREUk5PVEFWQUlMOjQsRU5FVFJFU0VUOjM5LEVJU0NPTk46MzAsRU5PVENPTk46NTMsRVRPT01BTllSRUZTOjE0MSxFVVNFUlM6MTM2LEVEUVVPVDoxOSxFU1RBTEU6NzIsRU5PVFNVUDoxMzgsRU5PTUVESVVNOjE0OCxFSUxTRVE6MjUsRU9WRVJGTE9XOjYxLEVDQU5DRUxFRDoxMSxFTk9UUkVDT1ZFUkFCTEU6NTYsRU9XTkVSREVBRDo2MixFU1RSUElQRToxMzV9O3ZhciBhc3luY0xvYWQ9YXN5bmMgdXJsPT57dmFyIGFycmF5QnVmZmVyPWF3YWl0IHJlYWRBc3luYyh1cmwpO2Fzc2VydChhcnJheUJ1ZmZlcixgTG9hZGluZyBkYXRhIGZpbGUgIiR7dXJsfSIgZmFpbGVkIChubyBhcnJheUJ1ZmZlcikuYCk7cmV0dXJuIG5ldyBVaW50OEFycmF5KGFycmF5QnVmZmVyKX07dmFyIEZTX2NyZWF0ZURhdGFGaWxlPSguLi5hcmdzKT0+RlMuY3JlYXRlRGF0YUZpbGUoLi4uYXJncyk7dmFyIGdldFVuaXF1ZVJ1bkRlcGVuZGVuY3k9aWQ9Pnt2YXIgb3JpZz1pZDt3aGlsZSgxKXtpZighcnVuRGVwZW5kZW5jeVRyYWNraW5nW2lkXSlyZXR1cm4gaWQ7aWQ9b3JpZytNYXRoLnJhbmRvbSgpfX07dmFyIHByZWxvYWRQbHVnaW5zPVtdO3ZhciBGU19oYW5kbGVkQnlQcmVsb2FkUGx1Z2luPWFzeW5jKGJ5dGVBcnJheSxmdWxsbmFtZSk9PntpZih0eXBlb2YgQnJvd3NlciE9InVuZGVmaW5lZCIpQnJvd3Nlci5pbml0KCk7Zm9yKHZhciBwbHVnaW4gb2YgcHJlbG9hZFBsdWdpbnMpe2lmKHBsdWdpblsiY2FuSGFuZGxlIl0oZnVsbG5hbWUpKXthc3NlcnQocGx1Z2luWyJoYW5kbGUiXS5jb25zdHJ1Y3Rvci5uYW1lPT09IkFzeW5jRnVuY3Rpb24iLCJGaWxlc3lzdGVtIHBsdWdpbiBoYW5kbGVycyBtdXN0IGJlIGFzeW5jIGZ1bmN0aW9ucyAoU2VlICMyNDkxNCkiKTtyZXR1cm4gcGx1Z2luWyJoYW5kbGUiXShieXRlQXJyYXksZnVsbG5hbWUpfX1yZXR1cm4gYnl0ZUFycmF5fTt2YXIgRlNfcHJlbG9hZEZpbGU9YXN5bmMocGFyZW50LG5hbWUsdXJsLGNhblJlYWQsY2FuV3JpdGUsZG9udENyZWF0ZUZpbGUsY2FuT3duLHByZUZpbmlzaCk9Pnt2YXIgZnVsbG5hbWU9bmFtZT9QQVRIX0ZTLnJlc29sdmUoUEFUSC5qb2luMihwYXJlbnQsbmFtZSkpOnBhcmVudDt2YXIgZGVwPWdldFVuaXF1ZVJ1bkRlcGVuZGVuY3koYGNwICR7ZnVsbG5hbWV9YCk7YWRkUnVuRGVwZW5kZW5jeShkZXApO3RyeXt2YXIgYnl0ZUFycmF5PXVybDtpZih0eXBlb2YgdXJsPT0ic3RyaW5nIil7Ynl0ZUFycmF5PWF3YWl0IGFzeW5jTG9hZCh1cmwpfWJ5dGVBcnJheT1hd2FpdCBGU19oYW5kbGVkQnlQcmVsb2FkUGx1Z2luKGJ5dGVBcnJheSxmdWxsbmFtZSk7cHJlRmluaXNoPy4oKTtpZighZG9udENyZWF0ZUZpbGUpe0ZTX2NyZWF0ZURhdGFGaWxlKHBhcmVudCxuYW1lLGJ5dGVBcnJheSxjYW5SZWFkLGNhbldyaXRlLGNhbk93bil9fWZpbmFsbHl7cmVtb3ZlUnVuRGVwZW5kZW5jeShkZXApfX07dmFyIEZTX2NyZWF0ZVByZWxvYWRlZEZpbGU9KHBhcmVudCxuYW1lLHVybCxjYW5SZWFkLGNhbldyaXRlLG9ubG9hZCxvbmVycm9yLGRvbnRDcmVhdGVGaWxlLGNhbk93bixwcmVGaW5pc2gpPT57RlNfcHJlbG9hZEZpbGUocGFyZW50LG5hbWUsdXJsLGNhblJlYWQsY2FuV3JpdGUsZG9udENyZWF0ZUZpbGUsY2FuT3duLHByZUZpbmlzaCkudGhlbihvbmxvYWQpLmNhdGNoKG9uZXJyb3IpfTt2YXIgRlM9e3Jvb3Q6bnVsbCxtb3VudHM6W10sZGV2aWNlczp7fSxzdHJlYW1zOltdLG5leHRJbm9kZToxLG5hbWVUYWJsZTpudWxsLGN1cnJlbnRQYXRoOiIvIixpbml0aWFsaXplZDpmYWxzZSxpZ25vcmVQZXJtaXNzaW9uczp0cnVlLGZpbGVzeXN0ZW1zOm51bGwsc3luY0ZTUmVxdWVzdHM6MCxyZWFkRmlsZXM6e30sRXJybm9FcnJvcjpjbGFzcyBleHRlbmRzIEVycm9ye25hbWU9IkVycm5vRXJyb3IiO2NvbnN0cnVjdG9yKGVycm5vKXtzdXBlcihydW50aW1lSW5pdGlhbGl6ZWQ/c3RyRXJyb3IoZXJybm8pOiIiKTt0aGlzLmVycm5vPWVycm5vO2Zvcih2YXIga2V5IGluIEVSUk5PX0NPREVTKXtpZihFUlJOT19DT0RFU1trZXldPT09ZXJybm8pe3RoaXMuY29kZT1rZXk7YnJlYWt9fX19LEZTU3RyZWFtOmNsYXNze3NoYXJlZD17fTtnZXQgb2JqZWN0KCl7cmV0dXJuIHRoaXMubm9kZX1zZXQgb2JqZWN0KHZhbCl7dGhpcy5ub2RlPXZhbH1nZXQgaXNSZWFkKCl7cmV0dXJuKHRoaXMuZmxhZ3MmMjA5NzE1NSkhPT0xfWdldCBpc1dyaXRlKCl7cmV0dXJuKHRoaXMuZmxhZ3MmMjA5NzE1NSkhPT0wfWdldCBpc0FwcGVuZCgpe3JldHVybiB0aGlzLmZsYWdzJjEwMjR9Z2V0IGZsYWdzKCl7cmV0dXJuIHRoaXMuc2hhcmVkLmZsYWdzfXNldCBmbGFncyh2YWwpe3RoaXMuc2hhcmVkLmZsYWdzPXZhbH1nZXQgcG9zaXRpb24oKXtyZXR1cm4gdGhpcy5zaGFyZWQucG9zaXRpb259c2V0IHBvc2l0aW9uKHZhbCl7dGhpcy5zaGFyZWQucG9zaXRpb249dmFsfX0sRlNOb2RlOmNsYXNze25vZGVfb3BzPXt9O3N0cmVhbV9vcHM9e307cmVhZE1vZGU9MjkyfDczO3dyaXRlTW9kZT0xNDY7bW91bnRlZD1udWxsO2NvbnN0cnVjdG9yKHBhcmVudCxuYW1lLG1vZGUscmRldil7aWYoIXBhcmVudCl7cGFyZW50PXRoaXN9dGhpcy5wYXJlbnQ9cGFyZW50O3RoaXMubW91bnQ9cGFyZW50Lm1vdW50O3RoaXMuaWQ9RlMubmV4dElub2RlKys7dGhpcy5uYW1lPW5hbWU7dGhpcy5tb2RlPW1vZGU7dGhpcy5yZGV2PXJkZXY7dGhpcy5hdGltZT10aGlzLm10aW1lPXRoaXMuY3RpbWU9RGF0ZS5ub3coKX1nZXQgcmVhZCgpe3JldHVybih0aGlzLm1vZGUmdGhpcy5yZWFkTW9kZSk9PT10aGlzLnJlYWRNb2RlfXNldCByZWFkKHZhbCl7dmFsP3RoaXMubW9kZXw9dGhpcy5yZWFkTW9kZTp0aGlzLm1vZGUmPX50aGlzLnJlYWRNb2RlfWdldCB3cml0ZSgpe3JldHVybih0aGlzLm1vZGUmdGhpcy53cml0ZU1vZGUpPT09dGhpcy53cml0ZU1vZGV9c2V0IHdyaXRlKHZhbCl7dmFsP3RoaXMubW9kZXw9dGhpcy53cml0ZU1vZGU6dGhpcy5tb2RlJj1+dGhpcy53cml0ZU1vZGV9Z2V0IGlzRm9sZGVyKCl7cmV0dXJuIEZTLmlzRGlyKHRoaXMubW9kZSl9Z2V0IGlzRGV2aWNlKCl7cmV0dXJuIEZTLmlzQ2hyZGV2KHRoaXMubW9kZSl9fSxsb29rdXBQYXRoKHBhdGgsb3B0cz17fSl7aWYoIXBhdGgpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ0KX1vcHRzLmZvbGxvd19tb3VudD8/PXRydWU7aWYoIVBBVEguaXNBYnMocGF0aCkpe3BhdGg9RlMuY3dkKCkrIi8iK3BhdGh9bGlua2xvb3A6Zm9yKHZhciBubGlua3M9MDtubGlua3M8NDA7bmxpbmtzKyspe3ZhciBwYXJ0cz1wYXRoLnNwbGl0KCIvIikuZmlsdGVyKHA9PiEhcCk7dmFyIGN1cnJlbnQ9RlMucm9vdDt2YXIgY3VycmVudF9wYXRoPSIvIjtmb3IodmFyIGk9MDtpPHBhcnRzLmxlbmd0aDtpKyspe3ZhciBpc2xhc3Q9aT09PXBhcnRzLmxlbmd0aC0xO2lmKGlzbGFzdCYmb3B0cy5wYXJlbnQpe2JyZWFrfWlmKHBhcnRzW2ldPT09Ii4iKXtjb250aW51ZX1pZihwYXJ0c1tpXT09PSIuLiIpe2N1cnJlbnRfcGF0aD1QQVRILmRpcm5hbWUoY3VycmVudF9wYXRoKTtpZihGUy5pc1Jvb3QoY3VycmVudCkpe3BhdGg9Y3VycmVudF9wYXRoKyIvIitwYXJ0cy5zbGljZShpKzEpLmpvaW4oIi8iKTtubGlua3MtLTtjb250aW51ZSBsaW5rbG9vcH1lbHNle2N1cnJlbnQ9Y3VycmVudC5wYXJlbnR9Y29udGludWV9Y3VycmVudF9wYXRoPVBBVEguam9pbjIoY3VycmVudF9wYXRoLHBhcnRzW2ldKTt0cnl7Y3VycmVudD1GUy5sb29rdXBOb2RlKGN1cnJlbnQscGFydHNbaV0pfWNhdGNoKGUpe2lmKGU/LmVycm5vPT09NDQmJmlzbGFzdCYmb3B0cy5ub2VudF9va2F5KXtyZXR1cm57cGF0aDpjdXJyZW50X3BhdGh9fXRocm93IGV9aWYoRlMuaXNNb3VudHBvaW50KGN1cnJlbnQpJiYoIWlzbGFzdHx8b3B0cy5mb2xsb3dfbW91bnQpKXtjdXJyZW50PWN1cnJlbnQubW91bnRlZC5yb290fWlmKEZTLmlzTGluayhjdXJyZW50Lm1vZGUpJiYoIWlzbGFzdHx8b3B0cy5mb2xsb3cpKXtpZighY3VycmVudC5ub2RlX29wcy5yZWFkbGluayl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTIpfXZhciBsaW5rPWN1cnJlbnQubm9kZV9vcHMucmVhZGxpbmsoY3VycmVudCk7aWYoIVBBVEguaXNBYnMobGluaykpe2xpbms9UEFUSC5kaXJuYW1lKGN1cnJlbnRfcGF0aCkrIi8iK2xpbmt9cGF0aD1saW5rKyIvIitwYXJ0cy5zbGljZShpKzEpLmpvaW4oIi8iKTtjb250aW51ZSBsaW5rbG9vcH19cmV0dXJue3BhdGg6Y3VycmVudF9wYXRoLG5vZGU6Y3VycmVudH19dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMzIpfSxnZXRQYXRoKG5vZGUpe3ZhciBwYXRoO3doaWxlKHRydWUpe2lmKEZTLmlzUm9vdChub2RlKSl7dmFyIG1vdW50PW5vZGUubW91bnQubW91bnRwb2ludDtpZighcGF0aClyZXR1cm4gbW91bnQ7cmV0dXJuIG1vdW50W21vdW50Lmxlbmd0aC0xXSE9PSIvIj9gJHttb3VudH0vJHtwYXRofWA6bW91bnQrcGF0aH1wYXRoPXBhdGg/YCR7bm9kZS5uYW1lfS8ke3BhdGh9YDpub2RlLm5hbWU7bm9kZT1ub2RlLnBhcmVudH19LGhhc2hOYW1lKHBhcmVudGlkLG5hbWUpe3ZhciBoYXNoPTA7Zm9yKHZhciBpPTA7aTxuYW1lLmxlbmd0aDtpKyspe2hhc2g9KGhhc2g8PDUpLWhhc2grbmFtZS5jaGFyQ29kZUF0KGkpfDB9cmV0dXJuKHBhcmVudGlkK2hhc2g+Pj4wKSVGUy5uYW1lVGFibGUubGVuZ3RofSxoYXNoQWRkTm9kZShub2RlKXt2YXIgaGFzaD1GUy5oYXNoTmFtZShub2RlLnBhcmVudC5pZCxub2RlLm5hbWUpO25vZGUubmFtZV9uZXh0PUZTLm5hbWVUYWJsZVtoYXNoXTtGUy5uYW1lVGFibGVbaGFzaF09bm9kZX0saGFzaFJlbW92ZU5vZGUobm9kZSl7dmFyIGhhc2g9RlMuaGFzaE5hbWUobm9kZS5wYXJlbnQuaWQsbm9kZS5uYW1lKTtpZihGUy5uYW1lVGFibGVbaGFzaF09PT1ub2RlKXtGUy5uYW1lVGFibGVbaGFzaF09bm9kZS5uYW1lX25leHR9ZWxzZXt2YXIgY3VycmVudD1GUy5uYW1lVGFibGVbaGFzaF07d2hpbGUoY3VycmVudCl7aWYoY3VycmVudC5uYW1lX25leHQ9PT1ub2RlKXtjdXJyZW50Lm5hbWVfbmV4dD1ub2RlLm5hbWVfbmV4dDticmVha31jdXJyZW50PWN1cnJlbnQubmFtZV9uZXh0fX19LGxvb2t1cE5vZGUocGFyZW50LG5hbWUpe3ZhciBlcnJDb2RlPUZTLm1heUxvb2t1cChwYXJlbnQpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfXZhciBoYXNoPUZTLmhhc2hOYW1lKHBhcmVudC5pZCxuYW1lKTtmb3IodmFyIG5vZGU9RlMubmFtZVRhYmxlW2hhc2hdO25vZGU7bm9kZT1ub2RlLm5hbWVfbmV4dCl7dmFyIG5vZGVOYW1lPW5vZGUubmFtZTtpZihub2RlLnBhcmVudC5pZD09PXBhcmVudC5pZCYmbm9kZU5hbWU9PT1uYW1lKXtyZXR1cm4gbm9kZX19cmV0dXJuIEZTLmxvb2t1cChwYXJlbnQsbmFtZSl9LGNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxyZGV2KXthc3NlcnQodHlwZW9mIHBhcmVudD09Im9iamVjdCIpO3ZhciBub2RlPW5ldyBGUy5GU05vZGUocGFyZW50LG5hbWUsbW9kZSxyZGV2KTtGUy5oYXNoQWRkTm9kZShub2RlKTtyZXR1cm4gbm9kZX0sZGVzdHJveU5vZGUobm9kZSl7RlMuaGFzaFJlbW92ZU5vZGUobm9kZSl9LGlzUm9vdChub2RlKXtyZXR1cm4gbm9kZT09PW5vZGUucGFyZW50fSxpc01vdW50cG9pbnQobm9kZSl7cmV0dXJuISFub2RlLm1vdW50ZWR9LGlzRmlsZShtb2RlKXtyZXR1cm4obW9kZSY2MTQ0MCk9PT0zMjc2OH0saXNEaXIobW9kZSl7cmV0dXJuKG1vZGUmNjE0NDApPT09MTYzODR9LGlzTGluayhtb2RlKXtyZXR1cm4obW9kZSY2MTQ0MCk9PT00MDk2MH0saXNDaHJkZXYobW9kZSl7cmV0dXJuKG1vZGUmNjE0NDApPT09ODE5Mn0saXNCbGtkZXYobW9kZSl7cmV0dXJuKG1vZGUmNjE0NDApPT09MjQ1NzZ9LGlzRklGTyhtb2RlKXtyZXR1cm4obW9kZSY2MTQ0MCk9PT00MDk2fSxpc1NvY2tldChtb2RlKXtyZXR1cm4obW9kZSY0OTE1Mik9PT00OTE1Mn0sZmxhZ3NUb1Blcm1pc3Npb25TdHJpbmcoZmxhZyl7dmFyIHBlcm1zPVsiciIsInciLCJydyJdW2ZsYWcmM107aWYoZmxhZyY1MTIpe3Blcm1zKz0idyJ9cmV0dXJuIHBlcm1zfSxub2RlUGVybWlzc2lvbnMobm9kZSxwZXJtcyl7aWYoRlMuaWdub3JlUGVybWlzc2lvbnMpe3JldHVybiAwfWlmKHBlcm1zLmluY2x1ZGVzKCJyIikmJiEobm9kZS5tb2RlJjI5Mikpe3JldHVybiAyfWVsc2UgaWYocGVybXMuaW5jbHVkZXMoInciKSYmIShub2RlLm1vZGUmMTQ2KSl7cmV0dXJuIDJ9ZWxzZSBpZihwZXJtcy5pbmNsdWRlcygieCIpJiYhKG5vZGUubW9kZSY3Mykpe3JldHVybiAyfXJldHVybiAwfSxtYXlMb29rdXAoZGlyKXtpZighRlMuaXNEaXIoZGlyLm1vZGUpKXJldHVybiA1NDt2YXIgZXJyQ29kZT1GUy5ub2RlUGVybWlzc2lvbnMoZGlyLCJ4Iik7aWYoZXJyQ29kZSlyZXR1cm4gZXJyQ29kZTtpZighZGlyLm5vZGVfb3BzLmxvb2t1cClyZXR1cm4gMjtyZXR1cm4gMH0sbWF5Q3JlYXRlKGRpcixuYW1lKXtpZighRlMuaXNEaXIoZGlyLm1vZGUpKXtyZXR1cm4gNTR9dHJ5e3ZhciBub2RlPUZTLmxvb2t1cE5vZGUoZGlyLG5hbWUpO3JldHVybiAyMH1jYXRjaChlKXt9cmV0dXJuIEZTLm5vZGVQZXJtaXNzaW9ucyhkaXIsInd4Iil9LG1heURlbGV0ZShkaXIsbmFtZSxpc2Rpcil7dmFyIG5vZGU7dHJ5e25vZGU9RlMubG9va3VwTm9kZShkaXIsbmFtZSl9Y2F0Y2goZSl7cmV0dXJuIGUuZXJybm99dmFyIGVyckNvZGU9RlMubm9kZVBlcm1pc3Npb25zKGRpciwid3giKTtpZihlcnJDb2RlKXtyZXR1cm4gZXJyQ29kZX1pZihpc2Rpcil7aWYoIUZTLmlzRGlyKG5vZGUubW9kZSkpe3JldHVybiA1NH1pZihGUy5pc1Jvb3Qobm9kZSl8fEZTLmdldFBhdGgobm9kZSk9PT1GUy5jd2QoKSl7cmV0dXJuIDEwfX1lbHNle2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe3JldHVybiAzMX19cmV0dXJuIDB9LG1heU9wZW4obm9kZSxmbGFncyl7aWYoIW5vZGUpe3JldHVybiA0NH1pZihGUy5pc0xpbmsobm9kZS5tb2RlKSl7cmV0dXJuIDMyfWVsc2UgaWYoRlMuaXNEaXIobm9kZS5tb2RlKSl7aWYoRlMuZmxhZ3NUb1Blcm1pc3Npb25TdHJpbmcoZmxhZ3MpIT09InIifHxmbGFncyYoNTEyfDY0KSl7cmV0dXJuIDMxfX1yZXR1cm4gRlMubm9kZVBlcm1pc3Npb25zKG5vZGUsRlMuZmxhZ3NUb1Blcm1pc3Npb25TdHJpbmcoZmxhZ3MpKX0sY2hlY2tPcEV4aXN0cyhvcCxlcnIpe2lmKCFvcCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyKX1yZXR1cm4gb3B9LE1BWF9PUEVOX0ZEUzo0MDk2LG5leHRmZCgpe2Zvcih2YXIgZmQ9MDtmZDw9RlMuTUFYX09QRU5fRkRTO2ZkKyspe2lmKCFGUy5zdHJlYW1zW2ZkXSl7cmV0dXJuIGZkfX10aHJvdyBuZXcgRlMuRXJybm9FcnJvcigzMyl9LGdldFN0cmVhbUNoZWNrZWQoZmQpe3ZhciBzdHJlYW09RlMuZ2V0U3RyZWFtKGZkKTtpZighc3RyZWFtKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig4KX1yZXR1cm4gc3RyZWFtfSxnZXRTdHJlYW06ZmQ9PkZTLnN0cmVhbXNbZmRdLGNyZWF0ZVN0cmVhbShzdHJlYW0sZmQ9LTEpe2Fzc2VydChmZD49LTEpO3N0cmVhbT1PYmplY3QuYXNzaWduKG5ldyBGUy5GU1N0cmVhbSxzdHJlYW0pO2lmKGZkPT0tMSl7ZmQ9RlMubmV4dGZkKCl9c3RyZWFtLmZkPWZkO0ZTLnN0cmVhbXNbZmRdPXN0cmVhbTtyZXR1cm4gc3RyZWFtfSxjbG9zZVN0cmVhbShmZCl7RlMuc3RyZWFtc1tmZF09bnVsbH0sZHVwU3RyZWFtKG9yaWdTdHJlYW0sZmQ9LTEpe3ZhciBzdHJlYW09RlMuY3JlYXRlU3RyZWFtKG9yaWdTdHJlYW0sZmQpO3N0cmVhbS5zdHJlYW1fb3BzPy5kdXA/LihzdHJlYW0pO3JldHVybiBzdHJlYW19LGRvU2V0QXR0cihzdHJlYW0sbm9kZSxhdHRyKXt2YXIgc2V0YXR0cj1zdHJlYW0/LnN0cmVhbV9vcHMuc2V0YXR0cjt2YXIgYXJnPXNldGF0dHI/c3RyZWFtOm5vZGU7c2V0YXR0cj8/PW5vZGUubm9kZV9vcHMuc2V0YXR0cjtGUy5jaGVja09wRXhpc3RzKHNldGF0dHIsNjMpO3NldGF0dHIoYXJnLGF0dHIpfSxjaHJkZXZfc3RyZWFtX29wczp7b3BlbihzdHJlYW0pe3ZhciBkZXZpY2U9RlMuZ2V0RGV2aWNlKHN0cmVhbS5ub2RlLnJkZXYpO3N0cmVhbS5zdHJlYW1fb3BzPWRldmljZS5zdHJlYW1fb3BzO3N0cmVhbS5zdHJlYW1fb3BzLm9wZW4/LihzdHJlYW0pfSxsbHNlZWsoKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig3MCl9fSxtYWpvcjpkZXY9PmRldj4+OCxtaW5vcjpkZXY9PmRldiYyNTUsbWFrZWRldjoobWEsbWkpPT5tYTw8OHxtaSxyZWdpc3RlckRldmljZShkZXYsb3BzKXtGUy5kZXZpY2VzW2Rldl09e3N0cmVhbV9vcHM6b3BzfX0sZ2V0RGV2aWNlOmRldj0+RlMuZGV2aWNlc1tkZXZdLGdldE1vdW50cyhtb3VudCl7dmFyIG1vdW50cz1bXTt2YXIgY2hlY2s9W21vdW50XTt3aGlsZShjaGVjay5sZW5ndGgpe3ZhciBtPWNoZWNrLnBvcCgpO21vdW50cy5wdXNoKG0pO2NoZWNrLnB1c2goLi4ubS5tb3VudHMpfXJldHVybiBtb3VudHN9LHN5bmNmcyhwb3B1bGF0ZSxjYWxsYmFjayl7aWYodHlwZW9mIHBvcHVsYXRlPT0iZnVuY3Rpb24iKXtjYWxsYmFjaz1wb3B1bGF0ZTtwb3B1bGF0ZT1mYWxzZX1GUy5zeW5jRlNSZXF1ZXN0cysrO2lmKEZTLnN5bmNGU1JlcXVlc3RzPjEpe2Vycihgd2FybmluZzogJHtGUy5zeW5jRlNSZXF1ZXN0c30gRlMuc3luY2ZzIG9wZXJhdGlvbnMgaW4gZmxpZ2h0IGF0IG9uY2UsIHByb2JhYmx5IGp1c3QgZG9pbmcgZXh0cmEgd29ya2ApfXZhciBtb3VudHM9RlMuZ2V0TW91bnRzKEZTLnJvb3QubW91bnQpO3ZhciBjb21wbGV0ZWQ9MDtmdW5jdGlvbiBkb0NhbGxiYWNrKGVyckNvZGUpe2Fzc2VydChGUy5zeW5jRlNSZXF1ZXN0cz4wKTtGUy5zeW5jRlNSZXF1ZXN0cy0tO3JldHVybiBjYWxsYmFjayhlcnJDb2RlKX1mdW5jdGlvbiBkb25lKGVyckNvZGUpe2lmKGVyckNvZGUpe2lmKCFkb25lLmVycm9yZWQpe2RvbmUuZXJyb3JlZD10cnVlO3JldHVybiBkb0NhbGxiYWNrKGVyckNvZGUpfXJldHVybn1pZigrK2NvbXBsZXRlZD49bW91bnRzLmxlbmd0aCl7ZG9DYWxsYmFjayhudWxsKX19Zm9yKHZhciBtb3VudCBvZiBtb3VudHMpe2lmKG1vdW50LnR5cGUuc3luY2ZzKXttb3VudC50eXBlLnN5bmNmcyhtb3VudCxwb3B1bGF0ZSxkb25lKX1lbHNle2RvbmUobnVsbCl9fX0sbW91bnQodHlwZSxvcHRzLG1vdW50cG9pbnQpe2lmKHR5cGVvZiB0eXBlPT0ic3RyaW5nIil7dGhyb3cgdHlwZX12YXIgcm9vdD1tb3VudHBvaW50PT09Ii8iO3ZhciBwc2V1ZG89IW1vdW50cG9pbnQ7dmFyIG5vZGU7aWYocm9vdCYmRlMucm9vdCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMTApfWVsc2UgaWYoIXJvb3QmJiFwc2V1ZG8pe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChtb3VudHBvaW50LHtmb2xsb3dfbW91bnQ6ZmFsc2V9KTttb3VudHBvaW50PWxvb2t1cC5wYXRoO25vZGU9bG9va3VwLm5vZGU7aWYoRlMuaXNNb3VudHBvaW50KG5vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigxMCl9aWYoIUZTLmlzRGlyKG5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDU0KX19dmFyIG1vdW50PXt0eXBlLG9wdHMsbW91bnRwb2ludCxtb3VudHM6W119O3ZhciBtb3VudFJvb3Q9dHlwZS5tb3VudChtb3VudCk7bW91bnRSb290Lm1vdW50PW1vdW50O21vdW50LnJvb3Q9bW91bnRSb290O2lmKHJvb3Qpe0ZTLnJvb3Q9bW91bnRSb290fWVsc2UgaWYobm9kZSl7bm9kZS5tb3VudGVkPW1vdW50O2lmKG5vZGUubW91bnQpe25vZGUubW91bnQubW91bnRzLnB1c2gobW91bnQpfX1yZXR1cm4gbW91bnRSb290fSx1bm1vdW50KG1vdW50cG9pbnQpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChtb3VudHBvaW50LHtmb2xsb3dfbW91bnQ6ZmFsc2V9KTtpZighRlMuaXNNb3VudHBvaW50KGxvb2t1cC5ub2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXZhciBub2RlPWxvb2t1cC5ub2RlO3ZhciBtb3VudD1ub2RlLm1vdW50ZWQ7dmFyIG1vdW50cz1GUy5nZXRNb3VudHMobW91bnQpO2Zvcih2YXJbaGFzaCxjdXJyZW50XW9mIE9iamVjdC5lbnRyaWVzKEZTLm5hbWVUYWJsZSkpe3doaWxlKGN1cnJlbnQpe3ZhciBuZXh0PWN1cnJlbnQubmFtZV9uZXh0O2lmKG1vdW50cy5pbmNsdWRlcyhjdXJyZW50Lm1vdW50KSl7RlMuZGVzdHJveU5vZGUoY3VycmVudCl9Y3VycmVudD1uZXh0fX1ub2RlLm1vdW50ZWQ9bnVsbDt2YXIgaWR4PW5vZGUubW91bnQubW91bnRzLmluZGV4T2YobW91bnQpO2Fzc2VydChpZHghPT0tMSk7bm9kZS5tb3VudC5tb3VudHMuc3BsaWNlKGlkeCwxKX0sbG9va3VwKHBhcmVudCxuYW1lKXtyZXR1cm4gcGFyZW50Lm5vZGVfb3BzLmxvb2t1cChwYXJlbnQsbmFtZSl9LG1rbm9kKHBhdGgsbW9kZSxkZXYpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtwYXJlbnQ6dHJ1ZX0pO3ZhciBwYXJlbnQ9bG9va3VwLm5vZGU7dmFyIG5hbWU9UEFUSC5iYXNlbmFtZShwYXRoKTtpZighbmFtZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfWlmKG5hbWU9PT0iLiJ8fG5hbWU9PT0iLi4iKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyMCl9dmFyIGVyckNvZGU9RlMubWF5Q3JlYXRlKHBhcmVudCxuYW1lKTtpZihlcnJDb2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcihlcnJDb2RlKX1pZighcGFyZW50Lm5vZGVfb3BzLm1rbm9kKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig2Myl9cmV0dXJuIHBhcmVudC5ub2RlX29wcy5ta25vZChwYXJlbnQsbmFtZSxtb2RlLGRldil9LHN0YXRmcyhwYXRoKXtyZXR1cm4gRlMuc3RhdGZzTm9kZShGUy5sb29rdXBQYXRoKHBhdGgse2ZvbGxvdzp0cnVlfSkubm9kZSl9LHN0YXRmc1N0cmVhbShzdHJlYW0pe3JldHVybiBGUy5zdGF0ZnNOb2RlKHN0cmVhbS5ub2RlKX0sc3RhdGZzTm9kZShub2RlKXt2YXIgcnRuPXtic2l6ZTo0MDk2LGZyc2l6ZTo0MDk2LGJsb2NrczoxZTYsYmZyZWU6NWU1LGJhdmFpbDo1ZTUsZmlsZXM6RlMubmV4dElub2RlLGZmcmVlOkZTLm5leHRJbm9kZS0xLGZzaWQ6NDIsZmxhZ3M6MixuYW1lbGVuOjI1NX07aWYobm9kZS5ub2RlX29wcy5zdGF0ZnMpe09iamVjdC5hc3NpZ24ocnRuLG5vZGUubm9kZV9vcHMuc3RhdGZzKG5vZGUubW91bnQub3B0cy5yb290KSl9cmV0dXJuIHJ0bn0sY3JlYXRlKHBhdGgsbW9kZT00Mzgpe21vZGUmPTQwOTU7bW9kZXw9MzI3Njg7cmV0dXJuIEZTLm1rbm9kKHBhdGgsbW9kZSwwKX0sbWtkaXIocGF0aCxtb2RlPTUxMSl7bW9kZSY9NTExfDUxMjttb2RlfD0xNjM4NDtyZXR1cm4gRlMubWtub2QocGF0aCxtb2RlLDApfSxta2RpclRyZWUocGF0aCxtb2RlKXt2YXIgZGlycz1wYXRoLnNwbGl0KCIvIik7dmFyIGQ9IiI7Zm9yKHZhciBkaXIgb2YgZGlycyl7aWYoIWRpciljb250aW51ZTtpZihkfHxQQVRILmlzQWJzKHBhdGgpKWQrPSIvIjtkKz1kaXI7dHJ5e0ZTLm1rZGlyKGQsbW9kZSl9Y2F0Y2goZSl7aWYoZS5lcnJubyE9MjApdGhyb3cgZX19fSxta2RldihwYXRoLG1vZGUsZGV2KXtpZih0eXBlb2YgZGV2PT0idW5kZWZpbmVkIil7ZGV2PW1vZGU7bW9kZT00Mzh9bW9kZXw9ODE5MjtyZXR1cm4gRlMubWtub2QocGF0aCxtb2RlLGRldil9LHN5bWxpbmsob2xkcGF0aCxuZXdwYXRoKXtpZighUEFUSF9GUy5yZXNvbHZlKG9sZHBhdGgpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKG5ld3BhdGgse3BhcmVudDp0cnVlfSk7dmFyIHBhcmVudD1sb29rdXAubm9kZTtpZighcGFyZW50KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9dmFyIG5ld25hbWU9UEFUSC5iYXNlbmFtZShuZXdwYXRoKTt2YXIgZXJyQ29kZT1GUy5tYXlDcmVhdGUocGFyZW50LG5ld25hbWUpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfWlmKCFwYXJlbnQubm9kZV9vcHMuc3ltbGluayl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNjMpfXJldHVybiBwYXJlbnQubm9kZV9vcHMuc3ltbGluayhwYXJlbnQsbmV3bmFtZSxvbGRwYXRoKX0scmVuYW1lKG9sZF9wYXRoLG5ld19wYXRoKXt2YXIgb2xkX2Rpcm5hbWU9UEFUSC5kaXJuYW1lKG9sZF9wYXRoKTt2YXIgbmV3X2Rpcm5hbWU9UEFUSC5kaXJuYW1lKG5ld19wYXRoKTt2YXIgb2xkX25hbWU9UEFUSC5iYXNlbmFtZShvbGRfcGF0aCk7dmFyIG5ld19uYW1lPVBBVEguYmFzZW5hbWUobmV3X3BhdGgpO3ZhciBsb29rdXAsb2xkX2RpcixuZXdfZGlyO2xvb2t1cD1GUy5sb29rdXBQYXRoKG9sZF9wYXRoLHtwYXJlbnQ6dHJ1ZX0pO29sZF9kaXI9bG9va3VwLm5vZGU7bG9va3VwPUZTLmxvb2t1cFBhdGgobmV3X3BhdGgse3BhcmVudDp0cnVlfSk7bmV3X2Rpcj1sb29rdXAubm9kZTtpZighb2xkX2Rpcnx8IW5ld19kaXIpdGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDQpO2lmKG9sZF9kaXIubW91bnQhPT1uZXdfZGlyLm1vdW50KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig3NSl9dmFyIG9sZF9ub2RlPUZTLmxvb2t1cE5vZGUob2xkX2RpcixvbGRfbmFtZSk7dmFyIHJlbGF0aXZlPVBBVEhfRlMucmVsYXRpdmUob2xkX3BhdGgsbmV3X2Rpcm5hbWUpO2lmKHJlbGF0aXZlLmNoYXJBdCgwKSE9PSIuIil7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXJlbGF0aXZlPVBBVEhfRlMucmVsYXRpdmUobmV3X3BhdGgsb2xkX2Rpcm5hbWUpO2lmKHJlbGF0aXZlLmNoYXJBdCgwKSE9PSIuIil7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTUpfXZhciBuZXdfbm9kZTt0cnl7bmV3X25vZGU9RlMubG9va3VwTm9kZShuZXdfZGlyLG5ld19uYW1lKX1jYXRjaChlKXt9aWYob2xkX25vZGU9PT1uZXdfbm9kZSl7cmV0dXJufXZhciBpc2Rpcj1GUy5pc0RpcihvbGRfbm9kZS5tb2RlKTt2YXIgZXJyQ29kZT1GUy5tYXlEZWxldGUob2xkX2RpcixvbGRfbmFtZSxpc2Rpcik7aWYoZXJyQ29kZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyQ29kZSl9ZXJyQ29kZT1uZXdfbm9kZT9GUy5tYXlEZWxldGUobmV3X2RpcixuZXdfbmFtZSxpc2Rpcik6RlMubWF5Q3JlYXRlKG5ld19kaXIsbmV3X25hbWUpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfWlmKCFvbGRfZGlyLm5vZGVfb3BzLnJlbmFtZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNjMpfWlmKEZTLmlzTW91bnRwb2ludChvbGRfbm9kZSl8fG5ld19ub2RlJiZGUy5pc01vdW50cG9pbnQobmV3X25vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigxMCl9aWYobmV3X2RpciE9PW9sZF9kaXIpe2VyckNvZGU9RlMubm9kZVBlcm1pc3Npb25zKG9sZF9kaXIsInciKTtpZihlcnJDb2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcihlcnJDb2RlKX19RlMuaGFzaFJlbW92ZU5vZGUob2xkX25vZGUpO3RyeXtvbGRfZGlyLm5vZGVfb3BzLnJlbmFtZShvbGRfbm9kZSxuZXdfZGlyLG5ld19uYW1lKTtvbGRfbm9kZS5wYXJlbnQ9bmV3X2Rpcn1jYXRjaChlKXt0aHJvdyBlfWZpbmFsbHl7RlMuaGFzaEFkZE5vZGUob2xkX25vZGUpfX0scm1kaXIocGF0aCl7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgse3BhcmVudDp0cnVlfSk7dmFyIHBhcmVudD1sb29rdXAubm9kZTt2YXIgbmFtZT1QQVRILmJhc2VuYW1lKHBhdGgpO3ZhciBub2RlPUZTLmxvb2t1cE5vZGUocGFyZW50LG5hbWUpO3ZhciBlcnJDb2RlPUZTLm1heURlbGV0ZShwYXJlbnQsbmFtZSx0cnVlKTtpZihlcnJDb2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcihlcnJDb2RlKX1pZighcGFyZW50Lm5vZGVfb3BzLnJtZGlyKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig2Myl9aWYoRlMuaXNNb3VudHBvaW50KG5vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigxMCl9cGFyZW50Lm5vZGVfb3BzLnJtZGlyKHBhcmVudCxuYW1lKTtGUy5kZXN0cm95Tm9kZShub2RlKX0scmVhZGRpcihwYXRoKXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OnRydWV9KTt2YXIgbm9kZT1sb29rdXAubm9kZTt2YXIgcmVhZGRpcj1GUy5jaGVja09wRXhpc3RzKG5vZGUubm9kZV9vcHMucmVhZGRpciw1NCk7cmV0dXJuIHJlYWRkaXIobm9kZSl9LHVubGluayhwYXRoKXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7cGFyZW50OnRydWV9KTt2YXIgcGFyZW50PWxvb2t1cC5ub2RlO2lmKCFwYXJlbnQpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ0KX12YXIgbmFtZT1QQVRILmJhc2VuYW1lKHBhdGgpO3ZhciBub2RlPUZTLmxvb2t1cE5vZGUocGFyZW50LG5hbWUpO3ZhciBlcnJDb2RlPUZTLm1heURlbGV0ZShwYXJlbnQsbmFtZSxmYWxzZSk7aWYoZXJyQ29kZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyQ29kZSl9aWYoIXBhcmVudC5ub2RlX29wcy51bmxpbmspe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYzKX1pZihGUy5pc01vdW50cG9pbnQobm9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDEwKX1wYXJlbnQubm9kZV9vcHMudW5saW5rKHBhcmVudCxuYW1lKTtGUy5kZXN0cm95Tm9kZShub2RlKX0scmVhZGxpbmsocGF0aCl7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgpO3ZhciBsaW5rPWxvb2t1cC5ub2RlO2lmKCFsaW5rKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9aWYoIWxpbmsubm9kZV9vcHMucmVhZGxpbmspe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI4KX1yZXR1cm4gbGluay5ub2RlX29wcy5yZWFkbGluayhsaW5rKX0sc3RhdChwYXRoLGRvbnRGb2xsb3cpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6IWRvbnRGb2xsb3d9KTt2YXIgbm9kZT1sb29rdXAubm9kZTt2YXIgZ2V0YXR0cj1GUy5jaGVja09wRXhpc3RzKG5vZGUubm9kZV9vcHMuZ2V0YXR0ciw2Myk7cmV0dXJuIGdldGF0dHIobm9kZSl9LGZzdGF0KGZkKXt2YXIgc3RyZWFtPUZTLmdldFN0cmVhbUNoZWNrZWQoZmQpO3ZhciBub2RlPXN0cmVhbS5ub2RlO3ZhciBnZXRhdHRyPXN0cmVhbS5zdHJlYW1fb3BzLmdldGF0dHI7dmFyIGFyZz1nZXRhdHRyP3N0cmVhbTpub2RlO2dldGF0dHI/Pz1ub2RlLm5vZGVfb3BzLmdldGF0dHI7RlMuY2hlY2tPcEV4aXN0cyhnZXRhdHRyLDYzKTtyZXR1cm4gZ2V0YXR0cihhcmcpfSxsc3RhdChwYXRoKXtyZXR1cm4gRlMuc3RhdChwYXRoLHRydWUpfSxkb0NobW9kKHN0cmVhbSxub2RlLG1vZGUsZG9udEZvbGxvdyl7RlMuZG9TZXRBdHRyKHN0cmVhbSxub2RlLHttb2RlOm1vZGUmNDA5NXxub2RlLm1vZGUmfjQwOTUsY3RpbWU6RGF0ZS5ub3coKSxkb250Rm9sbG93fSl9LGNobW9kKHBhdGgsbW9kZSxkb250Rm9sbG93KXt2YXIgbm9kZTtpZih0eXBlb2YgcGF0aD09InN0cmluZyIpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6IWRvbnRGb2xsb3d9KTtub2RlPWxvb2t1cC5ub2RlfWVsc2V7bm9kZT1wYXRofUZTLmRvQ2htb2QobnVsbCxub2RlLG1vZGUsZG9udEZvbGxvdyl9LGxjaG1vZChwYXRoLG1vZGUpe0ZTLmNobW9kKHBhdGgsbW9kZSx0cnVlKX0sZmNobW9kKGZkLG1vZGUpe3ZhciBzdHJlYW09RlMuZ2V0U3RyZWFtQ2hlY2tlZChmZCk7RlMuZG9DaG1vZChzdHJlYW0sc3RyZWFtLm5vZGUsbW9kZSxmYWxzZSl9LGRvQ2hvd24oc3RyZWFtLG5vZGUsZG9udEZvbGxvdyl7RlMuZG9TZXRBdHRyKHN0cmVhbSxub2RlLHt0aW1lc3RhbXA6RGF0ZS5ub3coKSxkb250Rm9sbG93fSl9LGNob3duKHBhdGgsdWlkLGdpZCxkb250Rm9sbG93KXt2YXIgbm9kZTtpZih0eXBlb2YgcGF0aD09InN0cmluZyIpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6IWRvbnRGb2xsb3d9KTtub2RlPWxvb2t1cC5ub2RlfWVsc2V7bm9kZT1wYXRofUZTLmRvQ2hvd24obnVsbCxub2RlLGRvbnRGb2xsb3cpfSxsY2hvd24ocGF0aCx1aWQsZ2lkKXtGUy5jaG93bihwYXRoLHVpZCxnaWQsdHJ1ZSl9LGZjaG93bihmZCx1aWQsZ2lkKXt2YXIgc3RyZWFtPUZTLmdldFN0cmVhbUNoZWNrZWQoZmQpO0ZTLmRvQ2hvd24oc3RyZWFtLHN0cmVhbS5ub2RlLGZhbHNlKX0sZG9UcnVuY2F0ZShzdHJlYW0sbm9kZSxsZW4pe2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDMxKX1pZighRlMuaXNGaWxlKG5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI4KX12YXIgZXJyQ29kZT1GUy5ub2RlUGVybWlzc2lvbnMobm9kZSwidyIpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfUZTLmRvU2V0QXR0cihzdHJlYW0sbm9kZSx7c2l6ZTpsZW4sdGltZXN0YW1wOkRhdGUubm93KCl9KX0sdHJ1bmNhdGUocGF0aCxsZW4pe2lmKGxlbjwwKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9dmFyIG5vZGU7aWYodHlwZW9mIHBhdGg9PSJzdHJpbmciKXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OnRydWV9KTtub2RlPWxvb2t1cC5ub2RlfWVsc2V7bm9kZT1wYXRofUZTLmRvVHJ1bmNhdGUobnVsbCxub2RlLGxlbil9LGZ0cnVuY2F0ZShmZCxsZW4pe3ZhciBzdHJlYW09RlMuZ2V0U3RyZWFtQ2hlY2tlZChmZCk7aWYobGVuPDB8fChzdHJlYW0uZmxhZ3MmMjA5NzE1NSk9PT0wKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9RlMuZG9UcnVuY2F0ZShzdHJlYW0sc3RyZWFtLm5vZGUsbGVuKX0sdXRpbWUocGF0aCxhdGltZSxtdGltZSl7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgse2ZvbGxvdzp0cnVlfSk7dmFyIG5vZGU9bG9va3VwLm5vZGU7dmFyIHNldGF0dHI9RlMuY2hlY2tPcEV4aXN0cyhub2RlLm5vZGVfb3BzLnNldGF0dHIsNjMpO3NldGF0dHIobm9kZSx7YXRpbWUsbXRpbWV9KX0sb3BlbihwYXRoLGZsYWdzLG1vZGU9NDM4KXtpZihwYXRoPT09IiIpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ0KX1mbGFncz10eXBlb2YgZmxhZ3M9PSJzdHJpbmciP0ZTX21vZGVTdHJpbmdUb0ZsYWdzKGZsYWdzKTpmbGFncztpZihmbGFncyY2NCl7bW9kZT1tb2RlJjQwOTV8MzI3Njh9ZWxzZXttb2RlPTB9dmFyIG5vZGU7dmFyIGlzRGlyUGF0aDtpZih0eXBlb2YgcGF0aD09Im9iamVjdCIpe25vZGU9cGF0aH1lbHNle2lzRGlyUGF0aD1wYXRoLmVuZHNXaXRoKCIvIik7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgse2ZvbGxvdzohKGZsYWdzJjEzMTA3Miksbm9lbnRfb2theTp0cnVlfSk7bm9kZT1sb29rdXAubm9kZTtwYXRoPWxvb2t1cC5wYXRofXZhciBjcmVhdGVkPWZhbHNlO2lmKGZsYWdzJjY0KXtpZihub2RlKXtpZihmbGFncyYxMjgpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDIwKX19ZWxzZSBpZihpc0RpclBhdGgpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDMxKX1lbHNle25vZGU9RlMubWtub2QocGF0aCxtb2RlfDUxMSwwKTtjcmVhdGVkPXRydWV9fWlmKCFub2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9aWYoRlMuaXNDaHJkZXYobm9kZS5tb2RlKSl7ZmxhZ3MmPX41MTJ9aWYoZmxhZ3MmNjU1MzYmJiFGUy5pc0Rpcihub2RlLm1vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig1NCl9aWYoIWNyZWF0ZWQpe3ZhciBlcnJDb2RlPUZTLm1heU9wZW4obm9kZSxmbGFncyk7aWYoZXJyQ29kZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyQ29kZSl9fWlmKGZsYWdzJjUxMiYmIWNyZWF0ZWQpe0ZTLnRydW5jYXRlKG5vZGUsMCl9ZmxhZ3MmPX4oMTI4fDUxMnwxMzEwNzIpO3ZhciBzdHJlYW09RlMuY3JlYXRlU3RyZWFtKHtub2RlLHBhdGg6RlMuZ2V0UGF0aChub2RlKSxmbGFncyxzZWVrYWJsZTp0cnVlLHBvc2l0aW9uOjAsc3RyZWFtX29wczpub2RlLnN0cmVhbV9vcHMsdW5nb3R0ZW46W10sZXJyb3I6ZmFsc2V9KTtpZihzdHJlYW0uc3RyZWFtX29wcy5vcGVuKXtzdHJlYW0uc3RyZWFtX29wcy5vcGVuKHN0cmVhbSl9aWYoY3JlYXRlZCl7RlMuY2htb2Qobm9kZSxtb2RlJjUxMSl9aWYoTW9kdWxlWyJsb2dSZWFkRmlsZXMiXSYmIShmbGFncyYxKSl7aWYoIShwYXRoIGluIEZTLnJlYWRGaWxlcykpe0ZTLnJlYWRGaWxlc1twYXRoXT0xfX1yZXR1cm4gc3RyZWFtfSxjbG9zZShzdHJlYW0pe2lmKEZTLmlzQ2xvc2VkKHN0cmVhbSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKHN0cmVhbS5nZXRkZW50cylzdHJlYW0uZ2V0ZGVudHM9bnVsbDt0cnl7aWYoc3RyZWFtLnN0cmVhbV9vcHMuY2xvc2Upe3N0cmVhbS5zdHJlYW1fb3BzLmNsb3NlKHN0cmVhbSl9fWNhdGNoKGUpe3Rocm93IGV9ZmluYWxseXtGUy5jbG9zZVN0cmVhbShzdHJlYW0uZmQpfXN0cmVhbS5mZD1udWxsfSxpc0Nsb3NlZChzdHJlYW0pe3JldHVybiBzdHJlYW0uZmQ9PT1udWxsfSxsbHNlZWsoc3RyZWFtLG9mZnNldCx3aGVuY2Upe2lmKEZTLmlzQ2xvc2VkKHN0cmVhbSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKCFzdHJlYW0uc2Vla2FibGV8fCFzdHJlYW0uc3RyZWFtX29wcy5sbHNlZWspe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDcwKX1pZih3aGVuY2UhPTAmJndoZW5jZSE9MSYmd2hlbmNlIT0yKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9c3RyZWFtLnBvc2l0aW9uPXN0cmVhbS5zdHJlYW1fb3BzLmxsc2VlayhzdHJlYW0sb2Zmc2V0LHdoZW5jZSk7c3RyZWFtLnVuZ290dGVuPVtdO3JldHVybiBzdHJlYW0ucG9zaXRpb259LHJlYWQoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uKXthc3NlcnQob2Zmc2V0Pj0wKTtpZihsZW5ndGg8MHx8cG9zaXRpb248MCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfWlmKEZTLmlzQ2xvc2VkKHN0cmVhbSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKChzdHJlYW0uZmxhZ3MmMjA5NzE1NSk9PT0xKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig4KX1pZihGUy5pc0RpcihzdHJlYW0ubm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMzEpfWlmKCFzdHJlYW0uc3RyZWFtX29wcy5yZWFkKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9dmFyIHNlZWtpbmc9dHlwZW9mIHBvc2l0aW9uIT0idW5kZWZpbmVkIjtpZighc2Vla2luZyl7cG9zaXRpb249c3RyZWFtLnBvc2l0aW9ufWVsc2UgaWYoIXN0cmVhbS5zZWVrYWJsZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNzApfXZhciBieXRlc1JlYWQ9c3RyZWFtLnN0cmVhbV9vcHMucmVhZChzdHJlYW0sYnVmZmVyLG9mZnNldCxsZW5ndGgscG9zaXRpb24pO2lmKCFzZWVraW5nKXN0cmVhbS5wb3NpdGlvbis9Ynl0ZXNSZWFkO3JldHVybiBieXRlc1JlYWR9LHdyaXRlKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbixjYW5Pd24pe2Fzc2VydChvZmZzZXQ+PTApO2lmKGxlbmd0aDwwfHxwb3NpdGlvbjwwKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9aWYoRlMuaXNDbG9zZWQoc3RyZWFtKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoOCl9aWYoKHN0cmVhbS5mbGFncyYyMDk3MTU1KT09PTApe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKEZTLmlzRGlyKHN0cmVhbS5ub2RlLm1vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigzMSl9aWYoIXN0cmVhbS5zdHJlYW1fb3BzLndyaXRlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9aWYoc3RyZWFtLnNlZWthYmxlJiZzdHJlYW0uZmxhZ3MmMTAyNCl7RlMubGxzZWVrKHN0cmVhbSwwLDIpfXZhciBzZWVraW5nPXR5cGVvZiBwb3NpdGlvbiE9InVuZGVmaW5lZCI7aWYoIXNlZWtpbmcpe3Bvc2l0aW9uPXN0cmVhbS5wb3NpdGlvbn1lbHNlIGlmKCFzdHJlYW0uc2Vla2FibGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDcwKX12YXIgYnl0ZXNXcml0dGVuPXN0cmVhbS5zdHJlYW1fb3BzLndyaXRlKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbixjYW5Pd24pO2lmKCFzZWVraW5nKXN0cmVhbS5wb3NpdGlvbis9Ynl0ZXNXcml0dGVuO3JldHVybiBieXRlc1dyaXR0ZW59LG1tYXAoc3RyZWFtLGxlbmd0aCxwb3NpdGlvbixwcm90LGZsYWdzKXtpZigocHJvdCYyKSE9PTAmJihmbGFncyYyKT09PTAmJihzdHJlYW0uZmxhZ3MmMjA5NzE1NSkhPT0yKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyKX1pZigoc3RyZWFtLmZsYWdzJjIwOTcxNTUpPT09MSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMil9aWYoIXN0cmVhbS5zdHJlYW1fb3BzLm1tYXApe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQzKX1pZighbGVuZ3RoKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9cmV0dXJuIHN0cmVhbS5zdHJlYW1fb3BzLm1tYXAoc3RyZWFtLGxlbmd0aCxwb3NpdGlvbixwcm90LGZsYWdzKX0sbXN5bmMoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLG1tYXBGbGFncyl7YXNzZXJ0KG9mZnNldD49MCk7aWYoIXN0cmVhbS5zdHJlYW1fb3BzLm1zeW5jKXtyZXR1cm4gMH1yZXR1cm4gc3RyZWFtLnN0cmVhbV9vcHMubXN5bmMoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLG1tYXBGbGFncyl9LGlvY3RsKHN0cmVhbSxjbWQsYXJnKXtpZighc3RyZWFtLnN0cmVhbV9vcHMuaW9jdGwpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDU5KX1yZXR1cm4gc3RyZWFtLnN0cmVhbV9vcHMuaW9jdGwoc3RyZWFtLGNtZCxhcmcpfSxyZWFkRmlsZShwYXRoLG9wdHM9e30pe29wdHMuZmxhZ3M9b3B0cy5mbGFnc3x8MDtvcHRzLmVuY29kaW5nPW9wdHMuZW5jb2Rpbmd8fCJiaW5hcnkiO2lmKG9wdHMuZW5jb2RpbmchPT0idXRmOCImJm9wdHMuZW5jb2RpbmchPT0iYmluYXJ5Iil7YWJvcnQoYEludmFsaWQgZW5jb2RpbmcgdHlwZSAiJHtvcHRzLmVuY29kaW5nfSJgKX12YXIgc3RyZWFtPUZTLm9wZW4ocGF0aCxvcHRzLmZsYWdzKTt2YXIgc3RhdD1GUy5zdGF0KHBhdGgpO3ZhciBsZW5ndGg9c3RhdC5zaXplO3ZhciBidWY9bmV3IFVpbnQ4QXJyYXkobGVuZ3RoKTtGUy5yZWFkKHN0cmVhbSxidWYsMCxsZW5ndGgsMCk7aWYob3B0cy5lbmNvZGluZz09PSJ1dGY4Iil7YnVmPVVURjhBcnJheVRvU3RyaW5nKGJ1Zil9RlMuY2xvc2Uoc3RyZWFtKTtyZXR1cm4gYnVmfSx3cml0ZUZpbGUocGF0aCxkYXRhLG9wdHM9e30pe29wdHMuZmxhZ3M9b3B0cy5mbGFnc3x8NTc3O3ZhciBzdHJlYW09RlMub3BlbihwYXRoLG9wdHMuZmxhZ3Msb3B0cy5tb2RlKTtpZih0eXBlb2YgZGF0YT09InN0cmluZyIpe2RhdGE9bmV3IFVpbnQ4QXJyYXkoaW50QXJyYXlGcm9tU3RyaW5nKGRhdGEsdHJ1ZSkpfWlmKEFycmF5QnVmZmVyLmlzVmlldyhkYXRhKSl7RlMud3JpdGUoc3RyZWFtLGRhdGEsMCxkYXRhLmJ5dGVMZW5ndGgsdW5kZWZpbmVkLG9wdHMuY2FuT3duKX1lbHNle2Fib3J0KCJVbnN1cHBvcnRlZCBkYXRhIHR5cGUiKX1GUy5jbG9zZShzdHJlYW0pfSxjd2Q6KCk9PkZTLmN1cnJlbnRQYXRoLGNoZGlyKHBhdGgpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6dHJ1ZX0pO2lmKGxvb2t1cC5ub2RlPT09bnVsbCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDQpfWlmKCFGUy5pc0Rpcihsb29rdXAubm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTQpfXZhciBlcnJDb2RlPUZTLm5vZGVQZXJtaXNzaW9ucyhsb29rdXAubm9kZSwieCIpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfUZTLmN1cnJlbnRQYXRoPWxvb2t1cC5wYXRofSxjcmVhdGVEZWZhdWx0RGlyZWN0b3JpZXMoKXtGUy5ta2RpcigiL3RtcCIpO0ZTLm1rZGlyKCIvaG9tZSIpO0ZTLm1rZGlyKCIvaG9tZS93ZWJfdXNlciIpfSxjcmVhdGVEZWZhdWx0RGV2aWNlcygpe0ZTLm1rZGlyKCIvZGV2Iik7RlMucmVnaXN0ZXJEZXZpY2UoRlMubWFrZWRldigxLDMpLHtyZWFkOigpPT4wLHdyaXRlOihzdHJlYW0sYnVmZmVyLG9mZnNldCxsZW5ndGgscG9zKT0+bGVuZ3RoLGxsc2VlazooKT0+MH0pO0ZTLm1rZGV2KCIvZGV2L251bGwiLEZTLm1ha2VkZXYoMSwzKSk7VFRZLnJlZ2lzdGVyKEZTLm1ha2VkZXYoNSwwKSxUVFkuZGVmYXVsdF90dHlfb3BzKTtUVFkucmVnaXN0ZXIoRlMubWFrZWRldig2LDApLFRUWS5kZWZhdWx0X3R0eTFfb3BzKTtGUy5ta2RldigiL2Rldi90dHkiLEZTLm1ha2VkZXYoNSwwKSk7RlMubWtkZXYoIi9kZXYvdHR5MSIsRlMubWFrZWRldig2LDApKTt2YXIgcmFuZG9tQnVmZmVyPW5ldyBVaW50OEFycmF5KDEwMjQpLHJhbmRvbUxlZnQ9MDt2YXIgcmFuZG9tQnl0ZT0oKT0+e2lmKHJhbmRvbUxlZnQ9PT0wKXtyYW5kb21GaWxsKHJhbmRvbUJ1ZmZlcik7cmFuZG9tTGVmdD1yYW5kb21CdWZmZXIuYnl0ZUxlbmd0aH1yZXR1cm4gcmFuZG9tQnVmZmVyWy0tcmFuZG9tTGVmdF19O0ZTLmNyZWF0ZURldmljZSgiL2RldiIsInJhbmRvbSIscmFuZG9tQnl0ZSk7RlMuY3JlYXRlRGV2aWNlKCIvZGV2IiwidXJhbmRvbSIscmFuZG9tQnl0ZSk7RlMubWtkaXIoIi9kZXYvc2htIik7RlMubWtkaXIoIi9kZXYvc2htL3RtcCIpfSxjcmVhdGVTcGVjaWFsRGlyZWN0b3JpZXMoKXtGUy5ta2RpcigiL3Byb2MiKTt2YXIgcHJvY19zZWxmPUZTLm1rZGlyKCIvcHJvYy9zZWxmIik7RlMubWtkaXIoIi9wcm9jL3NlbGYvZmQiKTtGUy5tb3VudCh7bW91bnQoKXt2YXIgbm9kZT1GUy5jcmVhdGVOb2RlKHByb2Nfc2VsZiwiZmQiLDE2ODk1LDczKTtub2RlLnN0cmVhbV9vcHM9e2xsc2VlazpNRU1GUy5zdHJlYW1fb3BzLmxsc2Vla307bm9kZS5ub2RlX29wcz17bG9va3VwKHBhcmVudCxuYW1lKXt2YXIgZmQ9K25hbWU7dmFyIHN0cmVhbT1GUy5nZXRTdHJlYW1DaGVja2VkKGZkKTt2YXIgcmV0PXtwYXJlbnQ6bnVsbCxtb3VudDp7bW91bnRwb2ludDoiZmFrZSJ9LG5vZGVfb3BzOntyZWFkbGluazooKT0+c3RyZWFtLnBhdGh9LGlkOmZkKzF9O3JldC5wYXJlbnQ9cmV0O3JldHVybiByZXR9LHJlYWRkaXIoKXtyZXR1cm4gQXJyYXkuZnJvbShGUy5zdHJlYW1zLmVudHJpZXMoKSkuZmlsdGVyKChbayx2XSk9PnYpLm1hcCgoW2ssdl0pPT5rLnRvU3RyaW5nKCkpfX07cmV0dXJuIG5vZGV9fSx7fSwiL3Byb2Mvc2VsZi9mZCIpfSxjcmVhdGVTdGFuZGFyZFN0cmVhbXMoaW5wdXQsb3V0cHV0LGVycm9yKXtpZihpbnB1dCl7RlMuY3JlYXRlRGV2aWNlKCIvZGV2Iiwic3RkaW4iLGlucHV0KX1lbHNle0ZTLnN5bWxpbmsoIi9kZXYvdHR5IiwiL2Rldi9zdGRpbiIpfWlmKG91dHB1dCl7RlMuY3JlYXRlRGV2aWNlKCIvZGV2Iiwic3Rkb3V0IixudWxsLG91dHB1dCl9ZWxzZXtGUy5zeW1saW5rKCIvZGV2L3R0eSIsIi9kZXYvc3Rkb3V0Iil9aWYoZXJyb3Ipe0ZTLmNyZWF0ZURldmljZSgiL2RldiIsInN0ZGVyciIsbnVsbCxlcnJvcil9ZWxzZXtGUy5zeW1saW5rKCIvZGV2L3R0eTEiLCIvZGV2L3N0ZGVyciIpfXZhciBzdGRpbj1GUy5vcGVuKCIvZGV2L3N0ZGluIiwwKTt2YXIgc3Rkb3V0PUZTLm9wZW4oIi9kZXYvc3Rkb3V0IiwxKTt2YXIgc3RkZXJyPUZTLm9wZW4oIi9kZXYvc3RkZXJyIiwxKTthc3NlcnQoc3RkaW4uZmQ9PT0wLGBpbnZhbGlkIGhhbmRsZSBmb3Igc3RkaW4gKCR7c3RkaW4uZmR9KWApO2Fzc2VydChzdGRvdXQuZmQ9PT0xLGBpbnZhbGlkIGhhbmRsZSBmb3Igc3Rkb3V0ICgke3N0ZG91dC5mZH0pYCk7YXNzZXJ0KHN0ZGVyci5mZD09PTIsYGludmFsaWQgaGFuZGxlIGZvciBzdGRlcnIgKCR7c3RkZXJyLmZkfSlgKX0sc3RhdGljSW5pdCgpe0ZTLm5hbWVUYWJsZT1uZXcgQXJyYXkoNDA5Nik7RlMubW91bnQoTUVNRlMse30sIi8iKTtGUy5jcmVhdGVEZWZhdWx0RGlyZWN0b3JpZXMoKTtGUy5jcmVhdGVEZWZhdWx0RGV2aWNlcygpO0ZTLmNyZWF0ZVNwZWNpYWxEaXJlY3RvcmllcygpO0ZTLmZpbGVzeXN0ZW1zPXtNRU1GU319LGluaXQoaW5wdXQsb3V0cHV0LGVycm9yKXthc3NlcnQoIUZTLmluaXRpYWxpemVkLCJGUy5pbml0IHdhcyBwcmV2aW91c2x5IGNhbGxlZC4gSWYgeW91IHdhbnQgdG8gaW5pdGlhbGl6ZSBsYXRlciB3aXRoIGN1c3RvbSBwYXJhbWV0ZXJzLCByZW1vdmUgYW55IGVhcmxpZXIgY2FsbHMgKG5vdGUgdGhhdCBvbmUgaXMgYXV0b21hdGljYWxseSBhZGRlZCB0byB0aGUgZ2VuZXJhdGVkIGNvZGUpIik7RlMuaW5pdGlhbGl6ZWQ9dHJ1ZTtpbnB1dD8/PU1vZHVsZVsic3RkaW4iXTtvdXRwdXQ/Pz1Nb2R1bGVbInN0ZG91dCJdO2Vycm9yPz89TW9kdWxlWyJzdGRlcnIiXTtGUy5jcmVhdGVTdGFuZGFyZFN0cmVhbXMoaW5wdXQsb3V0cHV0LGVycm9yKX0scXVpdCgpe0ZTLmluaXRpYWxpemVkPWZhbHNlO19mZmx1c2goMCk7Zm9yKHZhciBzdHJlYW0gb2YgRlMuc3RyZWFtcyl7aWYoc3RyZWFtKXtGUy5jbG9zZShzdHJlYW0pfX19LGZpbmRPYmplY3QocGF0aCxkb250UmVzb2x2ZUxhc3RMaW5rKXt2YXIgcmV0PUZTLmFuYWx5emVQYXRoKHBhdGgsZG9udFJlc29sdmVMYXN0TGluayk7aWYoIXJldC5leGlzdHMpe3JldHVybiBudWxsfXJldHVybiByZXQub2JqZWN0fSxhbmFseXplUGF0aChwYXRoLGRvbnRSZXNvbHZlTGFzdExpbmspe3RyeXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OiFkb250UmVzb2x2ZUxhc3RMaW5rfSk7cGF0aD1sb29rdXAucGF0aH1jYXRjaChlKXt9dmFyIHJldD17aXNSb290OmZhbHNlLGV4aXN0czpmYWxzZSxlcnJvcjowLG5hbWU6bnVsbCxwYXRoOm51bGwsb2JqZWN0Om51bGwscGFyZW50RXhpc3RzOmZhbHNlLHBhcmVudFBhdGg6bnVsbCxwYXJlbnRPYmplY3Q6bnVsbH07dHJ5e3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtwYXJlbnQ6dHJ1ZX0pO3JldC5wYXJlbnRFeGlzdHM9dHJ1ZTtyZXQucGFyZW50UGF0aD1sb29rdXAucGF0aDtyZXQucGFyZW50T2JqZWN0PWxvb2t1cC5ub2RlO3JldC5uYW1lPVBBVEguYmFzZW5hbWUocGF0aCk7bG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OiFkb250UmVzb2x2ZUxhc3RMaW5rfSk7cmV0LmV4aXN0cz10cnVlO3JldC5wYXRoPWxvb2t1cC5wYXRoO3JldC5vYmplY3Q9bG9va3VwLm5vZGU7cmV0Lm5hbWU9bG9va3VwLm5vZGUubmFtZTtyZXQuaXNSb290PWxvb2t1cC5wYXRoPT09Ii8ifWNhdGNoKGUpe3JldC5lcnJvcj1lLmVycm5vfXJldHVybiByZXR9LGNyZWF0ZVBhdGgocGFyZW50LHBhdGgsY2FuUmVhZCxjYW5Xcml0ZSl7cGFyZW50PXR5cGVvZiBwYXJlbnQ9PSJzdHJpbmciP3BhcmVudDpGUy5nZXRQYXRoKHBhcmVudCk7dmFyIHBhcnRzPXBhdGguc3BsaXQoIi8iKS5yZXZlcnNlKCk7d2hpbGUocGFydHMubGVuZ3RoKXt2YXIgcGFydD1wYXJ0cy5wb3AoKTtpZighcGFydCljb250aW51ZTt2YXIgY3VycmVudD1QQVRILmpvaW4yKHBhcmVudCxwYXJ0KTt0cnl7RlMubWtkaXIoY3VycmVudCl9Y2F0Y2goZSl7aWYoZS5lcnJubyE9MjApdGhyb3cgZX1wYXJlbnQ9Y3VycmVudH1yZXR1cm4gY3VycmVudH0sY3JlYXRlRmlsZShwYXJlbnQsbmFtZSxwcm9wZXJ0aWVzLGNhblJlYWQsY2FuV3JpdGUpe3ZhciBwYXRoPVBBVEguam9pbjIodHlwZW9mIHBhcmVudD09InN0cmluZyI/cGFyZW50OkZTLmdldFBhdGgocGFyZW50KSxuYW1lKTt2YXIgbW9kZT1GU19nZXRNb2RlKGNhblJlYWQsY2FuV3JpdGUpO3JldHVybiBGUy5jcmVhdGUocGF0aCxtb2RlKX0sY3JlYXRlRGF0YUZpbGUocGFyZW50LG5hbWUsZGF0YSxjYW5SZWFkLGNhbldyaXRlLGNhbk93bil7dmFyIHBhdGg9bmFtZTtpZihwYXJlbnQpe3BhcmVudD10eXBlb2YgcGFyZW50PT0ic3RyaW5nIj9wYXJlbnQ6RlMuZ2V0UGF0aChwYXJlbnQpO3BhdGg9bmFtZT9QQVRILmpvaW4yKHBhcmVudCxuYW1lKTpwYXJlbnR9dmFyIG1vZGU9RlNfZ2V0TW9kZShjYW5SZWFkLGNhbldyaXRlKTt2YXIgbm9kZT1GUy5jcmVhdGUocGF0aCxtb2RlKTtpZihkYXRhKXtpZih0eXBlb2YgZGF0YT09InN0cmluZyIpe3ZhciBhcnI9bmV3IEFycmF5KGRhdGEubGVuZ3RoKTtmb3IodmFyIGk9MCxsZW49ZGF0YS5sZW5ndGg7aTxsZW47KytpKWFycltpXT1kYXRhLmNoYXJDb2RlQXQoaSk7ZGF0YT1hcnJ9RlMuY2htb2Qobm9kZSxtb2RlfDE0Nik7dmFyIHN0cmVhbT1GUy5vcGVuKG5vZGUsNTc3KTtGUy53cml0ZShzdHJlYW0sZGF0YSwwLGRhdGEubGVuZ3RoLDAsY2FuT3duKTtGUy5jbG9zZShzdHJlYW0pO0ZTLmNobW9kKG5vZGUsbW9kZSl9fSxjcmVhdGVEZXZpY2UocGFyZW50LG5hbWUsaW5wdXQsb3V0cHV0KXt2YXIgcGF0aD1QQVRILmpvaW4yKHR5cGVvZiBwYXJlbnQ9PSJzdHJpbmciP3BhcmVudDpGUy5nZXRQYXRoKHBhcmVudCksbmFtZSk7dmFyIG1vZGU9RlNfZ2V0TW9kZSghIWlucHV0LCEhb3V0cHV0KTtGUy5jcmVhdGVEZXZpY2UubWFqb3I/Pz02NDt2YXIgZGV2PUZTLm1ha2VkZXYoRlMuY3JlYXRlRGV2aWNlLm1ham9yKyssMCk7RlMucmVnaXN0ZXJEZXZpY2UoZGV2LHtvcGVuKHN0cmVhbSl7c3RyZWFtLnNlZWthYmxlPWZhbHNlfSxjbG9zZShzdHJlYW0pe2lmKG91dHB1dD8uYnVmZmVyPy5sZW5ndGgpe291dHB1dCgxMCl9fSxyZWFkKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3Mpe3ZhciBieXRlc1JlYWQ9MDtmb3IodmFyIGk9MDtpPGxlbmd0aDtpKyspe3ZhciByZXN1bHQ7dHJ5e3Jlc3VsdD1pbnB1dCgpfWNhdGNoKGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI5KX1pZihyZXN1bHQ9PT11bmRlZmluZWQmJmJ5dGVzUmVhZD09PTApe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYpfWlmKHJlc3VsdD09PW51bGx8fHJlc3VsdD09PXVuZGVmaW5lZClicmVhaztieXRlc1JlYWQrKztidWZmZXJbb2Zmc2V0K2ldPXJlc3VsdH1pZihieXRlc1JlYWQpe3N0cmVhbS5ub2RlLmF0aW1lPURhdGUubm93KCl9cmV0dXJuIGJ5dGVzUmVhZH0sd3JpdGUoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvcyl7Zm9yKHZhciBpPTA7aTxsZW5ndGg7aSsrKXt0cnl7b3V0cHV0KGJ1ZmZlcltvZmZzZXQraV0pfWNhdGNoKGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI5KX19aWYobGVuZ3RoKXtzdHJlYW0ubm9kZS5tdGltZT1zdHJlYW0ubm9kZS5jdGltZT1EYXRlLm5vdygpfXJldHVybiBpfX0pO3JldHVybiBGUy5ta2RldihwYXRoLG1vZGUsZGV2KX0sZm9yY2VMb2FkRmlsZShvYmope2lmKG9iai5pc0RldmljZXx8b2JqLmlzRm9sZGVyfHxvYmoubGlua3x8b2JqLmNvbnRlbnRzKXJldHVybiB0cnVlO2lmKGdsb2JhbFRoaXMuWE1MSHR0cFJlcXVlc3Qpe2Fib3J0KCJMYXp5IGxvYWRpbmcgc2hvdWxkIGhhdmUgYmVlbiBwZXJmb3JtZWQgKGNvbnRlbnRzIHNldCkgaW4gY3JlYXRlTGF6eUZpbGUsIGJ1dCBpdCB3YXMgbm90LiBMYXp5IGxvYWRpbmcgb25seSB3b3JrcyBpbiB3ZWIgd29ya2Vycy4gVXNlIC0tZW1iZWQtZmlsZSBvciAtLXByZWxvYWQtZmlsZSBpbiBlbWNjIG9uIHRoZSBtYWluIHRocmVhZC4iKX1lbHNle3RyeXtvYmouY29udGVudHM9cmVhZEJpbmFyeShvYmoudXJsKX1jYXRjaChlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOSl9fX0sY3JlYXRlTGF6eUZpbGUocGFyZW50LG5hbWUsdXJsLGNhblJlYWQsY2FuV3JpdGUpe2NsYXNzIExhenlVaW50OEFycmF5e2xlbmd0aEtub3duPWZhbHNlO2NodW5rcz1bXTtnZXQoaWR4KXtpZihpZHg+dGhpcy5sZW5ndGgtMXx8aWR4PDApe3JldHVybiB1bmRlZmluZWR9dmFyIGNodW5rT2Zmc2V0PWlkeCV0aGlzLmNodW5rU2l6ZTt2YXIgY2h1bmtOdW09aWR4L3RoaXMuY2h1bmtTaXplfDA7cmV0dXJuIHRoaXMuZ2V0dGVyKGNodW5rTnVtKVtjaHVua09mZnNldF19c2V0RGF0YUdldHRlcihnZXR0ZXIpe3RoaXMuZ2V0dGVyPWdldHRlcn1jYWNoZUxlbmd0aCgpe3ZhciB4aHI9bmV3IFhNTEh0dHBSZXF1ZXN0O3hoci5vcGVuKCJIRUFEIix1cmwsZmFsc2UpO3hoci5zZW5kKG51bGwpO2lmKCEoeGhyLnN0YXR1cz49MjAwJiZ4aHIuc3RhdHVzPDMwMHx8eGhyLnN0YXR1cz09PTMwNCkpYWJvcnQoIkNvdWxkbid0IGxvYWQgIit1cmwrIi4gU3RhdHVzOiAiK3hoci5zdGF0dXMpO3ZhciBkYXRhbGVuZ3RoPU51bWJlcih4aHIuZ2V0UmVzcG9uc2VIZWFkZXIoIkNvbnRlbnQtbGVuZ3RoIikpO3ZhciBoZWFkZXI7dmFyIGhhc0J5dGVTZXJ2aW5nPShoZWFkZXI9eGhyLmdldFJlc3BvbnNlSGVhZGVyKCJBY2NlcHQtUmFuZ2VzIikpJiZoZWFkZXI9PT0iYnl0ZXMiO3ZhciB1c2VzR3ppcD0oaGVhZGVyPXhoci5nZXRSZXNwb25zZUhlYWRlcigiQ29udGVudC1FbmNvZGluZyIpKSYmaGVhZGVyPT09Imd6aXAiO3ZhciBjaHVua1NpemU9MTAyNCoxMDI0O2lmKCFoYXNCeXRlU2VydmluZyljaHVua1NpemU9ZGF0YWxlbmd0aDt2YXIgZG9YSFI9KGZyb20sdG8pPT57aWYoZnJvbT50bylhYm9ydCgiaW52YWxpZCByYW5nZSAoIitmcm9tKyIsICIrdG8rIikgb3Igbm8gYnl0ZXMgcmVxdWVzdGVkISIpO2lmKHRvPmRhdGFsZW5ndGgtMSlhYm9ydCgib25seSAiK2RhdGFsZW5ndGgrIiBieXRlcyBhdmFpbGFibGUhIHByb2dyYW1tZXIgZXJyb3IhIik7dmFyIHhocj1uZXcgWE1MSHR0cFJlcXVlc3Q7eGhyLm9wZW4oIkdFVCIsdXJsLGZhbHNlKTtpZihkYXRhbGVuZ3RoIT09Y2h1bmtTaXplKXhoci5zZXRSZXF1ZXN0SGVhZGVyKCJSYW5nZSIsImJ5dGVzPSIrZnJvbSsiLSIrdG8pO3hoci5yZXNwb25zZVR5cGU9ImFycmF5YnVmZmVyIjtpZih4aHIub3ZlcnJpZGVNaW1lVHlwZSl7eGhyLm92ZXJyaWRlTWltZVR5cGUoInRleHQvcGxhaW47IGNoYXJzZXQ9eC11c2VyLWRlZmluZWQiKX14aHIuc2VuZChudWxsKTtpZighKHhoci5zdGF0dXM+PTIwMCYmeGhyLnN0YXR1czwzMDB8fHhoci5zdGF0dXM9PT0zMDQpKWFib3J0KCJDb3VsZG4ndCBsb2FkICIrdXJsKyIuIFN0YXR1czogIit4aHIuc3RhdHVzKTtpZih4aHIucmVzcG9uc2UhPT11bmRlZmluZWQpe3JldHVybiBuZXcgVWludDhBcnJheSh4aHIucmVzcG9uc2V8fFtdKX1yZXR1cm4gaW50QXJyYXlGcm9tU3RyaW5nKHhoci5yZXNwb25zZVRleHR8fCIiLHRydWUpfTt2YXIgbGF6eUFycmF5PXRoaXM7bGF6eUFycmF5LnNldERhdGFHZXR0ZXIoY2h1bmtOdW09Pnt2YXIgc3RhcnQ9Y2h1bmtOdW0qY2h1bmtTaXplO3ZhciBlbmQ9KGNodW5rTnVtKzEpKmNodW5rU2l6ZS0xO2VuZD1NYXRoLm1pbihlbmQsZGF0YWxlbmd0aC0xKTtpZih0eXBlb2YgbGF6eUFycmF5LmNodW5rc1tjaHVua051bV09PSJ1bmRlZmluZWQiKXtsYXp5QXJyYXkuY2h1bmtzW2NodW5rTnVtXT1kb1hIUihzdGFydCxlbmQpfWlmKHR5cGVvZiBsYXp5QXJyYXkuY2h1bmtzW2NodW5rTnVtXT09InVuZGVmaW5lZCIpYWJvcnQoImRvWEhSIGZhaWxlZCEiKTtyZXR1cm4gbGF6eUFycmF5LmNodW5rc1tjaHVua051bV19KTtpZih1c2VzR3ppcHx8IWRhdGFsZW5ndGgpe2NodW5rU2l6ZT1kYXRhbGVuZ3RoPTE7ZGF0YWxlbmd0aD10aGlzLmdldHRlcigwKS5sZW5ndGg7Y2h1bmtTaXplPWRhdGFsZW5ndGg7b3V0KCJMYXp5RmlsZXMgb24gZ3ppcCBmb3JjZXMgZG93bmxvYWQgb2YgdGhlIHdob2xlIGZpbGUgd2hlbiBsZW5ndGggaXMgYWNjZXNzZWQiKX10aGlzLl9sZW5ndGg9ZGF0YWxlbmd0aDt0aGlzLl9jaHVua1NpemU9Y2h1bmtTaXplO3RoaXMubGVuZ3RoS25vd249dHJ1ZX1nZXQgbGVuZ3RoKCl7aWYoIXRoaXMubGVuZ3RoS25vd24pe3RoaXMuY2FjaGVMZW5ndGgoKX1yZXR1cm4gdGhpcy5fbGVuZ3RofWdldCBjaHVua1NpemUoKXtpZighdGhpcy5sZW5ndGhLbm93bil7dGhpcy5jYWNoZUxlbmd0aCgpfXJldHVybiB0aGlzLl9jaHVua1NpemV9fWlmKGdsb2JhbFRoaXMuWE1MSHR0cFJlcXVlc3Qpe2lmKCFFTlZJUk9OTUVOVF9JU19XT1JLRVIpYWJvcnQoIkNhbm5vdCBkbyBzeW5jaHJvbm91cyBiaW5hcnkgWEhScyBvdXRzaWRlIHdlYndvcmtlcnMgaW4gbW9kZXJuIGJyb3dzZXJzLiBVc2UgLS1lbWJlZC1maWxlIG9yIC0tcHJlbG9hZC1maWxlIGluIGVtY2MiKTt2YXIgbGF6eUFycmF5PW5ldyBMYXp5VWludDhBcnJheTt2YXIgcHJvcGVydGllcz17aXNEZXZpY2U6ZmFsc2UsY29udGVudHM6bGF6eUFycmF5fX1lbHNle3ZhciBwcm9wZXJ0aWVzPXtpc0RldmljZTpmYWxzZSx1cmx9fXZhciBub2RlPUZTLmNyZWF0ZUZpbGUocGFyZW50LG5hbWUscHJvcGVydGllcyxjYW5SZWFkLGNhbldyaXRlKTtpZihwcm9wZXJ0aWVzLmNvbnRlbnRzKXtub2RlLmNvbnRlbnRzPXByb3BlcnRpZXMuY29udGVudHN9ZWxzZSBpZihwcm9wZXJ0aWVzLnVybCl7bm9kZS5jb250ZW50cz1udWxsO25vZGUudXJsPXByb3BlcnRpZXMudXJsfU9iamVjdC5kZWZpbmVQcm9wZXJ0aWVzKG5vZGUse3VzZWRCeXRlczp7Z2V0OmZ1bmN0aW9uKCl7cmV0dXJuIHRoaXMuY29udGVudHMubGVuZ3RofX19KTt2YXIgc3RyZWFtX29wcz17fTtmb3IoY29uc3Rba2V5LGZuXW9mIE9iamVjdC5lbnRyaWVzKG5vZGUuc3RyZWFtX29wcykpe3N0cmVhbV9vcHNba2V5XT0oLi4uYXJncyk9PntGUy5mb3JjZUxvYWRGaWxlKG5vZGUpO3JldHVybiBmbiguLi5hcmdzKX19ZnVuY3Rpb24gd3JpdGVDaHVua3Moc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uKXt2YXIgY29udGVudHM9c3RyZWFtLm5vZGUuY29udGVudHM7aWYocG9zaXRpb24+PWNvbnRlbnRzLmxlbmd0aClyZXR1cm4gMDt2YXIgc2l6ZT1NYXRoLm1pbihjb250ZW50cy5sZW5ndGgtcG9zaXRpb24sbGVuZ3RoKTthc3NlcnQoc2l6ZT49MCk7aWYoY29udGVudHMuc2xpY2Upe2Zvcih2YXIgaT0wO2k8c2l6ZTtpKyspe2J1ZmZlcltvZmZzZXQraV09Y29udGVudHNbcG9zaXRpb24raV19fWVsc2V7Zm9yKHZhciBpPTA7aTxzaXplO2krKyl7YnVmZmVyW29mZnNldCtpXT1jb250ZW50cy5nZXQocG9zaXRpb24raSl9fXJldHVybiBzaXplfXN0cmVhbV9vcHMucmVhZD0oc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uKT0+e0ZTLmZvcmNlTG9hZEZpbGUobm9kZSk7cmV0dXJuIHdyaXRlQ2h1bmtzKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbil9O3N0cmVhbV9vcHMubW1hcD0oc3RyZWFtLGxlbmd0aCxwb3NpdGlvbixwcm90LGZsYWdzKT0+e0ZTLmZvcmNlTG9hZEZpbGUobm9kZSk7dmFyIHB0cj1tbWFwQWxsb2MobGVuZ3RoKTtpZighcHRyKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0OCl9d3JpdGVDaHVua3Moc3RyZWFtLChncm93TWVtVmlld3MoKSxIRUFQOCkscHRyLGxlbmd0aCxwb3NpdGlvbik7cmV0dXJue3B0cixhbGxvY2F0ZWQ6dHJ1ZX19O25vZGUuc3RyZWFtX29wcz1zdHJlYW1fb3BzO3JldHVybiBub2RlfSxhYnNvbHV0ZVBhdGgoKXthYm9ydCgiRlMuYWJzb2x1dGVQYXRoIGhhcyBiZWVuIHJlbW92ZWQ7IHVzZSBQQVRIX0ZTLnJlc29sdmUgaW5zdGVhZCIpfSxjcmVhdGVGb2xkZXIoKXthYm9ydCgiRlMuY3JlYXRlRm9sZGVyIGhhcyBiZWVuIHJlbW92ZWQ7IHVzZSBGUy5ta2RpciBpbnN0ZWFkIil9LGNyZWF0ZUxpbmsoKXthYm9ydCgiRlMuY3JlYXRlTGluayBoYXMgYmVlbiByZW1vdmVkOyB1c2UgRlMuc3ltbGluayBpbnN0ZWFkIil9LGpvaW5QYXRoKCl7YWJvcnQoIkZTLmpvaW5QYXRoIGhhcyBiZWVuIHJlbW92ZWQ7IHVzZSBQQVRILmpvaW4gaW5zdGVhZCIpfSxtbWFwQWxsb2MoKXthYm9ydCgiRlMubW1hcEFsbG9jIGhhcyBiZWVuIHJlcGxhY2VkIGJ5IHRoZSB0b3AgbGV2ZWwgZnVuY3Rpb24gbW1hcEFsbG9jIil9LHN0YW5kYXJkaXplUGF0aCgpe2Fib3J0KCJGUy5zdGFuZGFyZGl6ZVBhdGggaGFzIGJlZW4gcmVtb3ZlZDsgdXNlIFBBVEgubm9ybWFsaXplIGluc3RlYWQiKX19O3ZhciBTWVNDQUxMUz17REVGQVVMVF9QT0xMTUFTSzo1LGNhbGN1bGF0ZUF0KGRpcmZkLHBhdGgsYWxsb3dFbXB0eSl7aWYoUEFUSC5pc0FicyhwYXRoKSl7cmV0dXJuIHBhdGh9dmFyIGRpcjtpZihkaXJmZD09PS0xMDApe2Rpcj1GUy5jd2QoKX1lbHNle3ZhciBkaXJzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGRpcmZkKTtkaXI9ZGlyc3RyZWFtLnBhdGh9aWYocGF0aC5sZW5ndGg9PTApe2lmKCFhbGxvd0VtcHR5KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9cmV0dXJuIGRpcn1yZXR1cm4gZGlyKyIvIitwYXRofSx3cml0ZVN0YXQoYnVmLHN0YXQpeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWY+PjJdPXN0YXQuZGV2Oyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrND4+Ml09c3RhdC5tb2RlOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrOD4+Ml09c3RhdC5ubGluazsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzEyPj4yXT1zdGF0LnVpZDsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzE2Pj4yXT1zdGF0LmdpZDsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzIwPj4yXT1zdGF0LnJkZXY7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzI0Pj4zXT1CaWdJbnQoc3RhdC5zaXplKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVtidWYrMzI+PjJdPTQwOTY7KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYnVmKzM2Pj4yXT1zdGF0LmJsb2Nrczt2YXIgYXRpbWU9c3RhdC5hdGltZS5nZXRUaW1lKCk7dmFyIG10aW1lPXN0YXQubXRpbWUuZ2V0VGltZSgpO3ZhciBjdGltZT1zdGF0LmN0aW1lLmdldFRpbWUoKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtidWYrNDA+PjNdPUJpZ0ludChNYXRoLmZsb29yKGF0aW1lLzFlMykpOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrNDg+PjJdPWF0aW1lJTFlMyoxZTMqMWUzOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW2J1Zis1Nj4+M109QmlnSW50KE1hdGguZmxvb3IobXRpbWUvMWUzKSk7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2J1Zis2ND4+Ml09bXRpbWUlMWUzKjFlMyoxZTM7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzcyPj4zXT1CaWdJbnQoTWF0aC5mbG9vcihjdGltZS8xZTMpKTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzgwPj4yXT1jdGltZSUxZTMqMWUzKjFlMzsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtidWYrODg+PjNdPUJpZ0ludChzdGF0Lmlubyk7cmV0dXJuIDB9LHdyaXRlU3RhdEZzKGJ1ZixzdGF0cyl7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2J1Zis0Pj4yXT1zdGF0cy5ic2l6ZTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzYwPj4yXT1zdGF0cy5ic2l6ZTsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtidWYrOD4+M109QmlnSW50KHN0YXRzLmJsb2Nrcyk7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzE2Pj4zXT1CaWdJbnQoc3RhdHMuYmZyZWUpOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW2J1ZisyND4+M109QmlnSW50KHN0YXRzLmJhdmFpbCk7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzMyPj4zXT1CaWdJbnQoc3RhdHMuZmlsZXMpOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW2J1Zis0MD4+M109QmlnSW50KHN0YXRzLmZmcmVlKTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzQ4Pj4yXT1zdGF0cy5mc2lkOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrNjQ+PjJdPXN0YXRzLmZsYWdzOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrNTY+PjJdPXN0YXRzLm5hbWVsZW59LGRvTXN5bmMoYWRkcixzdHJlYW0sbGVuLGZsYWdzLG9mZnNldCl7aWYoIUZTLmlzRmlsZShzdHJlYW0ubm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDMpfWlmKGZsYWdzJjIpe3JldHVybiAwfXZhciBidWZmZXI9KGdyb3dNZW1WaWV3cygpLEhFQVBVOCkuc2xpY2UoYWRkcixhZGRyK2xlbik7RlMubXN5bmMoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuLGZsYWdzKX0sZ2V0U3RyZWFtRnJvbUZEKGZkKXt2YXIgc3RyZWFtPUZTLmdldFN0cmVhbUNoZWNrZWQoZmQpO3JldHVybiBzdHJlYW19LHZhcmFyZ3M6dW5kZWZpbmVkLGdldFN0cihwdHIpe3ZhciByZXQ9VVRGOFRvU3RyaW5nKHB0cik7cmV0dXJuIHJldH19O2Z1bmN0aW9uIF9fX3N5c2NhbGxfZmNudGw2NChmZCxjbWQsdmFyYXJncyl7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMywwLDEsZmQsY21kLHZhcmFyZ3MpO1NZU0NBTExTLnZhcmFyZ3M9dmFyYXJnczt0cnl7dmFyIHN0cmVhbT1TWVNDQUxMUy5nZXRTdHJlYW1Gcm9tRkQoZmQpO3N3aXRjaChjbWQpe2Nhc2UgMDp7dmFyIGFyZz1zeXNjYWxsR2V0VmFyYXJnSSgpO2lmKGFyZzwwKXtyZXR1cm4tMjh9d2hpbGUoRlMuc3RyZWFtc1thcmddKXthcmcrK312YXIgbmV3U3RyZWFtO25ld1N0cmVhbT1GUy5kdXBTdHJlYW0oc3RyZWFtLGFyZyk7cmV0dXJuIG5ld1N0cmVhbS5mZH1jYXNlIDE6Y2FzZSAyOnJldHVybiAwO2Nhc2UgMzpyZXR1cm4gc3RyZWFtLmZsYWdzO2Nhc2UgNDp7dmFyIGFyZz1zeXNjYWxsR2V0VmFyYXJnSSgpO3N0cmVhbS5mbGFnc3w9YXJnO3JldHVybiAwfWNhc2UgMTI6e3ZhciBhcmc9c3lzY2FsbEdldFZhcmFyZ1AoKTt2YXIgb2Zmc2V0PTA7KGdyb3dNZW1WaWV3cygpLEhFQVAxNilbYXJnK29mZnNldD4+MV09MjtyZXR1cm4gMH1jYXNlIDEzOmNhc2UgMTQ6cmV0dXJuIDB9cmV0dXJuLTI4fWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX1mdW5jdGlvbiBfX19zeXNjYWxsX2ZzdGF0NjQoZmQsYnVmKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg0LDAsMSxmZCxidWYpO3RyeXtyZXR1cm4gU1lTQ0FMTFMud3JpdGVTdGF0KGJ1ZixGUy5mc3RhdChmZCkpfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX1mdW5jdGlvbiBfX19zeXNjYWxsX2lvY3RsKGZkLG9wLHZhcmFyZ3Mpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDUsMCwxLGZkLG9wLHZhcmFyZ3MpO1NZU0NBTExTLnZhcmFyZ3M9dmFyYXJnczt0cnl7dmFyIHN0cmVhbT1TWVNDQUxMUy5nZXRTdHJlYW1Gcm9tRkQoZmQpO3N3aXRjaChvcCl7Y2FzZSAyMTUwOTp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O3JldHVybiAwfWNhc2UgMjE1MDU6e2lmKCFzdHJlYW0udHR5KXJldHVybi01OTtpZihzdHJlYW0udHR5Lm9wcy5pb2N0bF90Y2dldHMpe3ZhciB0ZXJtaW9zPXN0cmVhbS50dHkub3BzLmlvY3RsX3RjZ2V0cyhzdHJlYW0pO3ZhciBhcmdwPXN5c2NhbGxHZXRWYXJhcmdQKCk7KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYXJncD4+Ml09dGVybWlvcy5jX2lmbGFnfHwwOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArND4+Ml09dGVybWlvcy5jX29mbGFnfHwwOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArOD4+Ml09dGVybWlvcy5jX2NmbGFnfHwwOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArMTI+PjJdPXRlcm1pb3MuY19sZmxhZ3x8MDtmb3IodmFyIGk9MDtpPDMyO2krKyl7KGdyb3dNZW1WaWV3cygpLEhFQVA4KVthcmdwK2krMTddPXRlcm1pb3MuY19jY1tpXXx8MH1yZXR1cm4gMH1yZXR1cm4gMH1jYXNlIDIxNTEwOmNhc2UgMjE1MTE6Y2FzZSAyMTUxMjp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O3JldHVybiAwfWNhc2UgMjE1MDY6Y2FzZSAyMTUwNzpjYXNlIDIxNTA4OntpZighc3RyZWFtLnR0eSlyZXR1cm4tNTk7aWYoc3RyZWFtLnR0eS5vcHMuaW9jdGxfdGNzZXRzKXt2YXIgYXJncD1zeXNjYWxsR2V0VmFyYXJnUCgpO3ZhciBjX2lmbGFnPShncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3A+PjJdO3ZhciBjX29mbGFnPShncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArND4+Ml07dmFyIGNfY2ZsYWc9KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYXJncCs4Pj4yXTt2YXIgY19sZmxhZz0oZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVthcmdwKzEyPj4yXTt2YXIgY19jYz1bXTtmb3IodmFyIGk9MDtpPDMyO2krKyl7Y19jYy5wdXNoKChncm93TWVtVmlld3MoKSxIRUFQOClbYXJncCtpKzE3XSl9cmV0dXJuIHN0cmVhbS50dHkub3BzLmlvY3RsX3Rjc2V0cyhzdHJlYW0udHR5LG9wLHtjX2lmbGFnLGNfb2ZsYWcsY19jZmxhZyxjX2xmbGFnLGNfY2N9KX1yZXR1cm4gMH1jYXNlIDIxNTE5OntpZighc3RyZWFtLnR0eSlyZXR1cm4tNTk7dmFyIGFyZ3A9c3lzY2FsbEdldFZhcmFyZ1AoKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVthcmdwPj4yXT0wO3JldHVybiAwfWNhc2UgMjE1MjA6e2lmKCFzdHJlYW0udHR5KXJldHVybi01OTtyZXR1cm4tMjh9Y2FzZSAyMTUzNzpjYXNlIDIxNTMxOnt2YXIgYXJncD1zeXNjYWxsR2V0VmFyYXJnUCgpO3JldHVybiBGUy5pb2N0bChzdHJlYW0sb3AsYXJncCl9Y2FzZSAyMTUyMzp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O2lmKHN0cmVhbS50dHkub3BzLmlvY3RsX3Rpb2Nnd2luc3ope3ZhciB3aW5zaXplPXN0cmVhbS50dHkub3BzLmlvY3RsX3Rpb2Nnd2luc3ooc3RyZWFtLnR0eSk7dmFyIGFyZ3A9c3lzY2FsbEdldFZhcmFyZ1AoKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDE2KVthcmdwPj4xXT13aW5zaXplWzBdOyhncm93TWVtVmlld3MoKSxIRUFQMTYpW2FyZ3ArMj4+MV09d2luc2l6ZVsxXX1yZXR1cm4gMH1jYXNlIDIxNTI0OntpZighc3RyZWFtLnR0eSlyZXR1cm4tNTk7cmV0dXJuIDB9Y2FzZSAyMTUxNTp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O3JldHVybiAwfWRlZmF1bHQ6cmV0dXJuLTI4fX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319ZnVuY3Rpb24gX19fc3lzY2FsbF9sc3RhdDY0KHBhdGgsYnVmKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg2LDAsMSxwYXRoLGJ1Zik7dHJ5e3BhdGg9U1lTQ0FMTFMuZ2V0U3RyKHBhdGgpO3JldHVybiBTWVNDQUxMUy53cml0ZVN0YXQoYnVmLEZTLmxzdGF0KHBhdGgpKX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319ZnVuY3Rpb24gX19fc3lzY2FsbF9uZXdmc3RhdGF0KGRpcmZkLHBhdGgsYnVmLGZsYWdzKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg3LDAsMSxkaXJmZCxwYXRoLGJ1ZixmbGFncyk7dHJ5e3BhdGg9U1lTQ0FMTFMuZ2V0U3RyKHBhdGgpO3ZhciBub2ZvbGxvdz1mbGFncyYyNTY7dmFyIGFsbG93RW1wdHk9ZmxhZ3MmNDA5NjtmbGFncz1mbGFncyZ+NjQwMDthc3NlcnQoIWZsYWdzLGB1bmtub3duIGZsYWdzIGluIF9fc3lzY2FsbF9uZXdmc3RhdGF0OiAke2ZsYWdzfWApO3BhdGg9U1lTQ0FMTFMuY2FsY3VsYXRlQXQoZGlyZmQscGF0aCxhbGxvd0VtcHR5KTtyZXR1cm4gU1lTQ0FMTFMud3JpdGVTdGF0KGJ1Zixub2ZvbGxvdz9GUy5sc3RhdChwYXRoKTpGUy5zdGF0KHBhdGgpKX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319ZnVuY3Rpb24gX19fc3lzY2FsbF9vcGVuYXQoZGlyZmQscGF0aCxmbGFncyx2YXJhcmdzKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg4LDAsMSxkaXJmZCxwYXRoLGZsYWdzLHZhcmFyZ3MpO1NZU0NBTExTLnZhcmFyZ3M9dmFyYXJnczt0cnl7cGF0aD1TWVNDQUxMUy5nZXRTdHIocGF0aCk7cGF0aD1TWVNDQUxMUy5jYWxjdWxhdGVBdChkaXJmZCxwYXRoKTt2YXIgbW9kZT12YXJhcmdzP3N5c2NhbGxHZXRWYXJhcmdJKCk6MDtyZXR1cm4gRlMub3BlbihwYXRoLGZsYWdzLG1vZGUpLmZkfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX1mdW5jdGlvbiBfX19zeXNjYWxsX3N0YXQ2NChwYXRoLGJ1Zil7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoOSwwLDEscGF0aCxidWYpO3RyeXtwYXRoPVNZU0NBTExTLmdldFN0cihwYXRoKTtyZXR1cm4gU1lTQ0FMTFMud3JpdGVTdGF0KGJ1ZixGUy5zdGF0KHBhdGgpKX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319dmFyIF9fYWJvcnRfanM9KCk9PmFib3J0KCJuYXRpdmUgY29kZSBjYWxsZWQgYWJvcnQoKSIpO3ZhciBBc2NpaVRvU3RyaW5nPXB0cj0+e3ZhciBzdHI9IiI7d2hpbGUoMSl7dmFyIGNoPShncm93TWVtVmlld3MoKSxIRUFQVTgpW3B0cisrXTtpZighY2gpcmV0dXJuIHN0cjtzdHIrPVN0cmluZy5mcm9tQ2hhckNvZGUoY2gpfX07dmFyIGF3YWl0aW5nRGVwZW5kZW5jaWVzPXt9O3ZhciByZWdpc3RlcmVkVHlwZXM9e307dmFyIHR5cGVEZXBlbmRlbmNpZXM9e307dmFyIEJpbmRpbmdFcnJvcj1jbGFzcyBCaW5kaW5nRXJyb3IgZXh0ZW5kcyBFcnJvcntjb25zdHJ1Y3RvcihtZXNzYWdlKXtzdXBlcihtZXNzYWdlKTt0aGlzLm5hbWU9IkJpbmRpbmdFcnJvciJ9fTt2YXIgdGhyb3dCaW5kaW5nRXJyb3I9bWVzc2FnZT0+e3Rocm93IG5ldyBCaW5kaW5nRXJyb3IobWVzc2FnZSl9O2Z1bmN0aW9uIHNoYXJlZFJlZ2lzdGVyVHlwZShyYXdUeXBlLHJlZ2lzdGVyZWRJbnN0YW5jZSxvcHRpb25zPXt9KXt2YXIgbmFtZT1yZWdpc3RlcmVkSW5zdGFuY2UubmFtZTtpZighcmF3VHlwZSl7dGhyb3dCaW5kaW5nRXJyb3IoYHR5cGUgIiR7bmFtZX0iIG11c3QgaGF2ZSBhIHBvc2l0aXZlIGludGVnZXIgdHlwZWlkIHBvaW50ZXJgKX1pZihyZWdpc3RlcmVkVHlwZXMuaGFzT3duUHJvcGVydHkocmF3VHlwZSkpe2lmKG9wdGlvbnMuaWdub3JlRHVwbGljYXRlUmVnaXN0cmF0aW9ucyl7cmV0dXJufWVsc2V7dGhyb3dCaW5kaW5nRXJyb3IoYENhbm5vdCByZWdpc3RlciB0eXBlICcke25hbWV9JyB0d2ljZWApfX1yZWdpc3RlcmVkVHlwZXNbcmF3VHlwZV09cmVnaXN0ZXJlZEluc3RhbmNlO2RlbGV0ZSB0eXBlRGVwZW5kZW5jaWVzW3Jhd1R5cGVdO2lmKGF3YWl0aW5nRGVwZW5kZW5jaWVzLmhhc093blByb3BlcnR5KHJhd1R5cGUpKXt2YXIgY2FsbGJhY2tzPWF3YWl0aW5nRGVwZW5kZW5jaWVzW3Jhd1R5cGVdO2RlbGV0ZSBhd2FpdGluZ0RlcGVuZGVuY2llc1tyYXdUeXBlXTtjYWxsYmFja3MuZm9yRWFjaChjYj0+Y2IoKSl9fWZ1bmN0aW9uIHJlZ2lzdGVyVHlwZShyYXdUeXBlLHJlZ2lzdGVyZWRJbnN0YW5jZSxvcHRpb25zPXt9KXtyZXR1cm4gc2hhcmVkUmVnaXN0ZXJUeXBlKHJhd1R5cGUscmVnaXN0ZXJlZEluc3RhbmNlLG9wdGlvbnMpfXZhciBpbnRlZ2VyUmVhZFZhbHVlRnJvbVBvaW50ZXI9KG5hbWUsd2lkdGgsc2lnbmVkKT0+e3N3aXRjaCh3aWR0aCl7Y2FzZSAxOnJldHVybiBzaWduZWQ/cG9pbnRlcj0+KGdyb3dNZW1WaWV3cygpLEhFQVA4KVtwb2ludGVyXTpwb2ludGVyPT4oZ3Jvd01lbVZpZXdzKCksSEVBUFU4KVtwb2ludGVyXTtjYXNlIDI6cmV0dXJuIHNpZ25lZD9wb2ludGVyPT4oZ3Jvd01lbVZpZXdzKCksSEVBUDE2KVtwb2ludGVyPj4xXTpwb2ludGVyPT4oZ3Jvd01lbVZpZXdzKCksSEVBUFUxNilbcG9pbnRlcj4+MV07Y2FzZSA0OnJldHVybiBzaWduZWQ/cG9pbnRlcj0+KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbcG9pbnRlcj4+Ml06cG9pbnRlcj0+KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3BvaW50ZXI+PjJdO2Nhc2UgODpyZXR1cm4gc2lnbmVkP3BvaW50ZXI9Pihncm93TWVtVmlld3MoKSxIRUFQNjQpW3BvaW50ZXI+PjNdOnBvaW50ZXI9Pihncm93TWVtVmlld3MoKSxIRUFQVTY0KVtwb2ludGVyPj4zXTtkZWZhdWx0OnRocm93IG5ldyBUeXBlRXJyb3IoYGludmFsaWQgaW50ZWdlciB3aWR0aCAoJHt3aWR0aH0pOiAke25hbWV9YCl9fTt2YXIgZW1iaW5kUmVwcj12PT57aWYodj09PW51bGwpe3JldHVybiJudWxsIn12YXIgdD10eXBlb2YgdjtpZih0PT09Im9iamVjdCJ8fHQ9PT0iYXJyYXkifHx0PT09ImZ1bmN0aW9uIil7cmV0dXJuIHYudG9TdHJpbmcoKX1lbHNle3JldHVybiIiK3Z9fTt2YXIgYXNzZXJ0SW50ZWdlclJhbmdlPSh0eXBlTmFtZSx2YWx1ZSxtaW5SYW5nZSxtYXhSYW5nZSk9PntpZih2YWx1ZTxtaW5SYW5nZXx8dmFsdWU+bWF4UmFuZ2Upe3Rocm93IG5ldyBUeXBlRXJyb3IoYFBhc3NpbmcgYSBudW1iZXIgIiR7ZW1iaW5kUmVwcih2YWx1ZSl9IiBmcm9tIEpTIHNpZGUgdG8gQy9DKysgc2lkZSB0byBhbiBhcmd1bWVudCBvZiB0eXBlICIke3R5cGVOYW1lfSIsIHdoaWNoIGlzIG91dHNpZGUgdGhlIHZhbGlkIHJhbmdlIFske21pblJhbmdlfSwgJHttYXhSYW5nZX1dIWApfX07dmFyIF9fZW1iaW5kX3JlZ2lzdGVyX2JpZ2ludD0ocHJpbWl0aXZlVHlwZSxuYW1lLHNpemUsbWluUmFuZ2UsbWF4UmFuZ2UpPT57bmFtZT1Bc2NpaVRvU3RyaW5nKG5hbWUpO2NvbnN0IGlzVW5zaWduZWRUeXBlPW1pblJhbmdlPT09MG47bGV0IGZyb21XaXJlVHlwZT12YWx1ZT0+dmFsdWU7aWYoaXNVbnNpZ25lZFR5cGUpe2NvbnN0IGJpdFNpemU9c2l6ZSo4O2Zyb21XaXJlVHlwZT12YWx1ZT0+QmlnSW50LmFzVWludE4oYml0U2l6ZSx2YWx1ZSk7bWF4UmFuZ2U9ZnJvbVdpcmVUeXBlKG1heFJhbmdlKX1yZWdpc3RlclR5cGUocHJpbWl0aXZlVHlwZSx7bmFtZSxmcm9tV2lyZVR5cGUsdG9XaXJlVHlwZTooZGVzdHJ1Y3RvcnMsdmFsdWUpPT57aWYodHlwZW9mIHZhbHVlPT0ibnVtYmVyIil7dmFsdWU9QmlnSW50KHZhbHVlKX1lbHNlIGlmKHR5cGVvZiB2YWx1ZSE9ImJpZ2ludCIpe3Rocm93IG5ldyBUeXBlRXJyb3IoYENhbm5vdCBjb252ZXJ0ICIke2VtYmluZFJlcHIodmFsdWUpfSIgdG8gJHt0aGlzLm5hbWV9YCl9YXNzZXJ0SW50ZWdlclJhbmdlKG5hbWUsdmFsdWUsbWluUmFuZ2UsbWF4UmFuZ2UpO3JldHVybiB2YWx1ZX0scmVhZFZhbHVlRnJvbVBvaW50ZXI6aW50ZWdlclJlYWRWYWx1ZUZyb21Qb2ludGVyKG5hbWUsc2l6ZSwhaXNVbnNpZ25lZFR5cGUpLGRlc3RydWN0b3JGdW5jdGlvbjpudWxsfSl9O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9ib29sPShyYXdUeXBlLG5hbWUsdHJ1ZVZhbHVlLGZhbHNlVmFsdWUpPT57bmFtZT1Bc2NpaVRvU3RyaW5nKG5hbWUpO3JlZ2lzdGVyVHlwZShyYXdUeXBlLHtuYW1lLGZyb21XaXJlVHlwZTpmdW5jdGlvbih3dCl7cmV0dXJuISF3dH0sdG9XaXJlVHlwZTpmdW5jdGlvbihkZXN0cnVjdG9ycyxvKXtyZXR1cm4gbz90cnVlVmFsdWU6ZmFsc2VWYWx1ZX0scmVhZFZhbHVlRnJvbVBvaW50ZXI6ZnVuY3Rpb24ocG9pbnRlcil7cmV0dXJuIHRoaXMuZnJvbVdpcmVUeXBlKChncm93TWVtVmlld3MoKSxIRUFQVTgpW3BvaW50ZXJdKX0sZGVzdHJ1Y3RvckZ1bmN0aW9uOm51bGx9KX07dmFyIGVtdmFsX2ZyZWVsaXN0PVtdO3ZhciBlbXZhbF9oYW5kbGVzPVswLDEsLDEsbnVsbCwxLHRydWUsMSxmYWxzZSwxXTt2YXIgX19lbXZhbF9kZWNyZWY9aGFuZGxlPT57aWYoaGFuZGxlPjkmJjA9PT0tLWVtdmFsX2hhbmRsZXNbaGFuZGxlKzFdKXthc3NlcnQoZW12YWxfaGFuZGxlc1toYW5kbGVdIT09dW5kZWZpbmVkLGBEZWNyZWYgZm9yIHVuYWxsb2NhdGVkIGhhbmRsZS5gKTtlbXZhbF9oYW5kbGVzW2hhbmRsZV09dW5kZWZpbmVkO2VtdmFsX2ZyZWVsaXN0LnB1c2goaGFuZGxlKX19O3ZhciBFbXZhbD17dG9WYWx1ZTpoYW5kbGU9PntpZighaGFuZGxlKXt0aHJvd0JpbmRpbmdFcnJvcihgQ2Fubm90IHVzZSBkZWxldGVkIHZhbC4gaGFuZGxlID0gJHtoYW5kbGV9YCl9YXNzZXJ0KGhhbmRsZT09PTJ8fGVtdmFsX2hhbmRsZXNbaGFuZGxlXSE9PXVuZGVmaW5lZCYmaGFuZGxlJTI9PT0wLGBpbnZhbGlkIGhhbmRsZTogJHtoYW5kbGV9YCk7cmV0dXJuIGVtdmFsX2hhbmRsZXNbaGFuZGxlXX0sdG9IYW5kbGU6dmFsdWU9Pntzd2l0Y2godmFsdWUpe2Nhc2UgdW5kZWZpbmVkOnJldHVybiAyO2Nhc2UgbnVsbDpyZXR1cm4gNDtjYXNlIHRydWU6cmV0dXJuIDY7Y2FzZSBmYWxzZTpyZXR1cm4gODtkZWZhdWx0Ontjb25zdCBoYW5kbGU9ZW12YWxfZnJlZWxpc3QucG9wKCl8fGVtdmFsX2hhbmRsZXMubGVuZ3RoO2VtdmFsX2hhbmRsZXNbaGFuZGxlXT12YWx1ZTtlbXZhbF9oYW5kbGVzW2hhbmRsZSsxXT0xO3JldHVybiBoYW5kbGV9fX19O2Z1bmN0aW9uIHJlYWRQb2ludGVyKHBvaW50ZXIpe3JldHVybiB0aGlzLmZyb21XaXJlVHlwZSgoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbcG9pbnRlcj4+Ml0pfXZhciBFbVZhbFR5cGU9e25hbWU6ImVtc2NyaXB0ZW46OnZhbCIsZnJvbVdpcmVUeXBlOmhhbmRsZT0+e3ZhciBydj1FbXZhbC50b1ZhbHVlKGhhbmRsZSk7X19lbXZhbF9kZWNyZWYoaGFuZGxlKTtyZXR1cm4gcnZ9LHRvV2lyZVR5cGU6KGRlc3RydWN0b3JzLHZhbHVlKT0+RW12YWwudG9IYW5kbGUodmFsdWUpLHJlYWRWYWx1ZUZyb21Qb2ludGVyOnJlYWRQb2ludGVyLGRlc3RydWN0b3JGdW5jdGlvbjpudWxsfTt2YXIgX19lbWJpbmRfcmVnaXN0ZXJfZW12YWw9cmF3VHlwZT0+cmVnaXN0ZXJUeXBlKHJhd1R5cGUsRW1WYWxUeXBlKTt2YXIgZmxvYXRSZWFkVmFsdWVGcm9tUG9pbnRlcj0obmFtZSx3aWR0aCk9Pntzd2l0Y2god2lkdGgpe2Nhc2UgNDpyZXR1cm4gZnVuY3Rpb24ocG9pbnRlcil7cmV0dXJuIHRoaXMuZnJvbVdpcmVUeXBlKChncm93TWVtVmlld3MoKSxIRUFQRjMyKVtwb2ludGVyPj4yXSl9O2Nhc2UgODpyZXR1cm4gZnVuY3Rpb24ocG9pbnRlcil7cmV0dXJuIHRoaXMuZnJvbVdpcmVUeXBlKChncm93TWVtVmlld3MoKSxIRUFQRjY0KVtwb2ludGVyPj4zXSl9O2RlZmF1bHQ6dGhyb3cgbmV3IFR5cGVFcnJvcihgaW52YWxpZCBmbG9hdCB3aWR0aCAoJHt3aWR0aH0pOiAke25hbWV9YCl9fTt2YXIgX19lbWJpbmRfcmVnaXN0ZXJfZmxvYXQ9KHJhd1R5cGUsbmFtZSxzaXplKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtyZWdpc3RlclR5cGUocmF3VHlwZSx7bmFtZSxmcm9tV2lyZVR5cGU6dmFsdWU9PnZhbHVlLHRvV2lyZVR5cGU6KGRlc3RydWN0b3JzLHZhbHVlKT0+e2lmKHR5cGVvZiB2YWx1ZSE9Im51bWJlciImJnR5cGVvZiB2YWx1ZSE9ImJvb2xlYW4iKXt0aHJvdyBuZXcgVHlwZUVycm9yKGBDYW5ub3QgY29udmVydCAke2VtYmluZFJlcHIodmFsdWUpfSB0byAke3RoaXMubmFtZX1gKX1yZXR1cm4gdmFsdWV9LHJlYWRWYWx1ZUZyb21Qb2ludGVyOmZsb2F0UmVhZFZhbHVlRnJvbVBvaW50ZXIobmFtZSxzaXplKSxkZXN0cnVjdG9yRnVuY3Rpb246bnVsbH0pfTt2YXIgY3JlYXRlTmFtZWRGdW5jdGlvbj0obmFtZSxmdW5jKT0+T2JqZWN0LmRlZmluZVByb3BlcnR5KGZ1bmMsIm5hbWUiLHt2YWx1ZTpuYW1lfSk7dmFyIHJ1bkRlc3RydWN0b3JzPWRlc3RydWN0b3JzPT57d2hpbGUoZGVzdHJ1Y3RvcnMubGVuZ3RoKXt2YXIgcHRyPWRlc3RydWN0b3JzLnBvcCgpO3ZhciBkZWw9ZGVzdHJ1Y3RvcnMucG9wKCk7ZGVsKHB0cil9fTtmdW5jdGlvbiB1c2VzRGVzdHJ1Y3RvclN0YWNrKGFyZ1R5cGVzKXtmb3IodmFyIGk9MTtpPGFyZ1R5cGVzLmxlbmd0aDsrK2kpe2lmKGFyZ1R5cGVzW2ldIT09bnVsbCYmYXJnVHlwZXNbaV0uZGVzdHJ1Y3RvckZ1bmN0aW9uPT09dW5kZWZpbmVkKXtyZXR1cm4gdHJ1ZX19cmV0dXJuIGZhbHNlfWZ1bmN0aW9uIGNoZWNrQXJnQ291bnQobnVtQXJncyxtaW5BcmdzLG1heEFyZ3MsaHVtYW5OYW1lLHRocm93QmluZGluZ0Vycm9yKXtpZihudW1BcmdzPG1pbkFyZ3N8fG51bUFyZ3M+bWF4QXJncyl7dmFyIGFyZ0NvdW50TWVzc2FnZT1taW5BcmdzPT1tYXhBcmdzP21pbkFyZ3M6YCR7bWluQXJnc30gdG8gJHttYXhBcmdzfWA7dGhyb3dCaW5kaW5nRXJyb3IoYGZ1bmN0aW9uICR7aHVtYW5OYW1lfSBjYWxsZWQgd2l0aCAke251bUFyZ3N9IGFyZ3VtZW50cywgZXhwZWN0ZWQgJHthcmdDb3VudE1lc3NhZ2V9YCl9fWZ1bmN0aW9uIGNyZWF0ZUpzSW52b2tlcihhcmdUeXBlcyxpc0NsYXNzTWV0aG9kRnVuYyxyZXR1cm5zLGlzQXN5bmMpe3ZhciBuZWVkc0Rlc3RydWN0b3JTdGFjaz11c2VzRGVzdHJ1Y3RvclN0YWNrKGFyZ1R5cGVzKTt2YXIgYXJnQ291bnQ9YXJnVHlwZXMubGVuZ3RoLTI7dmFyIGFyZ3NMaXN0PVtdO3ZhciBhcmdzTGlzdFdpcmVkPVsiZm4iXTtpZihpc0NsYXNzTWV0aG9kRnVuYyl7YXJnc0xpc3RXaXJlZC5wdXNoKCJ0aGlzV2lyZWQiKX1mb3IodmFyIGk9MDtpPGFyZ0NvdW50OysraSl7YXJnc0xpc3QucHVzaChgYXJnJHtpfWApO2FyZ3NMaXN0V2lyZWQucHVzaChgYXJnJHtpfVdpcmVkYCl9YXJnc0xpc3Q9YXJnc0xpc3Quam9pbigiLCIpO2FyZ3NMaXN0V2lyZWQ9YXJnc0xpc3RXaXJlZC5qb2luKCIsIik7dmFyIGludm9rZXJGbkJvZHk9YHJldHVybiBmdW5jdGlvbiAoJHthcmdzTGlzdH0pIHtcbmA7aW52b2tlckZuQm9keSs9ImNoZWNrQXJnQ291bnQoYXJndW1lbnRzLmxlbmd0aCwgbWluQXJncywgbWF4QXJncywgaHVtYW5OYW1lLCB0aHJvd0JpbmRpbmdFcnJvcik7XG4iO2lmKG5lZWRzRGVzdHJ1Y3RvclN0YWNrKXtpbnZva2VyRm5Cb2R5Kz0idmFyIGRlc3RydWN0b3JzID0gW107XG4ifXZhciBkdG9yU3RhY2s9bmVlZHNEZXN0cnVjdG9yU3RhY2s/ImRlc3RydWN0b3JzIjoibnVsbCI7dmFyIGFyZ3MxPVsiaHVtYW5OYW1lIiwidGhyb3dCaW5kaW5nRXJyb3IiLCJpbnZva2VyIiwiZm4iLCJydW5EZXN0cnVjdG9ycyIsImZyb21SZXRXaXJlIiwidG9DbGFzc1BhcmFtV2lyZSJdO2lmKGlzQ2xhc3NNZXRob2RGdW5jKXtpbnZva2VyRm5Cb2R5Kz1gdmFyIHRoaXNXaXJlZCA9IHRvQ2xhc3NQYXJhbVdpcmUoJHtkdG9yU3RhY2t9LCB0aGlzKTtcbmB9Zm9yKHZhciBpPTA7aTxhcmdDb3VudDsrK2kpe3ZhciBhcmdOYW1lPWB0b0FyZyR7aX1XaXJlYDtpbnZva2VyRm5Cb2R5Kz1gdmFyIGFyZyR7aX1XaXJlZCA9ICR7YXJnTmFtZX0oJHtkdG9yU3RhY2t9LCBhcmcke2l9KTtcbmA7YXJnczEucHVzaChhcmdOYW1lKX1pbnZva2VyRm5Cb2R5Kz0ocmV0dXJuc3x8aXNBc3luYz8idmFyIHJ2ID0gIjoiIikrYGludm9rZXIoJHthcmdzTGlzdFdpcmVkfSk7XG5gO3ZhciByZXR1cm5WYWw9cmV0dXJucz8icnYiOiIiO2ludm9rZXJGbkJvZHkrPWBmdW5jdGlvbiBvbkRvbmUoJHtyZXR1cm5WYWx9KSB7XG5gO2lmKG5lZWRzRGVzdHJ1Y3RvclN0YWNrKXtpbnZva2VyRm5Cb2R5Kz0icnVuRGVzdHJ1Y3RvcnMoZGVzdHJ1Y3RvcnMpO1xuIn1lbHNle2Zvcih2YXIgaT1pc0NsYXNzTWV0aG9kRnVuYz8xOjI7aTxhcmdUeXBlcy5sZW5ndGg7KytpKXt2YXIgcGFyYW1OYW1lPWk9PT0xPyJ0aGlzV2lyZWQiOiJhcmciKyhpLTIpKyJXaXJlZCI7aWYoYXJnVHlwZXNbaV0uZGVzdHJ1Y3RvckZ1bmN0aW9uIT09bnVsbCl7aW52b2tlckZuQm9keSs9YCR7cGFyYW1OYW1lfV9kdG9yKCR7cGFyYW1OYW1lfSk7XG5gO2FyZ3MxLnB1c2goYCR7cGFyYW1OYW1lfV9kdG9yYCl9fX1pZihyZXR1cm5zKXtpbnZva2VyRm5Cb2R5Kz0idmFyIHJldCA9IGZyb21SZXRXaXJlKHJ2KTtcbiIrInJldHVybiByZXQ7XG4ifWVsc2V7fWludm9rZXJGbkJvZHkrPSJ9XG4iO2ludm9rZXJGbkJvZHkrPSJyZXR1cm4gIisoaXNBc3luYz8icnYudGhlbihvbkRvbmUpIjpgb25Eb25lKCR7cmV0dXJuVmFsfSlgKSsiOyI7aW52b2tlckZuQm9keSs9In1cbiI7YXJnczEucHVzaCgiY2hlY2tBcmdDb3VudCIsIm1pbkFyZ3MiLCJtYXhBcmdzIik7aW52b2tlckZuQm9keT1gaWYgKGFyZ3VtZW50cy5sZW5ndGggIT09ICR7YXJnczEubGVuZ3RofSl7IHRocm93IG5ldyBFcnJvcihodW1hbk5hbWUgKyAiRXhwZWN0ZWQgJHthcmdzMS5sZW5ndGh9IGNsb3N1cmUgYXJndW1lbnRzICIgKyBhcmd1bWVudHMubGVuZ3RoICsgIiBnaXZlbi4iKTsgfVxuJHtpbnZva2VyRm5Cb2R5fWA7cmV0dXJuIG5ldyBGdW5jdGlvbihhcmdzMSxpbnZva2VyRm5Cb2R5KX1mdW5jdGlvbiBnZXRSZXF1aXJlZEFyZ0NvdW50KGFyZ1R5cGVzKXt2YXIgcmVxdWlyZWRBcmdDb3VudD1hcmdUeXBlcy5sZW5ndGgtMjtmb3IodmFyIGk9YXJnVHlwZXMubGVuZ3RoLTE7aT49MjstLWkpe2lmKCFhcmdUeXBlc1tpXS5vcHRpb25hbCl7YnJlYWt9cmVxdWlyZWRBcmdDb3VudC0tfXJldHVybiByZXF1aXJlZEFyZ0NvdW50fWZ1bmN0aW9uIGNyYWZ0SW52b2tlckZ1bmN0aW9uKGh1bWFuTmFtZSxhcmdUeXBlcyxjbGFzc1R5cGUsY3BwSW52b2tlckZ1bmMsY3BwVGFyZ2V0RnVuYyxpc0FzeW5jKXt2YXIgYXJnQ291bnQ9YXJnVHlwZXMubGVuZ3RoO2lmKGFyZ0NvdW50PDIpe3Rocm93QmluZGluZ0Vycm9yKCJhcmdUeXBlcyBhcnJheSBzaXplIG1pc21hdGNoISBNdXN0IGF0IGxlYXN0IGdldCByZXR1cm4gdmFsdWUgYW5kICd0aGlzJyB0eXBlcyEiKX12YXIgaXNDbGFzc01ldGhvZEZ1bmM9YXJnVHlwZXNbMV0hPT1udWxsJiZjbGFzc1R5cGUhPT1udWxsO3ZhciBuZWVkc0Rlc3RydWN0b3JTdGFjaz11c2VzRGVzdHJ1Y3RvclN0YWNrKGFyZ1R5cGVzKTt2YXIgcmV0dXJucz0hYXJnVHlwZXNbMF0uaXNWb2lkO3ZhciBleHBlY3RlZEFyZ0NvdW50PWFyZ0NvdW50LTI7dmFyIG1pbkFyZ3M9Z2V0UmVxdWlyZWRBcmdDb3VudChhcmdUeXBlcyk7dmFyIHJldFR5cGU9YXJnVHlwZXNbMF07dmFyIGluc3RUeXBlPWFyZ1R5cGVzWzFdO3ZhciBjbG9zdXJlQXJncz1baHVtYW5OYW1lLHRocm93QmluZGluZ0Vycm9yLGNwcEludm9rZXJGdW5jLGNwcFRhcmdldEZ1bmMscnVuRGVzdHJ1Y3RvcnMscmV0VHlwZS5mcm9tV2lyZVR5cGUuYmluZChyZXRUeXBlKSxpbnN0VHlwZT8udG9XaXJlVHlwZS5iaW5kKGluc3RUeXBlKV07Zm9yKHZhciBpPTI7aTxhcmdDb3VudDsrK2kpe3ZhciBhcmdUeXBlPWFyZ1R5cGVzW2ldO2Nsb3N1cmVBcmdzLnB1c2goYXJnVHlwZS50b1dpcmVUeXBlLmJpbmQoYXJnVHlwZSkpfWlmKCFuZWVkc0Rlc3RydWN0b3JTdGFjayl7Zm9yKHZhciBpPWlzQ2xhc3NNZXRob2RGdW5jPzE6MjtpPGFyZ1R5cGVzLmxlbmd0aDsrK2kpe2lmKGFyZ1R5cGVzW2ldLmRlc3RydWN0b3JGdW5jdGlvbiE9PW51bGwpe2Nsb3N1cmVBcmdzLnB1c2goYXJnVHlwZXNbaV0uZGVzdHJ1Y3RvckZ1bmN0aW9uKX19fWNsb3N1cmVBcmdzLnB1c2goY2hlY2tBcmdDb3VudCxtaW5BcmdzLGV4cGVjdGVkQXJnQ291bnQpO2xldCBpbnZva2VyRmFjdG9yeT1jcmVhdGVKc0ludm9rZXIoYXJnVHlwZXMsaXNDbGFzc01ldGhvZEZ1bmMscmV0dXJucyxpc0FzeW5jKTt2YXIgaW52b2tlckZuPWludm9rZXJGYWN0b3J5KC4uLmNsb3N1cmVBcmdzKTtyZXR1cm4gY3JlYXRlTmFtZWRGdW5jdGlvbihodW1hbk5hbWUsaW52b2tlckZuKX12YXIgZW5zdXJlT3ZlcmxvYWRUYWJsZT0ocHJvdG8sbWV0aG9kTmFtZSxodW1hbk5hbWUpPT57aWYodW5kZWZpbmVkPT09cHJvdG9bbWV0aG9kTmFtZV0ub3ZlcmxvYWRUYWJsZSl7dmFyIHByZXZGdW5jPXByb3RvW21ldGhvZE5hbWVdO3Byb3RvW21ldGhvZE5hbWVdPWZ1bmN0aW9uKC4uLmFyZ3Mpe2lmKCFwcm90b1ttZXRob2ROYW1lXS5vdmVybG9hZFRhYmxlLmhhc093blByb3BlcnR5KGFyZ3MubGVuZ3RoKSl7dGhyb3dCaW5kaW5nRXJyb3IoYEZ1bmN0aW9uICcke2h1bWFuTmFtZX0nIGNhbGxlZCB3aXRoIGFuIGludmFsaWQgbnVtYmVyIG9mIGFyZ3VtZW50cyAoJHthcmdzLmxlbmd0aH0pIC0gZXhwZWN0cyBvbmUgb2YgKCR7cHJvdG9bbWV0aG9kTmFtZV0ub3ZlcmxvYWRUYWJsZX0pIWApfXJldHVybiBwcm90b1ttZXRob2ROYW1lXS5vdmVybG9hZFRhYmxlW2FyZ3MubGVuZ3RoXS5hcHBseSh0aGlzLGFyZ3MpfTtwcm90b1ttZXRob2ROYW1lXS5vdmVybG9hZFRhYmxlPVtdO3Byb3RvW21ldGhvZE5hbWVdLm92ZXJsb2FkVGFibGVbcHJldkZ1bmMuYXJnQ291bnRdPXByZXZGdW5jfX07dmFyIGV4cG9zZVB1YmxpY1N5bWJvbD0obmFtZSx2YWx1ZSxudW1Bcmd1bWVudHMpPT57aWYoTW9kdWxlLmhhc093blByb3BlcnR5KG5hbWUpKXtpZih1bmRlZmluZWQ9PT1udW1Bcmd1bWVudHN8fHVuZGVmaW5lZCE9PU1vZHVsZVtuYW1lXS5vdmVybG9hZFRhYmxlJiZ1bmRlZmluZWQhPT1Nb2R1bGVbbmFtZV0ub3ZlcmxvYWRUYWJsZVtudW1Bcmd1bWVudHNdKXt0aHJvd0JpbmRpbmdFcnJvcihgQ2Fubm90IHJlZ2lzdGVyIHB1YmxpYyBuYW1lICcke25hbWV9JyB0d2ljZWApfWVuc3VyZU92ZXJsb2FkVGFibGUoTW9kdWxlLG5hbWUsbmFtZSk7aWYoTW9kdWxlW25hbWVdLm92ZXJsb2FkVGFibGUuaGFzT3duUHJvcGVydHkobnVtQXJndW1lbnRzKSl7dGhyb3dCaW5kaW5nRXJyb3IoYENhbm5vdCByZWdpc3RlciBtdWx0aXBsZSBvdmVybG9hZHMgb2YgYSBmdW5jdGlvbiB3aXRoIHRoZSBzYW1lIG51bWJlciBvZiBhcmd1bWVudHMgKCR7bnVtQXJndW1lbnRzfSkhYCl9TW9kdWxlW25hbWVdLm92ZXJsb2FkVGFibGVbbnVtQXJndW1lbnRzXT12YWx1ZX1lbHNle01vZHVsZVtuYW1lXT12YWx1ZTtNb2R1bGVbbmFtZV0uYXJnQ291bnQ9bnVtQXJndW1lbnRzfX07dmFyIGhlYXAzMlZlY3RvclRvQXJyYXk9KGNvdW50LGZpcnN0RWxlbWVudCk9Pnt2YXIgYXJyYXk9W107Zm9yKHZhciBpPTA7aTxjb3VudDtpKyspe2FycmF5LnB1c2goKGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2ZpcnN0RWxlbWVudCtpKjQ+PjJdKX1yZXR1cm4gYXJyYXl9O3ZhciBJbnRlcm5hbEVycm9yPWNsYXNzIEludGVybmFsRXJyb3IgZXh0ZW5kcyBFcnJvcntjb25zdHJ1Y3RvcihtZXNzYWdlKXtzdXBlcihtZXNzYWdlKTt0aGlzLm5hbWU9IkludGVybmFsRXJyb3IifX07dmFyIHRocm93SW50ZXJuYWxFcnJvcj1tZXNzYWdlPT57dGhyb3cgbmV3IEludGVybmFsRXJyb3IobWVzc2FnZSl9O3ZhciByZXBsYWNlUHVibGljU3ltYm9sPShuYW1lLHZhbHVlLG51bUFyZ3VtZW50cyk9PntpZighTW9kdWxlLmhhc093blByb3BlcnR5KG5hbWUpKXt0aHJvd0ludGVybmFsRXJyb3IoIlJlcGxhY2luZyBub25leGlzdGVudCBwdWJsaWMgc3ltYm9sIil9aWYodW5kZWZpbmVkIT09TW9kdWxlW25hbWVdLm92ZXJsb2FkVGFibGUmJnVuZGVmaW5lZCE9PW51bUFyZ3VtZW50cyl7TW9kdWxlW25hbWVdLm92ZXJsb2FkVGFibGVbbnVtQXJndW1lbnRzXT12YWx1ZX1lbHNle01vZHVsZVtuYW1lXT12YWx1ZTtNb2R1bGVbbmFtZV0uYXJnQ291bnQ9bnVtQXJndW1lbnRzfX07dmFyIGVtYmluZF9fcmVxdWlyZUZ1bmN0aW9uPShzaWduYXR1cmUscmF3RnVuY3Rpb24saXNBc3luYz1mYWxzZSk9PntzaWduYXR1cmU9QXNjaWlUb1N0cmluZyhzaWduYXR1cmUpO2Z1bmN0aW9uIG1ha2VEeW5DYWxsZXIoKXt2YXIgcnRuPWdldFdhc21UYWJsZUVudHJ5KHJhd0Z1bmN0aW9uKTtpZihpc0FzeW5jKXtydG49V2ViQXNzZW1ibHkucHJvbWlzaW5nKHJ0bil9cmV0dXJuIHJ0bn12YXIgZnA9bWFrZUR5bkNhbGxlcigpO2lmKHR5cGVvZiBmcCE9ImZ1bmN0aW9uIil7dGhyb3dCaW5kaW5nRXJyb3IoYHVua25vd24gZnVuY3Rpb24gcG9pbnRlciB3aXRoIHNpZ25hdHVyZSAke3NpZ25hdHVyZX06ICR7cmF3RnVuY3Rpb259YCl9cmV0dXJuIGZwfTtjbGFzcyBVbmJvdW5kVHlwZUVycm9yIGV4dGVuZHMgRXJyb3J7fXZhciBnZXRUeXBlTmFtZT10eXBlPT57dmFyIHB0cj1fX19nZXRUeXBlTmFtZSh0eXBlKTt2YXIgcnY9QXNjaWlUb1N0cmluZyhwdHIpO19mcmVlKHB0cik7cmV0dXJuIHJ2fTt2YXIgdGhyb3dVbmJvdW5kVHlwZUVycm9yPShtZXNzYWdlLHR5cGVzKT0+e3ZhciB1bmJvdW5kVHlwZXM9W107dmFyIHNlZW49e307ZnVuY3Rpb24gdmlzaXQodHlwZSl7aWYoc2Vlblt0eXBlXSl7cmV0dXJufWlmKHJlZ2lzdGVyZWRUeXBlc1t0eXBlXSl7cmV0dXJufWlmKHR5cGVEZXBlbmRlbmNpZXNbdHlwZV0pe3R5cGVEZXBlbmRlbmNpZXNbdHlwZV0uZm9yRWFjaCh2aXNpdCk7cmV0dXJufXVuYm91bmRUeXBlcy5wdXNoKHR5cGUpO3NlZW5bdHlwZV09dHJ1ZX10eXBlcy5mb3JFYWNoKHZpc2l0KTt0aHJvdyBuZXcgVW5ib3VuZFR5cGVFcnJvcihgJHttZXNzYWdlfTogYCt1bmJvdW5kVHlwZXMubWFwKGdldFR5cGVOYW1lKS5qb2luKFsiLCAiXSkpfTt2YXIgd2hlbkRlcGVuZGVudFR5cGVzQXJlUmVzb2x2ZWQ9KG15VHlwZXMsZGVwZW5kZW50VHlwZXMsZ2V0VHlwZUNvbnZlcnRlcnMpPT57bXlUeXBlcy5mb3JFYWNoKHR5cGU9PnR5cGVEZXBlbmRlbmNpZXNbdHlwZV09ZGVwZW5kZW50VHlwZXMpO2Z1bmN0aW9uIG9uQ29tcGxldGUodHlwZUNvbnZlcnRlcnMpe3ZhciBteVR5cGVDb252ZXJ0ZXJzPWdldFR5cGVDb252ZXJ0ZXJzKHR5cGVDb252ZXJ0ZXJzKTtpZihteVR5cGVDb252ZXJ0ZXJzLmxlbmd0aCE9PW15VHlwZXMubGVuZ3RoKXt0aHJvd0ludGVybmFsRXJyb3IoIk1pc21hdGNoZWQgdHlwZSBjb252ZXJ0ZXIgY291bnQiKX1mb3IodmFyIGk9MDtpPG15VHlwZXMubGVuZ3RoOysraSl7cmVnaXN0ZXJUeXBlKG15VHlwZXNbaV0sbXlUeXBlQ29udmVydGVyc1tpXSl9fXZhciB0eXBlQ29udmVydGVycz1uZXcgQXJyYXkoZGVwZW5kZW50VHlwZXMubGVuZ3RoKTt2YXIgdW5yZWdpc3RlcmVkVHlwZXM9W107dmFyIHJlZ2lzdGVyZWQ9MDtmb3IobGV0W2ksZHRdb2YgZGVwZW5kZW50VHlwZXMuZW50cmllcygpKXtpZihyZWdpc3RlcmVkVHlwZXMuaGFzT3duUHJvcGVydHkoZHQpKXt0eXBlQ29udmVydGVyc1tpXT1yZWdpc3RlcmVkVHlwZXNbZHRdfWVsc2V7dW5yZWdpc3RlcmVkVHlwZXMucHVzaChkdCk7aWYoIWF3YWl0aW5nRGVwZW5kZW5jaWVzLmhhc093blByb3BlcnR5KGR0KSl7YXdhaXRpbmdEZXBlbmRlbmNpZXNbZHRdPVtdfWF3YWl0aW5nRGVwZW5kZW5jaWVzW2R0XS5wdXNoKCgpPT57dHlwZUNvbnZlcnRlcnNbaV09cmVnaXN0ZXJlZFR5cGVzW2R0XTsrK3JlZ2lzdGVyZWQ7aWYocmVnaXN0ZXJlZD09PXVucmVnaXN0ZXJlZFR5cGVzLmxlbmd0aCl7b25Db21wbGV0ZSh0eXBlQ29udmVydGVycyl9fSl9fWlmKDA9PT11bnJlZ2lzdGVyZWRUeXBlcy5sZW5ndGgpe29uQ29tcGxldGUodHlwZUNvbnZlcnRlcnMpfX07dmFyIGdldEZ1bmN0aW9uTmFtZT1zaWduYXR1cmU9PntzaWduYXR1cmU9c2lnbmF0dXJlLnRyaW0oKTtjb25zdCBhcmdzSW5kZXg9c2lnbmF0dXJlLmluZGV4T2YoIigiKTtpZihhcmdzSW5kZXg9PT0tMSlyZXR1cm4gc2lnbmF0dXJlO2Fzc2VydChzaWduYXR1cmUuZW5kc1dpdGgoIikiKSwiUGFyZW50aGVzZXMgZm9yIGFyZ3VtZW50IG5hbWVzIHNob3VsZCBtYXRjaC4iKTtyZXR1cm4gc2lnbmF0dXJlLnNsaWNlKDAsYXJnc0luZGV4KX07dmFyIF9fZW1iaW5kX3JlZ2lzdGVyX2Z1bmN0aW9uPShuYW1lLGFyZ0NvdW50LHJhd0FyZ1R5cGVzQWRkcixzaWduYXR1cmUscmF3SW52b2tlcixmbixpc0FzeW5jLGlzTm9ubnVsbFJldHVybik9Pnt2YXIgYXJnVHlwZXM9aGVhcDMyVmVjdG9yVG9BcnJheShhcmdDb3VudCxyYXdBcmdUeXBlc0FkZHIpO25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtuYW1lPWdldEZ1bmN0aW9uTmFtZShuYW1lKTtyYXdJbnZva2VyPWVtYmluZF9fcmVxdWlyZUZ1bmN0aW9uKHNpZ25hdHVyZSxyYXdJbnZva2VyLGlzQXN5bmMpO2V4cG9zZVB1YmxpY1N5bWJvbChuYW1lLGZ1bmN0aW9uKCl7dGhyb3dVbmJvdW5kVHlwZUVycm9yKGBDYW5ub3QgY2FsbCAke25hbWV9IGR1ZSB0byB1bmJvdW5kIHR5cGVzYCxhcmdUeXBlcyl9LGFyZ0NvdW50LTEpO3doZW5EZXBlbmRlbnRUeXBlc0FyZVJlc29sdmVkKFtdLGFyZ1R5cGVzLGFyZ1R5cGVzPT57dmFyIGludm9rZXJBcmdzQXJyYXk9W2FyZ1R5cGVzWzBdLG51bGxdLmNvbmNhdChhcmdUeXBlcy5zbGljZSgxKSk7cmVwbGFjZVB1YmxpY1N5bWJvbChuYW1lLGNyYWZ0SW52b2tlckZ1bmN0aW9uKG5hbWUsaW52b2tlckFyZ3NBcnJheSxudWxsLHJhd0ludm9rZXIsZm4saXNBc3luYyksYXJnQ291bnQtMSk7cmV0dXJuW119KX07dmFyIF9fZW1iaW5kX3JlZ2lzdGVyX2ludGVnZXI9KHByaW1pdGl2ZVR5cGUsbmFtZSxzaXplLG1pblJhbmdlLG1heFJhbmdlKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtjb25zdCBpc1Vuc2lnbmVkVHlwZT1taW5SYW5nZT09PTA7bGV0IGZyb21XaXJlVHlwZT12YWx1ZT0+dmFsdWU7aWYoaXNVbnNpZ25lZFR5cGUpe3ZhciBiaXRzaGlmdD0zMi04KnNpemU7ZnJvbVdpcmVUeXBlPXZhbHVlPT52YWx1ZTw8Yml0c2hpZnQ+Pj5iaXRzaGlmdDttYXhSYW5nZT1mcm9tV2lyZVR5cGUobWF4UmFuZ2UpfXJlZ2lzdGVyVHlwZShwcmltaXRpdmVUeXBlLHtuYW1lLGZyb21XaXJlVHlwZSx0b1dpcmVUeXBlOihkZXN0cnVjdG9ycyx2YWx1ZSk9PntpZih0eXBlb2YgdmFsdWUhPSJudW1iZXIiJiZ0eXBlb2YgdmFsdWUhPSJib29sZWFuIil7dGhyb3cgbmV3IFR5cGVFcnJvcihgQ2Fubm90IGNvbnZlcnQgIiR7ZW1iaW5kUmVwcih2YWx1ZSl9IiB0byAke25hbWV9YCl9YXNzZXJ0SW50ZWdlclJhbmdlKG5hbWUsdmFsdWUsbWluUmFuZ2UsbWF4UmFuZ2UpO3JldHVybiB2YWx1ZX0scmVhZFZhbHVlRnJvbVBvaW50ZXI6aW50ZWdlclJlYWRWYWx1ZUZyb21Qb2ludGVyKG5hbWUsc2l6ZSxtaW5SYW5nZSE9PTApLGRlc3RydWN0b3JGdW5jdGlvbjpudWxsfSl9O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9tZW1vcnlfdmlldz0ocmF3VHlwZSxkYXRhVHlwZUluZGV4LG5hbWUpPT57dmFyIHR5cGVNYXBwaW5nPVtJbnQ4QXJyYXksVWludDhBcnJheSxJbnQxNkFycmF5LFVpbnQxNkFycmF5LEludDMyQXJyYXksVWludDMyQXJyYXksRmxvYXQzMkFycmF5LEZsb2F0NjRBcnJheSxCaWdJbnQ2NEFycmF5LEJpZ1VpbnQ2NEFycmF5XTt2YXIgVEE9dHlwZU1hcHBpbmdbZGF0YVR5cGVJbmRleF07ZnVuY3Rpb24gZGVjb2RlTWVtb3J5VmlldyhoYW5kbGUpe3ZhciBzaXplPShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtoYW5kbGU+PjJdO3ZhciBkYXRhPShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtoYW5kbGUrND4+Ml07cmV0dXJuIG5ldyBUQSgoZ3Jvd01lbVZpZXdzKCksSEVBUDgpLmJ1ZmZlcixkYXRhLHNpemUpfW5hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtyZWdpc3RlclR5cGUocmF3VHlwZSx7bmFtZSxmcm9tV2lyZVR5cGU6ZGVjb2RlTWVtb3J5VmlldyxyZWFkVmFsdWVGcm9tUG9pbnRlcjpkZWNvZGVNZW1vcnlWaWV3fSx7aWdub3JlRHVwbGljYXRlUmVnaXN0cmF0aW9uczp0cnVlfSl9O3ZhciBzdHJpbmdUb1VURjg9KHN0cixvdXRQdHIsbWF4Qnl0ZXNUb1dyaXRlKT0+e2Fzc2VydCh0eXBlb2YgbWF4Qnl0ZXNUb1dyaXRlPT0ibnVtYmVyIiwic3RyaW5nVG9VVEY4KHN0ciwgb3V0UHRyLCBtYXhCeXRlc1RvV3JpdGUpIGlzIG1pc3NpbmcgdGhlIHRoaXJkIHBhcmFtZXRlciB0aGF0IHNwZWNpZmllcyB0aGUgbGVuZ3RoIG9mIHRoZSBvdXRwdXQgYnVmZmVyISIpO3JldHVybiBzdHJpbmdUb1VURjhBcnJheShzdHIsKGdyb3dNZW1WaWV3cygpLEhFQVBVOCksb3V0UHRyLG1heEJ5dGVzVG9Xcml0ZSl9O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9zdGRfc3RyaW5nPShyYXdUeXBlLG5hbWUpPT57bmFtZT1Bc2NpaVRvU3RyaW5nKG5hbWUpO3ZhciBzdGRTdHJpbmdJc1VURjg9dHJ1ZTtyZWdpc3RlclR5cGUocmF3VHlwZSx7bmFtZSxmcm9tV2lyZVR5cGUodmFsdWUpe3ZhciBsZW5ndGg9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3ZhbHVlPj4yXTt2YXIgcGF5bG9hZD12YWx1ZSs0O3ZhciBzdHI7aWYoc3RkU3RyaW5nSXNVVEY4KXtzdHI9VVRGOFRvU3RyaW5nKHBheWxvYWQsbGVuZ3RoLHRydWUpfWVsc2V7c3RyPSIiO2Zvcih2YXIgaT0wO2k8bGVuZ3RoOysraSl7c3RyKz1TdHJpbmcuZnJvbUNoYXJDb2RlKChncm93TWVtVmlld3MoKSxIRUFQVTgpW3BheWxvYWQraV0pfX1fZnJlZSh2YWx1ZSk7cmV0dXJuIHN0cn0sdG9XaXJlVHlwZShkZXN0cnVjdG9ycyx2YWx1ZSl7aWYodmFsdWUgaW5zdGFuY2VvZiBBcnJheUJ1ZmZlcil7dmFsdWU9bmV3IFVpbnQ4QXJyYXkodmFsdWUpfXZhciBsZW5ndGg7dmFyIHZhbHVlSXNPZlR5cGVTdHJpbmc9dHlwZW9mIHZhbHVlPT0ic3RyaW5nIjtpZighKHZhbHVlSXNPZlR5cGVTdHJpbmd8fEFycmF5QnVmZmVyLmlzVmlldyh2YWx1ZSkmJnZhbHVlLkJZVEVTX1BFUl9FTEVNRU5UPT0xKSl7dGhyb3dCaW5kaW5nRXJyb3IoIkNhbm5vdCBwYXNzIG5vbi1zdHJpbmcgdG8gc3RkOjpzdHJpbmciKX1pZihzdGRTdHJpbmdJc1VURjgmJnZhbHVlSXNPZlR5cGVTdHJpbmcpe2xlbmd0aD1sZW5ndGhCeXRlc1VURjgodmFsdWUpfWVsc2V7bGVuZ3RoPXZhbHVlLmxlbmd0aH12YXIgYmFzZT1fbWFsbG9jKDQrbGVuZ3RoKzEpO3ZhciBwdHI9YmFzZSs0Oyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtiYXNlPj4yXT1sZW5ndGg7aWYodmFsdWVJc09mVHlwZVN0cmluZyl7aWYoc3RkU3RyaW5nSXNVVEY4KXtzdHJpbmdUb1VURjgodmFsdWUscHRyLGxlbmd0aCsxKX1lbHNle2Zvcih2YXIgaT0wO2k8bGVuZ3RoOysraSl7dmFyIGNoYXJDb2RlPXZhbHVlLmNoYXJDb2RlQXQoaSk7aWYoY2hhckNvZGU+MjU1KXtfZnJlZShiYXNlKTt0aHJvd0JpbmRpbmdFcnJvcigiU3RyaW5nIGhhcyBVVEYtMTYgY29kZSB1bml0cyB0aGF0IGRvIG5vdCBmaXQgaW4gOCBiaXRzIil9KGdyb3dNZW1WaWV3cygpLEhFQVBVOClbcHRyK2ldPWNoYXJDb2RlfX19ZWxzZXsoZ3Jvd01lbVZpZXdzKCksSEVBUFU4KS5zZXQodmFsdWUscHRyKX1pZihkZXN0cnVjdG9ycyE9PW51bGwpe2Rlc3RydWN0b3JzLnB1c2goX2ZyZWUsYmFzZSl9cmV0dXJuIGJhc2V9LHJlYWRWYWx1ZUZyb21Qb2ludGVyOnJlYWRQb2ludGVyLGRlc3RydWN0b3JGdW5jdGlvbihwdHIpe19mcmVlKHB0cil9fSl9O3ZhciBVVEYxNkRlY29kZXI9Z2xvYmFsVGhpcy5UZXh0RGVjb2Rlcj9uZXcgVGV4dERlY29kZXIoInV0Zi0xNmxlIik6dW5kZWZpbmVkO3ZhciBVVEYxNlRvU3RyaW5nPShwdHIsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKT0+e2Fzc2VydChwdHIlMj09MCwiUG9pbnRlciBwYXNzZWQgdG8gVVRGMTZUb1N0cmluZyBtdXN0IGJlIGFsaWduZWQgdG8gdHdvIGJ5dGVzISIpO3ZhciBpZHg9cHRyPj4xO3ZhciBlbmRJZHg9ZmluZFN0cmluZ0VuZCgoZ3Jvd01lbVZpZXdzKCksSEVBUFUxNiksaWR4LG1heEJ5dGVzVG9SZWFkLzIsaWdub3JlTnVsKTtpZihlbmRJZHgtaWR4PjE2JiZVVEYxNkRlY29kZXIpcmV0dXJuIFVURjE2RGVjb2Rlci5kZWNvZGUoKGdyb3dNZW1WaWV3cygpLEhFQVBVMTYpLnNsaWNlKGlkeCxlbmRJZHgpKTt2YXIgc3RyPSIiO2Zvcih2YXIgaT1pZHg7aTxlbmRJZHg7KytpKXt2YXIgY29kZVVuaXQ9KGdyb3dNZW1WaWV3cygpLEhFQVBVMTYpW2ldO3N0cis9U3RyaW5nLmZyb21DaGFyQ29kZShjb2RlVW5pdCl9cmV0dXJuIHN0cn07dmFyIHN0cmluZ1RvVVRGMTY9KHN0cixvdXRQdHIsbWF4Qnl0ZXNUb1dyaXRlKT0+e2Fzc2VydChvdXRQdHIlMj09MCwiUG9pbnRlciBwYXNzZWQgdG8gc3RyaW5nVG9VVEYxNiBtdXN0IGJlIGFsaWduZWQgdG8gdHdvIGJ5dGVzISIpO2Fzc2VydCh0eXBlb2YgbWF4Qnl0ZXNUb1dyaXRlPT0ibnVtYmVyIiwic3RyaW5nVG9VVEYxNihzdHIsIG91dFB0ciwgbWF4Qnl0ZXNUb1dyaXRlKSBpcyBtaXNzaW5nIHRoZSB0aGlyZCBwYXJhbWV0ZXIgdGhhdCBzcGVjaWZpZXMgdGhlIGxlbmd0aCBvZiB0aGUgb3V0cHV0IGJ1ZmZlciEiKTttYXhCeXRlc1RvV3JpdGU/Pz0yMTQ3NDgzNjQ3O2lmKG1heEJ5dGVzVG9Xcml0ZTwyKXJldHVybiAwO21heEJ5dGVzVG9Xcml0ZS09Mjt2YXIgc3RhcnRQdHI9b3V0UHRyO3ZhciBudW1DaGFyc1RvV3JpdGU9bWF4Qnl0ZXNUb1dyaXRlPHN0ci5sZW5ndGgqMj9tYXhCeXRlc1RvV3JpdGUvMjpzdHIubGVuZ3RoO2Zvcih2YXIgaT0wO2k8bnVtQ2hhcnNUb1dyaXRlOysraSl7dmFyIGNvZGVVbml0PXN0ci5jaGFyQ29kZUF0KGkpOyhncm93TWVtVmlld3MoKSxIRUFQMTYpW291dFB0cj4+MV09Y29kZVVuaXQ7b3V0UHRyKz0yfShncm93TWVtVmlld3MoKSxIRUFQMTYpW291dFB0cj4+MV09MDtyZXR1cm4gb3V0UHRyLXN0YXJ0UHRyfTt2YXIgbGVuZ3RoQnl0ZXNVVEYxNj1zdHI9PnN0ci5sZW5ndGgqMjt2YXIgVVRGMzJUb1N0cmluZz0ocHRyLG1heEJ5dGVzVG9SZWFkLGlnbm9yZU51bCk9Pnthc3NlcnQocHRyJTQ9PTAsIlBvaW50ZXIgcGFzc2VkIHRvIFVURjMyVG9TdHJpbmcgbXVzdCBiZSBhbGlnbmVkIHRvIGZvdXIgYnl0ZXMhIik7dmFyIHN0cj0iIjt2YXIgc3RhcnRJZHg9cHRyPj4yO2Zvcih2YXIgaT0wOyEoaT49bWF4Qnl0ZXNUb1JlYWQvNCk7aSsrKXt2YXIgdXRmMzI9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3N0YXJ0SWR4K2ldO2lmKCF1dGYzMiYmIWlnbm9yZU51bClicmVhaztzdHIrPVN0cmluZy5mcm9tQ29kZVBvaW50KHV0ZjMyKX1yZXR1cm4gc3RyfTt2YXIgc3RyaW5nVG9VVEYzMj0oc3RyLG91dFB0cixtYXhCeXRlc1RvV3JpdGUpPT57YXNzZXJ0KG91dFB0ciU0PT0wLCJQb2ludGVyIHBhc3NlZCB0byBzdHJpbmdUb1VURjMyIG11c3QgYmUgYWxpZ25lZCB0byBmb3VyIGJ5dGVzISIpO2Fzc2VydCh0eXBlb2YgbWF4Qnl0ZXNUb1dyaXRlPT0ibnVtYmVyIiwic3RyaW5nVG9VVEYzMihzdHIsIG91dFB0ciwgbWF4Qnl0ZXNUb1dyaXRlKSBpcyBtaXNzaW5nIHRoZSB0aGlyZCBwYXJhbWV0ZXIgdGhhdCBzcGVjaWZpZXMgdGhlIGxlbmd0aCBvZiB0aGUgb3V0cHV0IGJ1ZmZlciEiKTttYXhCeXRlc1RvV3JpdGU/Pz0yMTQ3NDgzNjQ3O2lmKG1heEJ5dGVzVG9Xcml0ZTw0KXJldHVybiAwO3ZhciBzdGFydFB0cj1vdXRQdHI7dmFyIGVuZFB0cj1zdGFydFB0cittYXhCeXRlc1RvV3JpdGUtNDtmb3IodmFyIGk9MDtpPHN0ci5sZW5ndGg7KytpKXt2YXIgY29kZVBvaW50PXN0ci5jb2RlUG9pbnRBdChpKTtpZihjb2RlUG9pbnQ+NjU1MzUpe2krK30oZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVtvdXRQdHI+PjJdPWNvZGVQb2ludDtvdXRQdHIrPTQ7aWYob3V0UHRyKzQ+ZW5kUHRyKWJyZWFrfShncm93TWVtVmlld3MoKSxIRUFQMzIpW291dFB0cj4+Ml09MDtyZXR1cm4gb3V0UHRyLXN0YXJ0UHRyfTt2YXIgbGVuZ3RoQnl0ZXNVVEYzMj1zdHI9Pnt2YXIgbGVuPTA7Zm9yKHZhciBpPTA7aTxzdHIubGVuZ3RoOysraSl7dmFyIGNvZGVQb2ludD1zdHIuY29kZVBvaW50QXQoaSk7aWYoY29kZVBvaW50PjY1NTM1KXtpKyt9bGVuKz00fXJldHVybiBsZW59O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9zdGRfd3N0cmluZz0ocmF3VHlwZSxjaGFyU2l6ZSxuYW1lKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTt2YXIgZGVjb2RlU3RyaW5nLGVuY29kZVN0cmluZyxsZW5ndGhCeXRlc1VURjtpZihjaGFyU2l6ZT09PTIpe2RlY29kZVN0cmluZz1VVEYxNlRvU3RyaW5nO2VuY29kZVN0cmluZz1zdHJpbmdUb1VURjE2O2xlbmd0aEJ5dGVzVVRGPWxlbmd0aEJ5dGVzVVRGMTZ9ZWxzZXthc3NlcnQoY2hhclNpemU9PT00LCJvbmx5IDItYnl0ZSBhbmQgNC1ieXRlIHN0cmluZ3MgYXJlIGN1cnJlbnRseSBzdXBwb3J0ZWQiKTtkZWNvZGVTdHJpbmc9VVRGMzJUb1N0cmluZztlbmNvZGVTdHJpbmc9c3RyaW5nVG9VVEYzMjtsZW5ndGhCeXRlc1VURj1sZW5ndGhCeXRlc1VURjMyfXJlZ2lzdGVyVHlwZShyYXdUeXBlLHtuYW1lLGZyb21XaXJlVHlwZTp2YWx1ZT0+e3ZhciBsZW5ndGg9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3ZhbHVlPj4yXTt2YXIgc3RyPWRlY29kZVN0cmluZyh2YWx1ZSs0LGxlbmd0aCpjaGFyU2l6ZSx0cnVlKTtfZnJlZSh2YWx1ZSk7cmV0dXJuIHN0cn0sdG9XaXJlVHlwZTooZGVzdHJ1Y3RvcnMsdmFsdWUpPT57aWYoISh0eXBlb2YgdmFsdWU9PSJzdHJpbmciKSl7dGhyb3dCaW5kaW5nRXJyb3IoYENhbm5vdCBwYXNzIG5vbi1zdHJpbmcgdG8gQysrIHN0cmluZyB0eXBlICR7bmFtZX1gKX12YXIgbGVuZ3RoPWxlbmd0aEJ5dGVzVVRGKHZhbHVlKTt2YXIgcHRyPV9tYWxsb2MoNCtsZW5ndGgrY2hhclNpemUpOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwdHI+PjJdPWxlbmd0aC9jaGFyU2l6ZTtlbmNvZGVTdHJpbmcodmFsdWUscHRyKzQsbGVuZ3RoK2NoYXJTaXplKTtpZihkZXN0cnVjdG9ycyE9PW51bGwpe2Rlc3RydWN0b3JzLnB1c2goX2ZyZWUscHRyKX1yZXR1cm4gcHRyfSxyZWFkVmFsdWVGcm9tUG9pbnRlcjpyZWFkUG9pbnRlcixkZXN0cnVjdG9yRnVuY3Rpb24ocHRyKXtfZnJlZShwdHIpfX0pfTt2YXIgX19lbWJpbmRfcmVnaXN0ZXJfdm9pZD0ocmF3VHlwZSxuYW1lKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtyZWdpc3RlclR5cGUocmF3VHlwZSx7aXNWb2lkOnRydWUsbmFtZSxmcm9tV2lyZVR5cGU6KCk9PnVuZGVmaW5lZCx0b1dpcmVUeXBlOihkZXN0cnVjdG9ycyxvKT0+dW5kZWZpbmVkfSl9O3ZhciBfX2Vtc2NyaXB0ZW5faW5pdF9tYWluX3RocmVhZF9qcz10Yj0+e19fZW1zY3JpcHRlbl90aHJlYWRfaW5pdCh0YiwhRU5WSVJPTk1FTlRfSVNfV09SS0VSLDEsIUVOVklST05NRU5UX0lTX1dFQiw2NTUzNixmYWxzZSk7UFRocmVhZC50aHJlYWRJbml0VExTKCl9O3ZhciBoYW5kbGVFeGNlcHRpb249ZT0+e2lmKGUgaW5zdGFuY2VvZiBFeGl0U3RhdHVzfHxlPT0idW53aW5kIil7cmV0dXJuIEVYSVRTVEFUVVN9Y2hlY2tTdGFja0Nvb2tpZSgpO2lmKGUgaW5zdGFuY2VvZiBXZWJBc3NlbWJseS5SdW50aW1lRXJyb3Ipe2lmKF9lbXNjcmlwdGVuX3N0YWNrX2dldF9jdXJyZW50KCk8PTApe2VycigiU3RhY2sgb3ZlcmZsb3cgZGV0ZWN0ZWQuICBZb3UgY2FuIHRyeSBpbmNyZWFzaW5nIC1zU1RBQ0tfU0laRSAoY3VycmVudGx5IHNldCB0byA2NTUzNikiKX19cXVpdF8oMSxlKX07dmFyIG1heWJlRXhpdD0oKT0+e2lmKCFrZWVwUnVudGltZUFsaXZlKCkpe3RyeXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtpZihfcHRocmVhZF9zZWxmKCkpX19lbXNjcmlwdGVuX3RocmVhZF9leGl0KEVYSVRTVEFUVVMpO3JldHVybn1fZXhpdChFWElUU1RBVFVTKX1jYXRjaChlKXtoYW5kbGVFeGNlcHRpb24oZSl9fX07dmFyIGNhbGxVc2VyQ2FsbGJhY2s9ZnVuYz0+e2lmKEFCT1JUKXtlcnIoInVzZXIgY2FsbGJhY2sgdHJpZ2dlcmVkIGFmdGVyIHJ1bnRpbWUgZXhpdGVkIG9yIGFwcGxpY2F0aW9uIGFib3J0ZWQuICBJZ25vcmluZy4iKTtyZXR1cm59dHJ5e2Z1bmMoKTttYXliZUV4aXQoKX1jYXRjaChlKXtoYW5kbGVFeGNlcHRpb24oZSl9fTt2YXIgX19lbXNjcmlwdGVuX3RocmVhZF9tYWlsYm94X2F3YWl0PXB0aHJlYWRfcHRyPT57aWYoQXRvbWljcy53YWl0QXN5bmMpe3ZhciB3YWl0PUF0b21pY3Mud2FpdEFzeW5jKChncm93TWVtVmlld3MoKSxIRUFQMzIpLHB0aHJlYWRfcHRyPj4yLHB0aHJlYWRfcHRyKTthc3NlcnQod2FpdC5hc3luYyk7d2FpdC52YWx1ZS50aGVuKGNoZWNrTWFpbGJveCk7dmFyIHdhaXRpbmdBc3luYz1wdGhyZWFkX3B0cisxMjg7QXRvbWljcy5zdG9yZSgoZ3Jvd01lbVZpZXdzKCksSEVBUDMyKSx3YWl0aW5nQXN5bmM+PjIsMSl9fTt2YXIgY2hlY2tNYWlsYm94PSgpPT5jYWxsVXNlckNhbGxiYWNrKCgpPT57dmFyIHB0aHJlYWRfcHRyPV9wdGhyZWFkX3NlbGYoKTtpZihwdGhyZWFkX3B0cil7X19lbXNjcmlwdGVuX3RocmVhZF9tYWlsYm94X2F3YWl0KHB0aHJlYWRfcHRyKTtfX2Vtc2NyaXB0ZW5fY2hlY2tfbWFpbGJveCgpfX0pO3ZhciBfX2Vtc2NyaXB0ZW5fbm90aWZ5X21haWxib3hfcG9zdG1lc3NhZ2U9KHRhcmdldFRocmVhZCxjdXJyVGhyZWFkSWQpPT57aWYodGFyZ2V0VGhyZWFkPT1jdXJyVGhyZWFkSWQpe3NldFRpbWVvdXQoY2hlY2tNYWlsYm94KX1lbHNlIGlmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe3Bvc3RNZXNzYWdlKHt0YXJnZXRUaHJlYWQsY21kOiJjaGVja01haWxib3gifSl9ZWxzZXt2YXIgd29ya2VyPVBUaHJlYWQucHRocmVhZHNbdGFyZ2V0VGhyZWFkXTtpZighd29ya2VyKXtlcnIoYENhbm5vdCBzZW5kIG1lc3NhZ2UgdG8gdGhyZWFkIHdpdGggSUQgJHt0YXJnZXRUaHJlYWR9LCB1bmtub3duIHRocmVhZCBJRCFgKTtyZXR1cm59d29ya2VyLnBvc3RNZXNzYWdlKHtjbWQ6ImNoZWNrTWFpbGJveCJ9KX19O3ZhciBwcm94aWVkSlNDYWxsQXJncz1bXTt2YXIgX19lbXNjcmlwdGVuX3JlY2VpdmVfb25fbWFpbl90aHJlYWRfanM9KGZ1bmNJbmRleCxlbUFzbUFkZHIsY2FsbGluZ1RocmVhZCxudW1DYWxsQXJncyxhcmdzKT0+e251bUNhbGxBcmdzLz0yO3Byb3hpZWRKU0NhbGxBcmdzLmxlbmd0aD1udW1DYWxsQXJnczt2YXIgYj1hcmdzPj4zO2Zvcih2YXIgaT0wO2k8bnVtQ2FsbEFyZ3M7aSsrKXtpZigoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtiKzIqaV0pe3Byb3hpZWRKU0NhbGxBcmdzW2ldPShncm93TWVtVmlld3MoKSxIRUFQNjQpW2IrMippKzFdfWVsc2V7cHJveGllZEpTQ2FsbEFyZ3NbaV09KGdyb3dNZW1WaWV3cygpLEhFQVBGNjQpW2IrMippKzFdfX12YXIgZnVuYz1lbUFzbUFkZHI/QVNNX0NPTlNUU1tlbUFzbUFkZHJdOnByb3hpZWRGdW5jdGlvblRhYmxlW2Z1bmNJbmRleF07YXNzZXJ0KCEoZnVuY0luZGV4JiZlbUFzbUFkZHIpKTthc3NlcnQoZnVuYy5sZW5ndGg9PW51bUNhbGxBcmdzLCJDYWxsIGFyZ3MgbWlzbWF0Y2ggaW4gX2Vtc2NyaXB0ZW5fcmVjZWl2ZV9vbl9tYWluX3RocmVhZF9qcyIpO1BUaHJlYWQuY3VycmVudFByb3hpZWRPcGVyYXRpb25DYWxsZXJUaHJlYWQ9Y2FsbGluZ1RocmVhZDt2YXIgcnRuPWZ1bmMoLi4ucHJveGllZEpTQ2FsbEFyZ3MpO1BUaHJlYWQuY3VycmVudFByb3hpZWRPcGVyYXRpb25DYWxsZXJUaHJlYWQ9MDthc3NlcnQodHlwZW9mIHJ0biE9ImJpZ2ludCIpO3JldHVybiBydG59O3ZhciBfX2Vtc2NyaXB0ZW5fdGhyZWFkX2NsZWFudXA9dGhyZWFkPT57aWYoIUVOVklST05NRU5UX0lTX1BUSFJFQUQpY2xlYW51cFRocmVhZCh0aHJlYWQpO2Vsc2UgcG9zdE1lc3NhZ2Uoe2NtZDoiY2xlYW51cFRocmVhZCIsdGhyZWFkfSl9O3ZhciBfX2Vtc2NyaXB0ZW5fdGhyZWFkX3NldF9zdHJvbmdyZWY9dGhyZWFkPT57fTt2YXIgSU5UNTNfTUFYPTkwMDcxOTkyNTQ3NDA5OTI7dmFyIElOVDUzX01JTj0tOTAwNzE5OTI1NDc0MDk5Mjt2YXIgYmlnaW50VG9JNTNDaGVja2VkPW51bT0+bnVtPElOVDUzX01JTnx8bnVtPklOVDUzX01BWD9OYU46TnVtYmVyKG51bSk7ZnVuY3Rpb24gX19tbWFwX2pzKGxlbixwcm90LGZsYWdzLGZkLG9mZnNldCxhbGxvY2F0ZWQsYWRkcil7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMTAsMCwxLGxlbixwcm90LGZsYWdzLGZkLG9mZnNldCxhbGxvY2F0ZWQsYWRkcik7b2Zmc2V0PWJpZ2ludFRvSTUzQ2hlY2tlZChvZmZzZXQpO3RyeXthc3NlcnQoIWlzTmFOKG9mZnNldCkpO3ZhciBzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGZkKTt2YXIgcmVzPUZTLm1tYXAoc3RyZWFtLGxlbixvZmZzZXQscHJvdCxmbGFncyk7dmFyIHB0cj1yZXMucHRyOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2FsbG9jYXRlZD4+Ml09cmVzLmFsbG9jYXRlZDsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYWRkcj4+Ml09cHRyO3JldHVybiAwfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX1mdW5jdGlvbiBfX211bm1hcF9qcyhhZGRyLGxlbixwcm90LGZsYWdzLGZkLG9mZnNldCl7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMTEsMCwxLGFkZHIsbGVuLHByb3QsZmxhZ3MsZmQsb2Zmc2V0KTtvZmZzZXQ9YmlnaW50VG9JNTNDaGVja2VkKG9mZnNldCk7dHJ5e3ZhciBzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGZkKTtpZihwcm90JjIpe1NZU0NBTExTLmRvTXN5bmMoYWRkcixzdHJlYW0sbGVuLGZsYWdzLG9mZnNldCl9fWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX12YXIgX190enNldF9qcz0odGltZXpvbmUsZGF5bGlnaHQsc3RkX25hbWUsZHN0X25hbWUpPT57dmFyIGN1cnJlbnRZZWFyPShuZXcgRGF0ZSkuZ2V0RnVsbFllYXIoKTt2YXIgd2ludGVyPW5ldyBEYXRlKGN1cnJlbnRZZWFyLDAsMSk7dmFyIHN1bW1lcj1uZXcgRGF0ZShjdXJyZW50WWVhciw2LDEpO3ZhciB3aW50ZXJPZmZzZXQ9d2ludGVyLmdldFRpbWV6b25lT2Zmc2V0KCk7dmFyIHN1bW1lck9mZnNldD1zdW1tZXIuZ2V0VGltZXpvbmVPZmZzZXQoKTt2YXIgc3RkVGltZXpvbmVPZmZzZXQ9TWF0aC5tYXgod2ludGVyT2Zmc2V0LHN1bW1lck9mZnNldCk7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3RpbWV6b25lPj4yXT1zdGRUaW1lem9uZU9mZnNldCo2MDsoZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVtkYXlsaWdodD4+Ml09TnVtYmVyKHdpbnRlck9mZnNldCE9c3VtbWVyT2Zmc2V0KTt2YXIgZXh0cmFjdFpvbmU9dGltZXpvbmVPZmZzZXQ9Pnt2YXIgc2lnbj10aW1lem9uZU9mZnNldD49MD8iLSI6IisiO3ZhciBhYnNPZmZzZXQ9TWF0aC5hYnModGltZXpvbmVPZmZzZXQpO3ZhciBob3Vycz1TdHJpbmcoTWF0aC5mbG9vcihhYnNPZmZzZXQvNjApKS5wYWRTdGFydCgyLCIwIik7dmFyIG1pbnV0ZXM9U3RyaW5nKGFic09mZnNldCU2MCkucGFkU3RhcnQoMiwiMCIpO3JldHVybmBVVEMke3NpZ259JHtob3Vyc30ke21pbnV0ZXN9YH07dmFyIHdpbnRlck5hbWU9ZXh0cmFjdFpvbmUod2ludGVyT2Zmc2V0KTt2YXIgc3VtbWVyTmFtZT1leHRyYWN0Wm9uZShzdW1tZXJPZmZzZXQpO2Fzc2VydCh3aW50ZXJOYW1lKTthc3NlcnQoc3VtbWVyTmFtZSk7YXNzZXJ0KGxlbmd0aEJ5dGVzVVRGOCh3aW50ZXJOYW1lKTw9MTYsYHRpbWV6b25lIG5hbWUgdHJ1bmNhdGVkIHRvIGZpdCBpbiBUWk5BTUVfTUFYICgke3dpbnRlck5hbWV9KWApO2Fzc2VydChsZW5ndGhCeXRlc1VURjgoc3VtbWVyTmFtZSk8PTE2LGB0aW1lem9uZSBuYW1lIHRydW5jYXRlZCB0byBmaXQgaW4gVFpOQU1FX01BWCAoJHtzdW1tZXJOYW1lfSlgKTtpZihzdW1tZXJPZmZzZXQ8d2ludGVyT2Zmc2V0KXtzdHJpbmdUb1VURjgod2ludGVyTmFtZSxzdGRfbmFtZSwxNyk7c3RyaW5nVG9VVEY4KHN1bW1lck5hbWUsZHN0X25hbWUsMTcpfWVsc2V7c3RyaW5nVG9VVEY4KHdpbnRlck5hbWUsZHN0X25hbWUsMTcpO3N0cmluZ1RvVVRGOChzdW1tZXJOYW1lLHN0ZF9uYW1lLDE3KX19O3ZhciBfZW1zY3JpcHRlbl9nZXRfbm93PSgpPT5wZXJmb3JtYW5jZS50aW1lT3JpZ2luK3BlcmZvcm1hbmNlLm5vdygpO3ZhciBfZW1zY3JpcHRlbl9kYXRlX25vdz0oKT0+RGF0ZS5ub3coKTt2YXIgbm93SXNNb25vdG9uaWM9MTt2YXIgY2hlY2tXYXNpQ2xvY2s9Y2xvY2tfaWQ9PmNsb2NrX2lkPj0wJiZjbG9ja19pZDw9MztmdW5jdGlvbiBfY2xvY2tfdGltZV9nZXQoY2xrX2lkLGlnbm9yZWRfcHJlY2lzaW9uLHB0aW1lKXtpZ25vcmVkX3ByZWNpc2lvbj1iaWdpbnRUb0k1M0NoZWNrZWQoaWdub3JlZF9wcmVjaXNpb24pO2lmKCFjaGVja1dhc2lDbG9jayhjbGtfaWQpKXtyZXR1cm4gMjh9dmFyIG5vdztpZihjbGtfaWQ9PT0wKXtub3c9X2Vtc2NyaXB0ZW5fZGF0ZV9ub3coKX1lbHNlIGlmKG5vd0lzTW9ub3RvbmljKXtub3c9X2Vtc2NyaXB0ZW5fZ2V0X25vdygpfWVsc2V7cmV0dXJuIDUyfXZhciBuc2VjPU1hdGgucm91bmQobm93KjFlMyoxZTMpOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW3B0aW1lPj4zXT1CaWdJbnQobnNlYyk7cmV0dXJuIDB9dmFyIHJlYWRFbUFzbUFyZ3NBcnJheT1bXTt2YXIgcmVhZEVtQXNtQXJncz0oc2lnUHRyLGJ1Zik9Pnthc3NlcnQoQXJyYXkuaXNBcnJheShyZWFkRW1Bc21BcmdzQXJyYXkpKTthc3NlcnQoYnVmJTE2PT0wKTtyZWFkRW1Bc21BcmdzQXJyYXkubGVuZ3RoPTA7dmFyIGNoO3doaWxlKGNoPShncm93TWVtVmlld3MoKSxIRUFQVTgpW3NpZ1B0cisrXSl7dmFyIGNocj1TdHJpbmcuZnJvbUNoYXJDb2RlKGNoKTt2YXIgdmFsaWRDaGFycz1bImQiLCJmIiwiaSIsInAiXTt2YWxpZENoYXJzLnB1c2goImoiKTthc3NlcnQodmFsaWRDaGFycy5pbmNsdWRlcyhjaHIpLGBJbnZhbGlkIGNoYXJhY3RlciAke2NofSgiJHtjaHJ9IikgaW4gcmVhZEVtQXNtQXJncyEgVXNlIG9ubHkgWyR7dmFsaWRDaGFyc31dLCBhbmQgZG8gbm90IHNwZWNpZnkgInYiIGZvciB2b2lkIHJldHVybiBhcmd1bWVudC5gKTt2YXIgd2lkZT1jaCE9MTA1O3dpZGUmPWNoIT0xMTI7YnVmKz13aWRlJiZidWYlOD80OjA7cmVhZEVtQXNtQXJnc0FycmF5LnB1c2goY2g9PTExMj8oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmPj4yXTpjaD09MTA2Pyhncm93TWVtVmlld3MoKSxIRUFQNjQpW2J1Zj4+M106Y2g9PTEwNT8oZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVtidWY+PjJdOihncm93TWVtVmlld3MoKSxIRUFQRjY0KVtidWY+PjNdKTtidWYrPXdpZGU/ODo0fXJldHVybiByZWFkRW1Bc21BcmdzQXJyYXl9O3ZhciBydW5FbUFzbUZ1bmN0aW9uPShjb2RlLHNpZ1B0cixhcmdidWYpPT57dmFyIGFyZ3M9cmVhZEVtQXNtQXJncyhzaWdQdHIsYXJnYnVmKTthc3NlcnQoQVNNX0NPTlNUUy5oYXNPd25Qcm9wZXJ0eShjb2RlKSxgTm8gRU1fQVNNIGNvbnN0YW50IGZvdW5kIGF0IGFkZHJlc3MgJHtjb2RlfS4gIFRoZSBsb2FkZWQgV2ViQXNzZW1ibHkgZmlsZSBpcyBsaWtlbHkgb3V0IG9mIHN5bmMgd2l0aCB0aGUgZ2VuZXJhdGVkIEphdmFTY3JpcHQuYCk7cmV0dXJuIEFTTV9DT05TVFNbY29kZV0oLi4uYXJncyl9O3ZhciBfZW1zY3JpcHRlbl9hc21fY29uc3RfaW50PShjb2RlLHNpZ1B0cixhcmdidWYpPT5ydW5FbUFzbUZ1bmN0aW9uKGNvZGUsc2lnUHRyLGFyZ2J1Zik7dmFyIF9lbXNjcmlwdGVuX2NoZWNrX2Jsb2NraW5nX2FsbG93ZWQ9KCk9PntpZihFTlZJUk9OTUVOVF9JU19XT1JLRVIpcmV0dXJuO3dhcm5PbmNlKCJCbG9ja2luZyBvbiB0aGUgbWFpbiB0aHJlYWQgaXMgdmVyeSBkYW5nZXJvdXMsIHNlZSBodHRwczovL2Vtc2NyaXB0ZW4ub3JnL2RvY3MvcG9ydGluZy9wdGhyZWFkcy5odG1sI2Jsb2NraW5nLW9uLXRoZS1tYWluLWJyb3dzZXItdGhyZWFkIil9O3ZhciBfZW1zY3JpcHRlbl9lcnJuPShzdHIsbGVuKT0+ZXJyKFVURjhUb1N0cmluZyhzdHIsbGVuKSk7dmFyIHJ1bnRpbWVLZWVwYWxpdmVQdXNoPSgpPT57cnVudGltZUtlZXBhbGl2ZUNvdW50ZXIrPTF9O3ZhciBfZW1zY3JpcHRlbl9leGl0X3dpdGhfbGl2ZV9ydW50aW1lPSgpPT57cnVudGltZUtlZXBhbGl2ZVB1c2goKTt0aHJvdyJ1bndpbmQifTt2YXIgZ2V0SGVhcE1heD0oKT0+MjE0NzQ4MzY0ODt2YXIgX2Vtc2NyaXB0ZW5fZ2V0X2hlYXBfbWF4PSgpPT5nZXRIZWFwTWF4KCk7dmFyIF9lbXNjcmlwdGVuX251bV9sb2dpY2FsX2NvcmVzPSgpPT5uYXZpZ2F0b3JbImhhcmR3YXJlQ29uY3VycmVuY3kiXTt2YXIgVU5XSU5EX0NBQ0hFPXt9O3ZhciBzdHJpbmdUb05ld1VURjg9c3RyPT57dmFyIHNpemU9bGVuZ3RoQnl0ZXNVVEY4KHN0cikrMTt2YXIgcmV0PV9tYWxsb2Moc2l6ZSk7aWYocmV0KXN0cmluZ1RvVVRGOChzdHIscmV0LHNpemUpO3JldHVybiByZXR9O3ZhciBjb252ZXJ0RnJhbWVUb1BDPWZyYW1lPT57dmFyIG1hdGNoO2lmKG1hdGNoPS9cYndhc20tZnVuY3Rpb25cW1xkK1xdOigweFswLTlhLWZdKykvLmV4ZWMoZnJhbWUpKXtyZXR1cm4rbWF0Y2hbMV19ZWxzZSBpZihtYXRjaD0vXGJ3YXNtLWZ1bmN0aW9uXFsoXGQrKVxdOihcZCspLy5leGVjKGZyYW1lKSl7d2Fybk9uY2UoImxlZ2FjeSBiYWNrdHJhY2UgZm9ybWF0IGRldGVjdGVkLCB0aGlzIHZlcnNpb24gb2YgdjggaXMgbm8gbG9uZ2VyIHN1cHBvcnRlZCBieSB0aGUgZW1zY3JpcHRlbiBiYWNrdHJhY2UgbWVjaGFuaXNtIil9ZWxzZSBpZihtYXRjaD0vOihcZCspOlxkKyg/OlwpfCQpLy5leGVjKGZyYW1lKSl7cmV0dXJuIDIxNDc0ODM2NDh8K21hdGNoWzFdfXJldHVybiAwfTt2YXIgc2F2ZUluVW53aW5kQ2FjaGU9Y2FsbHN0YWNrPT57Zm9yKHZhciBsaW5lIG9mIGNhbGxzdGFjayl7dmFyIHBjPWNvbnZlcnRGcmFtZVRvUEMobGluZSk7aWYocGMpe1VOV0lORF9DQUNIRVtwY109bGluZX19fTt2YXIganNTdGFja1RyYWNlPSgpPT4obmV3IEVycm9yKS5zdGFjay50b1N0cmluZygpO3ZhciBfZW1zY3JpcHRlbl9zdGFja19zbmFwc2hvdD0oKT0+e3ZhciBjYWxsc3RhY2s9anNTdGFja1RyYWNlKCkuc3BsaXQoIlxuIik7aWYoY2FsbHN0YWNrWzBdPT0iRXJyb3IiKXtjYWxsc3RhY2suc2hpZnQoKX1zYXZlSW5VbndpbmRDYWNoZShjYWxsc3RhY2spO1VOV0lORF9DQUNIRS5sYXN0X2FkZHI9Y29udmVydEZyYW1lVG9QQyhjYWxsc3RhY2tbM10pO1VOV0lORF9DQUNIRS5sYXN0X3N0YWNrPWNhbGxzdGFjaztyZXR1cm4gVU5XSU5EX0NBQ0hFLmxhc3RfYWRkcn07dmFyIF9lbXNjcmlwdGVuX3BjX2dldF9mdW5jdGlvbj1wYz0+e3ZhciBmcmFtZT1VTldJTkRfQ0FDSEVbcGNdO2lmKCFmcmFtZSlyZXR1cm4gMDt2YXIgbmFtZTt2YXIgbWF0Y2g7aWYobWF0Y2g9L15ccythdCAuKlwud2FzbVwuKC4qKSBcKC4qXCkkLy5leGVjKGZyYW1lKSl7bmFtZT1tYXRjaFsxXX1lbHNlIGlmKG1hdGNoPS9eXHMrYXQgKC4qKSBcKC4qXCkkLy5leGVjKGZyYW1lKSl7bmFtZT1tYXRjaFsxXX1lbHNlIGlmKG1hdGNoPS9eKC4rPylALy5leGVjKGZyYW1lKSl7bmFtZT1tYXRjaFsxXX1lbHNle3JldHVybiAwfV9mcmVlKF9lbXNjcmlwdGVuX3BjX2dldF9mdW5jdGlvbi5yZXQ/PzApO19lbXNjcmlwdGVuX3BjX2dldF9mdW5jdGlvbi5yZXQ9c3RyaW5nVG9OZXdVVEY4KG5hbWUpO3JldHVybiBfZW1zY3JpcHRlbl9wY19nZXRfZnVuY3Rpb24ucmV0fTt2YXIgZ3Jvd01lbW9yeT1zaXplPT57dmFyIG9sZEhlYXBTaXplPXdhc21NZW1vcnkuYnVmZmVyLmJ5dGVMZW5ndGg7dmFyIHBhZ2VzPShzaXplLW9sZEhlYXBTaXplKzY1NTM1KS82NTUzNnwwO3RyeXt3YXNtTWVtb3J5Lmdyb3cocGFnZXMpO3VwZGF0ZU1lbW9yeVZpZXdzKCk7cmV0dXJuIDF9Y2F0Y2goZSl7ZXJyKGBncm93TWVtb3J5OiBBdHRlbXB0ZWQgdG8gZ3JvdyBoZWFwIGZyb20gJHtvbGRIZWFwU2l6ZX0gYnl0ZXMgdG8gJHtzaXplfSBieXRlcywgYnV0IGdvdCBlcnJvcjogJHtlfWApfX07dmFyIF9lbXNjcmlwdGVuX3Jlc2l6ZV9oZWFwPXJlcXVlc3RlZFNpemU9Pnt2YXIgb2xkU2l6ZT0oZ3Jvd01lbVZpZXdzKCksSEVBUFU4KS5sZW5ndGg7cmVxdWVzdGVkU2l6ZT4+Pj0wO2lmKHJlcXVlc3RlZFNpemU8PW9sZFNpemUpe3JldHVybiBmYWxzZX12YXIgbWF4SGVhcFNpemU9Z2V0SGVhcE1heCgpO2lmKHJlcXVlc3RlZFNpemU+bWF4SGVhcFNpemUpe2VycihgQ2Fubm90IGVubGFyZ2UgbWVtb3J5LCByZXF1ZXN0ZWQgJHtyZXF1ZXN0ZWRTaXplfSBieXRlcywgYnV0IHRoZSBsaW1pdCBpcyAke21heEhlYXBTaXplfSBieXRlcyFgKTtyZXR1cm4gZmFsc2V9Zm9yKHZhciBjdXREb3duPTE7Y3V0RG93bjw9NDtjdXREb3duKj0yKXt2YXIgb3Zlckdyb3duSGVhcFNpemU9b2xkU2l6ZSooMSsuMi9jdXREb3duKTtvdmVyR3Jvd25IZWFwU2l6ZT1NYXRoLm1pbihvdmVyR3Jvd25IZWFwU2l6ZSxyZXF1ZXN0ZWRTaXplKzEwMDY2MzI5Nik7dmFyIG5ld1NpemU9TWF0aC5taW4obWF4SGVhcFNpemUsYWxpZ25NZW1vcnkoTWF0aC5tYXgocmVxdWVzdGVkU2l6ZSxvdmVyR3Jvd25IZWFwU2l6ZSksNjU1MzYpKTt2YXIgcmVwbGFjZW1lbnQ9Z3Jvd01lbW9yeShuZXdTaXplKTtpZihyZXBsYWNlbWVudCl7cmV0dXJuIHRydWV9fWVycihgRmFpbGVkIHRvIGdyb3cgdGhlIGhlYXAgZnJvbSAke29sZFNpemV9IGJ5dGVzIHRvICR7bmV3U2l6ZX0gYnl0ZXMsIG5vdCBlbm91Z2ggbWVtb3J5IWApO3JldHVybiBmYWxzZX07dmFyIF9lbXNjcmlwdGVuX3N0YWNrX3Vud2luZF9idWZmZXI9KGFkZHIsYnVmZmVyLGNvdW50KT0+e3ZhciBzdGFjaztpZihVTldJTkRfQ0FDSEUubGFzdF9hZGRyPT1hZGRyKXtzdGFjaz1VTldJTkRfQ0FDSEUubGFzdF9zdGFja31lbHNle3N0YWNrPWpzU3RhY2tUcmFjZSgpLnNwbGl0KCJcbiIpO2lmKHN0YWNrWzBdPT0iRXJyb3IiKXtzdGFjay5zaGlmdCgpfXNhdmVJblVud2luZENhY2hlKHN0YWNrKX12YXIgb2Zmc2V0PTM7d2hpbGUoc3RhY2tbb2Zmc2V0XSYmY29udmVydEZyYW1lVG9QQyhzdGFja1tvZmZzZXRdKSE9YWRkcil7KytvZmZzZXR9Zm9yKHZhciBpPTA7aTxjb3VudCYmc3RhY2tbaStvZmZzZXRdOysraSl7KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYnVmZmVyK2kqND4+Ml09Y29udmVydEZyYW1lVG9QQyhzdGFja1tpK29mZnNldF0pfXJldHVybiBpfTt2YXIgRU5WPXt9O3ZhciBnZXRFeGVjdXRhYmxlTmFtZT0oKT0+dGhpc1Byb2dyYW18fCIuL3RoaXMucHJvZ3JhbSI7dmFyIGdldEVudlN0cmluZ3M9KCk9PntpZighZ2V0RW52U3RyaW5ncy5zdHJpbmdzKXt2YXIgbGFuZz0odHlwZW9mIG5hdmlnYXRvcj09Im9iamVjdCImJm5hdmlnYXRvci5sYW5ndWFnZXx8IkMiKS5yZXBsYWNlKCItIiwiXyIpKyIuVVRGLTgiO3ZhciBlbnY9e1VTRVI6IndlYl91c2VyIixMT0dOQU1FOiJ3ZWJfdXNlciIsUEFUSDoiLyIsUFdEOiIvIixIT01FOiIvaG9tZS93ZWJfdXNlciIsTEFORzpsYW5nLF86Z2V0RXhlY3V0YWJsZU5hbWUoKX07Zm9yKHZhciB4IGluIEVOVil7aWYoRU5WW3hdPT09dW5kZWZpbmVkKWRlbGV0ZSBlbnZbeF07ZWxzZSBlbnZbeF09RU5WW3hdfXZhciBzdHJpbmdzPVtdO2Zvcih2YXIgeCBpbiBlbnYpe3N0cmluZ3MucHVzaChgJHt4fT0ke2Vudlt4XX1gKX1nZXRFbnZTdHJpbmdzLnN0cmluZ3M9c3RyaW5nc31yZXR1cm4gZ2V0RW52U3RyaW5ncy5zdHJpbmdzfTtmdW5jdGlvbiBfZW52aXJvbl9nZXQoX19lbnZpcm9uLGVudmlyb25fYnVmKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxMiwwLDEsX19lbnZpcm9uLGVudmlyb25fYnVmKTt2YXIgYnVmU2l6ZT0wO3ZhciBlbnZwPTA7Zm9yKHZhciBzdHJpbmcgb2YgZ2V0RW52U3RyaW5ncygpKXt2YXIgcHRyPWVudmlyb25fYnVmK2J1ZlNpemU7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW19fZW52aXJvbitlbnZwPj4yXT1wdHI7YnVmU2l6ZSs9c3RyaW5nVG9VVEY4KHN0cmluZyxwdHIsSW5maW5pdHkpKzE7ZW52cCs9NH1yZXR1cm4gMH1mdW5jdGlvbiBfZW52aXJvbl9zaXplc19nZXQocGVudmlyb25fY291bnQscGVudmlyb25fYnVmX3NpemUpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDEzLDAsMSxwZW52aXJvbl9jb3VudCxwZW52aXJvbl9idWZfc2l6ZSk7dmFyIHN0cmluZ3M9Z2V0RW52U3RyaW5ncygpOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwZW52aXJvbl9jb3VudD4+Ml09c3RyaW5ncy5sZW5ndGg7dmFyIGJ1ZlNpemU9MDtmb3IodmFyIHN0cmluZyBvZiBzdHJpbmdzKXtidWZTaXplKz1sZW5ndGhCeXRlc1VURjgoc3RyaW5nKSsxfShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwZW52aXJvbl9idWZfc2l6ZT4+Ml09YnVmU2l6ZTtyZXR1cm4gMH1mdW5jdGlvbiBfZmRfY2xvc2UoZmQpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDE0LDAsMSxmZCk7dHJ5e3ZhciBzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGZkKTtGUy5jbG9zZShzdHJlYW0pO3JldHVybiAwfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybiBlLmVycm5vfX12YXIgZG9SZWFkdj0oc3RyZWFtLGlvdixpb3ZjbnQsb2Zmc2V0KT0+e3ZhciByZXQ9MDtmb3IodmFyIGk9MDtpPGlvdmNudDtpKyspe3ZhciBwdHI9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2lvdj4+Ml07dmFyIGxlbj0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbaW92KzQ+PjJdO2lvdis9ODt2YXIgY3Vycj1GUy5yZWFkKHN0cmVhbSwoZ3Jvd01lbVZpZXdzKCksSEVBUDgpLHB0cixsZW4sb2Zmc2V0KTtpZihjdXJyPDApcmV0dXJuLTE7cmV0Kz1jdXJyO2lmKGN1cnI8bGVuKWJyZWFrO2lmKHR5cGVvZiBvZmZzZXQhPSJ1bmRlZmluZWQiKXtvZmZzZXQrPWN1cnJ9fXJldHVybiByZXR9O2Z1bmN0aW9uIF9mZF9yZWFkKGZkLGlvdixpb3ZjbnQscG51bSl7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMTUsMCwxLGZkLGlvdixpb3ZjbnQscG51bSk7dHJ5e3ZhciBzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGZkKTt2YXIgbnVtPWRvUmVhZHYoc3RyZWFtLGlvdixpb3ZjbnQpOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwbnVtPj4yXT1udW07cmV0dXJuIDB9Y2F0Y2goZSl7aWYodHlwZW9mIEZTPT0idW5kZWZpbmVkInx8IShlLm5hbWU9PT0iRXJybm9FcnJvciIpKXRocm93IGU7cmV0dXJuIGUuZXJybm99fWZ1bmN0aW9uIF9mZF9zZWVrKGZkLG9mZnNldCx3aGVuY2UsbmV3T2Zmc2V0KXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxNiwwLDEsZmQsb2Zmc2V0LHdoZW5jZSxuZXdPZmZzZXQpO29mZnNldD1iaWdpbnRUb0k1M0NoZWNrZWQob2Zmc2V0KTt0cnl7aWYoaXNOYU4ob2Zmc2V0KSlyZXR1cm4gNjE7dmFyIHN0cmVhbT1TWVNDQUxMUy5nZXRTdHJlYW1Gcm9tRkQoZmQpO0ZTLmxsc2VlayhzdHJlYW0sb2Zmc2V0LHdoZW5jZSk7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbbmV3T2Zmc2V0Pj4zXT1CaWdJbnQoc3RyZWFtLnBvc2l0aW9uKTtpZihzdHJlYW0uZ2V0ZGVudHMmJm9mZnNldD09PTAmJndoZW5jZT09PTApc3RyZWFtLmdldGRlbnRzPW51bGw7cmV0dXJuIDB9Y2F0Y2goZSl7aWYodHlwZW9mIEZTPT0idW5kZWZpbmVkInx8IShlLm5hbWU9PT0iRXJybm9FcnJvciIpKXRocm93IGU7cmV0dXJuIGUuZXJybm99fXZhciBkb1dyaXRldj0oc3RyZWFtLGlvdixpb3ZjbnQsb2Zmc2V0KT0+e3ZhciByZXQ9MDtmb3IodmFyIGk9MDtpPGlvdmNudDtpKyspe3ZhciBwdHI9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2lvdj4+Ml07dmFyIGxlbj0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbaW92KzQ+PjJdO2lvdis9ODt2YXIgY3Vycj1GUy53cml0ZShzdHJlYW0sKGdyb3dNZW1WaWV3cygpLEhFQVA4KSxwdHIsbGVuLG9mZnNldCk7aWYoY3VycjwwKXJldHVybi0xO3JldCs9Y3VycjtpZihjdXJyPGxlbil7YnJlYWt9aWYodHlwZW9mIG9mZnNldCE9InVuZGVmaW5lZCIpe29mZnNldCs9Y3Vycn19cmV0dXJuIHJldH07ZnVuY3Rpb24gX2ZkX3dyaXRlKGZkLGlvdixpb3ZjbnQscG51bSl7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMTcsMCwxLGZkLGlvdixpb3ZjbnQscG51bSk7dHJ5e3ZhciBzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGZkKTt2YXIgbnVtPWRvV3JpdGV2KHN0cmVhbSxpb3YsaW92Y250KTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbcG51bT4+Ml09bnVtO3JldHVybiAwfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybiBlLmVycm5vfX1mdW5jdGlvbiBfcmFuZG9tX2dldChidWZmZXIsc2l6ZSl7dHJ5e3JhbmRvbUZpbGwoKGdyb3dNZW1WaWV3cygpLEhFQVBVOCkuc3ViYXJyYXkoYnVmZmVyLGJ1ZmZlcitzaXplKSk7cmV0dXJuIDB9Y2F0Y2goZSl7aWYodHlwZW9mIEZTPT0idW5kZWZpbmVkInx8IShlLm5hbWU9PT0iRXJybm9FcnJvciIpKXRocm93IGU7cmV0dXJuIGUuZXJybm99fXZhciBydW50aW1lS2VlcGFsaXZlUG9wPSgpPT57YXNzZXJ0KHJ1bnRpbWVLZWVwYWxpdmVDb3VudGVyPjApO3J1bnRpbWVLZWVwYWxpdmVDb3VudGVyLT0xfTt2YXIgQXN5bmNpZnk9e2luc3RydW1lbnRXYXNtSW1wb3J0cyhpbXBvcnRzKXthc3NlcnQoIlN1c3BlbmRpbmciaW4gV2ViQXNzZW1ibHksIkpTUEkgbm90IHN1cHBvcnRlZCBieSBjdXJyZW50IGVudmlyb25tZW50LiBQZXJoYXBzIGl0IG5lZWRzIHRvIGJlIGVuYWJsZWQgdmlhIGZsYWdzPyIpO3ZhciBpbXBvcnRQYXR0ZXJuPS9eKGludm9rZV8uKnxfX2FzeW5janNfXy4qKSQvO2ZvcihsZXRbeCxvcmlnaW5hbF1vZiBPYmplY3QuZW50cmllcyhpbXBvcnRzKSl7aWYodHlwZW9mIG9yaWdpbmFsPT0iZnVuY3Rpb24iKXtsZXQgaXNBc3luY2lmeUltcG9ydD1vcmlnaW5hbC5pc0FzeW5jfHxpbXBvcnRQYXR0ZXJuLnRlc3QoeCk7aWYoaXNBc3luY2lmeUltcG9ydCl7aW1wb3J0c1t4XT1vcmlnaW5hbD1uZXcgV2ViQXNzZW1ibHkuU3VzcGVuZGluZyhvcmlnaW5hbCl9fX19LGluc3RydW1lbnRGdW5jdGlvbihvcmlnaW5hbCl7dmFyIHdyYXBwZXI9KC4uLmFyZ3MpPT5vcmlnaW5hbCguLi5hcmdzKTt3cmFwcGVyPWNyZWF0ZU5hbWVkRnVuY3Rpb24oYF9fYXN5bmNpZnlfd3JhcHBlcl8ke29yaWdpbmFsLm5hbWV9YCx3cmFwcGVyKTtyZXR1cm4gd3JhcHBlcn0saW5zdHJ1bWVudFdhc21FeHBvcnRzKGV4cG9ydHMpe3ZhciBleHBvcnRQYXR0ZXJuPS9eKHNvbHZlX21vZGVsfHZhbGlkYXRlX21vZGVsfG1haW58X19tYWluX2FyZ2NfYXJndikkLztBc3luY2lmeS5hc3luY0V4cG9ydHM9bmV3IFNldDt2YXIgcmV0PXt9O2ZvcihsZXRbeCxvcmlnaW5hbF1vZiBPYmplY3QuZW50cmllcyhleHBvcnRzKSl7aWYodHlwZW9mIG9yaWdpbmFsPT0iZnVuY3Rpb24iKXtsZXQgaXNBc3luY2lmeUV4cG9ydD1leHBvcnRQYXR0ZXJuLnRlc3QoeCk7aWYoaXNBc3luY2lmeUV4cG9ydCl7QXN5bmNpZnkuYXN5bmNFeHBvcnRzLmFkZChvcmlnaW5hbCk7b3JpZ2luYWw9QXN5bmNpZnkubWFrZUFzeW5jRnVuY3Rpb24ob3JpZ2luYWwpfXZhciB3cmFwcGVyPUFzeW5jaWZ5Lmluc3RydW1lbnRGdW5jdGlvbihvcmlnaW5hbCk7cmV0W3hdPXdyYXBwZXJ9ZWxzZXtyZXRbeF09b3JpZ2luYWx9fXJldHVybiByZXR9LGFzeW5jRXhwb3J0czpudWxsLGlzQXN5bmNFeHBvcnQoZnVuYyl7cmV0dXJuIEFzeW5jaWZ5LmFzeW5jRXhwb3J0cz8uaGFzKGZ1bmMpfSxoYW5kbGVBc3luYzphc3luYyBzdGFydEFzeW5jPT57cnVudGltZUtlZXBhbGl2ZVB1c2goKTt0cnl7cmV0dXJuIGF3YWl0IHN0YXJ0QXN5bmMoKX1maW5hbGx5e3J1bnRpbWVLZWVwYWxpdmVQb3AoKX19LGhhbmRsZVNsZWVwOnN0YXJ0QXN5bmM9PkFzeW5jaWZ5LmhhbmRsZUFzeW5jKCgpPT5uZXcgUHJvbWlzZShzdGFydEFzeW5jKSksbWFrZUFzeW5jRnVuY3Rpb24ob3JpZ2luYWwpe3JldHVybiBXZWJBc3NlbWJseS5wcm9taXNpbmcob3JpZ2luYWwpfX07dmFyIGdldENGdW5jPWlkZW50PT57dmFyIGZ1bmM9TW9kdWxlWyJfIitpZGVudF07YXNzZXJ0KGZ1bmMsIkNhbm5vdCBjYWxsIHVua25vd24gZnVuY3Rpb24gIitpZGVudCsiLCBtYWtlIHN1cmUgaXQgaXMgZXhwb3J0ZWQiKTtyZXR1cm4gZnVuY307dmFyIHdyaXRlQXJyYXlUb01lbW9yeT0oYXJyYXksYnVmZmVyKT0+e2Fzc2VydChhcnJheS5sZW5ndGg+PTAsIndyaXRlQXJyYXlUb01lbW9yeSBhcnJheSBtdXN0IGhhdmUgYSBsZW5ndGggKHNob3VsZCBiZSBhbiBhcnJheSBvciB0eXBlZCBhcnJheSkiKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDgpLnNldChhcnJheSxidWZmZXIpfTt2YXIgc3RyaW5nVG9VVEY4T25TdGFjaz1zdHI9Pnt2YXIgc2l6ZT1sZW5ndGhCeXRlc1VURjgoc3RyKSsxO3ZhciByZXQ9c3RhY2tBbGxvYyhzaXplKTtzdHJpbmdUb1VURjgoc3RyLHJldCxzaXplKTtyZXR1cm4gcmV0fTt2YXIgY2NhbGw9KGlkZW50LHJldHVyblR5cGUsYXJnVHlwZXMsYXJncyxvcHRzKT0+e3ZhciB0b0M9e3N0cmluZzpzdHI9Pnt2YXIgcmV0PTA7aWYoc3RyIT09bnVsbCYmc3RyIT09dW5kZWZpbmVkJiZzdHIhPT0wKXtyZXQ9c3RyaW5nVG9VVEY4T25TdGFjayhzdHIpfXJldHVybiByZXR9LGFycmF5OmFycj0+e3ZhciByZXQ9c3RhY2tBbGxvYyhhcnIubGVuZ3RoKTt3cml0ZUFycmF5VG9NZW1vcnkoYXJyLHJldCk7cmV0dXJuIHJldH19O2Z1bmN0aW9uIGNvbnZlcnRSZXR1cm5WYWx1ZShyZXQpe2lmKHJldHVyblR5cGU9PT0ic3RyaW5nIil7cmV0dXJuIFVURjhUb1N0cmluZyhyZXQpfWlmKHJldHVyblR5cGU9PT0iYm9vbGVhbiIpcmV0dXJuIEJvb2xlYW4ocmV0KTtyZXR1cm4gcmV0fXZhciBmdW5jPWdldENGdW5jKGlkZW50KTt2YXIgY0FyZ3M9W107dmFyIHN0YWNrPTA7YXNzZXJ0KHJldHVyblR5cGUhPT0iYXJyYXkiLCdSZXR1cm4gdHlwZSBzaG91bGQgbm90IGJlICJhcnJheSIuJyk7aWYoYXJncyl7Zm9yKHZhciBpPTA7aTxhcmdzLmxlbmd0aDtpKyspe3ZhciBjb252ZXJ0ZXI9dG9DW2FyZ1R5cGVzW2ldXTtpZihjb252ZXJ0ZXIpe2lmKHN0YWNrPT09MClzdGFjaz1zdGFja1NhdmUoKTtjQXJnc1tpXT1jb252ZXJ0ZXIoYXJnc1tpXSl9ZWxzZXtjQXJnc1tpXT1hcmdzW2ldfX19dmFyIHJldD1mdW5jKC4uLmNBcmdzKTtmdW5jdGlvbiBvbkRvbmUocmV0KXtpZihzdGFjayE9PTApc3RhY2tSZXN0b3JlKHN0YWNrKTtyZXR1cm4gY29udmVydFJldHVyblZhbHVlKHJldCl9dmFyIGFzeW5jTW9kZT1vcHRzPy5hc3luYztpZihhc3luY01vZGUpcmV0dXJuIHJldC50aGVuKG9uRG9uZSk7cmV0PW9uRG9uZShyZXQpO3JldHVybiByZXR9O3ZhciBjd3JhcD0oaWRlbnQscmV0dXJuVHlwZSxhcmdUeXBlcyxvcHRzKT0+KC4uLmFyZ3MpPT5jY2FsbChpZGVudCxyZXR1cm5UeXBlLGFyZ1R5cGVzLGFyZ3Msb3B0cyk7dmFyIGFsbG9jYXRlVVRGOD0oLi4uYXJncyk9PnN0cmluZ1RvTmV3VVRGOCguLi5hcmdzKTtQVGhyZWFkLmluaXQoKTtGUy5jcmVhdGVQcmVsb2FkZWRGaWxlPUZTX2NyZWF0ZVByZWxvYWRlZEZpbGU7RlMucHJlbG9hZEZpbGU9RlNfcHJlbG9hZEZpbGU7RlMuc3RhdGljSW5pdCgpO2Fzc2VydChlbXZhbF9oYW5kbGVzLmxlbmd0aD09PTUqMik7e2luaXRNZW1vcnkoKTtpZihNb2R1bGVbIm5vRXhpdFJ1bnRpbWUiXSlub0V4aXRSdW50aW1lPU1vZHVsZVsibm9FeGl0UnVudGltZSJdO2lmKE1vZHVsZVsicHJlbG9hZFBsdWdpbnMiXSlwcmVsb2FkUGx1Z2lucz1Nb2R1bGVbInByZWxvYWRQbHVnaW5zIl07aWYoTW9kdWxlWyJwcmludCJdKW91dD1Nb2R1bGVbInByaW50Il07aWYoTW9kdWxlWyJwcmludEVyciJdKWVycj1Nb2R1bGVbInByaW50RXJyIl07aWYoTW9kdWxlWyJ3YXNtQmluYXJ5Il0pd2FzbUJpbmFyeT1Nb2R1bGVbIndhc21CaW5hcnkiXTtjaGVja0luY29taW5nTW9kdWxlQVBJKCk7aWYoTW9kdWxlWyJhcmd1bWVudHMiXSlhcmd1bWVudHNfPU1vZHVsZVsiYXJndW1lbnRzIl07aWYoTW9kdWxlWyJ0aGlzUHJvZ3JhbSJdKXRoaXNQcm9ncmFtPU1vZHVsZVsidGhpc1Byb2dyYW0iXTthc3NlcnQodHlwZW9mIE1vZHVsZVsibWVtb3J5SW5pdGlhbGl6ZXJQcmVmaXhVUkwiXT09InVuZGVmaW5lZCIsIk1vZHVsZS5tZW1vcnlJbml0aWFsaXplclByZWZpeFVSTCBvcHRpb24gd2FzIHJlbW92ZWQsIHVzZSBNb2R1bGUubG9jYXRlRmlsZSBpbnN0ZWFkIik7YXNzZXJ0KHR5cGVvZiBNb2R1bGVbInB0aHJlYWRNYWluUHJlZml4VVJMIl09PSJ1bmRlZmluZWQiLCJNb2R1bGUucHRocmVhZE1haW5QcmVmaXhVUkwgb3B0aW9uIHdhcyByZW1vdmVkLCB1c2UgTW9kdWxlLmxvY2F0ZUZpbGUgaW5zdGVhZCIpO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJjZEluaXRpYWxpemVyUHJlZml4VVJMIl09PSJ1bmRlZmluZWQiLCJNb2R1bGUuY2RJbml0aWFsaXplclByZWZpeFVSTCBvcHRpb24gd2FzIHJlbW92ZWQsIHVzZSBNb2R1bGUubG9jYXRlRmlsZSBpbnN0ZWFkIik7YXNzZXJ0KHR5cGVvZiBNb2R1bGVbImZpbGVQYWNrYWdlUHJlZml4VVJMIl09PSJ1bmRlZmluZWQiLCJNb2R1bGUuZmlsZVBhY2thZ2VQcmVmaXhVUkwgb3B0aW9uIHdhcyByZW1vdmVkLCB1c2UgTW9kdWxlLmxvY2F0ZUZpbGUgaW5zdGVhZCIpO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJyZWFkIl09PSJ1bmRlZmluZWQiLCJNb2R1bGUucmVhZCBvcHRpb24gd2FzIHJlbW92ZWQiKTthc3NlcnQodHlwZW9mIE1vZHVsZVsicmVhZEFzeW5jIl09PSJ1bmRlZmluZWQiLCJNb2R1bGUucmVhZEFzeW5jIG9wdGlvbiB3YXMgcmVtb3ZlZCAobW9kaWZ5IHJlYWRBc3luYyBpbiBKUykiKTthc3NlcnQodHlwZW9mIE1vZHVsZVsicmVhZEJpbmFyeSJdPT0idW5kZWZpbmVkIiwiTW9kdWxlLnJlYWRCaW5hcnkgb3B0aW9uIHdhcyByZW1vdmVkIChtb2RpZnkgcmVhZEJpbmFyeSBpbiBKUykiKTthc3NlcnQodHlwZW9mIE1vZHVsZVsic2V0V2luZG93VGl0bGUiXT09InVuZGVmaW5lZCIsIk1vZHVsZS5zZXRXaW5kb3dUaXRsZSBvcHRpb24gd2FzIHJlbW92ZWQgKG1vZGlmeSBlbXNjcmlwdGVuX3NldF93aW5kb3dfdGl0bGUgaW4gSlMpIik7YXNzZXJ0KHR5cGVvZiBNb2R1bGVbIlRPVEFMX01FTU9SWSJdPT0idW5kZWZpbmVkIiwiTW9kdWxlLlRPVEFMX01FTU9SWSBoYXMgYmVlbiByZW5hbWVkIE1vZHVsZS5JTklUSUFMX01FTU9SWSIpO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJFTlZJUk9OTUVOVCJdPT0idW5kZWZpbmVkIiwiTW9kdWxlLkVOVklST05NRU5UIGhhcyBiZWVuIGRlcHJlY2F0ZWQuIFRvIGZvcmNlIHRoZSBlbnZpcm9ubWVudCwgdXNlIHRoZSBFTlZJUk9OTUVOVCBjb21waWxlLXRpbWUgb3B0aW9uIChmb3IgZXhhbXBsZSwgLXNFTlZJUk9OTUVOVD13ZWIgb3IgLXNFTlZJUk9OTUVOVD1ub2RlKSIpO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJTVEFDS19TSVpFIl09PSJ1bmRlZmluZWQiLCJTVEFDS19TSVpFIGNhbiBubyBsb25nZXIgYmUgc2V0IGF0IHJ1bnRpbWUuICBVc2UgLXNTVEFDS19TSVpFIGF0IGxpbmsgdGltZSIpO2lmKE1vZHVsZVsicHJlSW5pdCJdKXtpZih0eXBlb2YgTW9kdWxlWyJwcmVJbml0Il09PSJmdW5jdGlvbiIpTW9kdWxlWyJwcmVJbml0Il09W01vZHVsZVsicHJlSW5pdCJdXTt3aGlsZShNb2R1bGVbInByZUluaXQiXS5sZW5ndGg+MCl7TW9kdWxlWyJwcmVJbml0Il0uc2hpZnQoKSgpfX1jb25zdW1lZE1vZHVsZVByb3AoInByZUluaXQiKX1Nb2R1bGVbImNjYWxsIl09Y2NhbGw7TW9kdWxlWyJjd3JhcCJdPWN3cmFwO01vZHVsZVsiVVRGOFRvU3RyaW5nIl09VVRGOFRvU3RyaW5nO01vZHVsZVsic3RyaW5nVG9VVEY4Il09c3RyaW5nVG9VVEY4O01vZHVsZVsiYWxsb2NhdGVVVEY4Il09YWxsb2NhdGVVVEY4O3ZhciBtaXNzaW5nTGlicmFyeVN5bWJvbHM9WyJ3cml0ZUk1M1RvSTY0Iiwid3JpdGVJNTNUb0k2NENsYW1wZWQiLCJ3cml0ZUk1M1RvSTY0U2lnbmFsaW5nIiwid3JpdGVJNTNUb1U2NENsYW1wZWQiLCJ3cml0ZUk1M1RvVTY0U2lnbmFsaW5nIiwicmVhZEk1M0Zyb21JNjQiLCJyZWFkSTUzRnJvbVU2NCIsImNvbnZlcnRJMzJQYWlyVG9JNTMiLCJjb252ZXJ0STMyUGFpclRvSTUzQ2hlY2tlZCIsImNvbnZlcnRVMzJQYWlyVG9JNTMiLCJnZXRUZW1wUmV0MCIsInNldFRlbXBSZXQwIiwid2l0aFN0YWNrU2F2ZSIsImluZXRQdG9uNCIsImluZXROdG9wNCIsImluZXRQdG9uNiIsImluZXROdG9wNiIsInJlYWRTb2NrYWRkciIsIndyaXRlU29ja2FkZHIiLCJydW5NYWluVGhyZWFkRW1Bc20iLCJqc3RvaV9xIiwiYXV0b1Jlc3VtZUF1ZGlvQ29udGV4dCIsImdldER5bkNhbGxlciIsImR5bkNhbGwiLCJhc21qc01hbmdsZSIsIkhhbmRsZUFsbG9jYXRvciIsImFkZE9uSW5pdCIsImFkZE9uUG9zdEN0b3IiLCJhZGRPblByZU1haW4iLCJhZGRPbkV4aXQiLCJTVEFDS19TSVpFIiwiU1RBQ0tfQUxJR04iLCJQT0lOVEVSX1NJWkUiLCJBU1NFUlRJT05TIiwiY29udmVydEpzRnVuY3Rpb25Ub1dhc20iLCJnZXRFbXB0eVRhYmxlU2xvdCIsInVwZGF0ZVRhYmxlTWFwIiwiZ2V0RnVuY3Rpb25BZGRyZXNzIiwiYWRkRnVuY3Rpb24iLCJyZW1vdmVGdW5jdGlvbiIsImludEFycmF5VG9TdHJpbmciLCJzdHJpbmdUb0FzY2lpIiwicmVnaXN0ZXJLZXlFdmVudENhbGxiYWNrIiwibWF5YmVDU3RyaW5nVG9Kc1N0cmluZyIsImZpbmRFdmVudFRhcmdldCIsImdldEJvdW5kaW5nQ2xpZW50UmVjdCIsImZpbGxNb3VzZUV2ZW50RGF0YSIsInJlZ2lzdGVyTW91c2VFdmVudENhbGxiYWNrIiwicmVnaXN0ZXJXaGVlbEV2ZW50Q2FsbGJhY2siLCJyZWdpc3RlclVpRXZlbnRDYWxsYmFjayIsInJlZ2lzdGVyRm9jdXNFdmVudENhbGxiYWNrIiwiZmlsbERldmljZU9yaWVudGF0aW9uRXZlbnREYXRhIiwicmVnaXN0ZXJEZXZpY2VPcmllbnRhdGlvbkV2ZW50Q2FsbGJhY2siLCJmaWxsRGV2aWNlTW90aW9uRXZlbnREYXRhIiwicmVnaXN0ZXJEZXZpY2VNb3Rpb25FdmVudENhbGxiYWNrIiwic2NyZWVuT3JpZW50YXRpb24iLCJmaWxsT3JpZW50YXRpb25DaGFuZ2VFdmVudERhdGEiLCJyZWdpc3Rlck9yaWVudGF0aW9uQ2hhbmdlRXZlbnRDYWxsYmFjayIsImZpbGxGdWxsc2NyZWVuQ2hhbmdlRXZlbnREYXRhIiwicmVnaXN0ZXJGdWxsc2NyZWVuQ2hhbmdlRXZlbnRDYWxsYmFjayIsIkpTRXZlbnRzX3JlcXVlc3RGdWxsc2NyZWVuIiwiSlNFdmVudHNfcmVzaXplQ2FudmFzRm9yRnVsbHNjcmVlbiIsInJlZ2lzdGVyUmVzdG9yZU9sZFN0eWxlIiwiaGlkZUV2ZXJ5dGhpbmdFeGNlcHRHaXZlbkVsZW1lbnQiLCJyZXN0b3JlSGlkZGVuRWxlbWVudHMiLCJzZXRMZXR0ZXJib3giLCJzb2Z0RnVsbHNjcmVlblJlc2l6ZVdlYkdMUmVuZGVyVGFyZ2V0IiwiZG9SZXF1ZXN0RnVsbHNjcmVlbiIsImZpbGxQb2ludGVybG9ja0NoYW5nZUV2ZW50RGF0YSIsInJlZ2lzdGVyUG9pbnRlcmxvY2tDaGFuZ2VFdmVudENhbGxiYWNrIiwicmVnaXN0ZXJQb2ludGVybG9ja0Vycm9yRXZlbnRDYWxsYmFjayIsInJlcXVlc3RQb2ludGVyTG9jayIsImZpbGxWaXNpYmlsaXR5Q2hhbmdlRXZlbnREYXRhIiwicmVnaXN0ZXJWaXNpYmlsaXR5Q2hhbmdlRXZlbnRDYWxsYmFjayIsInJlZ2lzdGVyVG91Y2hFdmVudENhbGxiYWNrIiwiZmlsbEdhbWVwYWRFdmVudERhdGEiLCJyZWdpc3RlckdhbWVwYWRFdmVudENhbGxiYWNrIiwicmVnaXN0ZXJCZWZvcmVVbmxvYWRFdmVudENhbGxiYWNrIiwiZmlsbEJhdHRlcnlFdmVudERhdGEiLCJyZWdpc3RlckJhdHRlcnlFdmVudENhbGxiYWNrIiwic2V0Q2FudmFzRWxlbWVudFNpemVDYWxsaW5nVGhyZWFkIiwic2V0Q2FudmFzRWxlbWVudFNpemVNYWluVGhyZWFkIiwic2V0Q2FudmFzRWxlbWVudFNpemUiLCJnZXRDYW52YXNTaXplQ2FsbGluZ1RocmVhZCIsImdldENhbnZhc1NpemVNYWluVGhyZWFkIiwiZ2V0Q2FudmFzRWxlbWVudFNpemUiLCJnZXRDYWxsc3RhY2siLCJjb252ZXJ0UEN0b1NvdXJjZUxvY2F0aW9uIiwid2FzaVJpZ2h0c1RvTXVzbE9GbGFncyIsIndhc2lPRmxhZ3NUb011c2xPRmxhZ3MiLCJzYWZlU2V0VGltZW91dCIsInNldEltbWVkaWF0ZVdyYXBwZWQiLCJzYWZlUmVxdWVzdEFuaW1hdGlvbkZyYW1lIiwiY2xlYXJJbW1lZGlhdGVXcmFwcGVkIiwicmVnaXN0ZXJQb3N0TWFpbkxvb3AiLCJyZWdpc3RlclByZU1haW5Mb29wIiwiZ2V0UHJvbWlzZSIsIm1ha2VQcm9taXNlIiwiaWRzVG9Qcm9taXNlcyIsIm1ha2VQcm9taXNlQ2FsbGJhY2siLCJmaW5kTWF0Y2hpbmdDYXRjaCIsIkJyb3dzZXJfYXN5bmNQcmVwYXJlRGF0YUNvdW50ZXIiLCJpc0xlYXBZZWFyIiwieWRheUZyb21EYXRlIiwiYXJyYXlTdW0iLCJhZGREYXlzIiwiZ2V0U29ja2V0RnJvbUZEIiwiZ2V0U29ja2V0QWRkcmVzcyIsIkZTX21rZGlyVHJlZSIsIl9zZXROZXR3b3JrQ2FsbGJhY2siLCJoZWFwT2JqZWN0Rm9yV2ViR0xUeXBlIiwidG9UeXBlZEFycmF5SW5kZXgiLCJ3ZWJnbF9lbmFibGVfQU5HTEVfaW5zdGFuY2VkX2FycmF5cyIsIndlYmdsX2VuYWJsZV9PRVNfdmVydGV4X2FycmF5X29iamVjdCIsIndlYmdsX2VuYWJsZV9XRUJHTF9kcmF3X2J1ZmZlcnMiLCJ3ZWJnbF9lbmFibGVfV0VCR0xfbXVsdGlfZHJhdyIsIndlYmdsX2VuYWJsZV9FWFRfcG9seWdvbl9vZmZzZXRfY2xhbXAiLCJ3ZWJnbF9lbmFibGVfRVhUX2NsaXBfY29udHJvbCIsIndlYmdsX2VuYWJsZV9XRUJHTF9wb2x5Z29uX21vZGUiLCJlbXNjcmlwdGVuV2ViR0xHZXQiLCJjb21wdXRlVW5wYWNrQWxpZ25lZEltYWdlU2l6ZSIsImNvbG9yQ2hhbm5lbHNJbkdsVGV4dHVyZUZvcm1hdCIsImVtc2NyaXB0ZW5XZWJHTEdldFRleFBpeGVsRGF0YSIsImVtc2NyaXB0ZW5XZWJHTEdldFVuaWZvcm0iLCJ3ZWJnbEdldFVuaWZvcm1Mb2NhdGlvbiIsIndlYmdsUHJlcGFyZVVuaWZvcm1Mb2NhdGlvbnNCZWZvcmVGaXJzdFVzZSIsIndlYmdsR2V0TGVmdEJyYWNlUG9zIiwiZW1zY3JpcHRlbldlYkdMR2V0VmVydGV4QXR0cmliIiwiX19nbEdldEFjdGl2ZUF0dHJpYk9yVW5pZm9ybSIsIndyaXRlR0xBcnJheSIsImVtc2NyaXB0ZW5fd2ViZ2xfZGVzdHJveV9jb250ZXh0X2JlZm9yZV9vbl9jYWxsaW5nX3RocmVhZCIsInJlZ2lzdGVyV2ViR2xFdmVudENhbGxiYWNrIiwiQUxMT0NfTk9STUFMIiwiQUxMT0NfU1RBQ0siLCJhbGxvY2F0ZSIsIndyaXRlU3RyaW5nVG9NZW1vcnkiLCJ3cml0ZUFzY2lpVG9NZW1vcnkiLCJhbGxvY2F0ZVVURjhPblN0YWNrIiwiZGVtYW5nbGUiLCJzdGFja1RyYWNlIiwiZ2V0TmF0aXZlVHlwZVNpemUiLCJnZXRGdW5jdGlvbkFyZ3NOYW1lIiwicmVxdWlyZVJlZ2lzdGVyZWRUeXBlIiwiY3JlYXRlSnNJbnZva2VyU2lnbmF0dXJlIiwiUHVyZVZpcnR1YWxFcnJvciIsImdldEJhc2VzdFBvaW50ZXIiLCJyZWdpc3RlckluaGVyaXRlZEluc3RhbmNlIiwidW5yZWdpc3RlckluaGVyaXRlZEluc3RhbmNlIiwiZ2V0SW5oZXJpdGVkSW5zdGFuY2UiLCJnZXRJbmhlcml0ZWRJbnN0YW5jZUNvdW50IiwiZ2V0TGl2ZUluaGVyaXRlZEluc3RhbmNlcyIsImVudW1SZWFkVmFsdWVGcm9tUG9pbnRlciIsImdlbmVyaWNQb2ludGVyVG9XaXJlVHlwZSIsImNvbnN0Tm9TbWFydFB0clJhd1BvaW50ZXJUb1dpcmVUeXBlIiwibm9uQ29uc3ROb1NtYXJ0UHRyUmF3UG9pbnRlclRvV2lyZVR5cGUiLCJpbml0X1JlZ2lzdGVyZWRQb2ludGVyIiwiUmVnaXN0ZXJlZFBvaW50ZXIiLCJSZWdpc3RlcmVkUG9pbnRlcl9mcm9tV2lyZVR5cGUiLCJydW5EZXN0cnVjdG9yIiwicmVsZWFzZUNsYXNzSGFuZGxlIiwiZGV0YWNoRmluYWxpemVyIiwiYXR0YWNoRmluYWxpemVyIiwibWFrZUNsYXNzSGFuZGxlIiwiaW5pdF9DbGFzc0hhbmRsZSIsIkNsYXNzSGFuZGxlIiwidGhyb3dJbnN0YW5jZUFscmVhZHlEZWxldGVkIiwiZmx1c2hQZW5kaW5nRGVsZXRlcyIsInNldERlbGF5RnVuY3Rpb24iLCJSZWdpc3RlcmVkQ2xhc3MiLCJzaGFsbG93Q29weUludGVybmFsUG9pbnRlciIsImRvd25jYXN0UG9pbnRlciIsInVwY2FzdFBvaW50ZXIiLCJ2YWxpZGF0ZVRoaXMiLCJjaGFyXzAiLCJjaGFyXzkiLCJtYWtlTGVnYWxGdW5jdGlvbk5hbWUiLCJjb3VudF9lbXZhbF9oYW5kbGVzIiwiZ2V0U3RyaW5nT3JTeW1ib2wiLCJlbXZhbF9yZXR1cm5WYWx1ZSIsImVtdmFsX2xvb2t1cFR5cGVzIiwiZW12YWxfYWRkTWV0aG9kQ2FsbGVyIl07bWlzc2luZ0xpYnJhcnlTeW1ib2xzLmZvckVhY2gobWlzc2luZ0xpYnJhcnlTeW1ib2wpO3ZhciB1bmV4cG9ydGVkU3ltYm9scz1bInJ1biIsIm91dCIsImVyciIsImNhbGxNYWluIiwiYWJvcnQiLCJ3YXNtRXhwb3J0cyIsIkhFQVBGMzIiLCJIRUFQRjY0IiwiSEVBUDgiLCJIRUFQMTYiLCJIRUFQVTE2IiwiSEVBUDMyIiwiSEVBUFUzMiIsIkhFQVA2NCIsIkhFQVBVNjQiLCJ3cml0ZVN0YWNrQ29va2llIiwiY2hlY2tTdGFja0Nvb2tpZSIsIklOVDUzX01BWCIsIklOVDUzX01JTiIsImJpZ2ludFRvSTUzQ2hlY2tlZCIsInN0YWNrU2F2ZSIsInN0YWNrUmVzdG9yZSIsInN0YWNrQWxsb2MiLCJjcmVhdGVOYW1lZEZ1bmN0aW9uIiwicHRyVG9TdHJpbmciLCJ6ZXJvTWVtb3J5IiwiZXhpdEpTIiwiZ2V0SGVhcE1heCIsImdyb3dNZW1vcnkiLCJFTlYiLCJFUlJOT19DT0RFUyIsInN0ckVycm9yIiwiRE5TIiwiUHJvdG9jb2xzIiwiU29ja2V0cyIsInRpbWVycyIsIndhcm5PbmNlIiwicmVhZEVtQXNtQXJnc0FycmF5IiwicmVhZEVtQXNtQXJncyIsInJ1bkVtQXNtRnVuY3Rpb24iLCJnZXRFeGVjdXRhYmxlTmFtZSIsImhhbmRsZUV4Y2VwdGlvbiIsImtlZXBSdW50aW1lQWxpdmUiLCJydW50aW1lS2VlcGFsaXZlUHVzaCIsInJ1bnRpbWVLZWVwYWxpdmVQb3AiLCJjYWxsVXNlckNhbGxiYWNrIiwibWF5YmVFeGl0IiwiYXN5bmNMb2FkIiwiYWxpZ25NZW1vcnkiLCJtbWFwQWxsb2MiLCJ3YXNtVGFibGUiLCJ3YXNtTWVtb3J5IiwiZ2V0VW5pcXVlUnVuRGVwZW5kZW5jeSIsIm5vRXhpdFJ1bnRpbWUiLCJhZGRSdW5EZXBlbmRlbmN5IiwicmVtb3ZlUnVuRGVwZW5kZW5jeSIsImFkZE9uUHJlUnVuIiwiYWRkT25Qb3N0UnVuIiwiZnJlZVRhYmxlSW5kZXhlcyIsImZ1bmN0aW9uc0luVGFibGVNYXAiLCJzZXRWYWx1ZSIsImdldFZhbHVlIiwiUEFUSCIsIlBBVEhfRlMiLCJVVEY4RGVjb2RlciIsIlVURjhBcnJheVRvU3RyaW5nIiwic3RyaW5nVG9VVEY4QXJyYXkiLCJsZW5ndGhCeXRlc1VURjgiLCJpbnRBcnJheUZyb21TdHJpbmciLCJBc2NpaVRvU3RyaW5nIiwiVVRGMTZEZWNvZGVyIiwiVVRGMTZUb1N0cmluZyIsInN0cmluZ1RvVVRGMTYiLCJsZW5ndGhCeXRlc1VURjE2IiwiVVRGMzJUb1N0cmluZyIsInN0cmluZ1RvVVRGMzIiLCJsZW5ndGhCeXRlc1VURjMyIiwic3RyaW5nVG9OZXdVVEY4Iiwic3RyaW5nVG9VVEY4T25TdGFjayIsIndyaXRlQXJyYXlUb01lbW9yeSIsIkpTRXZlbnRzIiwic3BlY2lhbEhUTUxUYXJnZXRzIiwiZmluZENhbnZhc0V2ZW50VGFyZ2V0IiwiY3VycmVudEZ1bGxzY3JlZW5TdHJhdGVneSIsInJlc3RvcmVPbGRXaW5kb3dlZFN0eWxlIiwianNTdGFja1RyYWNlIiwiVU5XSU5EX0NBQ0hFIiwiRXhpdFN0YXR1cyIsImdldEVudlN0cmluZ3MiLCJjaGVja1dhc2lDbG9jayIsImRvUmVhZHYiLCJkb1dyaXRldiIsImluaXRSYW5kb21GaWxsIiwicmFuZG9tRmlsbCIsImVtU2V0SW1tZWRpYXRlIiwiZW1DbGVhckltbWVkaWF0ZV9kZXBzIiwiZW1DbGVhckltbWVkaWF0ZSIsInByb21pc2VNYXAiLCJ1bmNhdWdodEV4Y2VwdGlvbkNvdW50IiwiZXhjZXB0aW9uTGFzdCIsImV4Y2VwdGlvbkNhdWdodCIsIkV4Y2VwdGlvbkluZm8iLCJCcm93c2VyIiwicmVxdWVzdEZ1bGxzY3JlZW4iLCJyZXF1ZXN0RnVsbFNjcmVlbiIsInNldENhbnZhc1NpemUiLCJnZXRVc2VyTWVkaWEiLCJjcmVhdGVDb250ZXh0IiwiZ2V0UHJlbG9hZGVkSW1hZ2VEYXRhX19kYXRhIiwid2dldCIsIk1PTlRIX0RBWVNfUkVHVUxBUiIsIk1PTlRIX0RBWVNfTEVBUCIsIk1PTlRIX0RBWVNfUkVHVUxBUl9DVU1VTEFUSVZFIiwiTU9OVEhfREFZU19MRUFQX0NVTVVMQVRJVkUiLCJTWVNDQUxMUyIsInByZWxvYWRQbHVnaW5zIiwiRlNfY3JlYXRlUHJlbG9hZGVkRmlsZSIsIkZTX3ByZWxvYWRGaWxlIiwiRlNfbW9kZVN0cmluZ1RvRmxhZ3MiLCJGU19nZXRNb2RlIiwiRlNfc3RkaW5fZ2V0Q2hhcl9idWZmZXIiLCJGU19zdGRpbl9nZXRDaGFyIiwiRlNfdW5saW5rIiwiRlNfY3JlYXRlUGF0aCIsIkZTX2NyZWF0ZURldmljZSIsIkZTX3JlYWRGaWxlIiwiRlMiLCJGU19yb290IiwiRlNfbW91bnRzIiwiRlNfZGV2aWNlcyIsIkZTX3N0cmVhbXMiLCJGU19uZXh0SW5vZGUiLCJGU19uYW1lVGFibGUiLCJGU19jdXJyZW50UGF0aCIsIkZTX2luaXRpYWxpemVkIiwiRlNfaWdub3JlUGVybWlzc2lvbnMiLCJGU19maWxlc3lzdGVtcyIsIkZTX3N5bmNGU1JlcXVlc3RzIiwiRlNfcmVhZEZpbGVzIiwiRlNfbG9va3VwUGF0aCIsIkZTX2dldFBhdGgiLCJGU19oYXNoTmFtZSIsIkZTX2hhc2hBZGROb2RlIiwiRlNfaGFzaFJlbW92ZU5vZGUiLCJGU19sb29rdXBOb2RlIiwiRlNfY3JlYXRlTm9kZSIsIkZTX2Rlc3Ryb3lOb2RlIiwiRlNfaXNSb290IiwiRlNfaXNNb3VudHBvaW50IiwiRlNfaXNGaWxlIiwiRlNfaXNEaXIiLCJGU19pc0xpbmsiLCJGU19pc0NocmRldiIsIkZTX2lzQmxrZGV2IiwiRlNfaXNGSUZPIiwiRlNfaXNTb2NrZXQiLCJGU19mbGFnc1RvUGVybWlzc2lvblN0cmluZyIsIkZTX25vZGVQZXJtaXNzaW9ucyIsIkZTX21heUxvb2t1cCIsIkZTX21heUNyZWF0ZSIsIkZTX21heURlbGV0ZSIsIkZTX21heU9wZW4iLCJGU19jaGVja09wRXhpc3RzIiwiRlNfbmV4dGZkIiwiRlNfZ2V0U3RyZWFtQ2hlY2tlZCIsIkZTX2dldFN0cmVhbSIsIkZTX2NyZWF0ZVN0cmVhbSIsIkZTX2Nsb3NlU3RyZWFtIiwiRlNfZHVwU3RyZWFtIiwiRlNfZG9TZXRBdHRyIiwiRlNfY2hyZGV2X3N0cmVhbV9vcHMiLCJGU19tYWpvciIsIkZTX21pbm9yIiwiRlNfbWFrZWRldiIsIkZTX3JlZ2lzdGVyRGV2aWNlIiwiRlNfZ2V0RGV2aWNlIiwiRlNfZ2V0TW91bnRzIiwiRlNfc3luY2ZzIiwiRlNfbW91bnQiLCJGU191bm1vdW50IiwiRlNfbG9va3VwIiwiRlNfbWtub2QiLCJGU19zdGF0ZnMiLCJGU19zdGF0ZnNTdHJlYW0iLCJGU19zdGF0ZnNOb2RlIiwiRlNfY3JlYXRlIiwiRlNfbWtkaXIiLCJGU19ta2RldiIsIkZTX3N5bWxpbmsiLCJGU19yZW5hbWUiLCJGU19ybWRpciIsIkZTX3JlYWRkaXIiLCJGU19yZWFkbGluayIsIkZTX3N0YXQiLCJGU19mc3RhdCIsIkZTX2xzdGF0IiwiRlNfZG9DaG1vZCIsIkZTX2NobW9kIiwiRlNfbGNobW9kIiwiRlNfZmNobW9kIiwiRlNfZG9DaG93biIsIkZTX2Nob3duIiwiRlNfbGNob3duIiwiRlNfZmNob3duIiwiRlNfZG9UcnVuY2F0ZSIsIkZTX3RydW5jYXRlIiwiRlNfZnRydW5jYXRlIiwiRlNfdXRpbWUiLCJGU19vcGVuIiwiRlNfY2xvc2UiLCJGU19pc0Nsb3NlZCIsIkZTX2xsc2VlayIsIkZTX3JlYWQiLCJGU193cml0ZSIsIkZTX21tYXAiLCJGU19tc3luYyIsIkZTX2lvY3RsIiwiRlNfd3JpdGVGaWxlIiwiRlNfY3dkIiwiRlNfY2hkaXIiLCJGU19jcmVhdGVEZWZhdWx0RGlyZWN0b3JpZXMiLCJGU19jcmVhdGVEZWZhdWx0RGV2aWNlcyIsIkZTX2NyZWF0ZVNwZWNpYWxEaXJlY3RvcmllcyIsIkZTX2NyZWF0ZVN0YW5kYXJkU3RyZWFtcyIsIkZTX3N0YXRpY0luaXQiLCJGU19pbml0IiwiRlNfcXVpdCIsIkZTX2ZpbmRPYmplY3QiLCJGU19hbmFseXplUGF0aCIsIkZTX2NyZWF0ZUZpbGUiLCJGU19jcmVhdGVEYXRhRmlsZSIsIkZTX2ZvcmNlTG9hZEZpbGUiLCJGU19jcmVhdGVMYXp5RmlsZSIsIkZTX2Fic29sdXRlUGF0aCIsIkZTX2NyZWF0ZUZvbGRlciIsIkZTX2NyZWF0ZUxpbmsiLCJGU19qb2luUGF0aCIsIkZTX21tYXBBbGxvYyIsIkZTX3N0YW5kYXJkaXplUGF0aCIsIk1FTUZTIiwiVFRZIiwiUElQRUZTIiwiU09DS0ZTIiwidGVtcEZpeGVkTGVuZ3RoQXJyYXkiLCJtaW5pVGVtcFdlYkdMRmxvYXRCdWZmZXJzIiwibWluaVRlbXBXZWJHTEludEJ1ZmZlcnMiLCJHTCIsIkFMIiwiR0xVVCIsIkVHTCIsIkdMRVciLCJJREJTdG9yZSIsInJ1bkFuZEFib3J0SWZFcnJvciIsIkFzeW5jaWZ5IiwiRmliZXJzIiwiU0RMIiwiU0RMX2dmeCIsInByaW50IiwicHJpbnRFcnIiLCJqc3RvaV9zIiwiUFRocmVhZCIsInRlcm1pbmF0ZVdvcmtlciIsImNsZWFudXBUaHJlYWQiLCJyZWdpc3RlclRMU0luaXQiLCJzcGF3blRocmVhZCIsImV4aXRPbk1haW5UaHJlYWQiLCJwcm94eVRvTWFpblRocmVhZCIsInByb3hpZWRKU0NhbGxBcmdzIiwiaW52b2tlRW50cnlQb2ludCIsImNoZWNrTWFpbGJveCIsIkludGVybmFsRXJyb3IiLCJCaW5kaW5nRXJyb3IiLCJ0aHJvd0ludGVybmFsRXJyb3IiLCJ0aHJvd0JpbmRpbmdFcnJvciIsInJlZ2lzdGVyZWRUeXBlcyIsImF3YWl0aW5nRGVwZW5kZW5jaWVzIiwidHlwZURlcGVuZGVuY2llcyIsInR1cGxlUmVnaXN0cmF0aW9ucyIsInN0cnVjdFJlZ2lzdHJhdGlvbnMiLCJzaGFyZWRSZWdpc3RlclR5cGUiLCJ3aGVuRGVwZW5kZW50VHlwZXNBcmVSZXNvbHZlZCIsImdldFR5cGVOYW1lIiwiZ2V0RnVuY3Rpb25OYW1lIiwiaGVhcDMyVmVjdG9yVG9BcnJheSIsInVzZXNEZXN0cnVjdG9yU3RhY2siLCJjaGVja0FyZ0NvdW50IiwiZ2V0UmVxdWlyZWRBcmdDb3VudCIsImNyZWF0ZUpzSW52b2tlciIsIlVuYm91bmRUeXBlRXJyb3IiLCJFbVZhbFR5cGUiLCJFbVZhbE9wdGlvbmFsVHlwZSIsInRocm93VW5ib3VuZFR5cGVFcnJvciIsImVuc3VyZU92ZXJsb2FkVGFibGUiLCJleHBvc2VQdWJsaWNTeW1ib2wiLCJyZXBsYWNlUHVibGljU3ltYm9sIiwiZW1iaW5kUmVwciIsInJlZ2lzdGVyZWRJbnN0YW5jZXMiLCJyZWdpc3RlcmVkUG9pbnRlcnMiLCJyZWdpc3RlclR5cGUiLCJpbnRlZ2VyUmVhZFZhbHVlRnJvbVBvaW50ZXIiLCJmbG9hdFJlYWRWYWx1ZUZyb21Qb2ludGVyIiwiYXNzZXJ0SW50ZWdlclJhbmdlIiwicmVhZFBvaW50ZXIiLCJydW5EZXN0cnVjdG9ycyIsImNyYWZ0SW52b2tlckZ1bmN0aW9uIiwiZW1iaW5kX19yZXF1aXJlRnVuY3Rpb24iLCJmaW5hbGl6YXRpb25SZWdpc3RyeSIsImRldGFjaEZpbmFsaXplcl9kZXBzIiwiZGVsZXRpb25RdWV1ZSIsImRlbGF5RnVuY3Rpb24iLCJlbXZhbF9mcmVlbGlzdCIsImVtdmFsX2hhbmRsZXMiLCJlbXZhbF9zeW1ib2xzIiwiRW12YWwiLCJlbXZhbF9tZXRob2RDYWxsZXJzIl07dW5leHBvcnRlZFN5bWJvbHMuZm9yRWFjaCh1bmV4cG9ydGVkUnVudGltZVN5bWJvbCk7dmFyIHByb3hpZWRGdW5jdGlvblRhYmxlPVtfcHJvY19leGl0LGV4aXRPbk1haW5UaHJlYWQscHRocmVhZENyZWF0ZVByb3hpZWQsX19fc3lzY2FsbF9mY250bDY0LF9fX3N5c2NhbGxfZnN0YXQ2NCxfX19zeXNjYWxsX2lvY3RsLF9fX3N5c2NhbGxfbHN0YXQ2NCxfX19zeXNjYWxsX25ld2ZzdGF0YXQsX19fc3lzY2FsbF9vcGVuYXQsX19fc3lzY2FsbF9zdGF0NjQsX19tbWFwX2pzLF9fbXVubWFwX2pzLF9lbnZpcm9uX2dldCxfZW52aXJvbl9zaXplc19nZXQsX2ZkX2Nsb3NlLF9mZF9yZWFkLF9mZF9zZWVrLF9mZF93cml0ZV07ZnVuY3Rpb24gY2hlY2tJbmNvbWluZ01vZHVsZUFQSSgpe2lnbm9yZWRNb2R1bGVQcm9wKCJmZXRjaFNldHRpbmdzIil9dmFyIEFTTV9DT05TVFM9ezU0MDkzMjooKT0+dHlwZW9mIHdhc21PZmZzZXRDb252ZXJ0ZXIhPT0idW5kZWZpbmVkIn07ZnVuY3Rpb24gSGF2ZU9mZnNldENvbnZlcnRlcigpe3JldHVybiB0eXBlb2Ygd2FzbU9mZnNldENvbnZlcnRlciE9PSJ1bmRlZmluZWQifUhhdmVPZmZzZXRDb252ZXJ0ZXIuc2lnPSJpIjt2YXIgX19fZ2V0VHlwZU5hbWU9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX19fZ2V0VHlwZU5hbWUiKTt2YXIgX19lbWJpbmRfaW5pdGlhbGl6ZV9iaW5kaW5ncz1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2VtYmluZF9pbml0aWFsaXplX2JpbmRpbmdzIik7dmFyIF9nZXRfY3BfbW9kZWxfc2NoZW1hPU1vZHVsZVsiX2dldF9jcF9tb2RlbF9zY2hlbWEiXT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZ2V0X2NwX21vZGVsX3NjaGVtYSIpO3ZhciBfZ2V0X3NhdF9wYXJhbWV0ZXJzX3NjaGVtYT1Nb2R1bGVbIl9nZXRfc2F0X3BhcmFtZXRlcnNfc2NoZW1hIl09bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2dldF9zYXRfcGFyYW1ldGVyc19zY2hlbWEiKTt2YXIgX3NvbHZlX21vZGVsPU1vZHVsZVsiX3NvbHZlX21vZGVsIl09bWFrZUludmFsaWRFYXJseUFjY2VzcygiX3NvbHZlX21vZGVsIik7dmFyIF9tYWxsb2M9TW9kdWxlWyJfbWFsbG9jIl09bWFrZUludmFsaWRFYXJseUFjY2VzcygiX21hbGxvYyIpO3ZhciBfZnJlZV9idWZmZXI9TW9kdWxlWyJfZnJlZV9idWZmZXIiXT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZnJlZV9idWZmZXIiKTt2YXIgX2ZyZWU9TW9kdWxlWyJfZnJlZSJdPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9mcmVlIik7dmFyIF9pbnRlcnJ1cHRfc29sdmU9TW9kdWxlWyJfaW50ZXJydXB0X3NvbHZlIl09bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2ludGVycnVwdF9zb2x2ZSIpO3ZhciBfdmFsaWRhdGVfbW9kZWw9TW9kdWxlWyJfdmFsaWRhdGVfbW9kZWwiXT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfdmFsaWRhdGVfbW9kZWwiKTt2YXIgX3B0aHJlYWRfc2VsZj1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfcHRocmVhZF9zZWxmIik7dmFyIF9mZmx1c2g9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2ZmbHVzaCIpO3ZhciBfc3RyZXJyb3I9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX3N0cmVycm9yIik7dmFyIF9fZW1zY3JpcHRlbl90bHNfaW5pdD1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2Vtc2NyaXB0ZW5fdGxzX2luaXQiKTt2YXIgX2Vtc2NyaXB0ZW5fYnVpbHRpbl9tZW1hbGlnbj1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZW1zY3JpcHRlbl9idWlsdGluX21lbWFsaWduIik7dmFyIF9fZW1zY3JpcHRlbl90aHJlYWRfaW5pdD1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2Vtc2NyaXB0ZW5fdGhyZWFkX2luaXQiKTt2YXIgX19lbXNjcmlwdGVuX3RocmVhZF9jcmFzaGVkPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1zY3JpcHRlbl90aHJlYWRfY3Jhc2hlZCIpO3ZhciBfZW1zY3JpcHRlbl9zdGFja19nZXRfZW5kPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9lbXNjcmlwdGVuX3N0YWNrX2dldF9lbmQiKTt2YXIgX2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2Jhc2U9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2Jhc2UiKTt2YXIgX19lbXNjcmlwdGVuX3J1bl9qc19vbl9tYWluX3RocmVhZD1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2Vtc2NyaXB0ZW5fcnVuX2pzX29uX21haW5fdGhyZWFkIik7dmFyIF9fZW1zY3JpcHRlbl90aHJlYWRfZnJlZV9kYXRhPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1zY3JpcHRlbl90aHJlYWRfZnJlZV9kYXRhIik7dmFyIF9fZW1zY3JpcHRlbl90aHJlYWRfZXhpdD1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2Vtc2NyaXB0ZW5fdGhyZWFkX2V4aXQiKTt2YXIgX19lbXNjcmlwdGVuX2NoZWNrX21haWxib3g9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX19lbXNjcmlwdGVuX2NoZWNrX21haWxib3giKTt2YXIgX2Vtc2NyaXB0ZW5fc3RhY2tfaW5pdD1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZW1zY3JpcHRlbl9zdGFja19pbml0Iik7dmFyIF9lbXNjcmlwdGVuX3N0YWNrX3NldF9saW1pdHM9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2Vtc2NyaXB0ZW5fc3RhY2tfc2V0X2xpbWl0cyIpO3ZhciBfZW1zY3JpcHRlbl9zdGFja19nZXRfZnJlZT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZW1zY3JpcHRlbl9zdGFja19nZXRfZnJlZSIpO3ZhciBfX2Vtc2NyaXB0ZW5fc3RhY2tfcmVzdG9yZT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2Vtc2NyaXB0ZW5fc3RhY2tfcmVzdG9yZSIpO3ZhciBfX2Vtc2NyaXB0ZW5fc3RhY2tfYWxsb2M9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX19lbXNjcmlwdGVuX3N0YWNrX2FsbG9jIik7dmFyIF9lbXNjcmlwdGVuX3N0YWNrX2dldF9jdXJyZW50PW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9lbXNjcmlwdGVuX3N0YWNrX2dldF9jdXJyZW50Iik7dmFyIF9faW5kaXJlY3RfZnVuY3Rpb25fdGFibGU9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX19pbmRpcmVjdF9mdW5jdGlvbl90YWJsZSIpO3ZhciB3YXNtVGFibGU9bWFrZUludmFsaWRFYXJseUFjY2Vzcygid2FzbVRhYmxlIik7ZnVuY3Rpb24gYXNzaWduV2FzbUV4cG9ydHMod2FzbUV4cG9ydHMpe2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9fZ2V0VHlwZU5hbWUiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9fZ2V0VHlwZU5hbWUiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1iaW5kX2luaXRpYWxpemVfYmluZGluZ3MiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9lbWJpbmRfaW5pdGlhbGl6ZV9iaW5kaW5ncyIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImdldF9jcF9tb2RlbF9zY2hlbWEiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGdldF9jcF9tb2RlbF9zY2hlbWEiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJnZXRfc2F0X3BhcmFtZXRlcnNfc2NoZW1hIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBnZXRfc2F0X3BhcmFtZXRlcnNfc2NoZW1hIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1sic29sdmVfbW9kZWwiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IHNvbHZlX21vZGVsIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1sibWFsbG9jIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBtYWxsb2MiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJmcmVlX2J1ZmZlciJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZnJlZV9idWZmZXIiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJmcmVlIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBmcmVlIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siaW50ZXJydXB0X3NvbHZlIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBpbnRlcnJ1cHRfc29sdmUiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJ2YWxpZGF0ZV9tb2RlbCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogdmFsaWRhdGVfbW9kZWwiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJwdGhyZWFkX3NlbGYiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IHB0aHJlYWRfc2VsZiIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImZmbHVzaCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZmZsdXNoIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1sic3RyZXJyb3IiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IHN0cmVycm9yIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siX2Vtc2NyaXB0ZW5fdGxzX2luaXQiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9lbXNjcmlwdGVuX3Rsc19pbml0Iik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9idWlsdGluX21lbWFsaWduIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBlbXNjcmlwdGVuX2J1aWx0aW5fbWVtYWxpZ24iKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl90aHJlYWRfaW5pdCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2Vtc2NyaXB0ZW5fdGhyZWFkX2luaXQiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl90aHJlYWRfY3Jhc2hlZCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2Vtc2NyaXB0ZW5fdGhyZWFkX2NyYXNoZWQiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX2dldF9lbmQiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2VuZCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2Jhc2UiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2Jhc2UiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl9ydW5fanNfb25fbWFpbl90aHJlYWQiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9lbXNjcmlwdGVuX3J1bl9qc19vbl9tYWluX3RocmVhZCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGEiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGEiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl90aHJlYWRfZXhpdCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2Vtc2NyaXB0ZW5fdGhyZWFkX2V4aXQiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl9jaGVja19tYWlsYm94Il0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBfZW1zY3JpcHRlbl9jaGVja19tYWlsYm94Iik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19pbml0Il0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBlbXNjcmlwdGVuX3N0YWNrX2luaXQiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX3NldF9saW1pdHMiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGVtc2NyaXB0ZW5fc3RhY2tfc2V0X2xpbWl0cyIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2ZyZWUiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2ZyZWUiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl9zdGFja19yZXN0b3JlIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBfZW1zY3JpcHRlbl9zdGFja19yZXN0b3JlIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siX2Vtc2NyaXB0ZW5fc3RhY2tfYWxsb2MiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9lbXNjcmlwdGVuX3N0YWNrX2FsbG9jIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19nZXRfY3VycmVudCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZW1zY3JpcHRlbl9zdGFja19nZXRfY3VycmVudCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9faW5kaXJlY3RfZnVuY3Rpb25fdGFibGUiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9faW5kaXJlY3RfZnVuY3Rpb25fdGFibGUiKTtfX19nZXRUeXBlTmFtZT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJfX2dldFR5cGVOYW1lIiwxKTtfX2VtYmluZF9pbml0aWFsaXplX2JpbmRpbmdzPWNyZWF0ZUV4cG9ydFdyYXBwZXIoIl9lbWJpbmRfaW5pdGlhbGl6ZV9iaW5kaW5ncyIsMCk7X2dldF9jcF9tb2RlbF9zY2hlbWE9TW9kdWxlWyJfZ2V0X2NwX21vZGVsX3NjaGVtYSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImdldF9jcF9tb2RlbF9zY2hlbWEiLDApO19nZXRfc2F0X3BhcmFtZXRlcnNfc2NoZW1hPU1vZHVsZVsiX2dldF9zYXRfcGFyYW1ldGVyc19zY2hlbWEiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJnZXRfc2F0X3BhcmFtZXRlcnNfc2NoZW1hIiwwKTtfc29sdmVfbW9kZWw9TW9kdWxlWyJfc29sdmVfbW9kZWwiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJzb2x2ZV9tb2RlbCIsNSk7X21hbGxvYz1Nb2R1bGVbIl9tYWxsb2MiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJtYWxsb2MiLDEpO19mcmVlX2J1ZmZlcj1Nb2R1bGVbIl9mcmVlX2J1ZmZlciJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImZyZWVfYnVmZmVyIiwxKTtfZnJlZT1Nb2R1bGVbIl9mcmVlIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZnJlZSIsMSk7X2ludGVycnVwdF9zb2x2ZT1Nb2R1bGVbIl9pbnRlcnJ1cHRfc29sdmUiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJpbnRlcnJ1cHRfc29sdmUiLDApO192YWxpZGF0ZV9tb2RlbD1Nb2R1bGVbIl92YWxpZGF0ZV9tb2RlbCJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoInZhbGlkYXRlX21vZGVsIiwzKTtfcHRocmVhZF9zZWxmPWNyZWF0ZUV4cG9ydFdyYXBwZXIoInB0aHJlYWRfc2VsZiIsMCk7X2ZmbHVzaD1jcmVhdGVFeHBvcnRXcmFwcGVyKCJmZmx1c2giLDEpO19zdHJlcnJvcj1jcmVhdGVFeHBvcnRXcmFwcGVyKCJzdHJlcnJvciIsMSk7X19lbXNjcmlwdGVuX3Rsc19pbml0PWNyZWF0ZUV4cG9ydFdyYXBwZXIoIl9lbXNjcmlwdGVuX3Rsc19pbml0IiwwKTtfZW1zY3JpcHRlbl9idWlsdGluX21lbWFsaWduPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImVtc2NyaXB0ZW5fYnVpbHRpbl9tZW1hbGlnbiIsMik7X19lbXNjcmlwdGVuX3RocmVhZF9pbml0PWNyZWF0ZUV4cG9ydFdyYXBwZXIoIl9lbXNjcmlwdGVuX3RocmVhZF9pbml0Iiw2KTtfX2Vtc2NyaXB0ZW5fdGhyZWFkX2NyYXNoZWQ9Y3JlYXRlRXhwb3J0V3JhcHBlcigiX2Vtc2NyaXB0ZW5fdGhyZWFkX2NyYXNoZWQiLDApO19lbXNjcmlwdGVuX3N0YWNrX2dldF9lbmQ9d2FzbUV4cG9ydHNbImVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2VuZCJdO19lbXNjcmlwdGVuX3N0YWNrX2dldF9iYXNlPXdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX2dldF9iYXNlIl07X19lbXNjcmlwdGVuX3J1bl9qc19vbl9tYWluX3RocmVhZD1jcmVhdGVFeHBvcnRXcmFwcGVyKCJfZW1zY3JpcHRlbl9ydW5fanNfb25fbWFpbl90aHJlYWQiLDUpO19fZW1zY3JpcHRlbl90aHJlYWRfZnJlZV9kYXRhPWNyZWF0ZUV4cG9ydFdyYXBwZXIoIl9lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGEiLDEpO19fZW1zY3JpcHRlbl90aHJlYWRfZXhpdD1jcmVhdGVFeHBvcnRXcmFwcGVyKCJfZW1zY3JpcHRlbl90aHJlYWRfZXhpdCIsMSk7X19lbXNjcmlwdGVuX2NoZWNrX21haWxib3g9Y3JlYXRlRXhwb3J0V3JhcHBlcigiX2Vtc2NyaXB0ZW5fY2hlY2tfbWFpbGJveCIsMCk7X2Vtc2NyaXB0ZW5fc3RhY2tfaW5pdD13YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19pbml0Il07X2Vtc2NyaXB0ZW5fc3RhY2tfc2V0X2xpbWl0cz13YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19zZXRfbGltaXRzIl07X2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2ZyZWU9d2FzbUV4cG9ydHNbImVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2ZyZWUiXTtfX2Vtc2NyaXB0ZW5fc3RhY2tfcmVzdG9yZT13YXNtRXhwb3J0c1siX2Vtc2NyaXB0ZW5fc3RhY2tfcmVzdG9yZSJdO19fZW1zY3JpcHRlbl9zdGFja19hbGxvYz13YXNtRXhwb3J0c1siX2Vtc2NyaXB0ZW5fc3RhY2tfYWxsb2MiXTtfZW1zY3JpcHRlbl9zdGFja19nZXRfY3VycmVudD13YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19nZXRfY3VycmVudCJdO19faW5kaXJlY3RfZnVuY3Rpb25fdGFibGU9d2FzbVRhYmxlPXdhc21FeHBvcnRzWyJfX2luZGlyZWN0X2Z1bmN0aW9uX3RhYmxlIl19dmFyIHdhc21JbXBvcnRzO2Z1bmN0aW9uIGFzc2lnbldhc21JbXBvcnRzKCl7d2FzbUltcG9ydHM9e0hhdmVPZmZzZXRDb252ZXJ0ZXIsX19hc3NlcnRfZmFpbDpfX19hc3NlcnRfZmFpbCxfX2N4YV90aHJvdzpfX19jeGFfdGhyb3csX19wdGhyZWFkX2NyZWF0ZV9qczpfX19wdGhyZWFkX2NyZWF0ZV9qcyxfX3N5c2NhbGxfZmNudGw2NDpfX19zeXNjYWxsX2ZjbnRsNjQsX19zeXNjYWxsX2ZzdGF0NjQ6X19fc3lzY2FsbF9mc3RhdDY0LF9fc3lzY2FsbF9pb2N0bDpfX19zeXNjYWxsX2lvY3RsLF9fc3lzY2FsbF9sc3RhdDY0Ol9fX3N5c2NhbGxfbHN0YXQ2NCxfX3N5c2NhbGxfbmV3ZnN0YXRhdDpfX19zeXNjYWxsX25ld2ZzdGF0YXQsX19zeXNjYWxsX29wZW5hdDpfX19zeXNjYWxsX29wZW5hdCxfX3N5c2NhbGxfc3RhdDY0Ol9fX3N5c2NhbGxfc3RhdDY0LF9hYm9ydF9qczpfX2Fib3J0X2pzLF9lbWJpbmRfcmVnaXN0ZXJfYmlnaW50Ol9fZW1iaW5kX3JlZ2lzdGVyX2JpZ2ludCxfZW1iaW5kX3JlZ2lzdGVyX2Jvb2w6X19lbWJpbmRfcmVnaXN0ZXJfYm9vbCxfZW1iaW5kX3JlZ2lzdGVyX2VtdmFsOl9fZW1iaW5kX3JlZ2lzdGVyX2VtdmFsLF9lbWJpbmRfcmVnaXN0ZXJfZmxvYXQ6X19lbWJpbmRfcmVnaXN0ZXJfZmxvYXQsX2VtYmluZF9yZWdpc3Rlcl9mdW5jdGlvbjpfX2VtYmluZF9yZWdpc3Rlcl9mdW5jdGlvbixfZW1iaW5kX3JlZ2lzdGVyX2ludGVnZXI6X19lbWJpbmRfcmVnaXN0ZXJfaW50ZWdlcixfZW1iaW5kX3JlZ2lzdGVyX21lbW9yeV92aWV3Ol9fZW1iaW5kX3JlZ2lzdGVyX21lbW9yeV92aWV3LF9lbWJpbmRfcmVnaXN0ZXJfc3RkX3N0cmluZzpfX2VtYmluZF9yZWdpc3Rlcl9zdGRfc3RyaW5nLF9lbWJpbmRfcmVnaXN0ZXJfc3RkX3dzdHJpbmc6X19lbWJpbmRfcmVnaXN0ZXJfc3RkX3dzdHJpbmcsX2VtYmluZF9yZWdpc3Rlcl92b2lkOl9fZW1iaW5kX3JlZ2lzdGVyX3ZvaWQsX2Vtc2NyaXB0ZW5faW5pdF9tYWluX3RocmVhZF9qczpfX2Vtc2NyaXB0ZW5faW5pdF9tYWluX3RocmVhZF9qcyxfZW1zY3JpcHRlbl9ub3RpZnlfbWFpbGJveF9wb3N0bWVzc2FnZTpfX2Vtc2NyaXB0ZW5fbm90aWZ5X21haWxib3hfcG9zdG1lc3NhZ2UsX2Vtc2NyaXB0ZW5fcmVjZWl2ZV9vbl9tYWluX3RocmVhZF9qczpfX2Vtc2NyaXB0ZW5fcmVjZWl2ZV9vbl9tYWluX3RocmVhZF9qcyxfZW1zY3JpcHRlbl90aHJlYWRfY2xlYW51cDpfX2Vtc2NyaXB0ZW5fdGhyZWFkX2NsZWFudXAsX2Vtc2NyaXB0ZW5fdGhyZWFkX21haWxib3hfYXdhaXQ6X19lbXNjcmlwdGVuX3RocmVhZF9tYWlsYm94X2F3YWl0LF9lbXNjcmlwdGVuX3RocmVhZF9zZXRfc3Ryb25ncmVmOl9fZW1zY3JpcHRlbl90aHJlYWRfc2V0X3N0cm9uZ3JlZixfbW1hcF9qczpfX21tYXBfanMsX211bm1hcF9qczpfX211bm1hcF9qcyxfdHpzZXRfanM6X190enNldF9qcyxjbG9ja190aW1lX2dldDpfY2xvY2tfdGltZV9nZXQsZW1zY3JpcHRlbl9hc21fY29uc3RfaW50Ol9lbXNjcmlwdGVuX2FzbV9jb25zdF9pbnQsZW1zY3JpcHRlbl9jaGVja19ibG9ja2luZ19hbGxvd2VkOl9lbXNjcmlwdGVuX2NoZWNrX2Jsb2NraW5nX2FsbG93ZWQsZW1zY3JpcHRlbl9lcnJuOl9lbXNjcmlwdGVuX2Vycm4sZW1zY3JpcHRlbl9leGl0X3dpdGhfbGl2ZV9ydW50aW1lOl9lbXNjcmlwdGVuX2V4aXRfd2l0aF9saXZlX3J1bnRpbWUsZW1zY3JpcHRlbl9nZXRfaGVhcF9tYXg6X2Vtc2NyaXB0ZW5fZ2V0X2hlYXBfbWF4LGVtc2NyaXB0ZW5fZ2V0X25vdzpfZW1zY3JpcHRlbl9nZXRfbm93LGVtc2NyaXB0ZW5fbnVtX2xvZ2ljYWxfY29yZXM6X2Vtc2NyaXB0ZW5fbnVtX2xvZ2ljYWxfY29yZXMsZW1zY3JpcHRlbl9wY19nZXRfZnVuY3Rpb246X2Vtc2NyaXB0ZW5fcGNfZ2V0X2Z1bmN0aW9uLGVtc2NyaXB0ZW5fcmVzaXplX2hlYXA6X2Vtc2NyaXB0ZW5fcmVzaXplX2hlYXAsZW1zY3JpcHRlbl9zdGFja19zbmFwc2hvdDpfZW1zY3JpcHRlbl9zdGFja19zbmFwc2hvdCxlbXNjcmlwdGVuX3N0YWNrX3Vud2luZF9idWZmZXI6X2Vtc2NyaXB0ZW5fc3RhY2tfdW53aW5kX2J1ZmZlcixlbnZpcm9uX2dldDpfZW52aXJvbl9nZXQsZW52aXJvbl9zaXplc19nZXQ6X2Vudmlyb25fc2l6ZXNfZ2V0LGV4aXQ6X2V4aXQsZmRfY2xvc2U6X2ZkX2Nsb3NlLGZkX3JlYWQ6X2ZkX3JlYWQsZmRfc2VlazpfZmRfc2VlayxmZF93cml0ZTpfZmRfd3JpdGUsbWVtb3J5Ondhc21NZW1vcnkscHJvY19leGl0Ol9wcm9jX2V4aXQscmFuZG9tX2dldDpfcmFuZG9tX2dldH19dmFyIGNhbGxlZFJ1bjtmdW5jdGlvbiBzdGFja0NoZWNrSW5pdCgpe2Fzc2VydCghRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCk7X2Vtc2NyaXB0ZW5fc3RhY2tfaW5pdCgpO3dyaXRlU3RhY2tDb29raWUoKX1mdW5jdGlvbiBydW4oKXtpZihydW5EZXBlbmRlbmNpZXM+MCl7ZGVwZW5kZW5jaWVzRnVsZmlsbGVkPXJ1bjtyZXR1cm59aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCl7cmVhZHlQcm9taXNlUmVzb2x2ZT8uKE1vZHVsZSk7aW5pdFJ1bnRpbWUoKTtyZXR1cm59c3RhY2tDaGVja0luaXQoKTtwcmVSdW4oKTtpZihydW5EZXBlbmRlbmNpZXM+MCl7ZGVwZW5kZW5jaWVzRnVsZmlsbGVkPXJ1bjtyZXR1cm59YXN5bmMgZnVuY3Rpb24gZG9SdW4oKXthc3NlcnQoIWNhbGxlZFJ1bik7Y2FsbGVkUnVuPXRydWU7TW9kdWxlWyJjYWxsZWRSdW4iXT10cnVlO2lmKEFCT1JUKXJldHVybjtpbml0UnVudGltZSgpO3JlYWR5UHJvbWlzZVJlc29sdmU/LihNb2R1bGUpO01vZHVsZVsib25SdW50aW1lSW5pdGlhbGl6ZWQiXT8uKCk7Y29uc3VtZWRNb2R1bGVQcm9wKCJvblJ1bnRpbWVJbml0aWFsaXplZCIpO2Fzc2VydCghTW9kdWxlWyJfbWFpbiJdLCdjb21waWxlZCB3aXRob3V0IGEgbWFpbiwgYnV0IG9uZSBpcyBwcmVzZW50LiBpZiB5b3UgYWRkZWQgaXQgZnJvbSBKUywgdXNlIE1vZHVsZVsib25SdW50aW1lSW5pdGlhbGl6ZWQiXScpO3Bvc3RSdW4oKX1pZihNb2R1bGVbInNldFN0YXR1cyJdKXtNb2R1bGVbInNldFN0YXR1cyJdKCJSdW5uaW5nLi4uIik7c2V0VGltZW91dCgoKT0+e3NldFRpbWVvdXQoKCk9Pk1vZHVsZVsic2V0U3RhdHVzIl0oIiIpLDEpO2RvUnVuKCl9LDEpfWVsc2V7ZG9SdW4oKX1jaGVja1N0YWNrQ29va2llKCl9ZnVuY3Rpb24gY2hlY2tVbmZsdXNoZWRDb250ZW50KCl7dmFyIG9sZE91dD1vdXQ7dmFyIG9sZEVycj1lcnI7dmFyIGhhcz1mYWxzZTtvdXQ9ZXJyPXg9PntoYXM9dHJ1ZX07dHJ5e19mZmx1c2goMCk7Zm9yKHZhciBuYW1lIG9mWyJzdGRvdXQiLCJzdGRlcnIiXSl7dmFyIGluZm89RlMuYW5hbHl6ZVBhdGgoIi9kZXYvIituYW1lKTtpZighaW5mbylyZXR1cm47dmFyIHN0cmVhbT1pbmZvLm9iamVjdDt2YXIgcmRldj1zdHJlYW0ucmRldjt2YXIgdHR5PVRUWS50dHlzW3JkZXZdO2lmKHR0eT8ub3V0cHV0Py5sZW5ndGgpe2hhcz10cnVlfX19Y2F0Y2goZSl7fW91dD1vbGRPdXQ7ZXJyPW9sZEVycjtpZihoYXMpe3dhcm5PbmNlKCJzdGRpbyBzdHJlYW1zIGhhZCBjb250ZW50IGluIHRoZW0gdGhhdCB3YXMgbm90IGZsdXNoZWQuIHlvdSBzaG91bGQgc2V0IEVYSVRfUlVOVElNRSB0byAxIChzZWUgdGhlIEVtc2NyaXB0ZW4gRkFRKSwgb3IgbWFrZSBzdXJlIHRvIGVtaXQgYSBuZXdsaW5lIHdoZW4geW91IHByaW50ZiBldGMuIil9fXZhciB3YXNtRXhwb3J0cztpZighRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCl7d2FzbUV4cG9ydHM9YXdhaXQgKGNyZWF0ZVdhc20oKSk7cnVuKCl9aWYocnVudGltZUluaXRpYWxpemVkKXttb2R1bGVSdG49TW9kdWxlfWVsc2V7bW9kdWxlUnRuPW5ldyBQcm9taXNlKChyZXNvbHZlLHJlamVjdCk9PntyZWFkeVByb21pc2VSZXNvbHZlPXJlc29sdmU7cmVhZHlQcm9taXNlUmVqZWN0PXJlamVjdH0pfWZvcihjb25zdCBwcm9wIG9mIE9iamVjdC5rZXlzKE1vZHVsZSkpe2lmKCEocHJvcCBpbiBtb2R1bGVBcmcpKXtPYmplY3QuZGVmaW5lUHJvcGVydHkobW9kdWxlQXJnLHByb3Ase2NvbmZpZ3VyYWJsZTp0cnVlLGdldCgpe2Fib3J0KGBBY2Nlc3MgdG8gbW9kdWxlIHByb3BlcnR5ICgnJHtwcm9wfScpIGlzIG5vIGxvbmdlciBwb3NzaWJsZSB2aWEgdGhlIG1vZHVsZSBjb25zdHJ1Y3RvciBhcmd1bWVudDsgSW5zdGVhZCwgdXNlIHRoZSByZXN1bHQgb2YgdGhlIG1vZHVsZSBjb25zdHJ1Y3Rvci5gKX19KX19CjtyZXR1cm4gbW9kdWxlUnRufWV4cG9ydCBkZWZhdWx0IGNwU2F0TW9kdWxlO3ZhciBpc1B0aHJlYWQ9Z2xvYmFsVGhpcy5zZWxmPy5uYW1lPy5zdGFydHNXaXRoKCJlbS1wdGhyZWFkIik7aXNQdGhyZWFkJiZjcFNhdE1vZHVsZSgpOwo=", import.meta.url),
      /* @vite-ignore */
      { type: "module", name: "em-pthread-" + p.nextWorkerID }
    );
    l.workerID = p.nextWorkerID++, p.unusedWorkers.push(l);
  }, getNewWorker() {
    if (p.unusedWorkers.length == 0) {
      u("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.");
      return;
    }
    return p.unusedWorkers.pop();
  } }, EZ = [], Wd = (l) => EZ.push(l);
  function Vd(l) {
    var Z = (i(), y)[l + 52 >> 2], c = (i(), y)[l + 56 >> 2], d = Z - c;
    V(Z != 0), V(d != 0), V(Z > d, "stackHigh must be higher then stackLow"), Mc(Z, d), $l(Z), FZ();
  }
  var cZ = [], MZ = (l) => {
    var Z = cZ[l];
    return Z || (cZ[l] = Z = jc.get(l), M.isAsyncExport(Z) && (cZ[l] = Z = M.makeAsyncFunction(Z))), Z;
  }, LZ = async (l, Z) => {
    il = 0, dZ = 0;
    var c = WebAssembly.promising(MZ(l))(Z);
    ul();
    function d(b) {
      if (kl()) {
        nl = b;
        return;
      }
      yZ(b);
    }
    c = await c, d(c);
  };
  LZ.isAsync = !0;
  var dZ = !0, id = (l) => p.tlsInitFunctions.push(l), $ = (l) => {
    $.shown ||= {}, $.shown[l] || ($.shown[l] = 1, u(l));
  }, C, QZ = globalThis.TextDecoder && new TextDecoder(), jZ = (l, Z, c, d) => {
    var b = Z + c;
    if (d) return b;
    for (; l[Z] && !(Z >= b); ) ++Z;
    return Z;
  }, al = (l, Z = 0, c, d) => {
    var b = jZ(l, Z, c, d);
    if (b - Z > 16 && l.buffer && QZ)
      return QZ.decode(l.buffer instanceof ArrayBuffer ? l.subarray(Z, b) : l.slice(Z, b));
    for (var t = ""; Z < b; ) {
      var m = l[Z++];
      if (!(m & 128)) {
        t += String.fromCharCode(m);
        continue;
      }
      var n = l[Z++] & 63;
      if ((m & 224) == 192) {
        t += String.fromCharCode((m & 31) << 6 | n);
        continue;
      }
      var W = l[Z++] & 63;
      if ((m & 240) == 224 ? m = (m & 15) << 12 | n << 6 | W : ((m & 248) != 240 && $("Invalid UTF-8 leading byte " + el(m) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), m = (m & 7) << 18 | n << 12 | W << 6 | l[Z++] & 63), m < 65536)
        t += String.fromCharCode(m);
      else {
        var X = m - 65536;
        t += String.fromCharCode(55296 | X >> 10, 56320 | X & 1023);
      }
    }
    return t;
  }, Q = (l, Z, c) => (V(typeof l == "number", `UTF8ToString expects a number (got ${typeof l})`), l ? al((i(), K), l, Z, c) : ""), ad = (l, Z, c, d) => N(`Assertion failed: ${Q(l)}, at: ` + [Z ? Q(Z) : "unknown filename", c, d ? Q(d) : "unknown function"]);
  class Xd {
    constructor(Z) {
      this.excPtr = Z, this.ptr = Z - 24;
    }
    set_type(Z) {
      (i(), y)[this.ptr + 4 >> 2] = Z;
    }
    get_type() {
      return (i(), y)[this.ptr + 4 >> 2];
    }
    set_destructor(Z) {
      (i(), y)[this.ptr + 8 >> 2] = Z;
    }
    get_destructor() {
      return (i(), y)[this.ptr + 8 >> 2];
    }
    set_caught(Z) {
      Z = Z ? 1 : 0, (i(), g)[this.ptr + 12] = Z;
    }
    get_caught() {
      return (i(), g)[this.ptr + 12] != 0;
    }
    set_rethrown(Z) {
      Z = Z ? 1 : 0, (i(), g)[this.ptr + 13] = Z;
    }
    get_rethrown() {
      return (i(), g)[this.ptr + 13] != 0;
    }
    init(Z, c) {
      this.set_adjusted_ptr(0), this.set_type(Z), this.set_destructor(c);
    }
    set_adjusted_ptr(Z) {
      (i(), y)[this.ptr + 16 >> 2] = Z;
    }
    get_adjusted_ptr() {
      return (i(), y)[this.ptr + 16 >> 2];
    }
  }
  var rd = (l, Z, c) => {
    var d = new Xd(l);
    d.init(Z, c), V(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
  };
  function PZ(l, Z, c, d) {
    return s ? f(2, 0, 1, l, Z, c, d) : OZ(l, Z, c, d);
  }
  var Gd = () => !!globalThis.SharedArrayBuffer, OZ = (l, Z, c, d) => {
    if (!Gd())
      return Pl("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
    var b = [], t = 0;
    if (s && (b.length === 0 || t))
      return PZ(l, Z, c, d);
    var m = { startRoutine: c, pthread_ptr: l, arg: d, transferList: b };
    return s ? (m.cmd = "spawnThread", postMessage(m, b), 0) : wZ(m);
  }, Hl = () => {
    V(v.varargs != null);
    var l = (i(), U)[+v.varargs >> 2];
    return v.varargs += 4, l;
  }, Xl = Hl, J = { isAbs: (l) => l.charAt(0) === "/", splitPath: (l) => {
    var Z = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
    return Z.exec(l).slice(1);
  }, normalizeArray: (l, Z) => {
    for (var c = 0, d = l.length - 1; d >= 0; d--) {
      var b = l[d];
      b === "." ? l.splice(d, 1) : b === ".." ? (l.splice(d, 1), c++) : c && (l.splice(d, 1), c--);
    }
    if (Z)
      for (; c; c--)
        l.unshift("..");
    return l;
  }, normalize: (l) => {
    var Z = J.isAbs(l), c = l.slice(-1) === "/";
    return l = J.normalizeArray(l.split("/").filter((d) => !!d), !Z).join("/"), !l && !Z && (l = "."), l && c && (l += "/"), (Z ? "/" : "") + l;
  }, dirname: (l) => {
    var Z = J.splitPath(l), c = Z[0], d = Z[1];
    return !c && !d ? "." : (d && (d = d.slice(0, -1)), c + d);
  }, basename: (l) => l && l.match(/([^\/]+|\/)\/*$/)[1], join: (...l) => J.normalize(l.join("/")), join2: (l, Z) => J.normalize(l + "/" + Z) }, yd = () => (l) => l.set(crypto.getRandomValues(new Uint8Array(l.byteLength))), eZ = (l) => {
    (eZ = yd())(l);
  }, rl = { resolve: (...l) => {
    for (var Z = "", c = !1, d = l.length - 1; d >= -1 && !c; d--) {
      var b = d >= 0 ? l[d] : e.cwd();
      if (typeof b != "string")
        throw new TypeError("Arguments to path.resolve must be strings");
      if (!b)
        return "";
      Z = b + "/" + Z, c = J.isAbs(b);
    }
    return Z = J.normalizeArray(Z.split("/").filter((t) => !!t), !c).join("/"), (c ? "/" : "") + Z || ".";
  }, relative: (l, Z) => {
    l = rl.resolve(l).slice(1), Z = rl.resolve(Z).slice(1);
    function c(X) {
      for (var G = 0; G < X.length && X[G] === ""; G++)
        ;
      for (var R = X.length - 1; R >= 0 && X[R] === ""; R--)
        ;
      return G > R ? [] : X.slice(G, R - G + 1);
    }
    for (var d = c(l.split("/")), b = c(Z.split("/")), t = Math.min(d.length, b.length), m = t, n = 0; n < t; n++)
      if (d[n] !== b[n]) {
        m = n;
        break;
      }
    for (var W = [], n = m; n < d.length; n++)
      W.push("..");
    return W = W.concat(b.slice(m)), W.join("/");
  } }, bZ = [], bl = (l) => {
    for (var Z = 0, c = 0; c < l.length; ++c) {
      var d = l.charCodeAt(c);
      d <= 127 ? Z++ : d <= 2047 ? Z += 2 : d >= 55296 && d <= 57343 ? (Z += 4, ++c) : Z += 3;
    }
    return Z;
  }, _Z = (l, Z, c, d) => {
    if (V(typeof l == "string", `stringToUTF8Array expects a string (got ${typeof l})`), !(d > 0)) return 0;
    for (var b = c, t = c + d - 1, m = 0; m < l.length; ++m) {
      var n = l.codePointAt(m);
      if (n <= 127) {
        if (c >= t) break;
        Z[c++] = n;
      } else if (n <= 2047) {
        if (c + 1 >= t) break;
        Z[c++] = 192 | n >> 6, Z[c++] = 128 | n & 63;
      } else if (n <= 65535) {
        if (c + 2 >= t) break;
        Z[c++] = 224 | n >> 12, Z[c++] = 128 | n >> 6 & 63, Z[c++] = 128 | n & 63;
      } else {
        if (c + 3 >= t) break;
        n > 1114111 && $("Invalid Unicode code point " + el(n) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), Z[c++] = 240 | n >> 18, Z[c++] = 128 | n >> 12 & 63, Z[c++] = 128 | n >> 6 & 63, Z[c++] = 128 | n & 63, m++;
      }
    }
    return Z[c] = 0, c - b;
  }, tZ = (l, Z, c) => {
    var d = bl(l) + 1, b = new Array(d), t = _Z(l, b, 0, b.length);
    return b.length = t, b;
  }, od = () => {
    if (!bZ.length) {
      var l = null;
      if (globalThis.window?.prompt && (l = window.prompt("Input: "), l !== null && (l += `
`)), !l)
        return null;
      bZ = tZ(l);
    }
    return bZ.shift();
  }, q = { ttys: [], init() {
  }, shutdown() {
  }, register(l, Z) {
    q.ttys[l] = { input: [], output: [], ops: Z }, e.registerDevice(l, q.stream_ops);
  }, stream_ops: { open(l) {
    var Z = q.ttys[l.node.rdev];
    if (!Z)
      throw new e.ErrnoError(43);
    l.tty = Z, l.seekable = !1;
  }, close(l) {
    l.tty.ops.fsync(l.tty);
  }, fsync(l) {
    l.tty.ops.fsync(l.tty);
  }, read(l, Z, c, d, b) {
    if (!l.tty || !l.tty.ops.get_char)
      throw new e.ErrnoError(60);
    for (var t = 0, m = 0; m < d; m++) {
      var n;
      try {
        n = l.tty.ops.get_char(l.tty);
      } catch {
        throw new e.ErrnoError(29);
      }
      if (n === void 0 && t === 0)
        throw new e.ErrnoError(6);
      if (n == null) break;
      t++, Z[c + m] = n;
    }
    return t && (l.node.atime = Date.now()), t;
  }, write(l, Z, c, d, b) {
    if (!l.tty || !l.tty.ops.put_char)
      throw new e.ErrnoError(60);
    try {
      for (var t = 0; t < d; t++)
        l.tty.ops.put_char(l.tty, Z[c + t]);
    } catch {
      throw new e.ErrnoError(29);
    }
    return d && (l.node.mtime = l.node.ctime = Date.now()), t;
  } }, default_tty_ops: { get_char(l) {
    return od();
  }, put_char(l, Z) {
    Z === null || Z === 10 ? (A(al(l.output)), l.output = []) : Z != 0 && l.output.push(Z);
  }, fsync(l) {
    l.output?.length > 0 && (A(al(l.output)), l.output = []);
  }, ioctl_tcgets(l) {
    return { c_iflag: 25856, c_oflag: 5, c_cflag: 191, c_lflag: 35387, c_cc: [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] };
  }, ioctl_tcsets(l, Z, c) {
    return 0;
  }, ioctl_tiocgwinsz(l) {
    return [24, 80];
  } }, default_tty1_ops: { put_char(l, Z) {
    Z === null || Z === 10 ? (u(al(l.output)), l.output = []) : Z != 0 && l.output.push(Z);
  }, fsync(l) {
    l.output?.length > 0 && (u(al(l.output)), l.output = []);
  } } }, Rd = (l, Z) => (i(), K).fill(0, l, l + Z), DZ = (l, Z) => (V(Z, "alignment argument is required"), Math.ceil(l / Z) * Z), AZ = (l) => {
    l = DZ(l, 65536);
    var Z = Sc(65536, l);
    return Z && Rd(Z, l), Z;
  }, h = { ops_table: null, mount(l) {
    return h.createNode(null, "/", 16895, 0);
  }, createNode(l, Z, c, d) {
    if (e.isBlkdev(c) || e.isFIFO(c))
      throw new e.ErrnoError(63);
    h.ops_table ||= { dir: { node: { getattr: h.node_ops.getattr, setattr: h.node_ops.setattr, lookup: h.node_ops.lookup, mknod: h.node_ops.mknod, rename: h.node_ops.rename, unlink: h.node_ops.unlink, rmdir: h.node_ops.rmdir, readdir: h.node_ops.readdir, symlink: h.node_ops.symlink }, stream: { llseek: h.stream_ops.llseek } }, file: { node: { getattr: h.node_ops.getattr, setattr: h.node_ops.setattr }, stream: { llseek: h.stream_ops.llseek, read: h.stream_ops.read, write: h.stream_ops.write, mmap: h.stream_ops.mmap, msync: h.stream_ops.msync } }, link: { node: { getattr: h.node_ops.getattr, setattr: h.node_ops.setattr, readlink: h.node_ops.readlink }, stream: {} }, chrdev: { node: { getattr: h.node_ops.getattr, setattr: h.node_ops.setattr }, stream: e.chrdev_stream_ops } };
    var b = e.createNode(l, Z, c, d);
    return e.isDir(b.mode) ? (b.node_ops = h.ops_table.dir.node, b.stream_ops = h.ops_table.dir.stream, b.contents = {}) : e.isFile(b.mode) ? (b.node_ops = h.ops_table.file.node, b.stream_ops = h.ops_table.file.stream, b.usedBytes = 0, b.contents = null) : e.isLink(b.mode) ? (b.node_ops = h.ops_table.link.node, b.stream_ops = h.ops_table.link.stream) : e.isChrdev(b.mode) && (b.node_ops = h.ops_table.chrdev.node, b.stream_ops = h.ops_table.chrdev.stream), b.atime = b.mtime = b.ctime = Date.now(), l && (l.contents[Z] = b, l.atime = l.mtime = l.ctime = b.atime), b;
  }, getFileDataAsTypedArray(l) {
    return l.contents ? l.contents.subarray ? l.contents.subarray(0, l.usedBytes) : new Uint8Array(l.contents) : new Uint8Array(0);
  }, expandFileStorage(l, Z) {
    var c = l.contents ? l.contents.length : 0;
    if (!(c >= Z)) {
      var d = 1024 * 1024;
      Z = Math.max(Z, c * (c < d ? 2 : 1.125) >>> 0), c != 0 && (Z = Math.max(Z, 256));
      var b = l.contents;
      l.contents = new Uint8Array(Z), l.usedBytes > 0 && l.contents.set(b.subarray(0, l.usedBytes), 0);
    }
  }, resizeFileStorage(l, Z) {
    if (l.usedBytes != Z)
      if (Z == 0)
        l.contents = null, l.usedBytes = 0;
      else {
        var c = l.contents;
        l.contents = new Uint8Array(Z), c && l.contents.set(c.subarray(0, Math.min(Z, l.usedBytes))), l.usedBytes = Z;
      }
  }, node_ops: { getattr(l) {
    var Z = {};
    return Z.dev = e.isChrdev(l.mode) ? l.id : 1, Z.ino = l.id, Z.mode = l.mode, Z.nlink = 1, Z.uid = 0, Z.gid = 0, Z.rdev = l.rdev, e.isDir(l.mode) ? Z.size = 4096 : e.isFile(l.mode) ? Z.size = l.usedBytes : e.isLink(l.mode) ? Z.size = l.link.length : Z.size = 0, Z.atime = new Date(l.atime), Z.mtime = new Date(l.mtime), Z.ctime = new Date(l.ctime), Z.blksize = 4096, Z.blocks = Math.ceil(Z.size / Z.blksize), Z;
  }, setattr(l, Z) {
    for (const c of ["mode", "atime", "mtime", "ctime"])
      Z[c] != null && (l[c] = Z[c]);
    Z.size !== void 0 && h.resizeFileStorage(l, Z.size);
  }, lookup(l, Z) {
    throw new e.ErrnoError(44);
  }, mknod(l, Z, c, d) {
    return h.createNode(l, Z, c, d);
  }, rename(l, Z, c) {
    var d;
    try {
      d = e.lookupNode(Z, c);
    } catch {
    }
    if (d) {
      if (e.isDir(l.mode))
        for (var b in d.contents)
          throw new e.ErrnoError(55);
      e.hashRemoveNode(d);
    }
    delete l.parent.contents[l.name], Z.contents[c] = l, l.name = c, Z.ctime = Z.mtime = l.parent.ctime = l.parent.mtime = Date.now();
  }, unlink(l, Z) {
    delete l.contents[Z], l.ctime = l.mtime = Date.now();
  }, rmdir(l, Z) {
    var c = e.lookupNode(l, Z);
    for (var d in c.contents)
      throw new e.ErrnoError(55);
    delete l.contents[Z], l.ctime = l.mtime = Date.now();
  }, readdir(l) {
    return [".", "..", ...Object.keys(l.contents)];
  }, symlink(l, Z, c) {
    var d = h.createNode(l, Z, 41471, 0);
    return d.link = c, d;
  }, readlink(l) {
    if (!e.isLink(l.mode))
      throw new e.ErrnoError(28);
    return l.link;
  } }, stream_ops: { read(l, Z, c, d, b) {
    var t = l.node.contents;
    if (b >= l.node.usedBytes) return 0;
    var m = Math.min(l.node.usedBytes - b, d);
    if (V(m >= 0), m > 8 && t.subarray)
      Z.set(t.subarray(b, b + m), c);
    else
      for (var n = 0; n < m; n++) Z[c + n] = t[b + n];
    return m;
  }, write(l, Z, c, d, b, t) {
    if (V(!(Z instanceof ArrayBuffer)), Z.buffer === (i(), g).buffer && (t = !1), !d) return 0;
    var m = l.node;
    if (m.mtime = m.ctime = Date.now(), Z.subarray && (!m.contents || m.contents.subarray)) {
      if (t)
        return V(b === 0, "canOwn must imply no weird position inside the file"), m.contents = Z.subarray(c, c + d), m.usedBytes = d, d;
      if (m.usedBytes === 0 && b === 0)
        return m.contents = Z.slice(c, c + d), m.usedBytes = d, d;
      if (b + d <= m.usedBytes)
        return m.contents.set(Z.subarray(c, c + d), b), d;
    }
    if (h.expandFileStorage(m, b + d), m.contents.subarray && Z.subarray)
      m.contents.set(Z.subarray(c, c + d), b);
    else
      for (var n = 0; n < d; n++)
        m.contents[b + n] = Z[c + n];
    return m.usedBytes = Math.max(m.usedBytes, b + d), d;
  }, llseek(l, Z, c) {
    var d = Z;
    if (c === 1 ? d += l.position : c === 2 && e.isFile(l.node.mode) && (d += l.node.usedBytes), d < 0)
      throw new e.ErrnoError(28);
    return d;
  }, mmap(l, Z, c, d, b) {
    if (!e.isFile(l.node.mode))
      throw new e.ErrnoError(43);
    var t, m, n = l.node.contents;
    if (!(b & 2) && n && n.buffer === (i(), g).buffer)
      m = !1, t = n.byteOffset;
    else {
      if (m = !0, t = AZ(Z), !t)
        throw new e.ErrnoError(48);
      n && ((c > 0 || c + Z < n.length) && (n.subarray ? n = n.subarray(c, c + Z) : n = Array.prototype.slice.call(n, c, c + Z)), (i(), g).set(n, t));
    }
    return { ptr: t, allocated: m };
  }, msync(l, Z, c, d, b) {
    return h.stream_ops.write(l, Z, 0, d, c, !1), 0;
  } } }, sd = (l) => {
    var Z = { r: 0, "r+": 2, w: 577, "w+": 578, a: 1089, "a+": 1090 }, c = Z[l];
    if (typeof c > "u")
      throw new Error(`Unknown file open mode: ${l}`);
    return c;
  }, mZ = (l, Z) => {
    var c = 0;
    return l && (c |= 365), Z && (c |= 146), c;
  }, hd = (l) => Q(zc(l)), $Z = { EPERM: 63, ENOENT: 44, ESRCH: 71, EINTR: 27, EIO: 29, ENXIO: 60, E2BIG: 1, ENOEXEC: 45, EBADF: 8, ECHILD: 12, EAGAIN: 6, EWOULDBLOCK: 6, ENOMEM: 48, EACCES: 2, EFAULT: 21, ENOTBLK: 105, EBUSY: 10, EEXIST: 20, EXDEV: 75, ENODEV: 43, ENOTDIR: 54, EISDIR: 31, EINVAL: 28, ENFILE: 41, EMFILE: 33, ENOTTY: 59, ETXTBSY: 74, EFBIG: 22, ENOSPC: 51, ESPIPE: 70, EROFS: 69, EMLINK: 34, EPIPE: 64, EDOM: 18, ERANGE: 68, ENOMSG: 49, EIDRM: 24, ECHRNG: 106, EL2NSYNC: 156, EL3HLT: 107, EL3RST: 108, ELNRNG: 109, EUNATCH: 110, ENOCSI: 111, EL2HLT: 112, EDEADLK: 16, ENOLCK: 46, EBADE: 113, EBADR: 114, EXFULL: 115, ENOANO: 104, EBADRQC: 103, EBADSLT: 102, EDEADLOCK: 16, EBFONT: 101, ENOSTR: 100, ENODATA: 116, ETIME: 117, ENOSR: 118, ENONET: 119, ENOPKG: 120, EREMOTE: 121, ENOLINK: 47, EADV: 122, ESRMNT: 123, ECOMM: 124, EPROTO: 65, EMULTIHOP: 36, EDOTDOT: 125, EBADMSG: 9, ENOTUNIQ: 126, EBADFD: 127, EREMCHG: 128, ELIBACC: 129, ELIBBAD: 130, ELIBSCN: 131, ELIBMAX: 132, ELIBEXEC: 133, ENOSYS: 52, ENOTEMPTY: 55, ENAMETOOLONG: 37, ELOOP: 32, EOPNOTSUPP: 138, EPFNOSUPPORT: 139, ECONNRESET: 15, ENOBUFS: 42, EAFNOSUPPORT: 5, EPROTOTYPE: 67, ENOTSOCK: 57, ENOPROTOOPT: 50, ESHUTDOWN: 140, ECONNREFUSED: 14, EADDRINUSE: 3, ECONNABORTED: 13, ENETUNREACH: 40, ENETDOWN: 38, ETIMEDOUT: 73, EHOSTDOWN: 142, EHOSTUNREACH: 23, EINPROGRESS: 26, EALREADY: 7, EDESTADDRREQ: 17, EMSGSIZE: 35, EPROTONOSUPPORT: 66, ESOCKTNOSUPPORT: 137, EADDRNOTAVAIL: 4, ENETRESET: 39, EISCONN: 30, ENOTCONN: 53, ETOOMANYREFS: 141, EUSERS: 136, EDQUOT: 19, ESTALE: 72, ENOTSUP: 138, ENOMEDIUM: 148, EILSEQ: 25, EOVERFLOW: 61, ECANCELED: 11, ENOTRECOVERABLE: 56, EOWNERDEAD: 62, ESTRPIPE: 135 }, ud = async (l) => {
    var Z = await jl(l);
    return V(Z, `Loading data file "${l}" failed (no arrayBuffer).`), new Uint8Array(Z);
  }, Fd = (...l) => e.createDataFile(...l), pd = (l) => {
    for (var Z = l; ; ) {
      if (!Vl[l]) return l;
      l = Z + Math.random();
    }
  }, qZ = [], Yd = async (l, Z) => {
    typeof Browser < "u" && Browser.init();
    for (var c of qZ)
      if (c.canHandle(Z))
        return V(c.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), c.handle(l, Z);
    return l;
  }, lc = async (l, Z, c, d, b, t, m, n) => {
    var W = Z ? rl.resolve(J.join2(l, Z)) : l, X = pd(`cp ${W}`);
    KZ(X);
    try {
      var G = c;
      typeof c == "string" && (G = await ud(c)), G = await Yd(G, W), n?.(), t || Fd(l, Z, G, d, b, m);
    } finally {
      SZ(X);
    }
  }, Jd = (l, Z, c, d, b, t, m, n, W, X) => {
    lc(l, Z, c, d, b, n, W, X).then(t).catch(m);
  }, e = { root: null, mounts: [], devices: {}, streams: [], nextInode: 1, nameTable: null, currentPath: "/", initialized: !1, ignorePermissions: !0, filesystems: null, syncFSRequests: 0, readFiles: {}, ErrnoError: class extends Error {
    name = "ErrnoError";
    constructor(l) {
      super(Wl ? hd(l) : ""), this.errno = l;
      for (var Z in $Z)
        if ($Z[Z] === l) {
          this.code = Z;
          break;
        }
    }
  }, FSStream: class {
    shared = {};
    get object() {
      return this.node;
    }
    set object(l) {
      this.node = l;
    }
    get isRead() {
      return (this.flags & 2097155) !== 1;
    }
    get isWrite() {
      return (this.flags & 2097155) !== 0;
    }
    get isAppend() {
      return this.flags & 1024;
    }
    get flags() {
      return this.shared.flags;
    }
    set flags(l) {
      this.shared.flags = l;
    }
    get position() {
      return this.shared.position;
    }
    set position(l) {
      this.shared.position = l;
    }
  }, FSNode: class {
    node_ops = {};
    stream_ops = {};
    readMode = 365;
    writeMode = 146;
    mounted = null;
    constructor(l, Z, c, d) {
      l || (l = this), this.parent = l, this.mount = l.mount, this.id = e.nextInode++, this.name = Z, this.mode = c, this.rdev = d, this.atime = this.mtime = this.ctime = Date.now();
    }
    get read() {
      return (this.mode & this.readMode) === this.readMode;
    }
    set read(l) {
      l ? this.mode |= this.readMode : this.mode &= ~this.readMode;
    }
    get write() {
      return (this.mode & this.writeMode) === this.writeMode;
    }
    set write(l) {
      l ? this.mode |= this.writeMode : this.mode &= ~this.writeMode;
    }
    get isFolder() {
      return e.isDir(this.mode);
    }
    get isDevice() {
      return e.isChrdev(this.mode);
    }
  }, lookupPath(l, Z = {}) {
    if (!l)
      throw new e.ErrnoError(44);
    Z.follow_mount ??= !0, J.isAbs(l) || (l = e.cwd() + "/" + l);
    l: for (var c = 0; c < 40; c++) {
      for (var d = l.split("/").filter((X) => !!X), b = e.root, t = "/", m = 0; m < d.length; m++) {
        var n = m === d.length - 1;
        if (n && Z.parent)
          break;
        if (d[m] !== ".") {
          if (d[m] === "..") {
            if (t = J.dirname(t), e.isRoot(b)) {
              l = t + "/" + d.slice(m + 1).join("/"), c--;
              continue l;
            } else
              b = b.parent;
            continue;
          }
          t = J.join2(t, d[m]);
          try {
            b = e.lookupNode(b, d[m]);
          } catch (X) {
            if (X?.errno === 44 && n && Z.noent_okay)
              return { path: t };
            throw X;
          }
          if (e.isMountpoint(b) && (!n || Z.follow_mount) && (b = b.mounted.root), e.isLink(b.mode) && (!n || Z.follow)) {
            if (!b.node_ops.readlink)
              throw new e.ErrnoError(52);
            var W = b.node_ops.readlink(b);
            J.isAbs(W) || (W = J.dirname(t) + "/" + W), l = W + "/" + d.slice(m + 1).join("/");
            continue l;
          }
        }
      }
      return { path: t, node: b };
    }
    throw new e.ErrnoError(32);
  }, getPath(l) {
    for (var Z; ; ) {
      if (e.isRoot(l)) {
        var c = l.mount.mountpoint;
        return Z ? c[c.length - 1] !== "/" ? `${c}/${Z}` : c + Z : c;
      }
      Z = Z ? `${l.name}/${Z}` : l.name, l = l.parent;
    }
  }, hashName(l, Z) {
    for (var c = 0, d = 0; d < Z.length; d++)
      c = (c << 5) - c + Z.charCodeAt(d) | 0;
    return (l + c >>> 0) % e.nameTable.length;
  }, hashAddNode(l) {
    var Z = e.hashName(l.parent.id, l.name);
    l.name_next = e.nameTable[Z], e.nameTable[Z] = l;
  }, hashRemoveNode(l) {
    var Z = e.hashName(l.parent.id, l.name);
    if (e.nameTable[Z] === l)
      e.nameTable[Z] = l.name_next;
    else
      for (var c = e.nameTable[Z]; c; ) {
        if (c.name_next === l) {
          c.name_next = l.name_next;
          break;
        }
        c = c.name_next;
      }
  }, lookupNode(l, Z) {
    var c = e.mayLookup(l);
    if (c)
      throw new e.ErrnoError(c);
    for (var d = e.hashName(l.id, Z), b = e.nameTable[d]; b; b = b.name_next) {
      var t = b.name;
      if (b.parent.id === l.id && t === Z)
        return b;
    }
    return e.lookup(l, Z);
  }, createNode(l, Z, c, d) {
    V(typeof l == "object");
    var b = new e.FSNode(l, Z, c, d);
    return e.hashAddNode(b), b;
  }, destroyNode(l) {
    e.hashRemoveNode(l);
  }, isRoot(l) {
    return l === l.parent;
  }, isMountpoint(l) {
    return !!l.mounted;
  }, isFile(l) {
    return (l & 61440) === 32768;
  }, isDir(l) {
    return (l & 61440) === 16384;
  }, isLink(l) {
    return (l & 61440) === 40960;
  }, isChrdev(l) {
    return (l & 61440) === 8192;
  }, isBlkdev(l) {
    return (l & 61440) === 24576;
  }, isFIFO(l) {
    return (l & 61440) === 4096;
  }, isSocket(l) {
    return (l & 49152) === 49152;
  }, flagsToPermissionString(l) {
    var Z = ["r", "w", "rw"][l & 3];
    return l & 512 && (Z += "w"), Z;
  }, nodePermissions(l, Z) {
    return e.ignorePermissions ? 0 : Z.includes("r") && !(l.mode & 292) || Z.includes("w") && !(l.mode & 146) || Z.includes("x") && !(l.mode & 73) ? 2 : 0;
  }, mayLookup(l) {
    if (!e.isDir(l.mode)) return 54;
    var Z = e.nodePermissions(l, "x");
    return Z || (l.node_ops.lookup ? 0 : 2);
  }, mayCreate(l, Z) {
    if (!e.isDir(l.mode))
      return 54;
    try {
      var c = e.lookupNode(l, Z);
      return 20;
    } catch {
    }
    return e.nodePermissions(l, "wx");
  }, mayDelete(l, Z, c) {
    var d;
    try {
      d = e.lookupNode(l, Z);
    } catch (t) {
      return t.errno;
    }
    var b = e.nodePermissions(l, "wx");
    if (b)
      return b;
    if (c) {
      if (!e.isDir(d.mode))
        return 54;
      if (e.isRoot(d) || e.getPath(d) === e.cwd())
        return 10;
    } else if (e.isDir(d.mode))
      return 31;
    return 0;
  }, mayOpen(l, Z) {
    return l ? e.isLink(l.mode) ? 32 : e.isDir(l.mode) && (e.flagsToPermissionString(Z) !== "r" || Z & 576) ? 31 : e.nodePermissions(l, e.flagsToPermissionString(Z)) : 44;
  }, checkOpExists(l, Z) {
    if (!l)
      throw new e.ErrnoError(Z);
    return l;
  }, MAX_OPEN_FDS: 4096, nextfd() {
    for (var l = 0; l <= e.MAX_OPEN_FDS; l++)
      if (!e.streams[l])
        return l;
    throw new e.ErrnoError(33);
  }, getStreamChecked(l) {
    var Z = e.getStream(l);
    if (!Z)
      throw new e.ErrnoError(8);
    return Z;
  }, getStream: (l) => e.streams[l], createStream(l, Z = -1) {
    return V(Z >= -1), l = Object.assign(new e.FSStream(), l), Z == -1 && (Z = e.nextfd()), l.fd = Z, e.streams[Z] = l, l;
  }, closeStream(l) {
    e.streams[l] = null;
  }, dupStream(l, Z = -1) {
    var c = e.createStream(l, Z);
    return c.stream_ops?.dup?.(c), c;
  }, doSetAttr(l, Z, c) {
    var d = l?.stream_ops.setattr, b = d ? l : Z;
    d ??= Z.node_ops.setattr, e.checkOpExists(d, 63), d(b, c);
  }, chrdev_stream_ops: { open(l) {
    var Z = e.getDevice(l.node.rdev);
    l.stream_ops = Z.stream_ops, l.stream_ops.open?.(l);
  }, llseek() {
    throw new e.ErrnoError(70);
  } }, major: (l) => l >> 8, minor: (l) => l & 255, makedev: (l, Z) => l << 8 | Z, registerDevice(l, Z) {
    e.devices[l] = { stream_ops: Z };
  }, getDevice: (l) => e.devices[l], getMounts(l) {
    for (var Z = [], c = [l]; c.length; ) {
      var d = c.pop();
      Z.push(d), c.push(...d.mounts);
    }
    return Z;
  }, syncfs(l, Z) {
    typeof l == "function" && (Z = l, l = !1), e.syncFSRequests++, e.syncFSRequests > 1 && u(`warning: ${e.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
    var c = e.getMounts(e.root.mount), d = 0;
    function b(n) {
      return V(e.syncFSRequests > 0), e.syncFSRequests--, Z(n);
    }
    function t(n) {
      if (n)
        return t.errored ? void 0 : (t.errored = !0, b(n));
      ++d >= c.length && b(null);
    }
    for (var m of c)
      m.type.syncfs ? m.type.syncfs(m, l, t) : t(null);
  }, mount(l, Z, c) {
    if (typeof l == "string")
      throw l;
    var d = c === "/", b = !c, t;
    if (d && e.root)
      throw new e.ErrnoError(10);
    if (!d && !b) {
      var m = e.lookupPath(c, { follow_mount: !1 });
      if (c = m.path, t = m.node, e.isMountpoint(t))
        throw new e.ErrnoError(10);
      if (!e.isDir(t.mode))
        throw new e.ErrnoError(54);
    }
    var n = { type: l, opts: Z, mountpoint: c, mounts: [] }, W = l.mount(n);
    return W.mount = n, n.root = W, d ? e.root = W : t && (t.mounted = n, t.mount && t.mount.mounts.push(n)), W;
  }, unmount(l) {
    var Z = e.lookupPath(l, { follow_mount: !1 });
    if (!e.isMountpoint(Z.node))
      throw new e.ErrnoError(28);
    var c = Z.node, d = c.mounted, b = e.getMounts(d);
    for (var [t, m] of Object.entries(e.nameTable))
      for (; m; ) {
        var n = m.name_next;
        b.includes(m.mount) && e.destroyNode(m), m = n;
      }
    c.mounted = null;
    var W = c.mount.mounts.indexOf(d);
    V(W !== -1), c.mount.mounts.splice(W, 1);
  }, lookup(l, Z) {
    return l.node_ops.lookup(l, Z);
  }, mknod(l, Z, c) {
    var d = e.lookupPath(l, { parent: !0 }), b = d.node, t = J.basename(l);
    if (!t)
      throw new e.ErrnoError(28);
    if (t === "." || t === "..")
      throw new e.ErrnoError(20);
    var m = e.mayCreate(b, t);
    if (m)
      throw new e.ErrnoError(m);
    if (!b.node_ops.mknod)
      throw new e.ErrnoError(63);
    return b.node_ops.mknod(b, t, Z, c);
  }, statfs(l) {
    return e.statfsNode(e.lookupPath(l, { follow: !0 }).node);
  }, statfsStream(l) {
    return e.statfsNode(l.node);
  }, statfsNode(l) {
    var Z = { bsize: 4096, frsize: 4096, blocks: 1e6, bfree: 5e5, bavail: 5e5, files: e.nextInode, ffree: e.nextInode - 1, fsid: 42, flags: 2, namelen: 255 };
    return l.node_ops.statfs && Object.assign(Z, l.node_ops.statfs(l.mount.opts.root)), Z;
  }, create(l, Z = 438) {
    return Z &= 4095, Z |= 32768, e.mknod(l, Z, 0);
  }, mkdir(l, Z = 511) {
    return Z &= 1023, Z |= 16384, e.mknod(l, Z, 0);
  }, mkdirTree(l, Z) {
    var c = l.split("/"), d = "";
    for (var b of c)
      if (b) {
        (d || J.isAbs(l)) && (d += "/"), d += b;
        try {
          e.mkdir(d, Z);
        } catch (t) {
          if (t.errno != 20) throw t;
        }
      }
  }, mkdev(l, Z, c) {
    return typeof c > "u" && (c = Z, Z = 438), Z |= 8192, e.mknod(l, Z, c);
  }, symlink(l, Z) {
    if (!rl.resolve(l))
      throw new e.ErrnoError(44);
    var c = e.lookupPath(Z, { parent: !0 }), d = c.node;
    if (!d)
      throw new e.ErrnoError(44);
    var b = J.basename(Z), t = e.mayCreate(d, b);
    if (t)
      throw new e.ErrnoError(t);
    if (!d.node_ops.symlink)
      throw new e.ErrnoError(63);
    return d.node_ops.symlink(d, b, l);
  }, rename(l, Z) {
    var c = J.dirname(l), d = J.dirname(Z), b = J.basename(l), t = J.basename(Z), m, n, W;
    if (m = e.lookupPath(l, { parent: !0 }), n = m.node, m = e.lookupPath(Z, { parent: !0 }), W = m.node, !n || !W) throw new e.ErrnoError(44);
    if (n.mount !== W.mount)
      throw new e.ErrnoError(75);
    var X = e.lookupNode(n, b), G = rl.relative(l, d);
    if (G.charAt(0) !== ".")
      throw new e.ErrnoError(28);
    if (G = rl.relative(Z, c), G.charAt(0) !== ".")
      throw new e.ErrnoError(55);
    var R;
    try {
      R = e.lookupNode(W, t);
    } catch {
    }
    if (X !== R) {
      var r = e.isDir(X.mode), o = e.mayDelete(n, b, r);
      if (o)
        throw new e.ErrnoError(o);
      if (o = R ? e.mayDelete(W, t, r) : e.mayCreate(W, t), o)
        throw new e.ErrnoError(o);
      if (!n.node_ops.rename)
        throw new e.ErrnoError(63);
      if (e.isMountpoint(X) || R && e.isMountpoint(R))
        throw new e.ErrnoError(10);
      if (W !== n && (o = e.nodePermissions(n, "w"), o))
        throw new e.ErrnoError(o);
      e.hashRemoveNode(X);
      try {
        n.node_ops.rename(X, W, t), X.parent = W;
      } catch (Y) {
        throw Y;
      } finally {
        e.hashAddNode(X);
      }
    }
  }, rmdir(l) {
    var Z = e.lookupPath(l, { parent: !0 }), c = Z.node, d = J.basename(l), b = e.lookupNode(c, d), t = e.mayDelete(c, d, !0);
    if (t)
      throw new e.ErrnoError(t);
    if (!c.node_ops.rmdir)
      throw new e.ErrnoError(63);
    if (e.isMountpoint(b))
      throw new e.ErrnoError(10);
    c.node_ops.rmdir(c, d), e.destroyNode(b);
  }, readdir(l) {
    var Z = e.lookupPath(l, { follow: !0 }), c = Z.node, d = e.checkOpExists(c.node_ops.readdir, 54);
    return d(c);
  }, unlink(l) {
    var Z = e.lookupPath(l, { parent: !0 }), c = Z.node;
    if (!c)
      throw new e.ErrnoError(44);
    var d = J.basename(l), b = e.lookupNode(c, d), t = e.mayDelete(c, d, !1);
    if (t)
      throw new e.ErrnoError(t);
    if (!c.node_ops.unlink)
      throw new e.ErrnoError(63);
    if (e.isMountpoint(b))
      throw new e.ErrnoError(10);
    c.node_ops.unlink(c, d), e.destroyNode(b);
  }, readlink(l) {
    var Z = e.lookupPath(l), c = Z.node;
    if (!c)
      throw new e.ErrnoError(44);
    if (!c.node_ops.readlink)
      throw new e.ErrnoError(28);
    return c.node_ops.readlink(c);
  }, stat(l, Z) {
    var c = e.lookupPath(l, { follow: !Z }), d = c.node, b = e.checkOpExists(d.node_ops.getattr, 63);
    return b(d);
  }, fstat(l) {
    var Z = e.getStreamChecked(l), c = Z.node, d = Z.stream_ops.getattr, b = d ? Z : c;
    return d ??= c.node_ops.getattr, e.checkOpExists(d, 63), d(b);
  }, lstat(l) {
    return e.stat(l, !0);
  }, doChmod(l, Z, c, d) {
    e.doSetAttr(l, Z, { mode: c & 4095 | Z.mode & -4096, ctime: Date.now(), dontFollow: d });
  }, chmod(l, Z, c) {
    var d;
    if (typeof l == "string") {
      var b = e.lookupPath(l, { follow: !c });
      d = b.node;
    } else
      d = l;
    e.doChmod(null, d, Z, c);
  }, lchmod(l, Z) {
    e.chmod(l, Z, !0);
  }, fchmod(l, Z) {
    var c = e.getStreamChecked(l);
    e.doChmod(c, c.node, Z, !1);
  }, doChown(l, Z, c) {
    e.doSetAttr(l, Z, { timestamp: Date.now(), dontFollow: c });
  }, chown(l, Z, c, d) {
    var b;
    if (typeof l == "string") {
      var t = e.lookupPath(l, { follow: !d });
      b = t.node;
    } else
      b = l;
    e.doChown(null, b, d);
  }, lchown(l, Z, c) {
    e.chown(l, Z, c, !0);
  }, fchown(l, Z, c) {
    var d = e.getStreamChecked(l);
    e.doChown(d, d.node, !1);
  }, doTruncate(l, Z, c) {
    if (e.isDir(Z.mode))
      throw new e.ErrnoError(31);
    if (!e.isFile(Z.mode))
      throw new e.ErrnoError(28);
    var d = e.nodePermissions(Z, "w");
    if (d)
      throw new e.ErrnoError(d);
    e.doSetAttr(l, Z, { size: c, timestamp: Date.now() });
  }, truncate(l, Z) {
    if (Z < 0)
      throw new e.ErrnoError(28);
    var c;
    if (typeof l == "string") {
      var d = e.lookupPath(l, { follow: !0 });
      c = d.node;
    } else
      c = l;
    e.doTruncate(null, c, Z);
  }, ftruncate(l, Z) {
    var c = e.getStreamChecked(l);
    if (Z < 0 || (c.flags & 2097155) === 0)
      throw new e.ErrnoError(28);
    e.doTruncate(c, c.node, Z);
  }, utime(l, Z, c) {
    var d = e.lookupPath(l, { follow: !0 }), b = d.node, t = e.checkOpExists(b.node_ops.setattr, 63);
    t(b, { atime: Z, mtime: c });
  }, open(l, Z, c = 438) {
    if (l === "")
      throw new e.ErrnoError(44);
    Z = typeof Z == "string" ? sd(Z) : Z, Z & 64 ? c = c & 4095 | 32768 : c = 0;
    var d, b;
    if (typeof l == "object")
      d = l;
    else {
      b = l.endsWith("/");
      var t = e.lookupPath(l, { follow: !(Z & 131072), noent_okay: !0 });
      d = t.node, l = t.path;
    }
    var m = !1;
    if (Z & 64)
      if (d) {
        if (Z & 128)
          throw new e.ErrnoError(20);
      } else {
        if (b)
          throw new e.ErrnoError(31);
        d = e.mknod(l, c | 511, 0), m = !0;
      }
    if (!d)
      throw new e.ErrnoError(44);
    if (e.isChrdev(d.mode) && (Z &= -513), Z & 65536 && !e.isDir(d.mode))
      throw new e.ErrnoError(54);
    if (!m) {
      var n = e.mayOpen(d, Z);
      if (n)
        throw new e.ErrnoError(n);
    }
    Z & 512 && !m && e.truncate(d, 0), Z &= -131713;
    var W = e.createStream({ node: d, path: e.getPath(d), flags: Z, seekable: !0, position: 0, stream_ops: d.stream_ops, ungotten: [], error: !1 });
    return W.stream_ops.open && W.stream_ops.open(W), m && e.chmod(d, c & 511), a.logReadFiles && !(Z & 1) && (l in e.readFiles || (e.readFiles[l] = 1)), W;
  }, close(l) {
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    l.getdents && (l.getdents = null);
    try {
      l.stream_ops.close && l.stream_ops.close(l);
    } catch (Z) {
      throw Z;
    } finally {
      e.closeStream(l.fd);
    }
    l.fd = null;
  }, isClosed(l) {
    return l.fd === null;
  }, llseek(l, Z, c) {
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    if (!l.seekable || !l.stream_ops.llseek)
      throw new e.ErrnoError(70);
    if (c != 0 && c != 1 && c != 2)
      throw new e.ErrnoError(28);
    return l.position = l.stream_ops.llseek(l, Z, c), l.ungotten = [], l.position;
  }, read(l, Z, c, d, b) {
    if (V(c >= 0), d < 0 || b < 0)
      throw new e.ErrnoError(28);
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    if ((l.flags & 2097155) === 1)
      throw new e.ErrnoError(8);
    if (e.isDir(l.node.mode))
      throw new e.ErrnoError(31);
    if (!l.stream_ops.read)
      throw new e.ErrnoError(28);
    var t = typeof b < "u";
    if (!t)
      b = l.position;
    else if (!l.seekable)
      throw new e.ErrnoError(70);
    var m = l.stream_ops.read(l, Z, c, d, b);
    return t || (l.position += m), m;
  }, write(l, Z, c, d, b, t) {
    if (V(c >= 0), d < 0 || b < 0)
      throw new e.ErrnoError(28);
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    if ((l.flags & 2097155) === 0)
      throw new e.ErrnoError(8);
    if (e.isDir(l.node.mode))
      throw new e.ErrnoError(31);
    if (!l.stream_ops.write)
      throw new e.ErrnoError(28);
    l.seekable && l.flags & 1024 && e.llseek(l, 0, 2);
    var m = typeof b < "u";
    if (!m)
      b = l.position;
    else if (!l.seekable)
      throw new e.ErrnoError(70);
    var n = l.stream_ops.write(l, Z, c, d, b, t);
    return m || (l.position += n), n;
  }, mmap(l, Z, c, d, b) {
    if ((d & 2) !== 0 && (b & 2) === 0 && (l.flags & 2097155) !== 2)
      throw new e.ErrnoError(2);
    if ((l.flags & 2097155) === 1)
      throw new e.ErrnoError(2);
    if (!l.stream_ops.mmap)
      throw new e.ErrnoError(43);
    if (!Z)
      throw new e.ErrnoError(28);
    return l.stream_ops.mmap(l, Z, c, d, b);
  }, msync(l, Z, c, d, b) {
    return V(c >= 0), l.stream_ops.msync ? l.stream_ops.msync(l, Z, c, d, b) : 0;
  }, ioctl(l, Z, c) {
    if (!l.stream_ops.ioctl)
      throw new e.ErrnoError(59);
    return l.stream_ops.ioctl(l, Z, c);
  }, readFile(l, Z = {}) {
    Z.flags = Z.flags || 0, Z.encoding = Z.encoding || "binary", Z.encoding !== "utf8" && Z.encoding !== "binary" && N(`Invalid encoding type "${Z.encoding}"`);
    var c = e.open(l, Z.flags), d = e.stat(l), b = d.size, t = new Uint8Array(b);
    return e.read(c, t, 0, b, 0), Z.encoding === "utf8" && (t = al(t)), e.close(c), t;
  }, writeFile(l, Z, c = {}) {
    c.flags = c.flags || 577;
    var d = e.open(l, c.flags, c.mode);
    typeof Z == "string" && (Z = new Uint8Array(tZ(Z))), ArrayBuffer.isView(Z) ? e.write(d, Z, 0, Z.byteLength, void 0, c.canOwn) : N("Unsupported data type"), e.close(d);
  }, cwd: () => e.currentPath, chdir(l) {
    var Z = e.lookupPath(l, { follow: !0 });
    if (Z.node === null)
      throw new e.ErrnoError(44);
    if (!e.isDir(Z.node.mode))
      throw new e.ErrnoError(54);
    var c = e.nodePermissions(Z.node, "x");
    if (c)
      throw new e.ErrnoError(c);
    e.currentPath = Z.path;
  }, createDefaultDirectories() {
    e.mkdir("/tmp"), e.mkdir("/home"), e.mkdir("/home/web_user");
  }, createDefaultDevices() {
    e.mkdir("/dev"), e.registerDevice(e.makedev(1, 3), { read: () => 0, write: (d, b, t, m, n) => m, llseek: () => 0 }), e.mkdev("/dev/null", e.makedev(1, 3)), q.register(e.makedev(5, 0), q.default_tty_ops), q.register(e.makedev(6, 0), q.default_tty1_ops), e.mkdev("/dev/tty", e.makedev(5, 0)), e.mkdev("/dev/tty1", e.makedev(6, 0));
    var l = new Uint8Array(1024), Z = 0, c = () => (Z === 0 && (eZ(l), Z = l.byteLength), l[--Z]);
    e.createDevice("/dev", "random", c), e.createDevice("/dev", "urandom", c), e.mkdir("/dev/shm"), e.mkdir("/dev/shm/tmp");
  }, createSpecialDirectories() {
    e.mkdir("/proc");
    var l = e.mkdir("/proc/self");
    e.mkdir("/proc/self/fd"), e.mount({ mount() {
      var Z = e.createNode(l, "fd", 16895, 73);
      return Z.stream_ops = { llseek: h.stream_ops.llseek }, Z.node_ops = { lookup(c, d) {
        var b = +d, t = e.getStreamChecked(b), m = { parent: null, mount: { mountpoint: "fake" }, node_ops: { readlink: () => t.path }, id: b + 1 };
        return m.parent = m, m;
      }, readdir() {
        return Array.from(e.streams.entries()).filter(([c, d]) => d).map(([c, d]) => c.toString());
      } }, Z;
    } }, {}, "/proc/self/fd");
  }, createStandardStreams(l, Z, c) {
    l ? e.createDevice("/dev", "stdin", l) : e.symlink("/dev/tty", "/dev/stdin"), Z ? e.createDevice("/dev", "stdout", null, Z) : e.symlink("/dev/tty", "/dev/stdout"), c ? e.createDevice("/dev", "stderr", null, c) : e.symlink("/dev/tty1", "/dev/stderr");
    var d = e.open("/dev/stdin", 0), b = e.open("/dev/stdout", 1), t = e.open("/dev/stderr", 1);
    V(d.fd === 0, `invalid handle for stdin (${d.fd})`), V(b.fd === 1, `invalid handle for stdout (${b.fd})`), V(t.fd === 2, `invalid handle for stderr (${t.fd})`);
  }, staticInit() {
    e.nameTable = new Array(4096), e.mount(h, {}, "/"), e.createDefaultDirectories(), e.createDefaultDevices(), e.createSpecialDirectories(), e.filesystems = { MEMFS: h };
  }, init(l, Z, c) {
    V(!e.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), e.initialized = !0, l ??= a.stdin, Z ??= a.stdout, c ??= a.stderr, e.createStandardStreams(l, Z, c);
  }, quit() {
    e.initialized = !1, XZ(0);
    for (var l of e.streams)
      l && e.close(l);
  }, findObject(l, Z) {
    var c = e.analyzePath(l, Z);
    return c.exists ? c.object : null;
  }, analyzePath(l, Z) {
    try {
      var c = e.lookupPath(l, { follow: !Z });
      l = c.path;
    } catch {
    }
    var d = { isRoot: !1, exists: !1, error: 0, name: null, path: null, object: null, parentExists: !1, parentPath: null, parentObject: null };
    try {
      var c = e.lookupPath(l, { parent: !0 });
      d.parentExists = !0, d.parentPath = c.path, d.parentObject = c.node, d.name = J.basename(l), c = e.lookupPath(l, { follow: !Z }), d.exists = !0, d.path = c.path, d.object = c.node, d.name = c.node.name, d.isRoot = c.path === "/";
    } catch (b) {
      d.error = b.errno;
    }
    return d;
  }, createPath(l, Z, c, d) {
    l = typeof l == "string" ? l : e.getPath(l);
    for (var b = Z.split("/").reverse(); b.length; ) {
      var t = b.pop();
      if (t) {
        var m = J.join2(l, t);
        try {
          e.mkdir(m);
        } catch (n) {
          if (n.errno != 20) throw n;
        }
        l = m;
      }
    }
    return m;
  }, createFile(l, Z, c, d, b) {
    var t = J.join2(typeof l == "string" ? l : e.getPath(l), Z), m = mZ(d, b);
    return e.create(t, m);
  }, createDataFile(l, Z, c, d, b, t) {
    var m = Z;
    l && (l = typeof l == "string" ? l : e.getPath(l), m = Z ? J.join2(l, Z) : l);
    var n = mZ(d, b), W = e.create(m, n);
    if (c) {
      if (typeof c == "string") {
        for (var X = new Array(c.length), G = 0, R = c.length; G < R; ++G) X[G] = c.charCodeAt(G);
        c = X;
      }
      e.chmod(W, n | 146);
      var r = e.open(W, 577);
      e.write(r, c, 0, c.length, 0, t), e.close(r), e.chmod(W, n);
    }
  }, createDevice(l, Z, c, d) {
    var b = J.join2(typeof l == "string" ? l : e.getPath(l), Z), t = mZ(!!c, !!d);
    e.createDevice.major ??= 64;
    var m = e.makedev(e.createDevice.major++, 0);
    return e.registerDevice(m, { open(n) {
      n.seekable = !1;
    }, close(n) {
      d?.buffer?.length && d(10);
    }, read(n, W, X, G, R) {
      for (var r = 0, o = 0; o < G; o++) {
        var Y;
        try {
          Y = c();
        } catch {
          throw new e.ErrnoError(29);
        }
        if (Y === void 0 && r === 0)
          throw new e.ErrnoError(6);
        if (Y == null) break;
        r++, W[X + o] = Y;
      }
      return r && (n.node.atime = Date.now()), r;
    }, write(n, W, X, G, R) {
      for (var r = 0; r < G; r++)
        try {
          d(W[X + r]);
        } catch {
          throw new e.ErrnoError(29);
        }
      return G && (n.node.mtime = n.node.ctime = Date.now()), r;
    } }), e.mkdev(b, t, m);
  }, forceLoadFile(l) {
    if (l.isDevice || l.isFolder || l.link || l.contents) return !0;
    if (globalThis.XMLHttpRequest)
      N("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    else
      try {
        l.contents = vl(l.url);
      } catch {
        throw new e.ErrnoError(29);
      }
  }, createLazyFile(l, Z, c, d, b) {
    class t {
      lengthKnown = !1;
      chunks = [];
      get(r) {
        if (!(r > this.length - 1 || r < 0)) {
          var o = r % this.chunkSize, Y = r / this.chunkSize | 0;
          return this.getter(Y)[o];
        }
      }
      setDataGetter(r) {
        this.getter = r;
      }
      cacheLength() {
        var r = new XMLHttpRequest();
        r.open("HEAD", c, !1), r.send(null), r.status >= 200 && r.status < 300 || r.status === 304 || N("Couldn't load " + c + ". Status: " + r.status);
        var o = Number(r.getResponseHeader("Content-length")), Y, F = (Y = r.getResponseHeader("Accept-Ranges")) && Y === "bytes", T = (Y = r.getResponseHeader("Content-Encoding")) && Y === "gzip", z = 1024 * 1024;
        F || (z = o);
        var S = (L, sl) => {
          L > sl && N("invalid range (" + L + ", " + sl + ") or no bytes requested!"), sl > o - 1 && N("only " + o + " bytes available! programmer error!");
          var B = new XMLHttpRequest();
          return B.open("GET", c, !1), o !== z && B.setRequestHeader("Range", "bytes=" + L + "-" + sl), B.responseType = "arraybuffer", B.overrideMimeType && B.overrideMimeType("text/plain; charset=x-user-defined"), B.send(null), B.status >= 200 && B.status < 300 || B.status === 304 || N("Couldn't load " + c + ". Status: " + B.status), B.response !== void 0 ? new Uint8Array(B.response || []) : tZ(B.responseText || "");
        }, Il = this;
        Il.setDataGetter((L) => {
          var sl = L * z, B = (L + 1) * z - 1;
          return B = Math.min(B, o - 1), typeof Il.chunks[L] > "u" && (Il.chunks[L] = S(sl, B)), typeof Il.chunks[L] > "u" && N("doXHR failed!"), Il.chunks[L];
        }), (T || !o) && (z = o = 1, o = this.getter(0).length, z = o, A("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = o, this._chunkSize = z, this.lengthKnown = !0;
      }
      get length() {
        return this.lengthKnown || this.cacheLength(), this._length;
      }
      get chunkSize() {
        return this.lengthKnown || this.cacheLength(), this._chunkSize;
      }
    }
    if (globalThis.XMLHttpRequest) {
      D || N("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
      var m = new t(), n = { isDevice: !1, contents: m };
    } else
      var n = { isDevice: !1, url: c };
    var W = e.createFile(l, Z, n, d, b);
    n.contents ? W.contents = n.contents : n.url && (W.contents = null, W.url = n.url), Object.defineProperties(W, { usedBytes: { get: function() {
      return this.contents.length;
    } } });
    var X = {};
    for (const [R, r] of Object.entries(W.stream_ops))
      X[R] = (...o) => (e.forceLoadFile(W), r(...o));
    function G(R, r, o, Y, F) {
      var T = R.node.contents;
      if (F >= T.length) return 0;
      var z = Math.min(T.length - F, Y);
      if (V(z >= 0), T.slice)
        for (var S = 0; S < z; S++)
          r[o + S] = T[F + S];
      else
        for (var S = 0; S < z; S++)
          r[o + S] = T.get(F + S);
      return z;
    }
    return X.read = (R, r, o, Y, F) => (e.forceLoadFile(W), G(R, r, o, Y, F)), X.mmap = (R, r, o, Y, F) => {
      e.forceLoadFile(W);
      var T = AZ(r);
      if (!T)
        throw new e.ErrnoError(48);
      return G(R, (i(), g), T, r, o), { ptr: T, allocated: !0 };
    }, W.stream_ops = X, W;
  }, absolutePath() {
    N("FS.absolutePath has been removed; use PATH_FS.resolve instead");
  }, createFolder() {
    N("FS.createFolder has been removed; use FS.mkdir instead");
  }, createLink() {
    N("FS.createLink has been removed; use FS.symlink instead");
  }, joinPath() {
    N("FS.joinPath has been removed; use PATH.join instead");
  }, mmapAlloc() {
    N("FS.mmapAlloc has been replaced by the top level function mmapAlloc");
  }, standardizePath() {
    N("FS.standardizePath has been removed; use PATH.normalize instead");
  } }, v = { DEFAULT_POLLMASK: 5, calculateAt(l, Z, c) {
    if (J.isAbs(Z))
      return Z;
    var d;
    if (l === -100)
      d = e.cwd();
    else {
      var b = v.getStreamFromFD(l);
      d = b.path;
    }
    if (Z.length == 0) {
      if (!c)
        throw new e.ErrnoError(44);
      return d;
    }
    return d + "/" + Z;
  }, writeStat(l, Z) {
    (i(), y)[l >> 2] = Z.dev, (i(), y)[l + 4 >> 2] = Z.mode, (i(), y)[l + 8 >> 2] = Z.nlink, (i(), y)[l + 12 >> 2] = Z.uid, (i(), y)[l + 16 >> 2] = Z.gid, (i(), y)[l + 20 >> 2] = Z.rdev, (i(), k)[l + 24 >> 3] = BigInt(Z.size), (i(), U)[l + 32 >> 2] = 4096, (i(), U)[l + 36 >> 2] = Z.blocks;
    var c = Z.atime.getTime(), d = Z.mtime.getTime(), b = Z.ctime.getTime();
    return (i(), k)[l + 40 >> 3] = BigInt(Math.floor(c / 1e3)), (i(), y)[l + 48 >> 2] = c % 1e3 * 1e3 * 1e3, (i(), k)[l + 56 >> 3] = BigInt(Math.floor(d / 1e3)), (i(), y)[l + 64 >> 2] = d % 1e3 * 1e3 * 1e3, (i(), k)[l + 72 >> 3] = BigInt(Math.floor(b / 1e3)), (i(), y)[l + 80 >> 2] = b % 1e3 * 1e3 * 1e3, (i(), k)[l + 88 >> 3] = BigInt(Z.ino), 0;
  }, writeStatFs(l, Z) {
    (i(), y)[l + 4 >> 2] = Z.bsize, (i(), y)[l + 60 >> 2] = Z.bsize, (i(), k)[l + 8 >> 3] = BigInt(Z.blocks), (i(), k)[l + 16 >> 3] = BigInt(Z.bfree), (i(), k)[l + 24 >> 3] = BigInt(Z.bavail), (i(), k)[l + 32 >> 3] = BigInt(Z.files), (i(), k)[l + 40 >> 3] = BigInt(Z.ffree), (i(), y)[l + 48 >> 2] = Z.fsid, (i(), y)[l + 64 >> 2] = Z.flags, (i(), y)[l + 56 >> 2] = Z.namelen;
  }, doMsync(l, Z, c, d, b) {
    if (!e.isFile(Z.node.mode))
      throw new e.ErrnoError(43);
    if (d & 2)
      return 0;
    var t = (i(), K).slice(l, l + c);
    e.msync(Z, t, b, c, d);
  }, getStreamFromFD(l) {
    var Z = e.getStreamChecked(l);
    return Z;
  }, varargs: void 0, getStr(l) {
    var Z = Q(l);
    return Z;
  } };
  function Zc(l, Z, c) {
    if (s) return f(3, 0, 1, l, Z, c);
    v.varargs = c;
    try {
      var d = v.getStreamFromFD(l);
      switch (Z) {
        case 0: {
          var b = Hl();
          if (b < 0)
            return -28;
          for (; e.streams[b]; )
            b++;
          var t;
          return t = e.dupStream(d, b), t.fd;
        }
        case 1:
        case 2:
          return 0;
        case 3:
          return d.flags;
        case 4: {
          var b = Hl();
          return d.flags |= b, 0;
        }
        case 12: {
          var b = Xl(), m = 0;
          return (i(), Zl)[b + m >> 1] = 2, 0;
        }
        case 13:
        case 14:
          return 0;
      }
      return -28;
    } catch (n) {
      if (typeof e > "u" || n.name !== "ErrnoError") throw n;
      return -n.errno;
    }
  }
  function cc(l, Z) {
    if (s) return f(4, 0, 1, l, Z);
    try {
      return v.writeStat(Z, e.fstat(l));
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return -c.errno;
    }
  }
  function dc(l, Z, c) {
    if (s) return f(5, 0, 1, l, Z, c);
    v.varargs = c;
    try {
      var d = v.getStreamFromFD(l);
      switch (Z) {
        case 21509:
          return d.tty ? 0 : -59;
        case 21505: {
          if (!d.tty) return -59;
          if (d.tty.ops.ioctl_tcgets) {
            var b = d.tty.ops.ioctl_tcgets(d), t = Xl();
            (i(), U)[t >> 2] = b.c_iflag || 0, (i(), U)[t + 4 >> 2] = b.c_oflag || 0, (i(), U)[t + 8 >> 2] = b.c_cflag || 0, (i(), U)[t + 12 >> 2] = b.c_lflag || 0;
            for (var m = 0; m < 32; m++)
              (i(), g)[t + m + 17] = b.c_cc[m] || 0;
            return 0;
          }
          return 0;
        }
        case 21510:
        case 21511:
        case 21512:
          return d.tty ? 0 : -59;
        case 21506:
        case 21507:
        case 21508: {
          if (!d.tty) return -59;
          if (d.tty.ops.ioctl_tcsets) {
            for (var t = Xl(), n = (i(), U)[t >> 2], W = (i(), U)[t + 4 >> 2], X = (i(), U)[t + 8 >> 2], G = (i(), U)[t + 12 >> 2], R = [], m = 0; m < 32; m++)
              R.push((i(), g)[t + m + 17]);
            return d.tty.ops.ioctl_tcsets(d.tty, Z, { c_iflag: n, c_oflag: W, c_cflag: X, c_lflag: G, c_cc: R });
          }
          return 0;
        }
        case 21519: {
          if (!d.tty) return -59;
          var t = Xl();
          return (i(), U)[t >> 2] = 0, 0;
        }
        case 21520:
          return d.tty ? -28 : -59;
        case 21537:
        case 21531: {
          var t = Xl();
          return e.ioctl(d, Z, t);
        }
        case 21523: {
          if (!d.tty) return -59;
          if (d.tty.ops.ioctl_tiocgwinsz) {
            var r = d.tty.ops.ioctl_tiocgwinsz(d.tty), t = Xl();
            (i(), Zl)[t >> 1] = r[0], (i(), Zl)[t + 2 >> 1] = r[1];
          }
          return 0;
        }
        case 21524:
          return d.tty ? 0 : -59;
        case 21515:
          return d.tty ? 0 : -59;
        default:
          return -28;
      }
    } catch (o) {
      if (typeof e > "u" || o.name !== "ErrnoError") throw o;
      return -o.errno;
    }
  }
  function ec(l, Z) {
    if (s) return f(6, 0, 1, l, Z);
    try {
      return l = v.getStr(l), v.writeStat(Z, e.lstat(l));
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return -c.errno;
    }
  }
  function bc(l, Z, c, d) {
    if (s) return f(7, 0, 1, l, Z, c, d);
    try {
      Z = v.getStr(Z);
      var b = d & 256, t = d & 4096;
      return d = d & -6401, V(!d, `unknown flags in __syscall_newfstatat: ${d}`), Z = v.calculateAt(l, Z, t), v.writeStat(c, b ? e.lstat(Z) : e.stat(Z));
    } catch (m) {
      if (typeof e > "u" || m.name !== "ErrnoError") throw m;
      return -m.errno;
    }
  }
  function tc(l, Z, c, d) {
    if (s) return f(8, 0, 1, l, Z, c, d);
    v.varargs = d;
    try {
      Z = v.getStr(Z), Z = v.calculateAt(l, Z);
      var b = d ? Hl() : 0;
      return e.open(Z, c, b).fd;
    } catch (t) {
      if (typeof e > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function mc(l, Z) {
    if (s) return f(9, 0, 1, l, Z);
    try {
      return l = v.getStr(l), v.writeStat(Z, e.stat(l));
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return -c.errno;
    }
  }
  var Id = () => N("native code called abort()"), w = (l) => {
    for (var Z = ""; ; ) {
      var c = (i(), K)[l++];
      if (!c) return Z;
      Z += String.fromCharCode(c);
    }
  }, Gl = {}, yl = {}, fl = {}, Nd = class extends Error {
    constructor(Z) {
      super(Z), this.name = "BindingError";
    }
  }, x = (l) => {
    throw new Nd(l);
  };
  function vd(l, Z, c = {}) {
    var d = Z.name;
    if (l || x(`type "${d}" must have a positive integer typeid pointer`), yl.hasOwnProperty(l)) {
      if (c.ignoreDuplicateRegistrations)
        return;
      x(`Cannot register type '${d}' twice`);
    }
    if (yl[l] = Z, delete fl[l], Gl.hasOwnProperty(l)) {
      var b = Gl[l];
      delete Gl[l], b.forEach((t) => t());
    }
  }
  function E(l, Z, c = {}) {
    return vd(l, Z, c);
  }
  var nc = (l, Z, c) => {
    switch (Z) {
      case 1:
        return c ? (d) => (i(), g)[d] : (d) => (i(), K)[d];
      case 2:
        return c ? (d) => (i(), Zl)[d >> 1] : (d) => (i(), Fl)[d >> 1];
      case 4:
        return c ? (d) => (i(), U)[d >> 2] : (d) => (i(), y)[d >> 2];
      case 8:
        return c ? (d) => (i(), k)[d >> 3] : (d) => (i(), NZ)[d >> 3];
      default:
        throw new TypeError(`invalid integer width (${Z}): ${l}`);
    }
  }, gl = (l) => {
    if (l === null)
      return "null";
    var Z = typeof l;
    return Z === "object" || Z === "array" || Z === "function" ? l.toString() : "" + l;
  }, Wc = (l, Z, c, d) => {
    if (Z < c || Z > d)
      throw new TypeError(`Passing a number "${gl(Z)}" from JS side to C/C++ side to an argument of type "${l}", which is outside the valid range [${c}, ${d}]!`);
  }, Ud = (l, Z, c, d, b) => {
    Z = w(Z);
    const t = d === 0n;
    let m = (n) => n;
    if (t) {
      const n = c * 8;
      m = (W) => BigInt.asUintN(n, W), b = m(b);
    }
    E(l, { name: Z, fromWireType: m, toWireType: (n, W) => {
      if (typeof W == "number")
        W = BigInt(W);
      else if (typeof W != "bigint")
        throw new TypeError(`Cannot convert "${gl(W)}" to ${this.name}`);
      return Wc(Z, W, d, b), W;
    }, readValueFromPointer: nc(Z, c, !t), destructorFunction: null });
  }, Td = (l, Z, c, d) => {
    Z = w(Z), E(l, { name: Z, fromWireType: function(b) {
      return !!b;
    }, toWireType: function(b, t) {
      return t ? c : d;
    }, readValueFromPointer: function(b) {
      return this.fromWireType((i(), K)[b]);
    }, destructorFunction: null });
  }, Vc = [], j = [0, 1, , 1, null, 1, !0, 1, !1, 1], kd = (l) => {
    l > 9 && --j[l + 1] === 0 && (V(j[l] !== void 0, "Decref for unallocated handle."), j[l] = void 0, Vc.push(l));
  }, ic = { toValue: (l) => (l || x(`Cannot use deleted val. handle = ${l}`), V(l === 2 || j[l] !== void 0 && l % 2 === 0, `invalid handle: ${l}`), j[l]), toHandle: (l) => {
    switch (l) {
      case void 0:
        return 2;
      case null:
        return 4;
      case !0:
        return 6;
      case !1:
        return 8;
      default: {
        const Z = Vc.pop() || j.length;
        return j[Z] = l, j[Z + 1] = 1, Z;
      }
    }
  } };
  function nZ(l) {
    return this.fromWireType((i(), y)[l >> 2]);
  }
  var Hd = { name: "emscripten::val", fromWireType: (l) => {
    var Z = ic.toValue(l);
    return kd(l), Z;
  }, toWireType: (l, Z) => ic.toHandle(Z), readValueFromPointer: nZ, destructorFunction: null }, fd = (l) => E(l, Hd), gd = (l, Z) => {
    switch (Z) {
      case 4:
        return function(c) {
          return this.fromWireType((i(), IZ)[c >> 2]);
        };
      case 8:
        return function(c) {
          return this.fromWireType((i(), pl)[c >> 3]);
        };
      default:
        throw new TypeError(`invalid float width (${Z}): ${l}`);
    }
  }, Bd = (l, Z, c) => {
    Z = w(Z), E(l, { name: Z, fromWireType: (d) => d, toWireType: (d, b) => {
      if (typeof b != "number" && typeof b != "boolean")
        throw new TypeError(`Cannot convert ${gl(b)} to ${this.name}`);
      return b;
    }, readValueFromPointer: gd(Z, c), destructorFunction: null });
  }, ac = (l, Z) => Object.defineProperty(Z, "name", { value: l }), zd = (l) => {
    for (; l.length; ) {
      var Z = l.pop(), c = l.pop();
      c(Z);
    }
  };
  function Xc(l) {
    for (var Z = 1; Z < l.length; ++Z)
      if (l[Z] !== null && l[Z].destructorFunction === void 0)
        return !0;
    return !1;
  }
  function Sd(l, Z, c, d, b) {
    if (l < Z || l > c) {
      var t = Z == c ? Z : `${Z} to ${c}`;
      b(`function ${d} called with ${l} arguments, expected ${t}`);
    }
  }
  function Kd(l, Z, c, d) {
    for (var b = Xc(l), t = l.length - 2, m = [], n = ["fn"], W = 0; W < t; ++W)
      m.push(`arg${W}`), n.push(`arg${W}Wired`);
    m = m.join(","), n = n.join(",");
    var X = `return function (${m}) {
`;
    X += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, b && (X += `var destructors = [];
`);
    for (var G = b ? "destructors" : "null", R = ["humanName", "throwBindingError", "invoker", "fn", "runDestructors", "fromRetWire", "toClassParamWire"], W = 0; W < t; ++W) {
      var r = `toArg${W}Wire`;
      X += `var arg${W}Wired = ${r}(${G}, arg${W});
`, R.push(r);
    }
    X += (c || d ? "var rv = " : "") + `invoker(${n});
`;
    var o = c ? "rv" : "";
    if (X += `function onDone(${o}) {
`, b)
      X += `runDestructors(destructors);
`;
    else
      for (var W = 2; W < l.length; ++W) {
        var Y = W === 1 ? "thisWired" : "arg" + (W - 2) + "Wired";
        l[W].destructorFunction !== null && (X += `${Y}_dtor(${Y});
`, R.push(`${Y}_dtor`));
      }
    return c && (X += `var ret = fromRetWire(rv);
return ret;
`), X += `}
`, X += "return " + (d ? "rv.then(onDone)" : `onDone(${o})`) + ";", X += `}
`, R.push("checkArgCount", "minArgs", "maxArgs"), X = `if (arguments.length !== ${R.length}){ throw new Error(humanName + "Expected ${R.length} closure arguments " + arguments.length + " given."); }
${X}`, new Function(R, X);
  }
  function wd(l) {
    for (var Z = l.length - 2, c = l.length - 1; c >= 2 && l[c].optional; --c)
      Z--;
    return Z;
  }
  function xd(l, Z, c, d, b, t) {
    var m = Z.length;
    m < 2 && x("argTypes array size mismatch! Must at least get return value and 'this' types!");
    for (var n = Z[1] !== null && c !== null, W = Xc(Z), X = !Z[0].isVoid, G = m - 2, R = wd(Z), r = Z[0], o = Z[1], Y = [l, x, d, b, zd, r.fromWireType.bind(r), o?.toWireType.bind(o)], F = 2; F < m; ++F) {
      var T = Z[F];
      Y.push(T.toWireType.bind(T));
    }
    if (!W)
      for (var F = 2; F < Z.length; ++F)
        Z[F].destructorFunction !== null && Y.push(Z[F].destructorFunction);
    Y.push(Sd, R, G);
    var S = Kd(Z, n, X, t)(...Y);
    return ac(l, S);
  }
  var Cd = (l, Z, c) => {
    if (l[Z].overloadTable === void 0) {
      var d = l[Z];
      l[Z] = function(...b) {
        return l[Z].overloadTable.hasOwnProperty(b.length) || x(`Function '${c}' called with an invalid number of arguments (${b.length}) - expects one of (${l[Z].overloadTable})!`), l[Z].overloadTable[b.length].apply(this, b);
      }, l[Z].overloadTable = [], l[Z].overloadTable[d.argCount] = d;
    }
  }, Ed = (l, Z, c) => {
    a.hasOwnProperty(l) ? ((c === void 0 || a[l].overloadTable !== void 0 && a[l].overloadTable[c] !== void 0) && x(`Cannot register public name '${l}' twice`), Cd(a, l, l), a[l].overloadTable.hasOwnProperty(c) && x(`Cannot register multiple overloads of a function with the same number of arguments (${c})!`), a[l].overloadTable[c] = Z) : (a[l] = Z, a[l].argCount = c);
  }, Md = (l, Z) => {
    for (var c = [], d = 0; d < l; d++)
      c.push((i(), y)[Z + d * 4 >> 2]);
    return c;
  }, Ld = class extends Error {
    constructor(Z) {
      super(Z), this.name = "InternalError";
    }
  }, rc = (l) => {
    throw new Ld(l);
  }, Qd = (l, Z, c) => {
    a.hasOwnProperty(l) || rc("Replacing nonexistent public symbol"), a[l].overloadTable !== void 0 && c !== void 0 ? a[l].overloadTable[c] = Z : (a[l] = Z, a[l].argCount = c);
  }, jd = (l, Z, c = !1) => {
    l = w(l);
    function d() {
      var t = MZ(Z);
      return c && (t = WebAssembly.promising(t)), t;
    }
    var b = d();
    return typeof b != "function" && x(`unknown function pointer with signature ${l}: ${Z}`), b;
  };
  class Pd extends Error {
  }
  var Od = (l) => {
    var Z = gc(l), c = w(Z);
    return O(Z), c;
  }, _d = (l, Z) => {
    var c = [], d = {};
    function b(t) {
      if (!d[t] && !yl[t]) {
        if (fl[t]) {
          fl[t].forEach(b);
          return;
        }
        c.push(t), d[t] = !0;
      }
    }
    throw Z.forEach(b), new Pd(`${l}: ` + c.map(Od).join([", "]));
  }, Dd = (l, Z, c) => {
    l.forEach((n) => fl[n] = Z);
    function d(n) {
      var W = c(n);
      W.length !== l.length && rc("Mismatched type converter count");
      for (var X = 0; X < l.length; ++X)
        E(l[X], W[X]);
    }
    var b = new Array(Z.length), t = [], m = 0;
    for (let [n, W] of Z.entries())
      yl.hasOwnProperty(W) ? b[n] = yl[W] : (t.push(W), Gl.hasOwnProperty(W) || (Gl[W] = []), Gl[W].push(() => {
        b[n] = yl[W], ++m, m === t.length && d(b);
      }));
    t.length === 0 && d(b);
  }, Ad = (l) => {
    l = l.trim();
    const Z = l.indexOf("(");
    return Z === -1 ? l : (V(l.endsWith(")"), "Parentheses for argument names should match."), l.slice(0, Z));
  }, $d = (l, Z, c, d, b, t, m, n) => {
    var W = Md(Z, c);
    l = w(l), l = Ad(l), b = jd(d, b, m), Ed(l, function() {
      _d(`Cannot call ${l} due to unbound types`, W);
    }, Z - 1), Dd([], W, (X) => {
      var G = [X[0], null].concat(X.slice(1));
      return Qd(l, xd(l, G, null, b, t, m), Z - 1), [];
    });
  }, qd = (l, Z, c, d, b) => {
    Z = w(Z);
    const t = d === 0;
    let m = (W) => W;
    if (t) {
      var n = 32 - 8 * c;
      m = (W) => W << n >>> n, b = m(b);
    }
    E(l, { name: Z, fromWireType: m, toWireType: (W, X) => {
      if (typeof X != "number" && typeof X != "boolean")
        throw new TypeError(`Cannot convert "${gl(X)}" to ${Z}`);
      return Wc(Z, X, d, b), X;
    }, readValueFromPointer: nc(Z, c, d !== 0), destructorFunction: null });
  }, le = (l, Z, c) => {
    var d = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array], b = d[Z];
    function t(m) {
      var n = (i(), y)[m >> 2], W = (i(), y)[m + 4 >> 2];
      return new b((i(), g).buffer, W, n);
    }
    c = w(c), E(l, { name: c, fromWireType: t, readValueFromPointer: t }, { ignoreDuplicateRegistrations: !0 });
  }, P = (l, Z, c) => (V(typeof c == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), _Z(l, (i(), K), Z, c)), Ze = (l, Z) => {
    Z = w(Z), E(l, { name: Z, fromWireType(c) {
      var d = (i(), y)[c >> 2], b = c + 4, t;
      return t = Q(b, d, !0), O(c), t;
    }, toWireType(c, d) {
      d instanceof ArrayBuffer && (d = new Uint8Array(d));
      var b, t = typeof d == "string";
      t || ArrayBuffer.isView(d) && d.BYTES_PER_ELEMENT == 1 || x("Cannot pass non-string to std::string"), t ? b = bl(d) : b = d.length;
      var m = xl(4 + b + 1), n = m + 4;
      return (i(), y)[m >> 2] = b, t ? P(d, n, b + 1) : (i(), K).set(d, n), c !== null && c.push(O, m), m;
    }, readValueFromPointer: nZ, destructorFunction(c) {
      O(c);
    } });
  }, Gc = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, ce = (l, Z, c) => {
    V(l % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
    var d = l >> 1, b = jZ((i(), Fl), d, Z / 2, c);
    if (b - d > 16 && Gc) return Gc.decode((i(), Fl).slice(d, b));
    for (var t = "", m = d; m < b; ++m) {
      var n = (i(), Fl)[m];
      t += String.fromCharCode(n);
    }
    return t;
  }, de = (l, Z, c) => {
    if (V(Z % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), V(typeof c == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), c ??= 2147483647, c < 2) return 0;
    c -= 2;
    for (var d = Z, b = c < l.length * 2 ? c / 2 : l.length, t = 0; t < b; ++t) {
      var m = l.charCodeAt(t);
      (i(), Zl)[Z >> 1] = m, Z += 2;
    }
    return (i(), Zl)[Z >> 1] = 0, Z - d;
  }, ee = (l) => l.length * 2, be = (l, Z, c) => {
    V(l % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
    for (var d = "", b = l >> 2, t = 0; !(t >= Z / 4); t++) {
      var m = (i(), y)[b + t];
      if (!m && !c) break;
      d += String.fromCodePoint(m);
    }
    return d;
  }, te = (l, Z, c) => {
    if (V(Z % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), V(typeof c == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), c ??= 2147483647, c < 4) return 0;
    for (var d = Z, b = d + c - 4, t = 0; t < l.length; ++t) {
      var m = l.codePointAt(t);
      if (m > 65535 && t++, (i(), U)[Z >> 2] = m, Z += 4, Z + 4 > b) break;
    }
    return (i(), U)[Z >> 2] = 0, Z - d;
  }, me = (l) => {
    for (var Z = 0, c = 0; c < l.length; ++c) {
      var d = l.codePointAt(c);
      d > 65535 && c++, Z += 4;
    }
    return Z;
  }, ne = (l, Z, c) => {
    c = w(c);
    var d, b, t;
    Z === 2 ? (d = ce, b = de, t = ee) : (V(Z === 4, "only 2-byte and 4-byte strings are currently supported"), d = be, b = te, t = me), E(l, { name: c, fromWireType: (m) => {
      var n = (i(), y)[m >> 2], W = d(m + 4, n * Z, !0);
      return O(m), W;
    }, toWireType: (m, n) => {
      typeof n != "string" && x(`Cannot pass non-string to C++ string type ${c}`);
      var W = t(n), X = xl(4 + W + Z);
      return (i(), y)[X >> 2] = W / Z, b(n, X + 4, W + Z), m !== null && m.push(O, X), X;
    }, readValueFromPointer: nZ, destructorFunction(m) {
      O(m);
    } });
  }, We = (l, Z) => {
    Z = w(Z), E(l, { isVoid: !0, name: Z, fromWireType: () => {
    }, toWireType: (c, d) => {
    } });
  }, Ve = (l) => {
    rZ(l, !D, 1, !Nl, 65536, !1), p.threadInitTLS();
  }, yc = (l) => {
    if (l instanceof kZ || l == "unwind")
      return nl;
    ul(), l instanceof WebAssembly.RuntimeError && oZ() <= 0 && u("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), hZ(1, l);
  }, ie = () => {
    if (!kl())
      try {
        if (s) {
          ol() && yZ(nl);
          return;
        }
        ZZ(nl);
      } catch (l) {
        yc(l);
      }
  }, oc = (l) => {
    if (ml) {
      u("user callback triggered after runtime exited or application aborted.  Ignoring.");
      return;
    }
    try {
      l(), ie();
    } catch (Z) {
      yc(Z);
    }
  }, WZ = (l) => {
    if (Atomics.waitAsync) {
      var Z = Atomics.waitAsync((i(), U), l >> 2, l);
      V(Z.async), Z.value.then(Bl);
      var c = l + 128;
      Atomics.store((i(), U), c >> 2, 1);
    }
  }, Bl = () => oc(() => {
    var l = ol();
    l && (WZ(l), Cc());
  }), ae = (l, Z) => {
    if (l == Z)
      setTimeout(Bl);
    else if (s)
      postMessage({ targetThread: l, cmd: "checkMailbox" });
    else {
      var c = p.pthreads[l];
      if (!c) {
        u(`Cannot send message to thread with ID ${l}, unknown thread ID!`);
        return;
      }
      c.postMessage({ cmd: "checkMailbox" });
    }
  }, zl = [], Xe = (l, Z, c, d, b) => {
    d /= 2, zl.length = d;
    for (var t = b >> 3, m = 0; m < d; m++)
      (i(), k)[t + 2 * m] ? zl[m] = (i(), k)[t + 2 * m + 1] : zl[m] = (i(), pl)[t + 2 * m + 1];
    var n = Z ? aZ[Z] : je[l];
    V(!(l && Z)), V(n.length == d, "Call args mismatch in _emscripten_receive_on_main_thread_js"), p.currentProxiedOperationCallerThread = c;
    var W = n(...zl);
    return p.currentProxiedOperationCallerThread = 0, V(typeof W != "bigint"), W;
  }, re = (l) => {
    s ? postMessage({ cmd: "cleanupThread", thread: l }) : fZ(l);
  }, Ge = (l) => {
  }, ye = 9007199254740992, oe = -9007199254740992, VZ = (l) => l < oe || l > ye ? NaN : Number(l);
  function Rc(l, Z, c, d, b, t, m) {
    if (s) return f(10, 0, 1, l, Z, c, d, b, t, m);
    b = VZ(b);
    try {
      V(!isNaN(b));
      var n = v.getStreamFromFD(d), W = e.mmap(n, l, b, Z, c), X = W.ptr;
      return (i(), U)[t >> 2] = W.allocated, (i(), y)[m >> 2] = X, 0;
    } catch (G) {
      if (typeof e > "u" || G.name !== "ErrnoError") throw G;
      return -G.errno;
    }
  }
  function sc(l, Z, c, d, b, t) {
    if (s) return f(11, 0, 1, l, Z, c, d, b, t);
    t = VZ(t);
    try {
      var m = v.getStreamFromFD(b);
      c & 2 && v.doMsync(l, m, Z, d, t);
    } catch (n) {
      if (typeof e > "u" || n.name !== "ErrnoError") throw n;
      return -n.errno;
    }
  }
  var Re = (l, Z, c, d) => {
    var b = (/* @__PURE__ */ new Date()).getFullYear(), t = new Date(b, 0, 1), m = new Date(b, 6, 1), n = t.getTimezoneOffset(), W = m.getTimezoneOffset(), X = Math.max(n, W);
    (i(), y)[l >> 2] = X * 60, (i(), U)[Z >> 2] = +(n != W);
    var G = (o) => {
      var Y = o >= 0 ? "-" : "+", F = Math.abs(o), T = String(Math.floor(F / 60)).padStart(2, "0"), z = String(F % 60).padStart(2, "0");
      return `UTC${Y}${T}${z}`;
    }, R = G(n), r = G(W);
    V(R), V(r), V(bl(R) <= 16, `timezone name truncated to fit in TZNAME_MAX (${R})`), V(bl(r) <= 16, `timezone name truncated to fit in TZNAME_MAX (${r})`), W < n ? (P(R, c, 17), P(r, d, 17)) : (P(R, d, 17), P(r, c, 17));
  }, hc = () => performance.timeOrigin + performance.now(), se = () => Date.now(), he = (l) => l >= 0 && l <= 3;
  function ue(l, Z, c) {
    if (!he(l))
      return 28;
    var d;
    l === 0 ? d = se() : d = hc();
    var b = Math.round(d * 1e3 * 1e3);
    return (i(), k)[c >> 3] = BigInt(b), 0;
  }
  var Sl = [], Fe = (l, Z) => {
    V(Array.isArray(Sl)), V(Z % 16 == 0), Sl.length = 0;
    for (var c; c = (i(), K)[l++]; ) {
      var d = String.fromCharCode(c), b = ["d", "f", "i", "p"];
      b.push("j"), V(b.includes(d), `Invalid character ${c}("${d}") in readEmAsmArgs! Use only [${b}], and do not specify "v" for void return argument.`);
      var t = c != 105;
      t &= c != 112, Z += t && Z % 8 ? 4 : 0, Sl.push(c == 112 ? (i(), y)[Z >> 2] : c == 106 ? (i(), k)[Z >> 3] : c == 105 ? (i(), U)[Z >> 2] : (i(), pl)[Z >> 3]), Z += t ? 8 : 4;
    }
    return Sl;
  }, pe = (l, Z, c) => {
    var d = Fe(Z, c);
    return V(aZ.hasOwnProperty(l), `No EM_ASM constant found at address ${l}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), aZ[l](...d);
  }, Ye = (l, Z, c) => pe(l, Z, c), Je = () => {
    D || $("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
  }, Ie = (l, Z) => u(Q(l, Z)), uc = () => {
    il += 1;
  }, Ne = () => {
    throw uc(), "unwind";
  }, Fc = () => 2147483648, ve = () => Fc(), Ue = () => navigator.hardwareConcurrency, tl = {}, pc = (l) => {
    var Z = bl(l) + 1, c = xl(Z);
    return c && P(l, c, Z), c;
  }, Kl = (l) => {
    var Z;
    if (Z = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(l))
      return +Z[1];
    if (Z = /\bwasm-function\[(\d+)\]:(\d+)/.exec(l))
      $("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
    else if (Z = /:(\d+):\d+(?:\)|$)/.exec(l))
      return 2147483648 | +Z[1];
    return 0;
  }, Yc = (l) => {
    for (var Z of l) {
      var c = Kl(Z);
      c && (tl[c] = Z);
    }
  }, Jc = () => new Error().stack.toString(), Te = () => {
    var l = Jc().split(`
`);
    return l[0] == "Error" && l.shift(), Yc(l), tl.last_addr = Kl(l[3]), tl.last_stack = l, tl.last_addr;
  }, wl = (l) => {
    var Z = tl[l];
    if (!Z) return 0;
    var c, d;
    if (d = /^\s+at .*\.wasm\.(.*) \(.*\)$/.exec(Z))
      c = d[1];
    else if (d = /^\s+at (.*) \(.*\)$/.exec(Z))
      c = d[1];
    else if (d = /^(.+?)@/.exec(Z))
      c = d[1];
    else
      return 0;
    return O(wl.ret ?? 0), wl.ret = pc(c), wl.ret;
  }, ke = (l) => {
    var Z = C.buffer.byteLength, c = (l - Z + 65535) / 65536 | 0;
    try {
      return C.grow(c), Tl(), 1;
    } catch (d) {
      u(`growMemory: Attempted to grow heap from ${Z} bytes to ${l} bytes, but got error: ${d}`);
    }
  }, He = (l) => {
    var Z = (i(), K).length;
    if (l >>>= 0, l <= Z)
      return !1;
    var c = Fc();
    if (l > c)
      return u(`Cannot enlarge memory, requested ${l} bytes, but the limit is ${c} bytes!`), !1;
    for (var d = 1; d <= 4; d *= 2) {
      var b = Z * (1 + 0.2 / d);
      b = Math.min(b, l + 100663296);
      var t = Math.min(c, DZ(Math.max(l, b), 65536)), m = ke(t);
      if (m)
        return !0;
    }
    return u(`Failed to grow the heap from ${Z} bytes to ${t} bytes, not enough memory!`), !1;
  }, fe = (l, Z, c) => {
    var d;
    tl.last_addr == l ? d = tl.last_stack : (d = Jc().split(`
`), d[0] == "Error" && d.shift(), Yc(d));
    for (var b = 3; d[b] && Kl(d[b]) != l; )
      ++b;
    for (var t = 0; t < c && d[t + b]; ++t)
      (i(), U)[Z + t * 4 >> 2] = Kl(d[t + b]);
    return t;
  }, iZ = {}, ge = () => sZ || "./this.program", Jl = () => {
    if (!Jl.strings) {
      var l = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", Z = { USER: "web_user", LOGNAME: "web_user", PATH: "/", PWD: "/", HOME: "/home/web_user", LANG: l, _: ge() };
      for (var c in iZ)
        iZ[c] === void 0 ? delete Z[c] : Z[c] = iZ[c];
      var d = [];
      for (var c in Z)
        d.push(`${c}=${Z[c]}`);
      Jl.strings = d;
    }
    return Jl.strings;
  };
  function Ic(l, Z) {
    if (s) return f(12, 0, 1, l, Z);
    var c = 0, d = 0;
    for (var b of Jl()) {
      var t = Z + c;
      (i(), y)[l + d >> 2] = t, c += P(b, t, 1 / 0) + 1, d += 4;
    }
    return 0;
  }
  function Nc(l, Z) {
    if (s) return f(13, 0, 1, l, Z);
    var c = Jl();
    (i(), y)[l >> 2] = c.length;
    var d = 0;
    for (var b of c)
      d += bl(b) + 1;
    return (i(), y)[Z >> 2] = d, 0;
  }
  function vc(l) {
    if (s) return f(14, 0, 1, l);
    try {
      var Z = v.getStreamFromFD(l);
      return e.close(Z), 0;
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return c.errno;
    }
  }
  var Be = (l, Z, c, d) => {
    for (var b = 0, t = 0; t < c; t++) {
      var m = (i(), y)[Z >> 2], n = (i(), y)[Z + 4 >> 2];
      Z += 8;
      var W = e.read(l, (i(), g), m, n, d);
      if (W < 0) return -1;
      if (b += W, W < n) break;
    }
    return b;
  };
  function Uc(l, Z, c, d) {
    if (s) return f(15, 0, 1, l, Z, c, d);
    try {
      var b = v.getStreamFromFD(l), t = Be(b, Z, c);
      return (i(), y)[d >> 2] = t, 0;
    } catch (m) {
      if (typeof e > "u" || m.name !== "ErrnoError") throw m;
      return m.errno;
    }
  }
  function Tc(l, Z, c, d) {
    if (s) return f(16, 0, 1, l, Z, c, d);
    Z = VZ(Z);
    try {
      if (isNaN(Z)) return 61;
      var b = v.getStreamFromFD(l);
      return e.llseek(b, Z, c), (i(), k)[d >> 3] = BigInt(b.position), b.getdents && Z === 0 && c === 0 && (b.getdents = null), 0;
    } catch (t) {
      if (typeof e > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var ze = (l, Z, c, d) => {
    for (var b = 0, t = 0; t < c; t++) {
      var m = (i(), y)[Z >> 2], n = (i(), y)[Z + 4 >> 2];
      Z += 8;
      var W = e.write(l, (i(), g), m, n, d);
      if (W < 0) return -1;
      if (b += W, W < n)
        break;
    }
    return b;
  };
  function kc(l, Z, c, d) {
    if (s) return f(17, 0, 1, l, Z, c, d);
    try {
      var b = v.getStreamFromFD(l), t = ze(b, Z, c);
      return (i(), y)[d >> 2] = t, 0;
    } catch (m) {
      if (typeof e > "u" || m.name !== "ErrnoError") throw m;
      return m.errno;
    }
  }
  function Se(l, Z) {
    try {
      return eZ((i(), K).subarray(l, l + Z)), 0;
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return c.errno;
    }
  }
  var Ke = () => {
    V(il > 0), il -= 1;
  }, M = { instrumentWasmImports(l) {
    V("Suspending" in WebAssembly, "JSPI not supported by current environment. Perhaps it needs to be enabled via flags?");
    var Z = /^(invoke_.*|__asyncjs__.*)$/;
    for (let [c, d] of Object.entries(l))
      typeof d == "function" && (d.isAsync || Z.test(c)) && (l[c] = d = new WebAssembly.Suspending(d));
  }, instrumentFunction(l) {
    var Z = (...c) => l(...c);
    return Z = ac(`__asyncify_wrapper_${l.name}`, Z), Z;
  }, instrumentWasmExports(l) {
    var Z = /^(solve_model|validate_model|main|__main_argc_argv)$/;
    M.asyncExports = /* @__PURE__ */ new Set();
    var c = {};
    for (let [b, t] of Object.entries(l))
      if (typeof t == "function") {
        Z.test(b) && (M.asyncExports.add(t), t = M.makeAsyncFunction(t));
        var d = M.instrumentFunction(t);
        c[b] = d;
      } else
        c[b] = t;
    return c;
  }, asyncExports: null, isAsyncExport(l) {
    return M.asyncExports?.has(l);
  }, handleAsync: async (l) => {
    uc();
    try {
      return await l();
    } finally {
      Ke();
    }
  }, handleSleep: (l) => M.handleAsync(() => new Promise(l)), makeAsyncFunction(l) {
    return WebAssembly.promising(l);
  } }, we = (l) => {
    var Z = a["_" + l];
    return V(Z, "Cannot call unknown function " + l + ", make sure it is exported"), Z;
  }, xe = (l, Z) => {
    V(l.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (i(), g).set(l, Z);
  }, Ce = (l) => {
    var Z = bl(l) + 1, c = ql(Z);
    return P(l, c, Z), c;
  }, Hc = (l, Z, c, d, b) => {
    var t = { string: (F) => {
      var T = 0;
      return F != null && F !== 0 && (T = Ce(F)), T;
    }, array: (F) => {
      var T = ql(F.length);
      return xe(F, T), T;
    } };
    function m(F) {
      return Z === "string" ? Q(F) : Z === "boolean" ? !!F : F;
    }
    var n = we(l), W = [], X = 0;
    if (V(Z !== "array", 'Return type should not be "array".'), d)
      for (var G = 0; G < d.length; G++) {
        var R = t[c[G]];
        R ? (X === 0 && (X = xZ()), W[G] = R(d[G])) : W[G] = d[G];
      }
    var r = n(...W);
    function o(F) {
      return X !== 0 && $l(X), m(F);
    }
    var Y = b?.async;
    return Y ? r.then(o) : (r = o(r), r);
  }, Ee = (l, Z, c, d) => (...b) => Hc(l, Z, c, b, d), Me = (...l) => pc(...l);
  p.init(), e.createPreloadedFile = Jd, e.preloadFile = lc, e.staticInit(), V(j.length === 10);
  {
    if (ld(), a.noExitRuntime && (dZ = a.noExitRuntime), a.preloadPlugins && (qZ = a.preloadPlugins), a.print && (A = a.print), a.printErr && (u = a.printErr), a.wasmBinary && (hl = a.wasmBinary), Pe(), a.arguments && a.arguments, a.thisProgram && (sZ = a.thisProgram), V(typeof a.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), V(typeof a.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), V(typeof a.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), V(typeof a.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), V(typeof a.read > "u", "Module.read option was removed"), V(typeof a.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), V(typeof a.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), V(typeof a.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), V(typeof a.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), V(typeof a.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), V(typeof a.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), a.preInit)
      for (typeof a.preInit == "function" && (a.preInit = [a.preInit]); a.preInit.length > 0; )
        a.preInit.shift()();
    Ul("preInit");
  }
  a.ccall = Hc, a.cwrap = Ee, a.UTF8ToString = Q, a.stringToUTF8 = P, a.allocateUTF8 = Me;
  var Le = ["writeI53ToI64", "writeI53ToI64Clamped", "writeI53ToI64Signaling", "writeI53ToU64Clamped", "writeI53ToU64Signaling", "readI53FromI64", "readI53FromU64", "convertI32PairToI53", "convertI32PairToI53Checked", "convertU32PairToI53", "getTempRet0", "setTempRet0", "withStackSave", "inetPton4", "inetNtop4", "inetPton6", "inetNtop6", "readSockaddr", "writeSockaddr", "runMainThreadEmAsm", "jstoi_q", "autoResumeAudioContext", "getDynCaller", "dynCall", "asmjsMangle", "HandleAllocator", "addOnInit", "addOnPostCtor", "addOnPreMain", "addOnExit", "STACK_SIZE", "STACK_ALIGN", "POINTER_SIZE", "ASSERTIONS", "convertJsFunctionToWasm", "getEmptyTableSlot", "updateTableMap", "getFunctionAddress", "addFunction", "removeFunction", "intArrayToString", "stringToAscii", "registerKeyEventCallback", "maybeCStringToJsString", "findEventTarget", "getBoundingClientRect", "fillMouseEventData", "registerMouseEventCallback", "registerWheelEventCallback", "registerUiEventCallback", "registerFocusEventCallback", "fillDeviceOrientationEventData", "registerDeviceOrientationEventCallback", "fillDeviceMotionEventData", "registerDeviceMotionEventCallback", "screenOrientation", "fillOrientationChangeEventData", "registerOrientationChangeEventCallback", "fillFullscreenChangeEventData", "registerFullscreenChangeEventCallback", "JSEvents_requestFullscreen", "JSEvents_resizeCanvasForFullscreen", "registerRestoreOldStyle", "hideEverythingExceptGivenElement", "restoreHiddenElements", "setLetterbox", "softFullscreenResizeWebGLRenderTarget", "doRequestFullscreen", "fillPointerlockChangeEventData", "registerPointerlockChangeEventCallback", "registerPointerlockErrorEventCallback", "requestPointerLock", "fillVisibilityChangeEventData", "registerVisibilityChangeEventCallback", "registerTouchEventCallback", "fillGamepadEventData", "registerGamepadEventCallback", "registerBeforeUnloadEventCallback", "fillBatteryEventData", "registerBatteryEventCallback", "setCanvasElementSizeCallingThread", "setCanvasElementSizeMainThread", "setCanvasElementSize", "getCanvasSizeCallingThread", "getCanvasSizeMainThread", "getCanvasElementSize", "getCallstack", "convertPCtoSourceLocation", "wasiRightsToMuslOFlags", "wasiOFlagsToMuslOFlags", "safeSetTimeout", "setImmediateWrapped", "safeRequestAnimationFrame", "clearImmediateWrapped", "registerPostMainLoop", "registerPreMainLoop", "getPromise", "makePromise", "idsToPromises", "makePromiseCallback", "findMatchingCatch", "Browser_asyncPrepareDataCounter", "isLeapYear", "ydayFromDate", "arraySum", "addDays", "getSocketFromFD", "getSocketAddress", "FS_mkdirTree", "_setNetworkCallback", "heapObjectForWebGLType", "toTypedArrayIndex", "webgl_enable_ANGLE_instanced_arrays", "webgl_enable_OES_vertex_array_object", "webgl_enable_WEBGL_draw_buffers", "webgl_enable_WEBGL_multi_draw", "webgl_enable_EXT_polygon_offset_clamp", "webgl_enable_EXT_clip_control", "webgl_enable_WEBGL_polygon_mode", "emscriptenWebGLGet", "computeUnpackAlignedImageSize", "colorChannelsInGlTextureFormat", "emscriptenWebGLGetTexPixelData", "emscriptenWebGLGetUniform", "webglGetUniformLocation", "webglPrepareUniformLocationsBeforeFirstUse", "webglGetLeftBracePos", "emscriptenWebGLGetVertexAttrib", "__glGetActiveAttribOrUniform", "writeGLArray", "emscripten_webgl_destroy_context_before_on_calling_thread", "registerWebGlEventCallback", "ALLOC_NORMAL", "ALLOC_STACK", "allocate", "writeStringToMemory", "writeAsciiToMemory", "allocateUTF8OnStack", "demangle", "stackTrace", "getNativeTypeSize", "getFunctionArgsName", "requireRegisteredType", "createJsInvokerSignature", "PureVirtualError", "getBasestPointer", "registerInheritedInstance", "unregisterInheritedInstance", "getInheritedInstance", "getInheritedInstanceCount", "getLiveInheritedInstances", "enumReadValueFromPointer", "genericPointerToWireType", "constNoSmartPtrRawPointerToWireType", "nonConstNoSmartPtrRawPointerToWireType", "init_RegisteredPointer", "RegisteredPointer", "RegisteredPointer_fromWireType", "runDestructor", "releaseClassHandle", "detachFinalizer", "attachFinalizer", "makeClassHandle", "init_ClassHandle", "ClassHandle", "throwInstanceAlreadyDeleted", "flushPendingDeletes", "setDelayFunction", "RegisteredClass", "shallowCopyInternalPointer", "downcastPointer", "upcastPointer", "validateThis", "char_0", "char_9", "makeLegalFunctionName", "count_emval_handles", "getStringOrSymbol", "emval_returnValue", "emval_lookupTypes", "emval_addMethodCaller"];
  Le.forEach($c);
  var Qe = ["run", "out", "err", "callMain", "abort", "wasmExports", "HEAPF32", "HEAPF64", "HEAP8", "HEAP16", "HEAPU16", "HEAP32", "HEAPU32", "HEAP64", "HEAPU64", "writeStackCookie", "checkStackCookie", "INT53_MAX", "INT53_MIN", "bigintToI53Checked", "stackSave", "stackRestore", "stackAlloc", "createNamedFunction", "ptrToString", "zeroMemory", "exitJS", "getHeapMax", "growMemory", "ENV", "ERRNO_CODES", "strError", "DNS", "Protocols", "Sockets", "timers", "warnOnce", "readEmAsmArgsArray", "readEmAsmArgs", "runEmAsmFunction", "getExecutableName", "handleException", "keepRuntimeAlive", "runtimeKeepalivePush", "runtimeKeepalivePop", "callUserCallback", "maybeExit", "asyncLoad", "alignMemory", "mmapAlloc", "wasmTable", "wasmMemory", "getUniqueRunDependency", "noExitRuntime", "addRunDependency", "removeRunDependency", "addOnPreRun", "addOnPostRun", "freeTableIndexes", "functionsInTableMap", "setValue", "getValue", "PATH", "PATH_FS", "UTF8Decoder", "UTF8ArrayToString", "stringToUTF8Array", "lengthBytesUTF8", "intArrayFromString", "AsciiToString", "UTF16Decoder", "UTF16ToString", "stringToUTF16", "lengthBytesUTF16", "UTF32ToString", "stringToUTF32", "lengthBytesUTF32", "stringToNewUTF8", "stringToUTF8OnStack", "writeArrayToMemory", "JSEvents", "specialHTMLTargets", "findCanvasEventTarget", "currentFullscreenStrategy", "restoreOldWindowedStyle", "jsStackTrace", "UNWIND_CACHE", "ExitStatus", "getEnvStrings", "checkWasiClock", "doReadv", "doWritev", "initRandomFill", "randomFill", "emSetImmediate", "emClearImmediate_deps", "emClearImmediate", "promiseMap", "uncaughtExceptionCount", "exceptionLast", "exceptionCaught", "ExceptionInfo", "Browser", "requestFullscreen", "requestFullScreen", "setCanvasSize", "getUserMedia", "createContext", "getPreloadedImageData__data", "wget", "MONTH_DAYS_REGULAR", "MONTH_DAYS_LEAP", "MONTH_DAYS_REGULAR_CUMULATIVE", "MONTH_DAYS_LEAP_CUMULATIVE", "SYSCALLS", "preloadPlugins", "FS_createPreloadedFile", "FS_preloadFile", "FS_modeStringToFlags", "FS_getMode", "FS_stdin_getChar_buffer", "FS_stdin_getChar", "FS_unlink", "FS_createPath", "FS_createDevice", "FS_readFile", "FS", "FS_root", "FS_mounts", "FS_devices", "FS_streams", "FS_nextInode", "FS_nameTable", "FS_currentPath", "FS_initialized", "FS_ignorePermissions", "FS_filesystems", "FS_syncFSRequests", "FS_readFiles", "FS_lookupPath", "FS_getPath", "FS_hashName", "FS_hashAddNode", "FS_hashRemoveNode", "FS_lookupNode", "FS_createNode", "FS_destroyNode", "FS_isRoot", "FS_isMountpoint", "FS_isFile", "FS_isDir", "FS_isLink", "FS_isChrdev", "FS_isBlkdev", "FS_isFIFO", "FS_isSocket", "FS_flagsToPermissionString", "FS_nodePermissions", "FS_mayLookup", "FS_mayCreate", "FS_mayDelete", "FS_mayOpen", "FS_checkOpExists", "FS_nextfd", "FS_getStreamChecked", "FS_getStream", "FS_createStream", "FS_closeStream", "FS_dupStream", "FS_doSetAttr", "FS_chrdev_stream_ops", "FS_major", "FS_minor", "FS_makedev", "FS_registerDevice", "FS_getDevice", "FS_getMounts", "FS_syncfs", "FS_mount", "FS_unmount", "FS_lookup", "FS_mknod", "FS_statfs", "FS_statfsStream", "FS_statfsNode", "FS_create", "FS_mkdir", "FS_mkdev", "FS_symlink", "FS_rename", "FS_rmdir", "FS_readdir", "FS_readlink", "FS_stat", "FS_fstat", "FS_lstat", "FS_doChmod", "FS_chmod", "FS_lchmod", "FS_fchmod", "FS_doChown", "FS_chown", "FS_lchown", "FS_fchown", "FS_doTruncate", "FS_truncate", "FS_ftruncate", "FS_utime", "FS_open", "FS_close", "FS_isClosed", "FS_llseek", "FS_read", "FS_write", "FS_mmap", "FS_msync", "FS_ioctl", "FS_writeFile", "FS_cwd", "FS_chdir", "FS_createDefaultDirectories", "FS_createDefaultDevices", "FS_createSpecialDirectories", "FS_createStandardStreams", "FS_staticInit", "FS_init", "FS_quit", "FS_findObject", "FS_analyzePath", "FS_createFile", "FS_createDataFile", "FS_forceLoadFile", "FS_createLazyFile", "FS_absolutePath", "FS_createFolder", "FS_createLink", "FS_joinPath", "FS_mmapAlloc", "FS_standardizePath", "MEMFS", "TTY", "PIPEFS", "SOCKFS", "tempFixedLengthArray", "miniTempWebGLFloatBuffers", "miniTempWebGLIntBuffers", "GL", "AL", "GLUT", "EGL", "GLEW", "IDBStore", "runAndAbortIfError", "Asyncify", "Fibers", "SDL", "SDL_gfx", "print", "printErr", "jstoi_s", "PThread", "terminateWorker", "cleanupThread", "registerTLSInit", "spawnThread", "exitOnMainThread", "proxyToMainThread", "proxiedJSCallArgs", "invokeEntryPoint", "checkMailbox", "InternalError", "BindingError", "throwInternalError", "throwBindingError", "registeredTypes", "awaitingDependencies", "typeDependencies", "tupleRegistrations", "structRegistrations", "sharedRegisterType", "whenDependentTypesAreResolved", "getTypeName", "getFunctionName", "heap32VectorToArray", "usesDestructorStack", "checkArgCount", "getRequiredArgCount", "createJsInvoker", "UnboundTypeError", "EmValType", "EmValOptionalType", "throwUnboundTypeError", "ensureOverloadTable", "exposePublicSymbol", "replacePublicSymbol", "embindRepr", "registeredInstances", "registeredPointers", "registerType", "integerReadValueFromPointer", "floatReadValueFromPointer", "assertIntegerRange", "readPointer", "runDestructors", "craftInvokerFunction", "embind__requireFunction", "finalizationRegistry", "detachFinalizer_deps", "deletionQueue", "delayFunction", "emval_freelist", "emval_handles", "emval_symbols", "Emval", "emval_methodCallers"];
  Qe.forEach(pZ);
  var je = [lZ, CZ, PZ, Zc, cc, dc, ec, bc, tc, mc, Rc, sc, Ic, Nc, vc, Uc, Tc, kc];
  function Pe() {
    Dc("fetchSettings");
  }
  var aZ = { 540932: () => typeof wasmOffsetConverter < "u" };
  function fc() {
    return typeof wasmOffsetConverter < "u";
  }
  fc.sig = "i";
  var gc = I("___getTypeName"), Bc = I("__embind_initialize_bindings");
  a._get_cp_model_schema = I("_get_cp_model_schema"), a._get_sat_parameters_schema = I("_get_sat_parameters_schema"), a._solve_model = I("_solve_model");
  var xl = a._malloc = I("_malloc");
  a._free_buffer = I("_free_buffer");
  var O = a._free = I("_free");
  a._interrupt_solve = I("_interrupt_solve"), a._validate_model = I("_validate_model");
  var ol = I("_pthread_self"), XZ = I("_fflush"), zc = I("_strerror"), Sc = I("_emscripten_builtin_memalign"), rZ = I("__emscripten_thread_init"), Kc = I("__emscripten_thread_crashed"), GZ = I("_emscripten_stack_get_end"), wc = I("__emscripten_run_js_on_main_thread"), xc = I("__emscripten_thread_free_data"), yZ = I("__emscripten_thread_exit"), Cc = I("__emscripten_check_mailbox"), Ec = I("_emscripten_stack_init"), Mc = I("_emscripten_stack_set_limits"), Lc = I("__emscripten_stack_restore"), Qc = I("__emscripten_stack_alloc"), oZ = I("_emscripten_stack_get_current"), jc = I("wasmTable");
  function Oe(l) {
    V(typeof l.__getTypeName < "u", "missing Wasm export: __getTypeName"), V(typeof l._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), V(typeof l.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), V(typeof l.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), V(typeof l.solve_model < "u", "missing Wasm export: solve_model"), V(typeof l.malloc < "u", "missing Wasm export: malloc"), V(typeof l.free_buffer < "u", "missing Wasm export: free_buffer"), V(typeof l.free < "u", "missing Wasm export: free"), V(typeof l.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), V(typeof l.validate_model < "u", "missing Wasm export: validate_model"), V(typeof l.pthread_self < "u", "missing Wasm export: pthread_self"), V(typeof l.fflush < "u", "missing Wasm export: fflush"), V(typeof l.strerror < "u", "missing Wasm export: strerror"), V(typeof l._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), V(typeof l.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), V(typeof l._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), V(typeof l._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), V(typeof l.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), V(typeof l.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), V(typeof l._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), V(typeof l._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), V(typeof l._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), V(typeof l._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), V(typeof l.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), V(typeof l.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), V(typeof l.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), V(typeof l._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), V(typeof l._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), V(typeof l.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), V(typeof l.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), gc = H("__getTypeName", 1), Bc = H("_embind_initialize_bindings", 0), a._get_cp_model_schema = H("get_cp_model_schema", 0), a._get_sat_parameters_schema = H("get_sat_parameters_schema", 0), a._solve_model = H("solve_model", 5), xl = a._malloc = H("malloc", 1), a._free_buffer = H("free_buffer", 1), O = a._free = H("free", 1), a._interrupt_solve = H("interrupt_solve", 0), a._validate_model = H("validate_model", 3), ol = H("pthread_self", 0), XZ = H("fflush", 1), zc = H("strerror", 1), Sc = H("emscripten_builtin_memalign", 2), rZ = H("_emscripten_thread_init", 6), Kc = H("_emscripten_thread_crashed", 0), GZ = l.emscripten_stack_get_end, l.emscripten_stack_get_base, wc = H("_emscripten_run_js_on_main_thread", 5), xc = H("_emscripten_thread_free_data", 1), yZ = H("_emscripten_thread_exit", 1), Cc = H("_emscripten_check_mailbox", 0), Ec = l.emscripten_stack_init, Mc = l.emscripten_stack_set_limits, l.emscripten_stack_get_free, Lc = l._emscripten_stack_restore, Qc = l._emscripten_stack_alloc, oZ = l.emscripten_stack_get_current, jc = l.__indirect_function_table;
  }
  var Rl;
  function _e() {
    Rl = { HaveOffsetConverter: fc, __assert_fail: ad, __cxa_throw: rd, __pthread_create_js: OZ, __syscall_fcntl64: Zc, __syscall_fstat64: cc, __syscall_ioctl: dc, __syscall_lstat64: ec, __syscall_newfstatat: bc, __syscall_openat: tc, __syscall_stat64: mc, _abort_js: Id, _embind_register_bigint: Ud, _embind_register_bool: Td, _embind_register_emval: fd, _embind_register_float: Bd, _embind_register_function: $d, _embind_register_integer: qd, _embind_register_memory_view: le, _embind_register_std_string: Ze, _embind_register_std_wstring: ne, _embind_register_void: We, _emscripten_init_main_thread_js: Ve, _emscripten_notify_mailbox_postmessage: ae, _emscripten_receive_on_main_thread_js: Xe, _emscripten_thread_cleanup: re, _emscripten_thread_mailbox_await: WZ, _emscripten_thread_set_strongref: Ge, _mmap_js: Rc, _munmap_js: sc, _tzset_js: Re, clock_time_get: ue, emscripten_asm_const_int: Ye, emscripten_check_blocking_allowed: Je, emscripten_errn: Ie, emscripten_exit_with_live_runtime: Ne, emscripten_get_heap_max: ve, emscripten_get_now: hc, emscripten_num_logical_cores: Ue, emscripten_pc_get_function: wl, emscripten_resize_heap: He, emscripten_stack_snapshot: Te, emscripten_stack_unwind_buffer: fe, environ_get: Ic, environ_sizes_get: Nc, exit: ZZ, fd_close: vc, fd_read: Uc, fd_seek: Tc, fd_write: kc, memory: C, proc_exit: lZ, random_get: Se };
  }
  var Pc;
  function De() {
    V(!s), Ec(), FZ();
  }
  function Cl() {
    if (cl > 0) {
      Yl = Cl;
      return;
    }
    if (s) {
      Ol?.(a), vZ();
      return;
    }
    if (De(), Zd(), cl > 0) {
      Yl = Cl;
      return;
    }
    async function l() {
      V(!Pc), Pc = !0, a.calledRun = !0, !ml && (vZ(), Ol?.(a), a.onRuntimeInitialized?.(), Ul("onRuntimeInitialized"), V(!a._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), cd());
    }
    a.setStatus ? (a.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => a.setStatus(""), 1), l();
    }, 1)) : l(), ul();
  }
  function Ae() {
    var l = A, Z = u, c = !1;
    A = u = (W) => {
      c = !0;
    };
    try {
      XZ(0);
      for (var d of ["stdout", "stderr"]) {
        var b = e.analyzePath("/dev/" + d);
        if (!b) return;
        var t = b.object, m = t.rdev, n = q.ttys[m];
        n?.output?.length && (c = !0);
      }
    } catch {
    }
    A = l, u = Z, c && $("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
  }
  var _;
  s || (_ = await TZ(), Cl()), Wl ? Ml = a : Ml = new Promise((l, Z) => {
    Ol = l, _l = Z;
  });
  for (const l of Object.keys(a))
    l in El || Object.defineProperty(El, l, { configurable: !0, get() {
      N(`Access to module property ('${l}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
    } });
  return Ml;
}
var qe = globalThis.self?.name?.startsWith("em-pthread");
qe && $e();
export {
  $e as default
};
//# sourceMappingURL=cp_sat_runtime-BbvQWOd5.js.map
